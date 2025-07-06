// HEADER PLACEHOLDER
// contact Jeff Nye, jeffnye-gh
//
//! \file Options.hpp  testbench options
#pragma once
#include <boost/program_options.hpp>
#include <iostream>
#include <string>
#include <vector>
namespace po = boost::program_options;

//! \brief container for boost program option settings
struct Options
{
    // ----------------------------------------------------------------
    //! \brief singleton
    // ----------------------------------------------------------------
    static Options* getInstance()
    {
        if (!instance)
            instance = new Options();
        return instance;
    }

    // ----------------------------------------------------------------
    // support methods
    // ----------------------------------------------------------------
    //! \brief ...
    void buildOptions(po::options_description &);

    //! \brief ...
    bool checkOptions(po::variables_map &, po::options_description &,bool);

    //! \brief ...
    void setupOptions(int, char**);

    //! \brief ...
    void usage(po::options_description & o) const
    {
        std::cout << o << std::endl;
    }

    //! \brief ...
    void version() const;

    // ----------------------------------------------------------------
    //! \brief future STF file support in test bench
    std::string stf_file{""};
    //! \brief ...
    std::string output_file{""};
    //! \brief ...
    std::vector<std::string> isa_files;
    //! \brief ...
    std::vector<std::string> fsl_files;
    //! \brief files with contorted style
    std::vector<std::string> fsl_syntax_files;
    //! \brief enable extra messages from the tb
    bool tb_verbose{false};
    // ----------------------------------------------------------------
    // ----------------------------------------------------------------
    //! \brief ...
    bool notify_error{false};
    //! \brief placeholder
    bool _query_options{false};
    //! \brief ...
    po::variables_map vm;
    //! \brief ...
    static Options* instance;

  private:
    //! \brief ...
    Options() {}

    //! \brief ...
    Options(const Options &) = delete;
    //! \brief ...
    Options(Options &&) = delete;
    //! \brief ...
    Options & operator=(const Options &) = delete;
};

//! \brief ...
extern std::shared_ptr<Options> opts;
