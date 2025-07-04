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
        if (!instance)
            instance = new Msg;
        return instance;
    }

    ~Msg() {} // dtor

    //! \brief this adds an identifier prefix to messages
    //!
    //! Example -I:MYUNIT: {message}
    void setWho(std::string _w) { w = _w + ": "; }

    //! \brief shared message method
    void mmsg(std::string p, std::string m) const
    {
        std::cout << p << w << m << std::endl;
    }

    //! \brief debug messages
    void dmsg(std::string m = "", int v = 4) const
    {
        if (v <= verbose)
            mmsg("-D: ", m);
    }

    //! \brief error messages
    void emsg(std::string m = "", int v = 1) const
    {
        if (v <= verbose)
            mmsg("-E: ", m);
    }

    //! \brief info messages
    void imsg(std::string m = "", int v = 3) const
    {
        if (v <= verbose)
            mmsg("-I: ", m);
    }

    //! \brief warining messages
    void wmsg(std::string m = "", int v = 2) const
    {
        if (v <= verbose)
            mmsg("-W: ", m);
    }

    //! \brief warining messages
    void mmsg(std::ostream & o, std::string p, std::string m) const
    {
        o << p << w << m << std::endl;
    }

    // ----------------------------------------------------------------
    //! \brief dmsg should be v level 4
    void dmsg(std::ostream & o, std::string m = "", int v = 4) const
    {
        mmsg(o, "-D: ", m);
    }

    //! \brief ...
    void emsg(std::ostream & o, std::string m = "", int v = 1) const
    {
        mmsg(o, "-E: ", m);
    }

    //! \brief ...
    void imsg(std::ostream & o, std::string m = "", int v = 3) const
    {
        mmsg(o, "-I: ", m);
    }

    //! \brief ...
    void wmsg(std::ostream & o, std::string m = "", int v = 2) const
    {
        mmsg(o, "-W: ", m);
    }

    //! \brief ...
    void msg(std::string m) const { std::cout << m << std::endl; }

    // ----------------------------------------------------------------
    //! \brief helper to show potentially empty strings
    std::string tq(std::string s) const { return "'" + s + "'"; }

    //! \brief ...
    std::string w;
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
    Msg(const Msg &) = delete;
    //! \brief ...move
    Msg(Msg &&) = delete;
    //! \brief ...assign
    Msg & operator=(const Msg &) = delete;
};

//! \brief ...
extern std::unique_ptr<Msg> msg;
