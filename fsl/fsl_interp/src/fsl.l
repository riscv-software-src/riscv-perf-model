%{
// HEADER PLACEHOLDER
// contact Jeff Nye, jeffnye-gh
//
//! \file Fsl.l  flex scanner for FSL
// ----------------------------------------------------------------
// Simple lexer derived from the original QParser
// Future: complete wrapping into FP, location info, api variants
// ----------------------------------------------------------------
#include <iomanip>
#include <iostream>
#include <memory>
#include <string>
#include "FslParser.h"
#include "yy.tab.hpp"
using namespace std;

extern FslParser *FP;

extern void comment(void);
extern int  check_type(void);
extern void count(void);
int column = 0;
// ----------------------------------------------------------------
// ----------------------------------------------------------------
#define YY_DECL extern int yylex()

#ifndef E
#define E(s) if(FP->TRACE_EN) cout<<dec<<FP->lineNo<<": "<<s<<endl;
#endif

#ifndef GET_STR
#define GET_STR yylval.str = new string(yytext)
#endif

//Future feature - _RET will help with capturing location info
#ifndef _RET
#define _RET(TOK) return TOK
#endif

#ifndef CAPT
#define CAPT(X) { E(#X); count(); _RET(X); }
#endif

%}

D      [0-9]
L      [a-zA-Z_]
H      [a-fA-F0-9]
E      ([Ee][+-]?{D}+)
P     ([Pp][+-]?{D}+)
FS    (f|F|l|L)
IS    ((u|U)|(u|U)?(l|L|ll|LL)|(l|L|ll|LL)(u|U))

UNSIGNED_CONST u[0-9]+[0-9]*
SIGNED_CONST   s[0-9]+[0-9]*
HEX_CONST      "0x"[0-9a-fA-F]+

VLOG_BIN  [0-9]+'[bB][01]+
VLOG_OCT  [0-9]+'[oO][0-7]+
VLOG_DEC  [0-9]+'[dD][0-9]+
VLOG_HEX  [0-9]+'[hH][0-9a-fA-F]+

%x COMMENT
%x SEQUENCE
%option nounput
%option noyywrap

%%
"/*"          { BEGIN(COMMENT); }
<COMMENT>{
  "*/"        { BEGIN(INITIAL); }
  [^*\n]+     { /* Ignore non-newline and non-asterisk characters */ }
  "*"         { /* Ignore standalone asterisk characters */ }
  "\n"        { E("C") ++FP->lineNo; } 
}

\/\/[^\n]*    { E("COMMENT") }

"sequence"    { BEGIN(SEQUENCE); }
<SEQUENCE>{
  "}"         { BEGIN(INITIAL); }
  [^}\n]+     { /* Ignore non-newline/non-brace characters */ }
  "\n"        { E("SEQ")  ++FP->lineNo; }
}

_fail_       { CAPT(FAIL);         }
_pass_       { CAPT(PASS);         }
constraints  { CAPT(CONSTRAINTS);  }
conversion   { CAPT(CONVERSION);   }
csr          { CAPT(CSR);          }
dst          { CAPT(DST); }
emit         { CAPT(EMIT);         }
encode_order { CAPT(ENCODE_ORDER); }
encoding     { CAPT(ENCODING);     }
false        { CAPT(FALSE);        }
fusion       { CAPT(FUSION);       }
gpr          { CAPT(GPR);          }
hasAttr      { CAPT(HASATTR);      }
if           { CAPT(IF);           }
instr        { CAPT(INSTR);        }
ioput        { CAPT(IOPUT);        }
isa          { CAPT(ISA);          }
mnemonic     { CAPT(MNEMONIC);     }
morph        { CAPT(MORPH);        }
opc          { CAPT(OPC);          }
prolog       { CAPT(PROLOG);       }
rdx          { CAPT(RDX);          }
replace      { CAPT(REPLACE);      }
rsx          { CAPT(RSX);          }
s0           { CAPT(S0);           }
s1           { CAPT(S1);           }
setof        { CAPT(SETOF);        }
string       { CAPT(STRING);       }
transform    { CAPT(TRANSFORM);    }
true         { CAPT(TRUE);         }
type         { CAPT(TYPE);         }
uarch        { CAPT(UARCH);        }
writePorts   { CAPT(WRITEPORTS);        }
readPorts    { CAPT(READPORTS);        }
requiredBits { CAPT(REQUIREDBITS);        }

_req_        {  /* get a unique ID for _req_ symbols and add to symtab */
               std::string symName = FP->newReqSymbol();
               FslSymbol sym(symName,FP->lineNo,FP->currentFile,"REQ_TYPE");
               FP->insertSymbol(symName,sym);
               CAPT(REQ_TOKEN);
             }

_opt_        { /* get a unique ID for _opt_ symbols and add to symtab */
               std::string symName = FP->newOptSymbol();
               FslSymbol sym(symName,FP->lineNo,FP->currentFile,"OPT_TYPE");
               FP->insertSymbol(symName,sym);
               CAPT(OPT_TOKEN);
             }

"else"       { CAPT(ELSE); }
"extern"     { CAPT(EXTERN); }
"for"        { CAPT(FOR); }
"imm"        { CAPT(IMM); }
"src"        { CAPT(SRC); }


{UNSIGNED_CONST} { count(); GET_STR; CAPT(UN_CONST);  }
{SIGNED_CONST}   { count(); GET_STR; CAPT(S_CONST);   }
{HEX_CONST}      { CAPT(HEX_CONST);  }
{VLOG_BIN}       { CAPT(VLOG_CONST); }
{VLOG_OCT}       { CAPT(VLOG_CONST); }
{VLOG_DEC}       { CAPT(VLOG_CONST); }
{VLOG_HEX}       { CAPT(VLOG_CONST); }

\"([^"\\]|\\(.|\n))*\" { yylval.sval = strdup(yytext); _RET(QSTRING); }

{L}({L}|{D})*       { count(); return(check_type()); }
0[xX]{H}+{IS}?      { count(); return(CONSTANT); }
0[0-7]*{IS}?        { count(); return(CONSTANT); }
[1-9]{D}*{IS}?      { count(); return(CONSTANT); }
L?'(\\.|[^\\'\n])+' { count(); return(CONSTANT); }

{D}+{E}{FS}?               { count(); return(CONSTANT); }
{D}*"."{D}+{E}?{FS}?       { count(); return(CONSTANT); }
{D}+"."{D}*{E}?{FS}?       { count(); return(CONSTANT); }
0[xX]{H}+{P}{FS}?          { count(); return(CONSTANT); }
0[xX]{H}*"."{H}+{P}?{FS}?  { count(); return(CONSTANT); }
0[xX]{H}+"."{H}*{P}?{FS}?  { count(); return(CONSTANT); }


L?\"(\\.|[^\\"\n])*\"  { count(); return(STRING_LITERAL); }

">>"       { count(); return(RIGHT_OP); }
"<<"       { count(); return(LEFT_OP); }
"++"       { count(); return(INC_OP); }
"--"       { count(); return(DEC_OP); }
"&&"       { count(); return(AND_OP); }
"||"       { count(); return(OR_OP); }
"<="       { count(); return(LE_OP); }
">="       { count(); return(GE_OP); }
"=="       { count(); return(EQ_OP); }
"!="       { count(); return(NE_OP); }
";"        { count(); return(';'); }
("{"|"<%") { count(); return('{'); }
("}"|"%>") { count(); return('}'); }
","        { count(); return(','); }
":"        { count(); return(':'); }
"="        { count(); return('='); }
"("        { count(); return('('); }
")"        { count(); return(')'); }
("["|"<:") { count(); return('['); }
("]"|":>") { count(); return(']'); }
"."        { E(".") count(); return('.'); }
"&"        { count(); return('&'); }
"!"        { count(); return('!'); }
"~"        { count(); return('~'); }
"-"        { count(); return('-'); }
"+"        { count(); return('+'); }
"*"        { count(); return('*'); }
"/"        { count(); return('/'); }
"%"        { count(); return('%'); }
"<"        { count(); return('<'); }
">"        { count(); return('>'); }
"^"        { count(); return('^'); }
"|"        { count(); return('|'); }
"?"        { count(); return('?'); }

[\n]       { FP->lineNo += 1; count(); }
[ \t]      { /* */ }
.          { E(yytext[0]) _RET(yytext[0]); }

%%

//Future pragma support
void comment(void) { }
//void comment(void)
//{
//  char c, prev = 0;
//  
//  while ((c = input()) != 0)      /* (EOF maps to 0) */
//  {
//    if (c == '/' && prev == '*')
//      return;
//    prev = c;
//  }
//  yyerror("unterminated comment");
//}

void count(void)
{
//  int i;

//  for (i = 0; yytext[i] != '\0'; i++)
//    if (yytext[i] == '\n')
//      column = 0;
//    else if (yytext[i] == '\t')
//      column += 8 - (column % 8);
//    else
//      column++;
//  ECHO;
}


int check_type(void)
{
/*
* pseudo code --- this is what it should check
*
*  if (yytext == type_name)
*    return TYPE_NAME;
*
*  return ID;
*/

/*
*  it actually will only return ID
*/
  E(yytext)
  return ID;
}
// ------------------------------------------------------------------
// ------------------------------------------------------------------
void yyerror(const char *s) {
  std::cout << "-E: "<<FP->currentFile<<": "
            << s <<" " << yytext <<", line "
            <<std::dec<<FP->lineNo<<std::endl;
}

