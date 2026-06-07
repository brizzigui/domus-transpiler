%{
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ast.h"
#include "yaml.h"

extern int yylex(void);
extern int yylineno;
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
%token CREATE ID DESCRIPTION MODE LISTEN WHEN DO IF ELSE AND OR NOT AFTER BEFORE IS AT
%token STATEKW TIMEKW DEVICE_ID ENTITY_ID WITH CHANGES TO SUN DEVICE EVENT OFFSET SKIP_CONDITION
%token AUTOMATION TRIGGERKW DELAY ABOVE BELOW ANY FROM FOR
%token LBRACKET RBRACKET LPAREN RPAREN COLON COMMA MINUS PLUS

%type <alist> attr_list attr
%type <item> listen_item condition_item cond_unit
%type <items> listen_items condition_items condition_group cond_seq
%type <alist> attr_seq
%type <str> id_list
%type <action> action_items action_item action_body action_block else_part simple_action action_stmt
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
        /* empty */ { $$ = NULL; }
    | listen_item { $$ = $1; }
    | listen_items COMMA listen_item { Item *p = $1; if(!p) $$ = $3; else { while(p->next) p = p->next; p->next = $3; $$ = $1; } }
    ;

listen_item:
        LPAREN attr_list RPAREN { Item *it = ast_new_item(); ast_item_set_attrs(it, $2); $$ = it; }
    | LPAREN error RPAREN { $$ = NULL; yyerrok; }
    ;

condition_items:
        /* empty */ { $$ = NULL; }
    | condition_item { $$ = $1; }
    | condition_items COMMA condition_item { Item *p = $1; if(!p) $$ = $3; else { while(p->next) p = p->next; p->next = $3; $$ = $1; } }
    ;

condition_item:
        LPAREN attr_list RPAREN { Item *it = ast_new_item(); ast_item_set_attrs(it, $2); $$ = it; }
    | LPAREN error RPAREN { $$ = NULL; yyerrok; }
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
        | LPAREN attr_seq RPAREN { Item *it = ast_new_item(); ast_item_set_attrs(it, $2); $$ = it; }
        | LBRACKET cond_seq RBRACKET { $$ = $2; }
        | attr_seq { Item *it = ast_new_item(); ast_item_set_attrs(it, $1); $$ = it; }
        | LPAREN error RPAREN { $$ = NULL; yyerrok; }
        ;

/* cond_seq: sequence of cond_unit joined by AND/OR */
cond_seq:
            cond_unit { $$ = $1; }
        | cond_seq AND cond_unit { Item *p = $1; if(!p) $$ = $3; else { while(p->next) p = p->next; p->next = $3; $$ = $1; } }
        | cond_seq OR cond_unit { Item *p = $1; if(!p) $$ = $3; else { while(p->next) p = p->next; p->next = $3; $$ = $1; } }
        ;

to_from:
    TO
    | FROM
    ;

attr:
        DEVICE_ID IDENTIFIER { $$ = ast_new_attr("device_id", $2); }
    | DEVICE_ID LBRACKET id_list RBRACKET { $$ = ast_new_attr("device_id", $3); }
    | ENTITY_ID IDENTIFIER { $$ = ast_new_attr("entity_id", $2); }
    | ENTITY_ID LBRACKET id_list RBRACKET { $$ = ast_new_attr("entity_id", $3); }
    | STATEKW IS IDENTIFIER { $$ = ast_new_attr("state", $3); }
    | STATEKW IS STRING {
        char *quoted = malloc(strlen($3) + 3); /* ' + string + ' + \0 */
        sprintf(quoted, "'%s'", $3);
        $$ = ast_new_attr("state", quoted);
    }
    | TIMEKW AFTER TIME_LITERAL { $$ = ast_new_attr("time_after", $3); }
    | TIMEKW BEFORE TIME_LITERAL { $$ = ast_new_attr("time_before", $3); }
    | TIMEKW AT TIME_LITERAL {
        Attr *t = ast_new_attr("time", NULL);
        Attr *at = ast_new_attr("at", $3);
        t->next = at;
        $$ = t;
    }
    | BEFORE IDENTIFIER { $$ = ast_new_attr("before", $2); }
    | AFTER IDENTIFIER { $$ = ast_new_attr("after", $2); }
    | STATEKW CHANGES to_from IDENTIFIER { $$ = ast_new_attr("state", $4); }
    | STATEKW CHANGES to_from STRING {
        char *quoted = malloc(strlen($4) + 3); /* ' + string + ' + \0 */
        sprintf(quoted, "'%s'", $4);
        $$ = ast_new_attr("state", quoted);
    }
    | EVENT IDENTIFIER { $$ = ast_new_attr("event", $2); }
    | ID IDENTIFIER { $$ = ast_new_attr("id", $2); }
    | ID STRING {
        char *quoted = malloc(strlen($2) + 3); /* ' + string + ' + \0 */
        sprintf(quoted, "'%s'", $2);
        $$ = ast_new_attr("id", quoted);
    }
    | TRIGGERKW IS IDENTIFIER { $$ = ast_new_attr("trigger", $3); }
    | TRIGGERKW IS STRING {
        char *quoted = malloc(strlen($3) + 3); /* ' + string + ' + \0 */
        sprintf(quoted, "'%s'", $3);
        $$ = ast_new_attr("trigger", quoted); 
    }
    | IDENTIFIER IS IDENTIFIER { $$ = ast_new_attr($1, $3); }
    | IDENTIFIER IDENTIFIER { $$ = ast_new_attr($1, $2); }
    | IDENTIFIER STRING {
        char *quoted = malloc(strlen($2) + 3); /* ' + string + ' + \0 */
        sprintf(quoted, "'%s'", $2);
        $$ = ast_new_attr($1, quoted); 
    }
    | IDENTIFIER LBRACKET id_list RBRACKET { $$ = ast_new_attr($1, $3); }
    | IDENTIFIER IS IDENTIFIER LBRACKET id_list RBRACKET { size_t n = strlen($3) + 1 + strlen($5) + 1; char *buf = malloc(n); strcpy(buf, $3); strcat(buf, ","); strcat(buf, $5); $$ = ast_new_attr($1, buf); free(buf); }
    | STATEKW IS ANY LBRACKET id_list RBRACKET {$$ = ast_new_attr("state", $5);}
    | TRIGGERKW IS ANY LBRACKET id_list RBRACKET {$$ = ast_new_attr("trigger", $5);}
    | IDENTIFIER IS ANY LBRACKET id_list RBRACKET {$$ = ast_new_attr($1, $5);}
    | OFFSET DURATION_LITERAL { $$ = ast_new_attr("offset", $2); }
    | FOR DURATION_LITERAL { $$ = ast_new_attr("for", $2); }
    | SKIP_CONDITION IDENTIFIER { $$ = ast_new_attr("skip_condition", $2); }
    | CHANGES to_from IDENTIFIER { $$ = ast_new_attr("changes_to", $3); }
    | WITH IDENTIFIER LBRACKET id_list RBRACKET { $$ = ast_new_attr($2, $4); }
    | IDENTIFIER NUMBER { $$ = ast_new_attr($1, $2); }
    | ABOVE NUMBER { $$ = ast_new_attr("above", $2); }
    | BELOW NUMBER { $$ = ast_new_attr("below", $2); }
    | IDENTIFIER { $$ = ast_new_attr($1, NULL); }
    ;



id_list:
        STRING {
        char *quoted = malloc(strlen($1) + 3); /* ' + string + ' + \0 */
        sprintf(quoted, "'%s'", $1);
        $$ = strdup(quoted); 
    }
    | IDENTIFIER { $$ = strdup($1); }
    | IDENTIFIER COMMA id_list { size_t n = strlen($1) + 1 + strlen($3) + 1; char *buf = malloc(n); strcpy(buf, $1); strcat(buf, ","); strcat(buf, $3); free($3); $$ = buf; }
    | STRING COMMA id_list {
        char *quoted = malloc(strlen($1) + 3); /* ' + string + ' + \0 */
        sprintf(quoted, "'%s'", $1);
        size_t n = strlen(quoted) + 1 + strlen($3) + 1; char *buf = malloc(n); strcpy(buf, quoted); strcat(buf, ","); strcat(buf, $3); free($3); $$ = buf;
    }
    ;

/* actions: each action is parenthesized */
action_items:
        action_item { $$ = $1; }
    | action_item COMMA action_items { Action *p = $1; if(!p) $$ = $3; else { while(p->next) p = p->next; p->next = $3; $$ = $1; } }
    ;

action_item:
        LPAREN action_body RPAREN { $$ = $2; }
    | LPAREN error RPAREN { $$ = NULL; yyerrok; }
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

action_stmt:
        simple_action
    | IF condition_group action_block else_part
      { $$ = ast_new_action_if($2, $3, $4); }
    ;

action_block:
        /* empty */ { $$ = NULL; }
    | action_stmt { $$ = $1; }
    | action_block action_stmt
      {
          Action *p = $1;
          if (!p)
              $$ = $2;
          else {
              while (p->next) p = p->next;
              p->next = $2;
              $$ = $1;
          }
      }
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
        fprintf(stderr, "Line %d: %s\n", yylineno, s);
}

int main(int argc, char **argv) {
    yydebug = 0; /* debug on */
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

