// HEADER PLACEHOLDER
// contact Jeff Nye, jeffnye-gh
//
//! \file fusionexceptions.h  fusion defined exceptions
#pragma once
#include "fusiontypes.h"
#include <exception>
#include <sstream>
#include <string>
#include <iomanip>

//! \class FusionExceptionBase
//! \class HashDuplicateError
//! \class HashIllegalValueError
//! \class ContextDuplicateError
//! \class DslSyntaxError
//! \class FusionExceptionBase
//! \class DslRuntimeError
//! \class JsonSyntaxError
//! \class JsonRuntimeError
//! \class JsonUnknownError
//! \class FieldExtUnknownField
//! \class FieldExtUnknownSpecialField

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
        explicit HashDuplicateError(const std::string & name,
                                    const fusion::HashType & hash)
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
        explicit HashIllegalValueError(const std::string & name,
                                       const fusion::HashType & hash)
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

    //! \brief QParser will throw this
    //!
    //! Throwing this from the parser has not been implemented
    //! yet in this draft PR.
    struct DslSyntaxError : FusionExceptionBase
    {
        //! \brief ...
        explicit DslSyntaxError(const std::string & msg,
                                const int & lineno)
        {
            ss << "DSL syntax error '" << msg << "'" << std::endl
               << "    at line: " << lineno;
            why_ = ss.str();
        }
    };

    //! \brief for errors discovered when using dsl values
    struct DslRuntimeError : FusionExceptionBase
    {
        //! \brief ...
        explicit DslRuntimeError(const std::string & msg)
        {
            ss << "DslRuntimeError placeholder: " << msg;
            why_ = ss.str();
        }
    };

    //! \brief catch all from the DSL code
    struct DslUnknownError : FusionExceptionBase
    {
        //! \brief ...
        explicit DslUnknownError(const std::string & msg)
        {
            ss << "DslUnknownError placeholder: " << msg;
            why_ = ss.str();
        }
    };

    //! \brief json syntax errors
    //!
    //! JSON support is planned but not implemented
    //!
    //! FIXME: check if nlohmann bombs out before
    //! we get here. If so remove this.
    struct JsonSyntaxError : FusionExceptionBase
    {
        //! \brief ...
        explicit JsonSyntaxError(const int & lineno)
        {
            ss << "JSON syntax error line: " << lineno;
            why_ = ss.str();
        }
    };

    //! \brief for errors discovered when using json values
    //!
    //! JSON support is planned but not implemented
    struct JsonRuntimeError : FusionExceptionBase
    {
        //! \brief ...
        explicit JsonRuntimeError(const std::string & msg)
        {
            ss << "JsonRuntimeError placeholder: " << msg;
            why_ = ss.str();
        }
    };

    //! \brief catch all from the JSON code
    //!
    //! JSON support is planned but not implemented
    struct JsonUnknownError : FusionExceptionBase
    {
        //! \brief ...
        explicit JsonUnknownError(const std::string & msg)
        {
            ss << "JsonUnknownError placeholder: " << msg;
            why_ = ss.str();
        }
    };

    //! \brief field extractor unknown field name
    //!
    //! FIXME: should add a field enum to string func,
    //! convert int to FieldName string
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
    //!
    //! FIXME: should add a field enum to string func,
    //! convert int to SFieldName string
    struct FieldExtUnknownSpecialField : FusionExceptionBase
    {
        //! \brief ...
        explicit FieldExtUnknownSpecialField(uint32_t sfn,
                                             std::string dasm)
        {
            ss << "Unknown special field: " << sfn << " in " << dasm;
            why_ = ss.str();
        }
    };

} // namespace fusion
