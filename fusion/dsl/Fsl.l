%{
// HEADER PLACEHOLDER
// contact Jeff Nye, jeffnye-gh
//
//! \file Fsl.l  flex scanner for FSL
// ----------------------------------------------------------------
// Simple lexer derived from the original QParser
// Future: complete wrapping into FP, location info, api variants
// ----------------------------------------------------------------
#include <cstdio>
#include <iomanip>
#include <iostream>
#include <memory>
#include <string>
#include "FslParser.hpp"
#include "yy.tab.hpp"
using namespace std;

extern FslParser *FP;
// ----------------------------------------------------------------
// ----------------------------------------------------------------
#define YY_DECL extern int yylex()

#define E(s) if(FP->TRACE_EN) cout<<dec<<FP->lineNo<<": "<<s<<endl;

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
  "\n"        { ++FP->lineNo; } // Increment line count on newline
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
               std::string symName = FP->newReqSymbol();
               FslSymbol sym(symName,FP->lineNo,FP->currentFile,"REQ_TYPE");
               FP->insertSymbol(symName,sym);
               RET(REQ_TOKEN);
             }

_opt_        { /* get a unique ID for _opt_ symbols and add to symtab */
               E("OPT")
               std::string symName = FP->newOptSymbol();
               FslSymbol sym(symName,FP->lineNo,FP->currentFile,"OPT_TYPE");
               FP->insertSymbol(symName,sym);
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
                 FslSymbol sym(yytext,FP->lineNo,FP->currentFile);
                 FP->insertSymbol(yytext,sym);
                 RET(ID);
                 return ID;
               }

[\n]   { ++FP->lineNo; FP->curCol=1; }
[ \t]  { /* this does not handle col position yet */; }
.      { ++FP->curCol; E(yytext[0]) RET(yytext[0]); }
%%
// ------------------------------------------------------------------
// ------------------------------------------------------------------
void yyerror(const char *s) {
  std::cout << "-E: "<<FP->currentFile<<": "
            << s <<" line "<<std::dec<<FP->lineNo<<std::endl;
}

