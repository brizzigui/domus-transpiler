#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
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

static void print_attr_value(FILE *out, int indent, const char *key, const char *val) {
    if(!val) {
        print_indented(out, indent, "%s: \n", key);
        return;
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
    for(Attr *p = a; p; p=p->next) {
        if(strcmp(p->key, "time_after")==0) { has_time_after=1; after=p->value; }
        if(strcmp(p->key, "time_before")==0) { has_time_before=1; before=p->value; }
    }
    if(has_time_after || has_time_before) {
        print_indented(out, indent, "- condition: time\n");
        if(after) print_indented(out, indent+2, "after: %s\n", after);
        if(before) print_indented(out, indent+2, "before: %s\n", before);
        return;
    }
    /* device/state conditions */
    /* if there is device_id or entity_id treat as device/state */
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
            else {
                /* domain/type like switch is_on -> key=switch value=is_on */
                if(p->key && p->value) {
                    print_attr_value(out, indent+2, "type", p->value);
                    print_attr_value(out, indent+2, "domain", p->key);
                }
            }
        }
        return;
    }
    /* fallback: print raw attrs */
    print_indented(out, indent, "- condition: \n");
    for(Attr *p = a; p; p=p->next) {
        print_attr_value(out, indent+2, p->key, p->value);
    }
}

static void print_actions(FILE *out, Action *act, int indent);

static void print_action_simple(FILE *out, Action *a, int indent) {
    if(a->subcmd) {
        print_indented(out, indent, "- type: %s\n", a->subcmd);
    } else {
        print_indented(out, indent, "- action: %s\n", a->cmd ? a->cmd : "");
    }
    /* print attrs */
    for(Attr *p = a->attrs; p; p=p->next) {
        if(strcmp(p->key, "entity_id")==0) print_attr_value(out, indent+2, "entity_id", p->value);
        else if(strcmp(p->key, "device_id")==0) print_attr_value(out, indent+2, "device_id", p->value);
        else print_attr_value(out, indent+2, p->key, p->value);
    }
    if(a->cmd) print_attr_value(out, indent+2, "domain", a->cmd);
}

static void print_action_delay(FILE *out, Action *a, int indent) {
    /* parse duration like 00h00m45s00ms -> hours, minutes, seconds, milliseconds */
    int h=0,m=0,s=0,ms=0;
    const char *p = a->subcmd ? a->subcmd : "";
    sscanf(p, "%2dh%2dm%2ds%2dms", &h, &m, &s, &ms);
    print_indented(out, indent, "- delay:\n");
    print_indented(out, indent+2, "hours: %d\n", h);
    print_indented(out, indent+2, "minutes: %d\n", m);
    print_indented(out, indent+2, "seconds: %d\n", s);
    print_indented(out, indent+2, "milliseconds: %d\n", ms);
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
    fprintf(out, "- id: '%s'\n", a->id ? a->id : "");
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
                if(p->key && p->value) fprintf(out, "%s: %s\n", p->key, p->value);
                else if(p->key) fprintf(out, "%s: \n", p->key);
                p = p->next;
                while(p) {
                    print_attr_value(out, 4, p->key, p->value);
                    p = p->next;
                }
            }
        }
    }
    if(a->conditions) {
        fprintf(out, "  conditions:\n");
        for(Item *it = a->conditions; it; it = it->next) {
            print_condition_item(out, it, 2);
        }
    }
    if(a->actions) {
        fprintf(out, "  actions:\n");
        print_actions(out, a->actions, 4);
    }
}
