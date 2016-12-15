/*
** grammar for xgc syntax
*/

%{
#define YYDEBUG 1

#include <stdio.h>
#include <X11/X.h>
#include "constants.h"

extern int yylineno;
extern FILE *yyin;

extern void GC_change_logical();
extern void GC_change_arithmetic();
extern void GC_change_constant();
extern void GC_change_math();
extern void GC_change_fillstyle();
extern void GC_change_planemask();
extern void change_test();
extern void run_test();
%}

%union
{
  int num;
  char *ptr;
};

%token <ptr> STRING
%token <num> NUMBER
%token <num> RUN
%token <num> LOGICAL LOGICALTYPE
%token <num> TEST TESTTYPE
%token <num> CONSTANT CONSTANTTYPE
%token <num> PLANEMASK PLANEMASKTYPE
%token <num> ARITHMETIC ARITHMETICTYPE
%token <num> MATH MATHTYPE
%token <num> FILLSTYLE FILLSTYLETYPE

%%

all		: stmts
		;

stmts		: /* empty */
		| stmts '\n'
		| stmts stmt '\n'
		;

stmt		: error
		| RUN 
	{ run_test(); }  
		| TEST TESTTYPE 
	{ change_test ($2, TRUE); }
		| LOGICAL LOGICALTYPE 
	{ GC_change_logical ($2, TRUE); }
		| LOGICAL LOGICALTYPE 
	{ GC_change_arithmetic ($2, TRUE); }
		| ARITHMETIC ARITHMETICTYPE
	{ GC_change_constant ($2, TRUE); }
		| CONSTANT NUMBER 
	{ GC_change_math ($2, TRUE); }
		| MATH MATHTYPE 
	{ GC_change_fillstyle ($2, TRUE); }
		| FILLSTYLE SOLID
	{ GC_change_planemask ($2, TRUE); }
		| PLANEMASK NUMBER
		;

%%
yyerror(s)
     char *s;
{
  fprintf(stderr, "xiegc: syntax error, line %d\n", yylineno);
}
