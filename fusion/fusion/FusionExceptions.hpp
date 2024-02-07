// HEADER PLACEHOLDER
// contact Jeff Nye, jeffnye-gh
//
//! \file FusionExceptions.hpp  fusion defined exceptions
#pragma once
#include "FusionTypes.hpp"

#include <exception>
#include <sstream>
#include <string>
#include <iomanip>

//! \class FusionExceptionBase
//! \class HashDuplicateError
//! \class HashIllegalValueError
//! \class ContextDuplicateError
//! \class FieldExtUnknownField
//! \class FieldExtUnknownSpecialField
//! \class FieldExtUnknownFunction

namespace fusion
{

    //! \brief exception base class for fusion operation exceptions
    //!
    //! There are a number of forward looking struct definitions.
    //! In the final implementation I expect this set to be modified
    class FusionExceptionBase : public std::exception
    {
      public:
        //! \brief ...
        const char* what() const noexcept override { return why_.c_str(); }

      protected:
        //! \brief ...
        std::string why_;
        //! \brief ...
        std::stringstream ss;
    };

    //! \brief hashes within a context can not overlap
    //!
    //! Each fusion group has a hash formed from the uids of the
    //! in the group. The hash is the key into a uo/map. These hashes
    //! are checked for uniqueness on construction
    struct HashDuplicateError : FusionExceptionBase
    {
        //! \brief ...
        explicit HashDuplicateError(const std::string & name, const fusion::HashType & hash)
        {
            ss << "Duplicated hash detected, '" << name << "'"
               << " 0x" << std::hex << hash;
            why_ = ss.str();
        }
    };

    //! \brief illegal hash value cause this to be thrown
    //!
    //! At the moment 0 is a reserved/illegal hash value
    struct HashIllegalValueError : FusionExceptionBase
    {
        //! \brief ...
        explicit HashIllegalValueError(const std::string & name, const fusion::HashType & hash)
        {
            ss << "Illegal hash value detected, '" << name << "'"
               << " 0x" << std::hex << hash;
            why_ = ss.str();
        }
    };

    //! \brief context name duplication was detected
    //!
    //! This supports (future feature) of multiple
    //! contexts keyed off the context name string
    struct ContextDuplicateError : FusionExceptionBase
    {
        //! \brief ...
        explicit ContextDuplicateError(const std::string & name)
        {
            ss << "Duplicated context detected, '" << name << "'";
            why_ = ss.str();
        }
    };

    //! \brief field extractor unknown field name
    struct FieldExtUnknownField : FusionExceptionBase
    {
        //! \brief ...
        explicit FieldExtUnknownField(uint32_t fn, std::string dasm)
        {
            ss << "Unknown field: " << fn << " in " << dasm;
            why_ = ss.str();
        }
    };

    //! \brief field extractor unknown special field name
    struct FieldExtUnknownSpecialField : FusionExceptionBase
    {
        //! \brief ...
        explicit FieldExtUnknownSpecialField(uint32_t sfn, std::string dasm)
        {
            ss << "Unknown special field: " << sfn << " in " << dasm;
            why_ = ss.str();
        }
    };

    //! \brief field extractor unknown function
    struct FieldExtUnknownFunction : FusionExceptionBase
    {
        //! \brief ...
        explicit FieldExtUnknownFunction(uint32_t func)
        {
            ss << "Unknown function selection: " << func;
            why_ = ss.str();
        }
    };

} // namespace fusion
