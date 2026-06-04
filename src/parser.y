%{
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ast.h"
#include "yaml.h"

extern int yylex(void);
void yyerror(const char *s);

/* global list of automations built by the parser */
Automation *automations_head = NULL;
Automation *automations_tail = NULL;
Automation *cur_aut = NULL;

%}

%code requires {
#include "ast.h"
}

%debug

%union {
        char *str;
        Attr *attr;
        Attr *alist;
        Item *item;
        Item *items;
        Action *action;
        Action *actions;
        Automation *automation;
}

%token <str> STRING IDENTIFIER NUMBER TIME_LITERAL DURATION_LITERAL
%token CREATE ID DESCRIPTION MODE LISTEN WHEN DO IF ELSE AND OR NOT AFTER BEFORE IS
%token STATEKW TIMEKW DEVICE_ID ENTITY_ID WITH CHANGES TO SUN EVENT OFFSET SKIP_CONDITION
%token AUTOMATION TRIGGERKW DELAY
%token LBRACKET RBRACKET LPAREN RPAREN COLON COMMA MINUS PLUS

%type <alist> attr_list attr
%type <item> listen_item condition_item cond_unit
%type <items> listen_items condition_items condition_group cond_seq
%type <alist> attr_seq
%type <str> id_list
%type <action> action_items action_item action_body action_block else_part simple_action
%type <automation> automation

%%

program:
            /* empty */
        | program automation
        ;

automation:
    CREATE STRING { cur_aut = ast_new_automation($2); } COLON fields {
        ast_append_automation(&automations_head, &automations_tail, cur_aut);
        cur_aut = NULL;
    }
    ;

fields:
            /* empty */
        | fields field
        ;

field:
        ID STRING { ast_set_id(cur_aut, $2); }
    | ID NUMBER { ast_set_id(cur_aut, $2); }
    | ID IDENTIFIER { ast_set_id(cur_aut, $2); }
    | DESCRIPTION STRING { ast_set_description(cur_aut, $2); }
    | MODE IDENTIFIER { ast_set_mode(cur_aut, $2); }
    | LISTEN LBRACKET listen_items RBRACKET { ast_append_items(&cur_aut->triggers, $3); }
    | WHEN LBRACKET condition_items RBRACKET { ast_append_items(&cur_aut->conditions, $3); }
    | DO LBRACKET action_items RBRACKET { ast_append_actions(&cur_aut->actions, $3); }
        ;

/* listen / when items are parenthesized groups of attributes */
listen_items:
        listen_item { $$ = $1; }
    | listen_items COMMA listen_item { Item *p = $1; if(!p) $$ = $3; else { while(p->next) p = p->next; p->next = $3; $$ = $1; } }
    ;

listen_item:
        LPAREN attr_list RPAREN { Item *it = ast_new_item(); ast_item_set_attrs(it, $2); $$ = it; }
    ;

condition_items:
        condition_item { $$ = $1; }
    | condition_items COMMA condition_item { Item *p = $1; if(!p) $$ = $3; else { while(p->next) p = p->next; p->next = $3; $$ = $1; } }
    ;

condition_item:
        LPAREN attr_list RPAREN { Item *it = ast_new_item(); ast_item_set_attrs(it, $2); $$ = it; }
    ;

/* attributes inside parentheses */
attr_list:
        /* empty */ { $$ = NULL; }
    | attr { $$ = $1; }
    | attr_list attr { Attr *p = $1; if(!p) $$ = $2; else { while(p->next) p = p->next; p->next = $2; $$ = $1; } }
    ;

/* sequence allowing AND/OR between attrs (used in IF conditions) */
attr_seq:
        attr { $$ = $1; }
    |   attr_seq AND attr { Attr *p = $1; if(!p) $$ = $3; else { while(p->next) p = p->next; p->next = $3; $$ = $1; } }
    |   attr_seq OR attr { Attr *p = $1; if(!p) $$ = $3; else { while(p->next) p = p->next; p->next = $3; $$ = $1; } }
    ;

/* cond_unit produces a single Item* (either a parenthesized attr_list, bracketed condition_items, or an attr_seq converted to an Item) */
cond_unit:
            LPAREN attr_list RPAREN { Item *it = ast_new_item(); ast_item_set_attrs(it, $2); $$ = it; }
        | LBRACKET cond_seq RBRACKET { $$ = $2; }
        | attr_seq { Item *it = ast_new_item(); ast_item_set_attrs(it, $1); $$ = it; }
        ;

/* cond_seq: sequence of cond_unit joined by AND/OR */
cond_seq:
            cond_unit { $$ = $1; }
        | cond_seq AND cond_unit { Item *p = $1; if(!p) $$ = $3; else { while(p->next) p = p->next; p->next = $3; $$ = $1; } }
        | cond_seq OR cond_unit { Item *p = $1; if(!p) $$ = $3; else { while(p->next) p = p->next; p->next = $3; $$ = $1; } }
        ;

attr:
        DEVICE_ID IDENTIFIER { $$ = ast_new_attr("device_id", $2); }
    | ENTITY_ID IDENTIFIER { $$ = ast_new_attr("entity_id", $2); }
    | STATEKW IS IDENTIFIER { $$ = ast_new_attr("state", $3); }
    | TIMEKW AFTER TIME_LITERAL { $$ = ast_new_attr("time_after", $3); }
    | TIMEKW BEFORE TIME_LITERAL { $$ = ast_new_attr("time_before", $3); }
    | STATEKW CHANGES TO IDENTIFIER { $$ = ast_new_attr("state", $4); }
    | STATEKW CHANGES TO STRING {
        char *quoted = malloc(strlen($4) + 3); /* ' + string + ' + \0 */
        sprintf(quoted, "'%s'", $4);
        $$ = ast_new_attr("state", quoted);
    }
    | SUN { $$ = ast_new_attr("sun", NULL); }
    | EVENT IDENTIFIER { $$ = ast_new_attr("event", $2); }
    | ID IDENTIFIER { $$ = ast_new_attr("id", $2); }
    | TRIGGERKW IS IDENTIFIER { $$ = ast_new_attr("trigger", $3); }
    | IDENTIFIER IS IDENTIFIER { $$ = ast_new_attr($1, $3); }
    | IDENTIFIER IDENTIFIER { $$ = ast_new_attr($1, $2); }
    | IDENTIFIER LBRACKET id_list RBRACKET { $$ = ast_new_attr($1, $3); }
    | IDENTIFIER IS IDENTIFIER LBRACKET id_list RBRACKET { size_t n = strlen($3) + 1 + strlen($5) + 1; char *buf = malloc(n); strcpy(buf, $3); strcat(buf, ","); strcat(buf, $5); $$ = ast_new_attr($1, buf); free(buf); }
    | STATEKW IS IDENTIFIER LBRACKET id_list RBRACKET { size_t n = strlen($3) + 1 + strlen($5) + 1; char *buf = malloc(n); strcpy(buf, $3); strcat(buf, ","); strcat(buf, $5); $$ = ast_new_attr("state", buf); free(buf); }
    | STATEKW IS STRING LBRACKET id_list RBRACKET { size_t n = strlen($3) + strlen($5) + 4; char *buf = malloc(n); sprintf(buf, "'%s',%s", $3, $5); $$ = ast_new_attr("state", buf); free(buf);}
    | OFFSET DURATION_LITERAL { $$ = ast_new_attr("offset", $2); }
    | SKIP_CONDITION IDENTIFIER { $$ = ast_new_attr("skip_condition", $2); }
    | CHANGES TO IDENTIFIER { $$ = ast_new_attr("changes_to", $3); }
    | WITH IDENTIFIER LBRACKET id_list RBRACKET { $$ = ast_new_attr($2, $4); }
    ;

id_list:
        IDENTIFIER { $$ = strdup($1); }
    | IDENTIFIER COMMA id_list { size_t n = strlen($1) + 1 + strlen($3) + 1; char *buf = malloc(n); strcpy(buf, $1); strcat(buf, ","); strcat(buf, $3); free($3); $$ = buf; }
    ;

/* actions: each action is parenthesized */
action_items:
        action_item
    | action_item COMMA action_items { Action *p = $1; if(!p) $$ = $3; else { while(p->next) p = p->next; p->next = $3; $$ = $1; } }
    ;

action_item:
        LPAREN action_body RPAREN { $$ = $2; }
    ;

action_body:
        IF condition_group action_block else_part {
                $$ = ast_new_action_if($2, $3, $4);
        }
    | simple_action { $$ = $1; }
    ;

condition_group:
    cond_seq { $$ = $1; }
    ;

action_block:
        /* empty */ { $$ = NULL; }
    | simple_action { $$ = $1; }
    | action_block simple_action { Action *p = $1; if(!p) $$ = $2; else { while(p->next) p = p->next; p->next = $2; $$ = $1; } }
    ;

else_part:
        ELSE IF condition_group action_block else_part { Action *ifnode = ast_new_action_if($3, $4, $5); $$ = ifnode; }
    | ELSE action_block { $$ = $2; }
    | /* empty */ { $$ = NULL; }
    ;

simple_action:
        DELAY DURATION_LITERAL { $$ = ast_new_action_delay($2); }
    | AUTOMATION TRIGGERKW attr_list { $$ = ast_new_action_automation_trigger($3); }
    | IDENTIFIER IDENTIFIER attr_list { $$ = ast_new_action_simple($1, $2, $3); }
    ;

%%

void yyerror(const char *s) {
        fprintf(stderr, "Parse error: %s\n", s);
}

int main(int argc, char **argv) {
    yydebug = 1; /* debug on */
    if (yyparse() == 0) {
        Automation *a = automations_head;
        while(a) {
            emit_yaml(a, stdout);
            a = a->next;
        }
        ast_free_automation(automations_head);
    }
    return 0;
}

