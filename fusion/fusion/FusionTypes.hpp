// HEADER PLACEHOLDER
// contact Jeff Nye, jeffnye-gh
//
//! \file FusionTypes.hpp marshalled types used by fusion
#pragma once
#include "fusion/Instruction.hpp"
#include "fusion/uArchInfo.hpp"
#include "mavis/DecoderTypes.h"
#include "mavis/Mavis.h"

#include <cstdint>
#include <string>
#include <vector>

namespace fusion
{
//! \brief ...
using MavisType = Mavis<Instruction<uArchInfo>, uArchInfo>;
//! \brief ...
using FileNameListType = std::vector<std::string>;
//! \brief ...
using UidType = mavis::InstructionUniqueID;
//! \brief ...
using InstUidListType = std::vector<mavis::InstructionUniqueID>;
//! \brief ...
using FieldName  = mavis::InstMetaData::OperandFieldID;
//! \brief ...
using SFieldName = mavis::ExtractorIF::SpecialField;
//! \brief ...
using Opcode = mavis::Opcode;
//! \brief ...
using OpcodeListType = std::vector<mavis::Opcode>;
//! \brief ...
using AsmStmtListType = std::vector<std::string>;
//! \brief ...
using InstPtrType = Instruction<uArchInfo>::PtrType;
//! \brief ...
using InstPtrListType = std::vector<Instruction<uArchInfo>::PtrType>;
//! \brief ...
using HashType = uint32_t;
//! \brief ...
using HashListType = std::vector<HashType>;

//! \brief This is provided but has limited use currently
template <class T> struct ShrPtrAlloc
{
    //! \brief ...
    using Tptr = std::shared_ptr<T>;

    //! \brief ...
    template <typename... Args> Tptr operator()(Args &&... args)
    {
        return std::make_shared<T>(std::forward<Args>(args)...);
    }
};

}
