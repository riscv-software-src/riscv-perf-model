//
// Created by David Murrell on 2/19/20.
//
#pragma once

#include <exception>
#include <sstream>
#include <string>

/**
 * Decoder exceptions base class
 */
class uArchInfoBaseException : public std::exception
{
  public:
    virtual const char* what() const noexcept { return why_.c_str(); }

  protected:
    std::string why_;
};

/**
 * uArchInfo parse error: the given unit for the instruction was not found by
 * the uArchInfo object.
 *
 * This can happen from a mis-spelling of the unit in the JSON, or from a need
 * to update the uArchInfo's list of supported units
 */
class uArchInfoUnknownUnit : public uArchInfoBaseException
{
  public:
    uArchInfoUnknownUnit(const std::string & mnemonic, const std::string & unit_name) :
        uArchInfoBaseException()
    {
        std::stringstream ss;
        ss << "Instruction '" << mnemonic << "': "
           << " unit '" << unit_name << "' "
           << "is not known (uArchInfo object). Could be a mis-spelling in the "
              "JSON file";
        why_ = ss.str();
    }
};

/**
 * uArchInfo parse error: the given issue target for the instruction was not
 * found by the uArchInfo object.
 *
 * This can happen from a mis-spelling of the issue target in the JSON, or from
 * a need to update the uArchInfo's list of supported units
 */
class uArchInfoUnknownIssueTarget : public uArchInfoBaseException
{
  public:
    uArchInfoUnknownIssueTarget(const std::string & mnemonic, const std::string & target_name) :
        uArchInfoBaseException()
    {
        std::stringstream ss;
        ss << "Instruction '" << mnemonic << "': "
           << " issue target '" << target_name << "' "
           << "is not known (uArchInfo object). Could be a mis-spelling in the "
              "JSON file";
        why_ = ss.str();
    }
};

/**
 * uArchInfo parsing error: the given ROB group begin/end stanza in the JSON is
 * malformed
 *
 * This can happen from a mis-spelling of the rob_group key in the JSON. The
 * form should be one of the following:
 *
 * "rob_group" : ["begin"]
 * "rob_group" : ["end"]
 * "rob_group" : ["begin", "end"]
 */
class uArchInfoROBGroupParseError : public uArchInfoBaseException
{
  public:
    uArchInfoROBGroupParseError(const std::string & mnemonic, const std::string & bad_string) :
        uArchInfoBaseException()
    {
        std::stringstream ss;
        ss << "Instruction '" << mnemonic << "': "
           << " rob_group element '" << bad_string << "'"
           << " is not valid. Needs to be one of 'begin' or 'end'";
        why_ = ss.str();
    }
};
