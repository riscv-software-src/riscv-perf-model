#pragma once
#include <iostream>
#include <string>
#include <vector>
#include <map>
#ifndef Q_MOC_RUN
#include <boost/program_options.hpp>
#include <boost/graph/directed_graph.hpp>
#include <boost/graph/graphviz.hpp>
#endif
namespace po = boost::program_options;

// ----------------------------------------------------------------
//! \brief Stubbed version of QParser for this draft PR
//!
//! This is an edited version of QParser. It supports the fusion
//! dsl proposal, other functionality was removed, so the structure
//! looks like over kill.
// ----------------------------------------------------------------
struct QParser
{
    QParser() : isInLine(true) {}

    QParser(int, char**) : isInLine(false) {}

    ~QParser() {}

    //! \brief restore parser state between files
    void resetParser() {}

    //! \brief cmd line options when !isInLine
    void buildOpts(po::options_description &) {}

    //! \brief cmd line option checks when !isInLine
    bool checkOpts(po::variables_map & vm, po::options_description & desc)
    {
        return true;
    }

    //! \brief set the inputFile vector when isInLine
    void setInputFiles(const std::vector<std::string> & fv)
    {
        inputFiles = fv;
    }

    //! \brief parse the inputFile vector
    //!
    //! This returns a shell type err value, this is historical
    //! and conventional w/ parsers built with FLEX/BISON.
    int parse()
    {
        for (auto f : inputFiles)
        {
            if (!parse(f))
                return 1;
        }
        return 0;
    }

    //! \brief parse one file
    bool parse(std::string) { return true; }

    //! \brief limited version of qparse supports 1 syntax
    bool isValidIsa(std::string n) { return n == "q" ? true : false; }

    // --------------------------------------------------------------
    //! \brief std boost program option var map
    po::variables_map vm;

    //! \brief parser mode, inLine = is embedded in an application
    bool isInLine{true};

    //! \brief verbose lexer console output
    uint32_t TRACE_EN{false};
    //! \brief lineNo of current file
    uint32_t lineNo{1};
    //! \brief file being parsed
    std::string currentFile{""};
    //! \brief default ISA name is always q in this version
    std::string isaName{"q"};
    //! \brief last error
    std::string errMsg{""};
    //! \brief list of files
    //!
    //! This is the only case so far where this could be a fusion
    //! type as in FileNameListType. Standard QParser does not know
    //! about those types of course.
    std::vector<std::string> inputFiles;
};
