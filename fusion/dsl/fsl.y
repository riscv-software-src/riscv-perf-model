// HEADER PLACEHOLDER
// contact Jeff Nye, jeffnye-gh
//
//! \file fsl.y  bison grammar for FSL
// ----------------------------------------------------------------
// Simplified grammar with ID location info and symbol table
//
// There are no expected r/r s/r conflicts in this grammar.
// Future: %language, variant api, unique_ptr
// ----------------------------------------------------------------
%{
#include <cstdio>
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <iostream>
#include "qparser.h"
using namespace std;

extern QParser *QP;

extern int yylex();
extern int yyparse();
extern FILE *yyin;
extern char *yytext;
extern void yyerror(const char *s);
%}

%union {
  int     ival;
  float   fval;
  char   *sval;
  std::string *str;
}

%token CONSTRAINTS
%token ENCODE_ORDER
%token ENCODING
%token EMIT
%token FUSION
%token GPR
%token INPUT_SEQ
%token ISA
%token MNEMONIC
%token SEQUENCE
%token TRANSFORM
%token UARCH
%token REQ_TOKEN
%token OPT_TOKEN

%token NOT_EQ IS_EQ
%token <str>  UN_CONST S_CONST
%type  <str>  id
%token <ival> INT
%token <fval> FLOAT
%token <str>  QSTRING
%token <sval> ID

%locations
%start top

%%
top:
    source_line
  | top source_line
  ;

source_line:
    fusion_decl
  | fusion_statement
  ;

fusion_decl:
    FUSION id '{'  fusion_statements '}'
    {
      QP->setSymType(*$2,"FUSION_DECL_NAME");
    }
  | FUSION id '{' '}'
    {
      QP->setSymType(*$2,"FUSION_DECL_NAME");
    }
  ;

fusion_statements:
    fusion_statement
  | fusion_statements fusion_statement
  ;

fusion_statement:
    constraints_decl
  | encoding_decl
  | isa_decl
  | input_seq_decl
  | sequence_decl
  | transform_decl
  | uarch_decl
  ;

isa_decl:
    ISA id
    {
      QP->setSymType(*$2,"ISA_DECL_NAME");
    }
  ;

input_seq_decl:
    INPUT_SEQ id
    {
      QP->setSymType(*$2,"INPUT_SEQ_DECL_NAME");
    }
  ;

uarch_decl:
    UARCH id
    {
      QP->setSymType(*$2,"UARCH_DECL_NAME");
    }
  ;

constraints_decl:
    CONSTRAINTS id '(' arg_list ')' '{' constraints_statements '}'
    {
      QP->setSymType(*$2,"CONSTRAINTS_DECL_NAME");
    }
  | CONSTRAINTS id '(' arg_list ')' '{' '}'
    {
      QP->setSymType(*$2,"CONSTRAINTS_DECL_NAME");
    }
  | constraints_ctor
  ;

constraints_ctor:
    CONSTRAINTS id '(' arg_list ')'
    {
      QP->setSymType(*$2,"CONSTRAINTS_CTOR_NAME");
    }
  | CONSTRAINTS id '('          ')'
    {
      QP->setSymType(*$2,"CONSTRAINTS_CTOR_NAME");
    }
  ;

constraints_statements:
    constraints_statement
  | constraints_statements constraints_statement
  ;

constraints_statement:
    gpr_type      id_list
  | unsigned_type id_list
  | signed_type   id_list
  | constraint_expression
  ;

constraint_expression:
    chained_equal_statement
  | chained_not_equal_statement
  ;

chained_equal_statement:
    id IS_EQ id
  | chained_equal_statement IS_EQ id
  ;

chained_not_equal_statement:
    id NOT_EQ id
  | chained_not_equal_statement NOT_EQ id
  ;

sequence_decl:
    SEQUENCE id '(' arg_list ')' '{' sequence_statements '}'
    {
      QP->setSymType(*$2,"SEQUENCE_DECL_NAME");
    }
  | SEQUENCE id '(' arg_list ')' '{' '}'
    {
      QP->setSymType(*$2,"SEQUENCE_DECL_NAME");
    }
  | sequence_ctor
  ;

sequence_ctor:
    SEQUENCE id '(' arg_list ')'
    {
      QP->setSymType(*$2,"SEQUENCE_CTOR_NAME");
    }
  | SEQUENCE id '('          ')'
    {
      QP->setSymType(*$2,"SEQUENCE_CTOR_NAME");
    }
  ;

sequence_statements:
    sequence_statement
  | sequence_statements sequence_statement
  ;

sequence_statement:
    id id_list 
    {
      QP->setSymType(*$1,"MNEMONIC");
    }
  | REQ_TOKEN
    {
     //symtab is handled by flex to generate a unique id name
    }
  | OPT_TOKEN
    {
     //symtab is handled by flex to generate a unique id name
    }
  ;

transform_decl:
    TRANSFORM id '(' arg_list ')' '{' transform_statements '}'
    {
      QP->setSymType(*$2,"TRANSFORM_DECL_NAME");
    }
  | TRANSFORM id '(' arg_list ')' '{' '}'
    {
      QP->setSymType(*$2,"TRANSFORM_DECL_NAME");
    }
  | transform_ctor
  ;

transform_ctor:
    TRANSFORM id '(' arg_list ')'
    {
      QP->setSymType(*$2,"TRANSFORM_CTOR_NAME");
    }
  | TRANSFORM id '('          ')'
    {
      QP->setSymType(*$2,"TRANSFORM_CTOR_NAME");
    }
  ;

transform_statements:
    transform_statement
  | transform_statements transform_statement
  ;

transform_statement:
    encoding_decl
  | emit_statement
  ;

encoding_decl:
    ENCODING id '(' arg_list ')' '{' encoding_statements '}'
    {
      QP->setSymType(*$2,"ENCODING_DECL_NAME");
    }
  | ENCODING id '(' arg_list ')' '{' '}'
    {
      QP->setSymType(*$2,"ENCODING_DECL_NAME");
    }
  | encoding_ctor

encoding_ctor:
    ENCODING id '(' arg_list ')'
    {
      QP->setSymType(*$2,"ENCODING_CTOR_NAME");
    }
  | ENCODING id '('          ')'
    {
      QP->setSymType(*$2,"ENCODING_CTOR_NAME");
    }
  ;

encoding_statements:
    encoding_statement
  | encoding_statements encoding_statement
  ;

encoding_statement:
    unsigned_type id
    {
      QP->setSymType(*$2,"UNSIGNED");
    }
  | signed_type id
    {
      QP->setSymType(*$2,"SIGNED");
    }
  | gpr_type id
    {
      QP->setSymType(*$2,"GPR");
    }
  | encode_order_statement
  ;

encode_order_statement:
    ENCODE_ORDER '(' arg_list ')'
  ;

emit_statement:
    EMIT '(' encoding_use_call ')'
  | EMIT '(' id ')'
  ;

encoding_use_call:
    id '(' arg_list ')'
  ;

assignment_expression:
    id '=' id
  ;

id_list:
    id
  | id_list ',' id
  ;

arg_list:
    arg
  | arg_list ',' arg
  ;

arg:
    id
  | assignment_expression
  ;

gpr_type:
    GPR
    {
    }
  ;

unsigned_type:
    UN_CONST
    {
      @$ = @1;
    }
  ;

signed_type:
    S_CONST
    {
      @$ = @1;
    }
  ;

id:
    ID
    {
      $$ = new std::string($1);
//      $$ = make_shared<string>($1);
      @$ = @1;
    }
  ;
%%

