%{
#include <stdio.h>
#include <stdlib.h>
%}

%option noyywrap
%option yylineno

TIME        [0-9]{2}:[0-9]{2}:[0-9]{2}
DURATION    [0-9]{2}h[0-9]{2}m[0-9]{2}s([0-9]{2,3}ms)?
ATOM        [A-Za-z0-9_][A-Za-z0-9_.-]*

%%

[ \t\r\n]+                 ;

"#".*                      ;
"//".*                     ;

"create"                   { printf("CREATE               %s\n", yytext); }
"id"                       { printf("ID                   %s\n", yytext); }
"description"              { printf("DESCRIPTION          %s\n", yytext); }
"mode"                     { printf("MODE                 %s\n", yytext); }

"listen"                   { printf("LISTEN               %s\n", yytext); }
"when"                     { printf("WHEN                 %s\n", yytext); }
"do"                       { printf("DO                   %s\n", yytext); }

"if"                       { printf("IF                   %s\n", yytext); }
"else"                     { printf("ELSE                 %s\n", yytext); }

"and"                      { printf("AND                  %s\n", yytext); }
"or"                       { printf("OR                   %s\n", yytext); }
"not"                      { printf("NOT                  %s\n", yytext); }

"state"                    { printf("STATE                %s\n", yytext); }
"time"                     { printf("TIME                 %s\n", yytext); }

"device_id"               { printf("DEVICE_ID            %s\n", yytext); }
"entity_id"               { printf("ENTITY_ID            %s\n", yytext); }

"["                        { printf("LBRACKET             [\n"); }
"]"                        { printf("RBRACKET             ]\n"); }

"("                        { printf("LPAREN               (\n"); }
")"                        { printf("RPAREN               )\n"); }

":"                        { printf("COLON                :\n"); }
","                        { printf("COMMA                ,\n"); }

{TIME} {
    printf("TIME_LITERAL         %s\n", yytext);
}

{DURATION} {
    printf("DURATION             %s\n", yytext);
}

\'([^\\\']|\\.)*\' {
    printf("STRING               %s\n", yytext);
}

{ATOM} {
    printf("IDENTIFIER           %s\n", yytext);
}

. {
    printf("UNKNOWN              %s\n", yytext);
}

%%
int main(void)
{
    yylex();
    return 0;
}