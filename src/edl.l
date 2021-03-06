%{
#include <string>
#include "ast.h"
#include "parser.hpp"
#define SAVE_TOKEN yylval.string = new std::string(yytext, yyleng)
#define TOKEN(t) (yylval.token = t)

unsigned int g_CurLine=1;		/* For error handling */

extern "C" int yywrap() { }
%}

%%

DECLARE					{ return TOKEN(TOK_DECLARE); }
HANDLER					{ return TOKEN(TOK_HANDLER); }
STATES					{ return TOKEN(TOK_STATES); }
STATE					{ return TOKEN(TOK_STATE); }
ALIAS					{ return TOKEN(TOK_ALIAS); }
IF					{ return TOKEN(TOK_IF); }
NEXT					{ return TOKEN(TOK_NEXT); }
PUSH					{ return TOKEN(TOK_PUSH); }
POP					{ return TOKEN(TOK_POP); }
INSTRUCTION				{ return TOKEN(TOK_INSTRUCTION); }
EXECUTE					{ return TOKEN(TOK_EXECUTE); }
ROL					{ return TOKEN(TOK_ROL); }
ROR					{ return TOKEN(TOK_ROR); }
MAPPING					{ return TOKEN(TOK_MAPPING); }
AFFECT					{ return TOKEN(TOK_AFFECT); }
AS					{ return TOKEN(TOK_AS); }
PIN					{ return TOKEN(TOK_PIN); }
FUNCTION				{ return TOKEN(TOK_FUNCTION); }
CALL					{ return TOKEN(TOK_CALL); }

INSTANCE				{ return TOKEN(TOK_INSTANCE); }

IN					{ return TOKEN(TOK_IN); }
OUT					{ return TOKEN(TOK_OUT); }
BIDIRECTIONAL				{ return TOKEN(TOK_BIDIRECTIONAL); }

ALWAYS					{ return TOKEN(TOK_ALWAYS); }
CHANGED					{ return TOKEN(TOK_CHANGED); }
TRANSITION				{ return TOKEN(TOK_TRANSITION); }


CONNECT					{ return TOKEN(TOK_CONNECT); }
CLOCK_GEN				{ return TOKEN(TOK_CLOCK_GEN); }
PULLUP					{ return TOKEN(TOK_PULLUP); }
HIGH_IMPEDANCE				{ return TOKEN(TOK_HIGH_IMPEDANCE); }


INTERNAL				{ return TOKEN(TOK_INTERNAL); }

ZERO					{ return TOKEN(TOK_ZERO); }
NONZERO					{ return TOKEN(TOK_NONZERO); }
SIGN					{ return TOKEN(TOK_SIGN); }
NOSIGN					{ return TOKEN(TOK_NOSIGN); }
PARITYEVEN				{ return TOKEN(TOK_PARITYEVEN); }
PARITYODD				{ return TOKEN(TOK_PARITYODD); }
CARRY					{ return TOKEN(TOK_CARRY); }
NOCARRY					{ return TOKEN(TOK_NOCARRY); }
BIT					{ return TOKEN(TOK_BIT); }
INVBIT					{ return TOKEN(TOK_INVBIT); }
FORCERESET				{ return TOKEN(TOK_FORCERESET); }
FORCESET				{ return TOKEN(TOK_FORCESET); }
OVERFLOW				{ return TOKEN(TOK_OVERFLOW); }
NOOVERFLOW				{ return TOKEN(TOK_NOOVERFLOW); }

DEBUG_TRACE				{ return TOKEN(TOK_TRACE); }
BASE					{ return TOKEN(TOK_BASE); }

C_FUNC_EXTERN				{ return TOKEN(TOK_C_FUNC_EXTERN); }

[a-zA-Z_][a-zA-Z0-9_]*			{ SAVE_TOKEN; return TOK_IDENTIFIER; }
[0-9]+					{ SAVE_TOKEN; return TOK_INTEGER; /* decimal */}
\%[01]+					{ SAVE_TOKEN; return TOK_INTEGER; /* binary */}
\$[0-9a-fA-F]+				{ SAVE_TOKEN; return TOK_INTEGER; /* hexadecimal */}
\"(\\.|[^"])*\"				{ SAVE_TOKEN; return TOK_STRING; }

"<->"					{ return TOKEN(TOK_EXCHANGE); }
"[["					{ return TOKEN(TOK_INDEXOPEN); }
"]]"					{ return TOKEN(TOK_INDEXCLOSE); }
"++"					{ return TOKEN(TOK_DADD); }
"--"					{ return TOKEN(TOK_DSUB); }
".."					{ return TOKEN(TOK_DDOT); }
"!="					{ return TOKEN(TOK_CMPNEQ); }
"=="					{ return TOKEN(TOK_CMPEQ); }
"//"					{ return TOKEN(TOK_SDIV); }
"%%"					{ return TOKEN(TOK_SMOD); }
"{"					{ return TOKEN(TOK_LBRACE); }
"}"					{ return TOKEN(TOK_RBRACE); }
"["					{ return TOKEN(TOK_LSQR); }
"]"					{ return TOKEN(TOK_RSQR); }
":"					{ return TOKEN(TOK_COLON); }
";"					{ return TOKEN(TOK_EOS); }
","					{ return TOKEN(TOK_COMMA); }
"<-"					{ return TOKEN(TOK_ASSIGNLEFT); }
"->"					{ return TOKEN(TOK_ASSIGNRIGHT); }
"+"					{ return TOKEN(TOK_ADD); }
"-"					{ return TOKEN(TOK_SUB); }
"*"					{ return TOKEN(TOK_MUL); }
"/"					{ return TOKEN(TOK_DIV); }
"%"					{ return TOKEN(TOK_MOD); }
"("					{ return TOKEN(TOK_OBR); }
")"					{ return TOKEN(TOK_CBR); }
"|"					{ return TOKEN(TOK_BAR); }
"&"					{ return TOKEN(TOK_AMP); }
"^"					{ return TOKEN(TOK_HAT); }
"."					{ return TOKEN(TOK_DOT); }
"@"					{ return TOKEN(TOK_AT); }
"~"					{ return TOKEN(TOK_TILDE); }
"<="					{ return TOKEN(TOK_CMPLESSEQ); }
"<"					{ return TOKEN(TOK_CMPLESS); }
">="					{ return TOKEN(TOK_CMPGREATEREQ); }
">"					{ return TOKEN(TOK_CMPGREATER); }

[ \t\r]					{ /* Consume white space */ }
#.*					{ /* Ignore comments */ }

[\n]					{ g_CurLine++; }

.					{ printf("Unrecognised character in input : %c  (line %d)\n",*yytext,g_CurLine); yyterminate(); }

%%
