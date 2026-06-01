#ifndef AST_H
#define AST_H

#include <stdio.h>

typedef struct Attr {
    char *key;
    char *value;
    struct Attr *next;
} Attr;

typedef struct Item {
    Attr *attrs; /* a group of attributes inside parentheses */
    struct Item *next;
} Item;

typedef struct Action {
    int kind;
    char *cmd;      /* e.g., "light" */
    char *subcmd;   /* e.g., "turn_on" */
    Attr *attrs;    /* extra attributes (device_id, entity_id, etc) */
    Item *cond;     /* for IF: list of condition items */
    struct Action *then_actions; /* linked list for then */
    struct Action *else_action;  /* single else or if chain */
    struct Action *next;         /* sequence linking */
} Action;

typedef struct Automation {
    char *alias;
    char *id;
    char *description;
    char *mode;
    Item *triggers;   /* listen */
    Item *conditions; /* when */
    Action *actions;  /* do */
    struct Automation *next;
} Automation;

enum { ACT_SIMPLE = 0, ACT_IF = 1, ACT_DELAY = 2, ACT_AUTOMATION_TRIGGER = 3 };

/* Automation lifecycle */
Automation *ast_new_automation(char *alias);
void ast_set_id(Automation *a, char *id);
void ast_set_description(Automation *a, char *desc);
void ast_set_mode(Automation *a, char *mode);
void ast_append_automation(Automation **head, Automation **tail, Automation *a);

/* Items/Attrs helpers */
Attr *ast_new_attr(char *k, char *v);
void ast_append_attr(Attr **list, Attr *a);
Item *ast_new_item(void);
void ast_item_set_attrs(Item *it, Attr *attrs);
Item *ast_append_item_list(Item *head, Item *tail);
void ast_append_items(Item **dest, Item *src);

/* Actions helpers */
Action *ast_new_action_simple(char *cmd, char *subcmd, Attr *attrs);
Action *ast_new_action_delay(char *duration);
Action *ast_new_action_automation_trigger(Attr *attrs);
Action *ast_new_action_if(Item *cond_items, Action *then_actions, Action *else_action);
Action *ast_append_action_list(Action *head, Action *tail);
void ast_append_actions(Action **dest, Action *src);

/* Freeing */
void ast_free_attr(Attr *a);
void ast_free_items(Item *it);
void ast_free_actions(Action *a);
void ast_free_automation(Automation *a);

#endif
