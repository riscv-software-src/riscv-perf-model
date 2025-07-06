// HEADER PLACEHOLDER
// contact Jeff Nye, jeffnye-gh
//
//! \file FusionExceptions.h  fusion defined exceptions
#pragma once
#include "fsl_api/FusionTypes.h"

#include <exception>
#include <iomanip>
#include <sstream>
#include <string>

//! \class ContextDuplicateError
//! \class FieldExtUnknownField
//! \class FieldExtUnknownSpecialField
//! \class FileIoError
//! \class FslRuntimeError
//! \class FslSyntaxError
//! \class FusionExceptionBase
//! \class FusionExceptionBase
//! \class FusionInitializationError
//! \class HashDuplicateError
//! \class HashIllegalValueError
//! \class JsonRuntimeError
//! \class JsonSyntaxError
//! \class JsonUnknownError

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

    //! \brief report any issues during the Fusion context
    //!
    //! Fusion is initialized by a call from the Decoder ctor to
    //! an initialization function. This handles default or
    //! non-specific initialization failures.
    struct FusionInitializationError : FusionExceptionBase
    {
        //! \brief ...
        explicit FusionInitializationError(const std::string & cause)
        {
            ss << "Fusion intialization failed: " << cause;
            why_ = ss.str();
        }
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
    struct FslSyntaxError : FusionExceptionBase
    {
        //! \brief ...
        explicit FslSyntaxError(const std::string & msg,
                                const int & lineno)
        {
            ss << "DSL syntax error '" << msg << "'" << std::endl
               << "    at line: " << lineno;
            why_ = ss.str();
        }
    };

    //! \brief for errors discovered when using dsl values
    struct FslRuntimeError : FusionExceptionBase
    {
        //! \brief ...
        explicit FslRuntimeError(const std::string & msg)
        {
            ss << "FslRuntimeError placeholder: " << msg;
            why_ = ss.str();
        }
    };

    //! \brief catch all from the DSL code
    struct FslUnknownError : FusionExceptionBase
    {
        //! \brief ...
        explicit FslUnknownError(const std::string & msg)
        {
            ss << "FslUnknownError placeholder: " << msg;
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
        //! \brief ...
        explicit JsonSyntaxError()
        {
            ss << "JSON syntax error reported by nlohmann";
            why_ = ss.str();
        }
    };

    //! \brief for errors discovered when using json values
    struct JsonRuntimeError : FusionExceptionBase
    {
        //! \brief ...
        explicit JsonRuntimeError(const std::string & msg)
        {
            ss << "JsonRuntimeError: " << msg;
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
            ss << "JsonUnknownError: " << msg;
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

    //! \brief file access exception
    struct FileIoError : FusionExceptionBase
    {
        //! \brief ...
        explicit FileIoError(std::string action,
                             std::string fileName)
        {
            ss << "File access error on '" << action 
               << "' for file " << fileName;
            why_ = ss.str();
        }
    };

} // namespace fusion
