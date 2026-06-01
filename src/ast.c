#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ast.h"

static char *strdup_safe(const char *s) { if(!s) return NULL; char *r = malloc(strlen(s)+1); strcpy(r,s); return r; }

Automation *ast_new_automation(char *alias) {
    Automation *a = calloc(1, sizeof(Automation));
    a->alias = alias ? strdup_safe(alias) : NULL;
    a->id = NULL;
    a->description = NULL;
    a->mode = NULL;
    a->triggers = NULL;
    a->conditions = NULL;
    a->actions = NULL;
    a->next = NULL;
    return a;
}

void ast_set_id(Automation *a, char *id) { if(!a) return; if(a->id) free(a->id); a->id = id ? strdup_safe(id) : NULL; }
void ast_set_description(Automation *a, char *desc) { if(!a) return; if(a->description) free(a->description); a->description = desc ? strdup_safe(desc) : NULL; }
void ast_set_mode(Automation *a, char *mode) { if(!a) return; if(a->mode) free(a->mode); a->mode = mode ? strdup_safe(mode) : NULL; }

void ast_append_automation(Automation **head, Automation **tail, Automation *a) {
    if(!*head) { *head = *tail = a; a->next = NULL; return; }
    (*tail)->next = a; *tail = a; a->next = NULL;
}

/* Attr helpers */
Attr *ast_new_attr(char *k, char *v) {
    Attr *a = malloc(sizeof(Attr));
    a->key = k ? strdup_safe(k) : NULL;
    a->value = v ? strdup_safe(v) : NULL;
    a->next = NULL;
    return a;
}

void ast_append_attr(Attr **list, Attr *a) {
    if(!*list) { *list = a; return; }
    Attr *p = *list; while(p->next) p = p->next; p->next = a;
}

Item *ast_new_item(void) { Item *it = calloc(1, sizeof(Item)); it->attrs = NULL; it->next = NULL; return it; }
void ast_item_set_attrs(Item *it, Attr *attrs) { if(!it) return; it->attrs = attrs; }

Item *ast_append_item_list(Item *head, Item *tail) {
    if(!head) return tail;
    Item *p = head; while(p->next) p = p->next; p->next = tail; return head;
}

void ast_append_items(Item **dest, Item *src) {
    if(!src) return;
    if(!*dest) { *dest = src; return; }
    Item *p = *dest; while(p->next) p = p->next; p->next = src;
}

/* Actions */
Action *ast_new_action_simple(char *cmd, char *subcmd, Attr *attrs) {
    Action *a = calloc(1, sizeof(Action));
    a->kind = ACT_SIMPLE;
    a->cmd = cmd ? strdup_safe(cmd) : NULL;
    a->subcmd = subcmd ? strdup_safe(subcmd) : NULL;
    a->attrs = attrs;
    a->cond = NULL;
    a->then_actions = NULL;
    a->else_action = NULL;
    a->next = NULL;
    return a;
}

Action *ast_new_action_delay(char *duration) {
    Action *a = calloc(1, sizeof(Action));
    a->kind = ACT_DELAY;
    a->cmd = NULL;
    a->subcmd = duration ? strdup_safe(duration) : NULL;
    a->attrs = NULL;
    a->cond = NULL;
    a->then_actions = NULL;
    a->else_action = NULL;
    a->next = NULL;
    return a;
}

Action *ast_new_action_automation_trigger(Attr *attrs) {
    Action *a = calloc(1, sizeof(Action));
    a->kind = ACT_AUTOMATION_TRIGGER;
    a->cmd = NULL;
    a->subcmd = NULL;
    a->attrs = attrs;
    a->cond = NULL;
    a->then_actions = NULL;
    a->else_action = NULL;
    a->next = NULL;
    return a;
}

Action *ast_new_action_if(Item *cond_items, Action *then_actions, Action *else_action) {
    Action *a = calloc(1, sizeof(Action));
    a->kind = ACT_IF;
    a->cmd = NULL;
    a->subcmd = NULL;
    a->attrs = NULL;
    a->cond = cond_items;
    a->then_actions = then_actions;
    a->else_action = else_action;
    a->next = NULL;
    return a;
}

Action *ast_append_action_list(Action *head, Action *tail) {
    if(!head) return tail;
    Action *p = head; while(p->next) p = p->next; p->next = tail; return head;
}

void ast_append_actions(Action **dest, Action *src) {
    if(!src) return;
    if(!*dest) { *dest = src; return; }
    Action *p = *dest; while(p->next) p = p->next; p->next = src;
}

/* Freeing */
void ast_free_attr(Attr *a) {
    while(a) {
        Attr *n = a->next;
        if(a->key) free(a->key);
        if(a->value) free(a->value);
        free(a);
        a = n;
    }
}

void ast_free_items(Item *it) {
    while(it) {
        Item *n = it->next;
        ast_free_attr(it->attrs);
        free(it);
        it = n;
    }
}

void ast_free_actions(Action *a) {
    while(a) {
        Action *n = a->next;
        if(a->cmd) free(a->cmd);
        if(a->subcmd) free(a->subcmd);
        ast_free_attr(a->attrs);
        ast_free_items(a->cond);
        /* free nested then/else */
        if(a->then_actions) ast_free_actions(a->then_actions);
        if(a->else_action) ast_free_actions(a->else_action);
        free(a);
        a = n;
    }
}

void ast_free_automation(Automation *a) {
    while(a) {
        Automation *n = a->next;
        if(a->alias) free(a->alias);
        if(a->id) free(a->id);
        if(a->description) free(a->description);
        if(a->mode) free(a->mode);
        ast_free_items(a->triggers);
        ast_free_items(a->conditions);
        if(a->actions) ast_free_actions(a->actions);
        free(a);
        a = n;
    }
}
