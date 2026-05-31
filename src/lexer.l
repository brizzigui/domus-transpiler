%{
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void print_token(const char *type, const char *value)
{
    printf("%-20s %s\n", type, value);
}
%}

%option noyywrap

%%

"create"                { print_token("CREATE", yytext); }
"id"                    { print_token("ID", yytext); }
"description"           { print_token("DESCRIPTION", yytext); }
"mode"                  { print_token("MODE", yytext); }

"listen"                { print_token("LISTEN", yytext); }
"when"                  { print_token("WHEN", yytext); }
"do"                    { print_token("DO", yytext); }

"if"                    { print_token("IF", yytext); }
"else"                  { print_token("ELSE", yytext); }

"and"                   { print_token("AND", yytext); }
"or"                    { print_token("OR", yytext); }

"state"                 { print_token("STATE", yytext); }
"device"                { print_token("DEVICE", yytext); }
"sun"                   { print_token("SUN", yytext); }
"time"                  { print_token("TIME", yytext); }
"trigger"               { print_token("TRIGGER", yytext); }

"entity_id"             { print_token("ENTITY_ID", yytext); }
"device_id"             { print_token("DEVICE_ID", yytext); }

"service"               { print_token("SERVICE", yytext); }
"target"                { print_token("TARGET", yytext); }
"data"                  { print_token("DATA", yytext); }

"["                     { print_token("LBRACKET", yytext); }
"]"                     { print_token("RBRACKET", yytext); }

"("                     { print_token("LPAREN", yytext); }
")"                     { print_token("RPAREN", yytext); }

"{"                     { print_token("LBRACE", yytext); }
"}"                     { print_token("RBRACE", yytext); }

":"                     { print_token("COLON", yytext); }
","                     { print_token("COMMA", yytext); }

[0-9]{2}:[0-9]{2}:[0-9]{2} {
    print_token("TIME_LITERAL", yytext);
}

-?[0-9]+h[0-9]+m[0-9]+s([0-9]+ms)? {
    print_token("DURATION", yytext);
}

\"([^\\\"]|\\.)*\" {
    print_token("STRING", yytext);
}

[0-9]+(\.[0-9]+)? {
    print_token("NUMBER", yytext);
}

[a-zA-Z_][a-zA-Z0-9_.-]* {
    print_token("IDENTIFIER", yytext);
}

[ \t\r\n]+ ;

. {
    printf("UNKNOWN             %s\n", yytext);
}

%%

int main(void)
{
    yylex();
    return 0;
}