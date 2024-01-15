%{
// HEADER PLACEHOLDER
// contact Jeff Nye, jeffnye-gh
//
//! \file fsl.l  flex scanner for FSL
// ----------------------------------------------------------------
// Simple lexer derived from the original qparser
// Future: complete wrapping into QP, location info, api variants
// ----------------------------------------------------------------
#include <cstdio>
#include <iomanip>
#include <iostream>
#include <memory>
#include <string>
#include "qparser.h"
#include "yy.tab.hpp"
using namespace std;

extern QParser *QP;

const std::string DIGIT = "[0-9]";
const std::string LETTER = "[a-zA-Z_]";
const std::string HEX_DIGIT = "[a-fA-F0-9]";
const std::string EXPONENT = "[Ee][+-]?" + DIGIT + "+";

//D   [0-9]
//L   [a-zA-Z_]
//H   [a-fA-F0-9]
//E   [Ee][+-]?{D}+
//FS  (f|F|l|L)
//IS  (u|U|l|L)*

// ----------------------------------------------------------------
// ----------------------------------------------------------------
#define YY_DECL extern int yylex()

#define E(s) if(QP->TRACE_EN) cout<<dec<<QP->lineNo<<": "<<s<<endl;

#define GET_STR yylval.str = new string(yytext)
//Future feature - RET will help with capturing location info
#define RET(TOK) return TOK

%}

NOT_EQ     "!="
IS_EQ      "=="

UNSIGNED_CONST u[0-9]+[0-9]*
SIGNED_CONST   s[0-9]+[0-9]*

%x COMMENT

%option nounput
%option noyywrap

%%

"/*"          { BEGIN(COMMENT); }

<COMMENT>{
  "*/"        { BEGIN(INITIAL); } 
  [^*\n]+     { /* Ignore non-newline and non-asterisk characters */ }
  "*"         { /* Ignore standalone asterisk characters */ }
  "\n"        { ++QP->lineNo; } // Increment line count on newline
}

\/\/[^\n]*   { E("COMMENT") }

constraints  { E("CONSTRAINTS")  RET(CONSTRAINTS);  }
emit         { E("EMIT")         RET(EMIT);         }
encode_order { E("ENCODE_ORDER") RET(ENCODE_ORDER); }
encoding     { E("ENCODING")     RET(ENCODING);  }
fusion       { E("FUSION")       RET(FUSION);    }
gpr          { E("GPR")          RET(GPR);       }
input_seq    { E("INPUT_SEQ")    RET(INPUT_SEQ); }
isa          { E("ISA")          RET(ISA);       }
sequence     { E("SEQUENCE")     RET(SEQUENCE);  }
transform    { E("TRANSFORM")    RET(TRANSFORM); }
uarch        { E("UARCH")        RET(UARCH);     }

_req_        {  /* get a unique ID for _req_ symbols and add to symtab */
               E("REQ")
               std::string symName = QP->newReqSymbol();
               FslSymbol sym(symName,QP->lineNo,QP->currentFile,"REQ_TYPE");
               QP->insertSymbol(symName,sym);
               RET(REQ_TOKEN);
             }

_opt_        { /* get a unique ID for _opt_ symbols and add to symtab */
               E("OPT")
               std::string symName = QP->newOptSymbol();
               FslSymbol sym(symName,QP->lineNo,QP->currentFile,"OPT_TYPE");
               QP->insertSymbol(symName,sym);
               RET(OPT_TOKEN);
             }

{UNSIGNED_CONST} { E("UN_CONST") GET_STR; RET(UN_CONST); }
{SIGNED_CONST}   { E("S_CONST")  GET_STR; RET(S_CONST); }

{NOT_EQ}  { E("NOT_EQ") RET(NOT_EQ); }
{IS_EQ}   { E("IS_EQ")  RET(IS_EQ); }

[\.]*[_a-z\.A-Z0-9]+  {
                 //E("ID")
                 yylval.sval = strdup(yytext);
                 E(yytext)
                 FslSymbol sym(yytext,QP->lineNo,QP->currentFile);
                 QP->insertSymbol(yytext,sym);
                 RET(ID);
    return ID;
               }

[\n]   { ++QP->lineNo; QP->curCol=1; }
[ \t]  { /* this does not handle col position yet */; }
.      { ++QP->curCol; E(yytext[0]) RET(yytext[0]); }
%%
// ------------------------------------------------------------------
// ------------------------------------------------------------------
void yyerror(const char *s) {
  std::cout << "-E: "<<QP->currentFile<<": "
            << s <<" line "<<std::dec<<QP->lineNo<<std::endl;
}

