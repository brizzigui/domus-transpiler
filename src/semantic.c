#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "semantic.h"
#include "symtab.h"

static int error_count = 0;

static void semantic_error(int line, const char *msg) {
    fprintf(stderr, "--------------------------------------\n");
    fprintf(stderr, "Semantic Error at Line %d:\n\t%s\n", line, msg);
    fprintf(stderr, "--------------------------------------\n");
    error_count++;
}

static char* strip_quotes(char *str) {
    if (!str) return NULL;
    char *start = str;
    while (*start == ' ' || *start == '\'') start++;
    char *end = start + strlen(start) - 1;
    while (end >= start && (*end == ' ' || *end == '\'')) {
        *end = '\0';
        end--;
    }
    return start;
}

static void register_entity(const char *entity_id) {
    if (!entity_id) return;
    
    char *dup = strdup(entity_id);
    char *token = strtok(dup, ",");
    while (token != NULL) {
        char *start = strip_quotes(token);

        char *dot = strchr(start, '.');
        if (dot) {
            char domain[MAX_DOMAIN_LEN];
            size_t len = dot - start;
            if (len >= MAX_DOMAIN_LEN) len = MAX_DOMAIN_LEN - 1;
            strncpy(domain, start, len);
            domain[len] = '\0';
            symtab_insert(start, domain);
        }
        token = strtok(NULL, ",");
    }
    free(dup);
}

static void pass1_attr(Attr *attr) {
    while (attr) {
        if (strcmp(attr->key, "entity_id") == 0 && attr->value != NULL) {
            register_entity(attr->value);
        }
        attr = attr->next;
    }
}

static void pass1_items(Item *items) {
    while (items) {
        pass1_attr(items->attrs);
        items = items->next;
    }
}

static void pass1_actions(Action *action) {
    while (action) {
        pass1_attr(action->attrs);
        pass1_items(action->cond);
        pass1_actions(action->then_actions);
        pass1_actions(action->else_action);
        action = action->next;
    }
}

static void pass1_automation(Automation *aut) {
    while (aut) {
        pass1_items(aut->triggers);
        pass1_items(aut->conditions);
        pass1_actions(aut->actions);
        aut = aut->next;
    }
}

static const char* get_entity_id(Attr *attrs) {
    while (attrs) {
        if (strcmp(attrs->key, "entity_id") == 0 && attrs->value != NULL) {
            static char buf[256];
            strncpy(buf, attrs->value, 255);
            buf[255] = '\0';
            char *start = strip_quotes(buf);
            char *comma = strchr(start, ',');
            if (comma) *comma = '\0';
            return start;
        }
        attrs = attrs->next;
    }
    return NULL;
}

static void pass2_attr(Attr *attr, int line, const char *entity_context) {
    while (attr) {
        if ((strcmp(attr->key, "state") == 0 || strcmp(attr->key, "to") == 0 || strcmp(attr->key, "from") == 0) && attr->value != NULL) {
            if (entity_context) {
                Symbol *s = symtab_lookup(entity_context);
                if (s) {
                    if (strcmp(s->domain, "binary_sensor") == 0 ||
                        strcmp(s->domain, "light") == 0 ||
                        strcmp(s->domain, "switch") == 0) {
                        
                        char val_no_quotes[256] = {0};
                        strncpy(val_no_quotes, attr->value, 255);
                        char *start = strip_quotes(val_no_quotes);

                        if (strcmp(start, "on") != 0 && strcmp(start, "off") != 0) {
                            char msg[512];
                            snprintf(msg, sizeof(msg), "Invalid state '%s' for boolean domain '%s' of entity '%s'. Expected 'on' or 'off'.", start, s->domain, entity_context);
                            semantic_error(line, msg);
                        }
                    }
                }
            }
        }
        attr = attr->next;
    }
}

static void pass2_items(Item *items) {
    while (items) {
        const char *entity = get_entity_id(items->attrs);
        pass2_attr(items->attrs, items->line, entity);
        items = items->next;
    }
}

static void pass2_actions(Action *action) {
    while (action) {
        if (action->kind == ACT_SIMPLE) {
            const char *entity = get_entity_id(action->attrs);
            if (entity && action->cmd) {
                Symbol *s = symtab_lookup(entity);
                if (s && strcmp(s->domain, action->cmd) != 0) {
                    char msg[512];
                    snprintf(msg, sizeof(msg), "External Consistency Error: Calling service domain '%s' on entity '%s' of domain '%s'.", action->cmd, entity, s->domain);
                    semantic_error(action->line, msg);
                }
            }
        }
        
        const char *entity = get_entity_id(action->attrs);
        pass2_attr(action->attrs, action->line, entity);
        pass2_items(action->cond);
        pass2_actions(action->then_actions);
        pass2_actions(action->else_action);
        action = action->next;
    }
}

static void pass2_automation(Automation *aut) {
    while (aut) {
        pass2_items(aut->triggers);
        pass2_items(aut->conditions);
        pass2_actions(aut->actions);
        aut = aut->next;
    }
}

int semantic_analyze(Automation *head) {
    error_count = 0;
    symtab_init();
    
    pass1_automation(head);
    pass2_automation(head);
    
    symtab_free();
    
    if (error_count > 0) {
        fprintf(stderr, "Semantic analysis failed with %d errors.\n", error_count);
        return 0; // Failed
    }
    return 1; // Success
}
