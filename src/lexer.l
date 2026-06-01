%{
#include "ast.h"
#include "parser.tab.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern YYSTYPE yylval;
extern char *yytext;

/* helper to set string value */
static char *strdup_safe(const char *s) { if(!s) return NULL; size_t n=strlen(s); char *r=malloc(n+1); strcpy(r,s); return r; }

%}

%option noyywrap
%option yylineno

TIME        [0-9]{2}:[0-9]{2}:[0-9]{2}
DURATION    -?[0-9]{2}h[0-9]{2}m[0-9]{2}s([0-9]{2,3}ms)?
NUMBER      [0-9]+
ATOM        ([A-Za-z_][A-Za-z0-9_.-]*)|([0-9][A-Za-z0-9_.-]+)

%%

[ \t\r\n]+                 ;

"#".*                      ;
"//".*                     ;

"create"                   return CREATE;
"id"                       return ID;
"description"              return DESCRIPTION;
"mode"                     return MODE;

"listen"                   return LISTEN;
"when"                     return WHEN;
"do"                       return DO;

"if"                       return IF;
"else"                     return ELSE;

"and"                      return AND;
"or"                       return OR;
"not"                      return NOT;

"is"                       return IS;

"after"                    return AFTER;
"before"                   return BEFORE;

"state"                    return STATEKW;
"time"                     return TIMEKW;

"device_id"               return DEVICE_ID;
"entity_id"               return ENTITY_ID;

"with"                     return WITH;
"changes"                  return CHANGES;
"to"                       return TO;
"sun"                      return SUN;
"event"                    return EVENT;
"offset"                   return OFFSET;
"skip_condition"           return SKIP_CONDITION;
"automation"               return AUTOMATION;
"trigger"                  return TRIGGERKW;
"delay"                    return DELAY;

"["                        return LBRACKET;

"]"                        return RBRACKET;

"("                        return LPAREN;

")"                        return RPAREN;

":"                        return COLON;

","                        return COMMA;

"-"                        return MINUS;
"+"                        return PLUS;

{TIME} {
    yylval.str = strdup_safe(yytext);
    return TIME_LITERAL;
}

{DURATION} {
    yylval.str = strdup_safe(yytext);
    return DURATION_LITERAL;
}

\'([^\\\']|\\.)*\' {
    /* strip quotes */
    size_t n = yyleng;
    char *s = (char*)malloc(n-1);
    strncpy(s, yytext+1, n-2);
    s[n-2] = '\0';
    yylval.str = s;
    return STRING;
}

{NUMBER} {
    yylval.str = strdup_safe(yytext);
    return NUMBER;
}

{ATOM} {
    yylval.str = strdup_safe(yytext);
    return IDENTIFIER;
}

. {
    /* unknown character */
    return yytext[0];
}

%%

/* yywrap is disabled via %option noyywrap */