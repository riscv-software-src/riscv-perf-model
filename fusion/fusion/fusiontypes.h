// HEADER PLACEHOLDER
// contact Jeff Nye, jeffnye-gh
//
//! \file fusiontypes.h marshalled types used by fusion
#pragma once
#include "Mavis.h"
#include "mavis/DecoderTypes.h"
#include "Inst.h"
#include "uArchInfo.h"
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
using Opcode = mavis::Opcode;
//! \brief ...
using OpcodeListType = std::vector<mavis::Opcode>;

//! \brief ...
using AsmStmtListType = std::vector<std::string>;

//! \brief ...
using InstPtrListType = std::vector<Instruction<uArchInfo>::PtrType>;
//! \brief ...
using HashType = uint32_t;

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
