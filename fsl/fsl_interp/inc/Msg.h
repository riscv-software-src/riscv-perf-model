// HEADER PLACEHOLDER
// contact Jeff Nye, jeffnye-gh
//
//! \file Msg.hpp   header for simple uniform messages
#pragma once
#include <string>
#include <iostream>
#include <fstream>
#include <memory>

//! \brief singleton for standardized messages
//!
//! I use this to standardize local output.
//!
//! I'm using an ad hoc test bench, a compliant testbench
//! would be part of a fusion capable Sparta unit. In that
//! case this would be replaced with the mechanism found
//! in the unit benches.
struct Msg
{
    //! \brief get the singleton instance
    static Msg* getInstance()
    {
        if (instance == nullptr)
        {
            instance = new Msg;
        }
        return instance;
    }

    ~Msg() = default;

    //! \brief this adds an identifier prefix to messages
    //!
    //! Example -I:MYUNIT: {message}
    void setWho(const std::string & _w) { w = _w + ": "; }

    //! \brief shared message method
    void mmsg(const std::string &p, const std::string &m) const //NOLINT
        { std::cout << p << w << m << std::endl; }

    //! \brief debug messages
    void dmsg(std::string m = "", int v = 4) const //NOLINT
        { if (v <= verbose) { mmsg("-D: ", m); } }

    //! \brief error messages
    void emsg(std::string m = "", int v = 1) const //NOLINT
        { if (v <= verbose) { mmsg("-E: ", m); } }

    //! \brief info messages
    void imsg(std::string m = "", int v = 3) const //NOLINT
        { if (v <= verbose) { mmsg("-I: ", m); } }

    //! \brief warining messages
    void wmsg(std::string m = "", int v = 2) const //NOLINT
        { if (v <= verbose) { mmsg("-W: ", m); } }

    //! \brief warining messages
    void mmsg(std::ostream & o, std::string p, std::string m) const //NOLINT
        { o << p << w << m << std::endl; }

    // ----------------------------------------------------------------
    //! \brief dmsg should be v level 4
    void dmsg(std::ostream & o, std::string m = "", int v = 4) const //NOLINT
        { mmsg(o, "-D: ", std::move(m)); }

    //! \brief ...
    void emsg(std::ostream & o, std::string m = "", int v = 1) const //NOLINT
        { mmsg(o, "-E: ", std::move(m)); }

    //! \brief ...
    void imsg(std::ostream & o, std::string m = "", int v = 3) const //NOLINT
        { mmsg(o, "-I: ", std::move(m)); }

    //! \brief ...
    void wmsg(std::ostream & o, std::string m = "", int v = 2) const //NOLINT
        { mmsg(o, "-W: ", std::move(m)); }

    //! \brief ...
    void msg(std::string m) const { std::cout << m << std::endl; } //NOLINT

    // ----------------------------------------------------------------
    //! \brief helper to show potentially empty strings
    std::string tq(std::string s) const { return "'" + s + "'"; } //NOLINT

    //! \brief ...
    std::string w; //NOLINT
    /**
     * \brief verbosity setting
     *
     * \verbatim
     * verbose 0 - silent
     *         1 - errors
     *         2 - errors,warnings
     *         3 - errors,warnings,info
     *         >= 4 - errors,warnings,info,debug4
     *              - debug messages can be a various levels, debugN
     * \endverbatim
     */

    //! \brief ...
    int verbose{3};

    //! \brief ...
    static Msg* instance;

  private:
    //! \brief ...
    Msg()
    {
        w = "";
        verbose = 3;
    }

    // -------------------------------------------------------------------
    //  Msg(std::string _who="",int _verbose=3)
    //    : w(_who+": "),
    //      verbose(_verbose)
    //  {}
    // -------------------------------------------------------------------
    //! \brief ...copy
    Msg(const Msg &) = delete; //NOLINT
    //! \brief ...move
    Msg(Msg &&) = delete; //NOLINT
    //! \brief ...assign
    Msg & operator=(const Msg &) = delete; //NOLINT
};

//! \brief ...
extern std::unique_ptr<Msg> msg;
