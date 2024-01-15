// HEADER PLACEHOLDER
// contact Jeff Nye, jeffnye-gh
//
//! \file qparser.h  wrapper around parser state machine(s)
#pragma once
#include <fstream>
#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>

// ----------------------------------------------------------------
//! \brief FSL symbol table entry
// ----------------------------------------------------------------
struct FslSymbol
{
    //! \brief ...
    FslSymbol(std::string _n, uint32_t _ln = 0,
              std::string _fn = "", std::string _ty = "UNKNOWN") :
        name(_n),
        lineNo(_ln),
        srcFile(_fn),
        type(_ty)
    {
    }

    //! \brief symbol name
    std::string name;

    //! \brief line no of decl in source file
    uint32_t lineNo;

    //! \brief source file
    std::string srcFile;

    //! \brief symbol type
    //!
    //! FIXME: to be an enum
    std::string type;
};

// ----------------------------------------------------------------
//! \brief FSL symbol table type
// ----------------------------------------------------------------
struct SymbolTable
{
    //! \brief symbol table iterator
    using TableItrType = std::unordered_map<std::string, FslSymbol>::iterator;

    //! \brief look up name in table
    bool findSymbol(std::string name)
    {
        if (table.find(name) != table.end())
            return true;
        return false;
    }

    //! \brief if not already in symtab, insert it
    void insertSymbol(std::string name, FslSymbol s)
    {
        if (!findSymbol(name))
            table.insert(make_pair(name, s));
    }

    //! \brief ...
    void setType(std::string name, std::string type)
    {
        TableItrType itr = table.find(name);
        if (itr != table.end())
        {
            itr->second.type = type;
        }
    }

    //! \brief ...
    void clear() { table.clear(); }

    //! \brief symbol info to stream
    void info(std::ostream & os, bool shortPath = false);

    //! \brief the symbol table
    std::unordered_map<std::string, FslSymbol> table;
};

// ----------------------------------------------------------------
//! \brief QParser support for flex/bison
//!
//! This is an edited version of QParser. It supports the fusion
//! dsl proposal, other functionality was removed, so the structure
//! looks like over kill. 
// ----------------------------------------------------------------
struct QParser
{
    //! \brief ...
    QParser();

    //! \brief ...
    ~QParser() {}

    //! \brief initialize parser state between code bases
    void coldReset()
    {
        warmReset();
        reqId = 0;
        optId = 0;
        symtab.clear();
    }

    //! \brief initialize parser state between files
    void warmReset()
    {
        lineNo = 1;
        curCol = 1;
        currentFile = "";
        errMsg = "";
    }

    //! \brief set the inputFile vector when isInLine
    void setInputFiles(const std::vector<std::string> & fv) { inputFiles = fv; }

    //! \brief parse all input files
    bool parse() { return parseFiles(); }

    //! \brief parse all input files
    bool parseFiles();

    //! \brief parse one file
    bool parse(std::string);

    //! \brief remove the Msg dependency for QParser
    void emsg(std::string m) { std::cout << "-E:QP: " << m << std::endl; }

    // --------------------------------------------------------------
    // Symbol table shim
    // --------------------------------------------------------------
    //! \brief look up string in symbol table
    bool findSymbol(const std::string name) { return symtab.findSymbol(name); }

    //! \brief look up const/char in symbol table
    bool findSymbol(const char* name)
        { return symtab.findSymbol(std::string(name)); }

    //! \brief insert string symbol name into table if not duplicated
    void insertSymbol(const std::string sym, FslSymbol s)
        { symtab.insertSymbol(sym, s); }

    //! \brief insert const/char symbol name into table if not duplicated
    void insertSymbol(const char* sym, FslSymbol s)
        { symtab.insertSymbol(std::string(sym), s); }

    //! \brief set type field of symbol using string name
    void setSymType(const std::string sym, std::string typ)
        { symtab.setType(sym, typ); }

    //! \brief set type field of symbol using const/char name
    void setSymType(const char* sym, std::string typ)
        { symtab.setType(std::string(sym), typ); }

    //! \brief create a unique string id for a _req_ symbol
    //!
    //! _reg + string(reqId++)
    std::string newReqSymbol()
        { return std::string("_req" + std::to_string(reqId++)); }

    //! \brief create a unique id for a _opt_ symbol
    //!
    //! _opt + string(optId++)
    std::string newOptSymbol()
        { return std::string("_opt" + std::to_string(optId++)); }

    // --------------------------------------------------------------
    //! \brief verbose lexer console output
    uint32_t TRACE_EN{false};

    //! \brief lineNo of current file
    uint32_t lineNo{1};

    //! \brief current column location
    //!
    //! Future feature - locations in yyerror reports
    uint32_t curCol{1};

    //! \brief file being parsed
    std::string currentFile{""};

    //! \brief default syntax name is always fsl in this version
    std::string syntaxName{"fsl"};

    //! \brief last error
    std::string errMsg{""};

    //! \brief unique ID counter for _req_ objects
    uint32_t reqId{0};

    //! \brief unique ID counter for _opt_ objects
    uint32_t optId{0};

    //! \brief list of files
    //!
    //! This is the only case so far where this could be a fusion
    //! type as in FileNameListType. Standard QParser does not know
    //! about those types of course.
    std::vector<std::string> inputFiles;

    //! \brief FSL symbol table
    SymbolTable symtab;
};
