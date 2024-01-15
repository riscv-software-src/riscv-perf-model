// HEADER PLACEHOLDER
// contact Jeff Nye, jeffnye-gh
//
//! \file qparser.cpp  wrapper around parser state machine(s)
#include "qparser.h"
#include <iomanip>
using namespace std;

extern FILE* yyin;
extern int yyparse();
extern QParser* PR;

// ----------------------------------------------------------------
// ----------------------------------------------------------------
QParser::QParser() :
    TRACE_EN(0),
    lineNo(1),
    curCol(1),
    currentFile(""),
    syntaxName(""),
    inputFiles()
{
}

// ----------------------------------------------------------------
// ----------------------------------------------------------------
bool QParser::parse(string fn)
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
bool QParser::parseFiles()
{
    for (size_t i = 0; i < inputFiles.size(); ++i)
    {
        if (!parse(inputFiles[i]))
        {
            // parse() emits the error message
            return false;
        }
        warmReset();
    }
    return true;
}

// ----------------------------------------------------------------
// ----------------------------------------------------------------
void SymbolTable::info(ostream & os, bool shortPath)
{
    size_t maxNameLen = 0, maxTypeLen = 0, maxFileLen = 0;

    for (const auto & pair : table)
    {
        const FslSymbol & symbol = pair.second;
        maxNameLen = max(maxNameLen, symbol.name.length());
        maxTypeLen = max(maxTypeLen, symbol.type.length());
        maxFileLen = max(maxFileLen, symbol.srcFile.length());
    }

    size_t elideLen = 32;
    string elideSep = "....";

    if (shortPath && maxFileLen > elideLen)
    {
        maxFileLen = elideLen + elideSep.length();
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

        if (shortPath && srcFile.length() > elideLen)
        {
            srcFile =
                elideSep + srcFile.substr(srcFile.length() - elideLen);
        }

        os << left << setw(maxNameLen) << symbol.name << " "
           << setw(maxTypeLen) << symbol.type << " " << setw(7)
           << symbol.lineNo << " " << setw(maxFileLen) << srcFile << "\n";
    }

    os << "\n";
    os << "Symbol table total entries " << table.size() << "\n";
    os << endl;
}
