// HEADER PLACEHOLDER
// contact Jeff Nye, jeffnye-gh
//
//! \file testbench options
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
    void build_options(po::options_description &);

    //! \brief ...
    bool check_options(po::variables_map &, po::options_description &,
                       bool);

    //! \brief ...
    void setup_options(int, char**);

    //! \brief ...
    void usage(po::options_description & o)
    {
        std::cout << o << std::endl;
    }

    //! \brief placeholder
    void version();
    //! \brief placeholder
    void query_options();
    // ----------------------------------------------------------------
    //! \brief future STF file support in test bench
    std::string stf_file{""};

    //! \brief ...
    std::string output_file{""};

    //! \brief ...
    std::vector<std::string> isa_files;
    //! \brief ...
    std::vector<std::string> dsl_files;
    //! \brief placeholder
    std::vector<std::string> cfg_files;

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
