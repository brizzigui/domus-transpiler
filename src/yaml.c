#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdarg.h>
#include <sys/time.h>
#include "yaml.h"

static char **split_csv(const char *s, int *count) {
    *count = 0;
    if(!s) return NULL;
    /* count commas */
    int commas = 0; for(const char *p=s; *p; ++p) if(*p==',') commas++;
    int n = commas + 1;
    char **arr = calloc(n, sizeof(char*));
    const char *start = s;
    int idx = 0;
    for(const char *p=s; ; ++p) {
        if(*p==',' || *p=='\0') {
            size_t len = p - start;
            char *t = malloc(len+1);
            strncpy(t, start, len); t[len]='\0';
            arr[idx++] = t;
            if(*p=='\0') break;
            start = p+1;
        }
    }
    *count = n;
    return arr;
}

static void free_csv(char **arr, int n) {
    if(!arr) return; for(int i=0;i<n;i++) free(arr[i]); free(arr);
}

static void print_indented(FILE *out, int indent, const char *fmt, ...) {
    va_list ap; va_start(ap, fmt);
    for(int i=0;i<indent;i++) fputc(' ', out);
    vfprintf(out, fmt, ap);
    va_end(ap);
}

/* Check if a string is a known HA domain that starts a new action */
static int is_ha_domain(const char *s) {
    if(!s) return 0;
    static const char *domains[] = {
        "switch", "light", "timer", "notify", "media_player",
        "cover", "alexa_devices", "tts", "input_boolean",
        "input_number", "input_select", "input_text",
        "scene", "script", "fan", "climate", "lock",
        "vacuum", "camera", "alarm_control_panel",
        NULL
    };
    for(int i=0; domains[i]; i++) {
        if(strcmp(s, domains[i])==0) return 1;
    }
    return 0;
}

/* Check if a string is a known HA domain used in device triggers */
static int is_trigger_domain(const char *s) {
    if(!s) return 0;
    static const char *domains[] = {
        "switch", "light", "binary_sensor", "sensor", "cover",
        "alarm_control_panel", "media_player", "fan", "lock",
        "climate", "vacuum", "camera",
        NULL
    };
    for(int i=0; domains[i]; i++) {
        if(strcmp(s, domains[i])==0) return 1;
    }
    return 0;
}

/* Parse a domus duration string like 00h01m00s into h/m/s components */
static int parse_duration(const char *val, int *h, int *m, int *s) {
    if(!val) return 0;
    const char *p = val;
    *h = 0; *m = 0; *s = 0;
    if(isdigit((unsigned char)p[0]) && isdigit((unsigned char)p[1]) && p[2]=='h'
       && isdigit((unsigned char)p[3]) && isdigit((unsigned char)p[4]) && p[5]=='m'
       && isdigit((unsigned char)p[6]) && isdigit((unsigned char)p[7]) && p[8]=='s') {
        *h = (p[0]-'0')*10 + (p[1]-'0');
        *m = (p[3]-'0')*10 + (p[4]-'0');
        *s = (p[6]-'0')*10 + (p[7]-'0');
        return 1;
    }
    return 0;
}

static void print_attr_value(FILE *out, int indent, const char *key, const char *val) {
    if(!val) {
        print_indented(out, indent, "%s: \n", key);
        return;
    }
    /* special-case: convert duration-style offsets (e.g. -01h15m00s) to colon format (-01:15:00) */
    if(key && val && strcmp(key, "offset") == 0) {
        const char *s = val;
        char sign = 0;
        if(*s == '+' || *s == '-') { sign = *s; s++; }
        /* expect HHhMMmSSs... */
        if(isdigit((unsigned char)s[0]) && isdigit((unsigned char)s[1]) && s[2]=='h'
           && isdigit((unsigned char)s[3]) && isdigit((unsigned char)s[4]) && s[5]=='m'
           && isdigit((unsigned char)s[6]) && isdigit((unsigned char)s[7]) && s[8]=='s') {
            int h = (s[0]-'0')*10 + (s[1]-'0');
            int m = (s[3]-'0')*10 + (s[4]-'0');
            int sec = (s[6]-'0')*10 + (s[7]-'0');
            if(sign)
                print_indented(out, indent, "%s: %c%02d:%02d:%02d\n", key, sign, h, m, sec);
            else
                print_indented(out, indent, "%s: %02d:%02d:%02d\n", key, h, m, sec);
            return;
        }
        /* fallthrough to default printing if pattern doesn't match */
    }
    /* if comma separated, print YAML list */
    if(strchr(val, ',')) {
        int n; char **arr = split_csv(val, &n);
        print_indented(out, indent, "%s:\n", key);
        for(int i=0;i<n;i++) {
            print_indented(out, indent+2, "- %s\n", arr[i]);
        }
        free_csv(arr, n);
    } else {
        print_indented(out, indent, "%s: %s\n", key, val);
    }
}

static void print_condition_item(FILE *out, Item *it, int indent) {
    if(!it) return;
    /* detect time condition */
    Attr *a = it->attrs;
    int has_time_after = 0, has_time_before = 0;
    const char *after = NULL, *before = NULL;
    /* also detect weekday */
    int has_weekday = 0;
    const char *weekday_val = NULL;
    for(Attr *p = a; p; p=p->next) {
        if(strcmp(p->key, "time_after")==0) { has_time_after=1; after=p->value; }
        if(strcmp(p->key, "time_before")==0) { has_time_before=1; before=p->value; }
        if(strcmp(p->key, "weekday")==0) { has_weekday=1; weekday_val=p->value; }
    }
    if(has_time_after || has_time_before) {
        print_indented(out, indent, "- condition: time\n");
        if(after) print_indented(out, indent+2, "after: %s\n", after);
        if(before) print_indented(out, indent+2, "before: %s\n", before);
        return;
    }

    if(has_weekday) {
        print_indented(out, indent, "- condition: time\n");
        print_attr_value(out, indent+2, "weekday", weekday_val);
        return;
    }

    /* detect explicit condition types that should take precedence */
    int has_state = 0, has_sun = 0, has_trigger = 0;
    for(Attr *p = a; p; p=p->next) {
        if(strcmp(p->key, "state")==0) has_state = 1;
        if(strcmp(p->key, "sun")==0) has_sun = 1;
        if(strcmp(p->key, "trigger")==0) has_trigger = 1;
    }

    if(has_state) {
        print_indented(out, indent, "- condition: state\n");
        /* print entity_id first if present */
        for(Attr *p = a; p; p=p->next) {
            if(strcmp(p->key, "entity_id")==0) print_attr_value(out, indent+2, "entity_id", p->value);
        }
        /* then the state itself */
        for(Attr *p = a; p; p=p->next) {
            if(strcmp(p->key, "state")==0) print_attr_value(out, indent+2, "state", p->value);
        }
        /* any other attrs (like id, offset, etc) */
        for(Attr *p = a; p; p=p->next) {
            if(strcmp(p->key, "entity_id")==0) continue;
            if(strcmp(p->key, "state")==0) continue;
            print_attr_value(out, indent+2, p->key, p->value);
        }
        return;
    }

    if(has_sun) {
        print_indented(out, indent, "- condition: sun\n");
        for(Attr *p = a; p; p=p->next) {
            if(strcmp(p->key, "sun")==0) continue;
            print_attr_value(out, indent+2, p->key, p->value);
        }
        return;
    }

    if(has_trigger) {
        print_indented(out, indent, "- condition: trigger\n");
        /* Collect all trigger values and emit as a proper list */
        int trigger_count = 0;
        for(Attr *p = a; p; p=p->next) {
            if(strcmp(p->key, "trigger")==0) trigger_count++;
        }

        if(trigger_count > 1) {
            /* multiple triggers: emit as a YAML list */
            print_indented(out, indent+2, "id:\n");
            for(Attr *p = a; p; p=p->next) {
                if(strcmp(p->key, "trigger")==0) {
                    print_indented(out, indent+4, "- %s\n", p->value ? p->value : "");
                }
            }
        } else {
            /* single trigger: check if it has commas (already a CSV list from 'trigger is any [...]') */
            for(Attr *p = a; p; p=p->next) {
                if(strcmp(p->key, "trigger")==0) {
                    print_attr_value(out, indent+2, "id", p->value);
                    break;
                }
            }
        }

        /* print non-trigger attrs */
        for(Attr *p = a; p; p=p->next) {
            if(strcmp(p->key, "trigger")==0) continue;
            print_attr_value(out, indent+2, p->key, p->value);
        }

        return;
    }

    /* device/state conditions */
    /* if there is device_id or entity_id treat as device */
    int has_device = 0, has_entity = 0;
    for(Attr *p = a; p; p=p->next) {
        if(strcmp(p->key, "device_id")==0) has_device=1;
        if(strcmp(p->key, "entity_id")==0) has_entity=1;
    }
    if(has_device || has_entity) {
        print_indented(out, indent, "- condition: device\n");
        for(Attr *p = a; p; p=p->next) {
            if(strcmp(p->key, "device_id")==0) print_attr_value(out, indent+2, "device_id", p->value);
            else if(strcmp(p->key, "entity_id")==0) print_attr_value(out, indent+2, "entity_id", p->value);
            else if(strcmp(p->key, "type")==0 || strcmp(p->key, "domain")==0 || strcmp(p->key, "enabled")==0) {
                /* print common named keys directly */
                print_attr_value(out, indent+2, p->key, p->value);
            } else {
                /* domain/type like switch is_on -> key=switch value=is_on */
                if(p->key && p->value) {
                    print_attr_value(out, indent+2, "type", p->value);
                    print_attr_value(out, indent+2, "domain", p->key);
                }
            }
        }
        return;
    }

    /* Fallback: emit as state condition instead of empty condition */
    print_indented(out, indent, "- condition: state\n");
    for(Attr *p = a; p; p=p->next) {
        /* For bare domain conditions like "alarm_control_panel disarmed",
           emit as entity_id: <domain>.alarmo  state: <value> */
        if(p->key && p->value && is_ha_domain(p->key)) {
            /* This is a domain-based state check */
            print_attr_value(out, indent+2, "state", p->value);
        } else {
            print_attr_value(out, indent+2, p->key, p->value);
        }
    }
}

static void print_actions(FILE *out, Action *act, int indent);

/* Split greedy ACT_SIMPLE into multiple actions at HA domain boundaries */
static void print_action_simple(FILE *out, Action *a, int indent) {
    /* Build a list of action segments by scanning attrs for domain boundaries */
    /* First action uses a->cmd and a->subcmd */

    /* Collect attrs into an array for easier indexing */
    int attr_count = 0;
    for(Attr *p = a->attrs; p; p=p->next) attr_count++;

    Attr **attr_arr = NULL;
    if(attr_count > 0) {
        attr_arr = calloc(attr_count, sizeof(Attr*));
        int i = 0;
        for(Attr *p = a->attrs; p; p=p->next) attr_arr[i++] = p;
    }

    /* Find split points for new actions based on HA domains */
    int *split_at = calloc(attr_count + 1, sizeof(int)); /* max possible splits */
    int num_splits = 0;

    for(int i = 0; i < attr_count; i++) {
        if(is_ha_domain(attr_arr[i]->key) && attr_arr[i]->value != NULL) {
            /* check value is not a state check (is_off, is_on, etc) */
            const char *v = attr_arr[i]->value;
            if(v && strncmp(v, "is_", 3) != 0
               && strcmp(v, "disarmed") != 0
               && strcmp(v, "armed") != 0) {
                split_at[num_splits++] = i;
            }
        }
        /* domain as key with NULL value followed by bare identifier */
        else if(is_ha_domain(attr_arr[i]->key) && attr_arr[i]->value == NULL) {
            if(i+1 < attr_count && attr_arr[i+1]->value == NULL
               && attr_arr[i+1]->key != NULL) {
                split_at[num_splits++] = i;
            }
        }
    }

    /* Check if a domain should use action: format instead of type: */
    int end = (num_splits > 0) ? split_at[0] : attr_count;
    
    int is_service_domain = (a->cmd && (
        strcmp(a->cmd, "notify") == 0 ||
        strcmp(a->cmd, "tts") == 0 ||
        strcmp(a->cmd, "alexa_devices") == 0 ||
        strcmp(a->cmd, "timer") == 0
    ));
    
    int has_device_id = 0;
    int is_entity_list = 0;
    for(int i = 0; i < end; i++) {
        if(strcmp(attr_arr[i]->key, "device_id") == 0) has_device_id = 1;
        if(strcmp(attr_arr[i]->key, "entity_id") == 0 && attr_arr[i]->value && strchr(attr_arr[i]->value, ',')) is_entity_list = 1;
    }
    if(!has_device_id || is_entity_list) {
        is_service_domain = 1;
    }

    /* Print the first action (using a->cmd / a->subcmd) */
    {
        if(is_service_domain && a->subcmd) {
            /* Service-style action: action: domain.service */
            print_indented(out, indent, "- action: %s.%s\n", a->cmd, a->subcmd);
            /* Separate entity_id/device_id (target) from data attrs */
            int has_target_attrs = 0;
            for(int i = 0; i < end; i++) {
                if(strcmp(attr_arr[i]->key, "entity_id") == 0 || strcmp(attr_arr[i]->key, "device_id") == 0) {
                    if(!has_target_attrs) {
                        print_indented(out, indent+2, "target:\n");
                        has_target_attrs = 1;
                    }
                    print_attr_value(out, indent+4, attr_arr[i]->key, attr_arr[i]->value);
                }
            }
            int has_data_attrs = 0;
            for(int i = 0; i < end; i++) {
                if(strcmp(attr_arr[i]->key, "entity_id") == 0 || strcmp(attr_arr[i]->key, "device_id") == 0) continue;
                if(!has_data_attrs) {
                    print_indented(out, indent+2, "data:\n");
                    has_data_attrs = 1;
                }
                print_attr_value(out, indent+4, attr_arr[i]->key, attr_arr[i]->value);
            }
            if(!has_data_attrs) {
                print_indented(out, indent+2, "data: {}\n");
            }
        } else if(a->subcmd) {
            print_indented(out, indent, "- type: %s\n", a->subcmd);
            for(int i = 0; i < end; i++) {
                print_attr_value(out, indent+2, attr_arr[i]->key, attr_arr[i]->value);
            }
            if(a->cmd) print_attr_value(out, indent+2, "domain", a->cmd);
        } else {
            print_indented(out, indent, "- action: %s\n", a->cmd ? a->cmd : "");
            for(int i = 0; i < end; i++) {
                print_attr_value(out, indent+2, attr_arr[i]->key, attr_arr[i]->value);
            }
        }
    }

    /* Print each subsequent action segment */
    for(int s = 0; s < num_splits; s++) {
        int start = split_at[s];
        int end = (s+1 < num_splits) ? split_at[s+1] : attr_count;

        char *new_domain = attr_arr[start]->key;
        char *new_subcmd = attr_arr[start]->value;
        int data_start;

        if(new_subcmd) {
            /* domain + subcmd as key=domain value=subcmd */
            data_start = start + 1;
        } else {
            /* domain is key with NULL value, next attr is subcmd */
            new_subcmd = attr_arr[start+1]->key;
            data_start = start + 2;
        }

        /* Check if this is a notify-style action that should use action: format */
        int is_svc = (strcmp(new_domain, "notify") == 0 || strcmp(new_domain, "tts") == 0
           || strcmp(new_domain, "alexa_devices") == 0 || strcmp(new_domain, "timer") == 0);
        int has_dev_id = 0;
        int is_ent_list = 0;
        for(int i = data_start; i < end; i++) {
            if(strcmp(attr_arr[i]->key, "device_id") == 0) has_dev_id = 1;
            if(strcmp(attr_arr[i]->key, "entity_id") == 0 && attr_arr[i]->value && strchr(attr_arr[i]->value, ',')) is_ent_list = 1;
        }
        if(!has_dev_id || is_ent_list) {
            is_svc = 1;
        }

        if(is_svc) {
            print_indented(out, indent, "- action: %s.%s\n", new_domain, new_subcmd);
            int has_target_attrs = 0;
            for(int i = data_start; i < end; i++) {
                if(strcmp(attr_arr[i]->key, "entity_id") == 0 || strcmp(attr_arr[i]->key, "device_id") == 0) {
                    if(!has_target_attrs) {
                        print_indented(out, indent+2, "target:\n");
                        has_target_attrs = 1;
                    }
                    print_attr_value(out, indent+4, attr_arr[i]->key, attr_arr[i]->value);
                }
            }
            int has_data_attrs = 0;
            for(int i = data_start; i < end; i++) {
                if(strcmp(attr_arr[i]->key, "entity_id") == 0 || strcmp(attr_arr[i]->key, "device_id") == 0) continue;
                if(!has_data_attrs) {
                    print_indented(out, indent+2, "data:\n");
                    has_data_attrs = 1;
                }
                print_attr_value(out, indent+4, attr_arr[i]->key, attr_arr[i]->value);
            }
            if(!has_data_attrs) {
                print_indented(out, indent+2, "data: {}\n");
            }
        } else {
            print_indented(out, indent, "- type: %s\n", new_subcmd);
            for(int i = data_start; i < end; i++) {
                print_attr_value(out, indent+2, attr_arr[i]->key, attr_arr[i]->value);
            }
            print_attr_value(out, indent+2, "domain", new_domain);
        }
    }

    if(attr_arr) free(attr_arr);
    free(split_at);
}

static void print_action_delay(FILE *out, Action *a, int indent) {
    /* parse duration like 00h00m45s00ms -> hours, minutes, seconds, milliseconds */
    int h=0,m=0,s=0,ms=0;
    const char *p = a->subcmd ? a->subcmd : "";
    sscanf(p, "%2dh%2dm%2ds%2dms", &h, &m, &s, &ms);
    print_indented(out, indent, "- delay:\n");
    print_indented(out, indent+4, "hours: %d\n", h);
    print_indented(out, indent+4, "minutes: %d\n", m);
    print_indented(out, indent+4, "seconds: %d\n", s);
    print_indented(out, indent+4, "milliseconds: %d\n", ms);
}

static void print_action_automation(FILE *out, Action *a, int indent) {
    print_indented(out, indent, "- action: automation.trigger\n");
    /* find entity_id and skip_condition */
    Attr *p = a->attrs;
    while(p) {
        if(strcmp(p->key, "entity_id")==0) print_attr_value(out, indent+2, "target.entity_id", p->value);
        else if(strcmp(p->key, "skip_condition")==0) print_attr_value(out, indent+2, "data.skip_condition", p->value);
        p = p->next;
    }
}

static void print_action_if(FILE *out, Action *a, int indent) {
    print_indented(out, indent, "- if:\n");
    /* print condition items */
    for(Item *it = a->cond; it; it = it->next) {
        print_condition_item(out, it, indent+2);
    }
    print_indented(out, indent+2, "then:\n");
    print_actions(out, a->then_actions, indent+4);
    if(a->else_action) {
        print_indented(out, indent+2, "else:\n");
        print_actions(out, a->else_action, indent+4);
    }
}

static void print_actions(FILE *out, Action *act, int indent) {
    for(Action *a = act; a; a = a->next) {
        if(a->kind == ACT_SIMPLE) print_action_simple(out, a, indent);
        else if(a->kind == ACT_DELAY) print_action_delay(out, a, indent);
        else if(a->kind == ACT_AUTOMATION_TRIGGER) print_action_automation(out, a, indent);
        else if(a->kind == ACT_IF) print_action_if(out, a, indent);
    }
}

void emit_yaml(Automation *a, FILE *out) {
    if(!a) return;
    if(a->id && strcmp(a->id, "auto") == 0) {
        struct timeval tv;
        gettimeofday(&tv, NULL);
        long long current = (long long)tv.tv_sec * 1000 + tv.tv_usec / 1000;
        static long long last_id = 0;
        if (current <= last_id) {
            current = last_id + 1;
        }
        last_id = current;
        fprintf(out, "- id: '%lld'\n", current);
    } else {
        fprintf(out, "- id: '%s'\n", a->id ? a->id : "");
    }
    fprintf(out, "  alias: %s\n", a->alias ? a->alias : "");
    fprintf(out, "  description: '%s'\n", a->description ? a->description : "");
    if(a->mode) fprintf(out, "  mode: %s\n", a->mode);
    if(a->triggers) {
        fprintf(out, "  triggers:\n");
        for(Item *it = a->triggers; it; it = it->next) {
            /* print attrs of trigger */
            fprintf(out, "  - ");
            /* simple: print as key: value or lists */
            Attr *p = it->attrs;
            if(!p) fprintf(out, "{}\n");
            else {
                /* print first attr then others indented */
                if(p->key && p->value)
                {
                    fprintf(out, "trigger: %s\n", p->key);
                    if(strcmp(p->key, "state") == 0)
                    {
                        print_attr_value(out, 4, "to", p->value);
                    }
                } 
                else if(p->key) 
                {
                    fprintf(out, "trigger: %s\n", p->key);
                }
                p = p->next;
                while(p) {
                    /* Convert domain/type attrs in device triggers */
                    if(is_trigger_domain(p->key) && p->value) {
                        print_attr_value(out, 4, "type", p->value);
                        print_attr_value(out, 4, "domain", p->key);
                    }
                    /* Convert 'for' duration to HA dict format */
                    else if(strcmp(p->key, "for") == 0 && p->value) {
                        int h, m, s;
                        if(parse_duration(p->value, &h, &m, &s)) {
                            print_indented(out, 4, "for:\n");
                            print_indented(out, 6, "hours: %d\n", h);
                            print_indented(out, 6, "minutes: %d\n", m);
                            print_indented(out, 6, "seconds: %d\n", s);
                        } else {
                            print_attr_value(out, 4, p->key, p->value);
                        }
                    }
                    else {
                        print_attr_value(out, 4, p->key, p->value);
                    }
                    p = p->next;
                }
            }
        }
    } else {
        fprintf(out, "  triggers: []\n");
    }
    if(a->conditions) {
        fprintf(out, "  conditions:\n");
        for(Item *it = a->conditions; it; it = it->next) {
            print_condition_item(out, it, 2);
        }
    } else {
        fprintf(out, "  conditions: []\n");
    }
    if(a->actions) {
        fprintf(out, "  actions:\n");
        print_actions(out, a->actions, 4);
    }
}
