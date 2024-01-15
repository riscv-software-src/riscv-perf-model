// HEADER PLACEHOLDER
// contact Jeff Nye, jeffnye-gh
//
//! \file fieldextractor.h  mavis shim, extract fields from encodings
#pragma once
#include "fusionexceptions.h"
#include "uArchInfo.h"
#include "Inst.h"
#include "Mavis.h"
#include <vector>
#include <optional>
#include <ostream>
#include <iostream>
using namespace std;

//! \class FieldExtractor
//!
//! \brief example class for extracting field values from mavis encodings
//!
//! This implementation provides a shim between operations typically
//! used during fusion contraint checking and transformation and mavis.
//!
//! The intent is any alternative to FieldExtractor could be created
//! if compliant with the interface. This has not been tested.
//!
//! It would also be interesting to determine if more of this could be
//! delegated to existing methods in mavis.
class FieldExtractor
{
  public:
    //! \brief Part of mechanism to extract named fields from encodings
    //!
    //! OperandFieldID has no entry for immediates
    //! \note I am overloading RS_MAX when referencing immediate fields
    using FieldName = mavis::InstMetaData::OperandFieldID;
    //! \brief alias used to extract Special fields by enum
    using SFieldName = mavis::ExtractorIF::SpecialField;

    //! \brief ...
    using InstPtrType = Instruction<uArchInfo>::PtrType;
    //! \brief ...
    using InstPtrListType = std::vector<Instruction<uArchInfo>::PtrType>;
    //! \brief type for getXRegs operations from mavis
    using MavisBitMaskType = mavis::DecodedInstructionInfo::BitMask;
    //! \brief type for getXRegs operations from mavis
    using RegsGetter =
        MavisBitMaskType (Instruction<uArchInfo>::*)() const;
    //! \brief duplicated arguments do not need to be explicitly expressed
    using OptArg = std::optional<FieldName>;

    //!  \brief compairson function primitives
    enum FUNC
    {
        EQ,
        LT
    };

    //!  \brief placeholder
    void info(std::ostream & os = std::cout)
    {
        os << "FieldExtractor info()" << std::endl;
    }

    //!  \brief extract value of named encoding field
    //!
    //! handles field name and immediate checking
    //! \note RS_MAX is overloaded to identify get's for immediate fields
    uint32_t getField(InstPtrType inst, FieldName f) const
    {
        bool isDest = false;
        if (!checkInstHasField(inst, f, isDest))
        {
            throw fusion::FieldExtUnknownField((uint32_t)f,
                                               inst->dasmString());
        }

        if (f == FieldName::RS_MAX)
            return getImmField(inst);

        return getFieldById(inst, f, isDest);
    }

    //! \brief get the encoded value of a named special field from an
    //! instptr
    //!
    //! This is simpler than getField, a return value of 0 from mavis
    //! indicates the field does not exist so checkInstHasSField()
    //! is redundant. getSFieldById() is also not necessary since
    //! SpecialFields are not distinguished as a source or destination
    //! operand and can be accessed from the same method.
    uint32_t getSField(InstPtrType inst, SFieldName f) const
    {
        uint32_t value = inst->getSpecialField(f);
        if (value == 0)
        {
            throw fusion::FieldExtUnknownSpecialField((uint32_t)f,
                                                      inst->dasmString());
        }
        return value;
        //        return getSFieldById(inst, f);
    }

    //! \brief get the encoded value of the full immediate field
    //!
    //! split immediate fields are (will be) ordered msb:lsb and
    //! concatenated into one unsigned value
    uint32_t getImmField(InstPtrType inst) const { return 0; }

    //!  \brief helper function for getField, src/dst switch
    //!
    //! isDest is set previously by checkInstHasField()
    uint32_t getFieldById(InstPtrType inst, FieldName f, bool isDest) const
    {
        if (isDest)
        {
            return inst->getDestOpInfo().getFieldValue(f);
        }
        return inst->getSourceOpInfo().getFieldValue(f);
    }

    //    //!  \brief helper function for getSField
    //    //!
    //    //! not that interesting except checkInstHasSField was called if
    //    this is
    //    //! not called directly
    //    uint32_t getSFieldById(InstPtrType inst, SFieldName sf) const
    //    {
    //        return inst->getSpecialField(sf);
    //    }

    //! \brief Use mavis to determine if the FieldName exists in this inst
    //! obj
    //!
    //! Special case check for immediates;
    //! srcOp and dstOps have to be checked separately.
    bool checkInstHasField(InstPtrType inst, FieldName f,
                           bool & isDest) const
    {
        if (inst->hasImmediate() && f == FieldName::RS_MAX)
        {
            return true;
        }

        if (inst->getSourceOpInfo().hasFieldID(f)
            && f != FieldName::RS_MAX)
        {
            return true;
        }

        // FIXME: I do not see the reason but this if statement will fail
        // with
        //             terminate called after throwing an instance of
        //                          'mavis::OperandInfoInvalidFieldID'
        //                what():  OperandInfo field ID '' is invalid
        //
        //   if(inst->getDestOpInfo().hasFieldID(f) && f !=
        //   FieldName::RS_MAX)
        //   {
        //     isDest = true;
        //     return true;
        //   }
        //
        //  But doing this is fine...
        bool idok = inst->getDestOpInfo().hasFieldID(f);
        bool fok = f != FieldName::RS_MAX;
        if (idok && fok)
        {
            isDest = true;
            return true;
        }

        // catch fall through
        throw fusion::FieldExtUnknownField((uint32_t)f,
                                           inst->dasmString());
        return false; ///...
    }

    //    //! \brief Use mavis to determine if the SFieldName exists in
    //    this inst obj
    //    //!
    //    //!
    //    bool checkInstHasSField(InstPtrType inst, SFieldName f) const
    //    {
    //        return false; ///...
    //    }

    //! \brief equality
    bool eq(const InstPtrListType & input, size_t a, size_t b,
            const FieldName f1, const OptArg f2 = std::nullopt) const
    {
        return compare(input[a], input[b], f1, f2, EQ);
    }

    //! \brief less than
    bool lt(const InstPtrListType & input, size_t a, size_t b,
            const FieldName f1, const OptArg f2 = std::nullopt) const
    {
        return compare(input[a], input[b], f1, f2, LT);
    }

    //! \brief not equal
    bool noteq(const InstPtrListType & input, size_t a, size_t b,
               const FieldName f1, const OptArg f2 = std::nullopt) const
    {
        return !compare(input[a], input[b], f1, f2, EQ);
    }

    //! \brief greater than
    bool gt(const InstPtrListType & input, size_t a, size_t b,
            const FieldName f1, const OptArg f2 = std::nullopt) const
    {
        return compare(input[b], input[a], f1, f2, LT);
    }

    //! \brief less than or equal
    bool lteq(const InstPtrListType & input, size_t a, size_t b,
              const FieldName f1, const OptArg f2 = std::nullopt) const
    {
        return !compare(input[b], input[a], f1, f2, LT);
    }

    //! \brief greater than or equal
    bool gteq(const InstPtrListType & input, size_t a, size_t b,
              const FieldName f1, const OptArg f2 = std::nullopt) const
    {
        return !compare(input[a], input[b], f1, f2, LT);
    }

    //! \brief return the integer read ports used by the input fusion group
    uint32_t getIntRdPorts(const InstPtrListType & input) const
    {
        return countPorts(input,
                          &Instruction<uArchInfo>::getIntSourceRegs);
    }

    //! \brief return the integer write ports used by the input fusion
    //! group
    uint32_t getIntWrPorts(const InstPtrListType & input) const
    {
        return countPorts(input, &Instruction<uArchInfo>::getIntDestRegs);
    }

    //! \brief return the float read ports used by the input fusion group
    uint32_t getFloatRdPorts(const InstPtrListType & input) const
    {
        return countPorts(input,
                          &Instruction<uArchInfo>::getFloatSourceRegs);
    }

    //! \brief return the float write ports used by the input fusion group
    uint32_t getFloatWrPorts(const InstPtrListType & input) const
    {
        return countPorts(input,
                          &Instruction<uArchInfo>::getFloatDestRegs);
    }

    //! \brief return the vector read ports used by the input fusion group
    uint32_t getVecRdPorts(const InstPtrListType & input) const
    {
        return countPorts(input,
                          &Instruction<uArchInfo>::getVectorSourceRegs);
    }

    //! \brief return the vector write ports used by the input fusion group
    uint32_t getVecWrPorts(const InstPtrListType & input) const
    {
        return countPorts(input,
                          &Instruction<uArchInfo>::getVectorDestRegs);
    }

  private:
    //! \brief compare common method
    bool compare(const InstPtrType A, const InstPtrType B,
                 const FieldName f1, const OptArg _f2, FUNC func) const
    {
        FieldName f2 = _f2.value_or(f1);
        auto lhs = getField(A, f1);
        auto rhs = getField(B, f2);
        switch (func)
        {
        case FUNC::LT:
            return lhs < rhs;
        case FUNC::EQ:
            return lhs == rhs;
        default:
            return false;
        }
    }

    //! \brief count the number of read or write ports required by the
    //! group
    uint32_t countPorts(const InstPtrListType & input,
                        RegsGetter getRegs) const
    {
        mavis::DecodedInstructionInfo::BitMask mask;
        for (const auto & i : input)
        {
            mask |= (i.get()->*getRegs)();
        }
        return mask.count();
    }
};
