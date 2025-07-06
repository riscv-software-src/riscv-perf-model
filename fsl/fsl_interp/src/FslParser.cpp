//! \file fslparser.cpp  wrapper around parser state machine(s)
#include "FslParser.h"
#include "Msg.h"
#include "Options.h"
#include <filesystem>
#include <iomanip>
using namespace std;

extern FILE* yyin;
extern int yyparse();
extern FslParser* FP;

// ----------------------------------------------------------------
// ----------------------------------------------------------------
FslParser::FslParser()
  : TRACE_EN(0),
    lineNo(1),
    curCol(1),
    currentFile(""),
    syntaxName(""),
    inputFiles()
{
  FP = this;
}

// ----------------------------------------------------------------
// ----------------------------------------------------------------
bool FslParser::parse(const string &fn)
{
    currentFile = fn;

    FILE* file = fopen(currentFile.c_str(), "r");

    if (!file)
    {
        emsg("Can not open file '" + currentFile + "'");
        return false;
    }

    int fail = 0;

    yyin = file;
    fail = yyparse();

    fclose(file);

    if (fail)
    {
        // yyerror() reports the error
        return false;
    }

    return true;
}

// ----------------------------------------------------------------
// ----------------------------------------------------------------
bool FslParser::parseFiles()
{
  if(opts->input_files.empty()) {
    msg->emsg("No input files.");
    return false;
  }

  for(const auto & fn : opts->input_files) {
    if(opts->verbose) msg->imsg("Parsing "+tq(fn));

    if (!parse(fn)) {
        // parse() emits the error message
        return false;
    }
    warmReset();
  }

  return true;
}

// ----------------------------------------------------------------
// ----------------------------------------------------------------
void SymbolTable::info(ostream & os, bool justFileName) const
{
    size_t maxNameLen = 0, maxTypeLen = 0, maxFileLen = 0;

    for (const auto & pair : table)
    {
        const FslSymbol & symbol = pair.second;

        size_t srcFileLength = symbol.srcFile.length();
        //Get the max length of the path or just file name+ext
        if(justFileName)
        {
            filesystem::path path(symbol.srcFile);
            string baseName  = path.filename().string();
            string extension = path.extension().string();
            srcFileLength  = string(baseName+"."+extension).length();
        } 
    
        maxNameLen = max(maxNameLen, symbol.name.length());
        maxTypeLen = max(maxTypeLen, symbol.type.length());
        maxFileLen = max(maxFileLen, srcFileLength);
    }

    size_t totalLen = maxNameLen + maxTypeLen + maxFileLen;

    os << endl;

    os << string(totalLen + 10, '-') << "\n";
    os << "Symbol table\n";

    os << left << setw(maxNameLen) << "Name"
       << " " << setw(maxTypeLen) << "Type"
       << " " << setw(8) << "Line"
       << " " << setw(maxFileLen) << "File"
       << "\n";

    os << string(totalLen + 10, '-') << "\n";

    for (const auto & pair : table)
    {
        const FslSymbol & symbol = pair.second;

        string srcFile = symbol.srcFile;

        if(justFileName)
        {
            filesystem::path path(srcFile);
            srcFile  = string(path.filename().string()); 
        } 

        os << left << setw(maxNameLen) << symbol.name << " "
           << setw(maxTypeLen) << symbol.type << " " << setw(7)
           << symbol.lineNo << " " << setw(maxFileLen) << srcFile << "\n";
    }

    os << "\n";
    os << "Symbol table total entries " << table.size() << "\n";
    os << endl;
}
