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
        if (instance == nullptr) { instance = new Options(); }
        return instance;
    }

    // ----------------------------------------------------------------
    // support methods
    // ----------------------------------------------------------------
    //! \brief ...
    void buildOptions(po::options_description &);

    //! \brief ...
    static bool checkOptions(po::variables_map &,
                             po::options_description &,bool);

    //! \brief ...
    void setupOptions(int, char**);

    //! \brief ...
    static void usage(po::options_description & opt)
    {
        std::cout << opt << std::endl;
    }

    //! \brief ...
    static void version();

    // ----------------------------------------------------------------
    //! \brief ...
    std::string output_file;
    //! \brief ...
    std::vector<std::string> input_files;
    //! \brief ...
    bool trace_en{false};
    //! \brief ...
    bool verbose{false};
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
    Options() = default;

    //! \brief ...
    Options(const Options &) = delete; //NOLINT
    //! \brief ...
    Options(Options &&) = delete; //NOLINT
    //! \brief ...
    Options & operator=(const Options &) = delete; //NOLINT
};

//! \brief ...
extern std::shared_ptr<Options> opts;
