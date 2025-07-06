// HEADER PLACEHOLDER
// contact Jeff Nye, jeffnye-gh
//
//! \file FusionGroup.h  holds fusion definitions and transforms
#pragma once
#include "fsl_api/FusionTypes.h"
#include "fsl_api/Instruction.h"
#include "mavis/Mavis.h"

#include <cstdint>
#include <iomanip>
#include <iostream>
#include <memory>
#include <optional>
#include <ostream>
#include <vector>

//! \class FusionGroupCfg

namespace fusion
{
// ----------------------------------------------------------------------
// ----------------------------------------------------------------------
//! \brief forward decl
template <typename MachineInfoType, typename FieldExtractorType>
class FusionGroup;
// ---------------------------------------------------------------
//! \brief FusionGroup ctor helper
//!
//! FusionGroupCfg helps construct FusionGroups from combinations
//! of ctor arguments. There is more work to do here.
//!
//! MachineInfoType provides access to implementation details of the machine
//!
//! FieldExtractor provides an interface to mavis and support functions
//! for boolean operations.
//!
//! (will) support:
//!    UIDs      implemented
//!    opcodes   not implemented, future feature
//!    asm text  not implemented, future feature
// ---------------------------------------------------------------
template <typename MachineInfoType, typename FieldExtractorType>
struct FusionGroupCfg
{
    //! \brief convenient group type
    using FusionGroupType =
        FusionGroup<MachineInfoType, FieldExtractorType>;

    //! \brief transform functor signature
    using TransformFuncType =
        bool (*)(FusionGroup<MachineInfoType, FieldExtractorType> &,
                 InstPtrListType &, InstPtrListType &);

    // Future feature
    // uids can be derived from opcs, but not the other way
    // if conversion from opcs to uids is required so is Mavis
    // std::optional<OpcodeListType>    opcs;
    // std::optional<AsmStmtListType>   asms;
    // fusion::MavisType *mavis{nullptr};

    //! \brief default transform functor
    //!
    //! The default transform makes no changes to machine state, it
    //! provides an argument signature for TransformFuncType.
    static bool default_transform(FusionGroupType &, InstPtrListType & in,
                                  InstPtrListType & out)
    {
        (void) in;
        (void) out;
        return true;
    }
    //! \brief convenient name string
    const std::string name;
    //! \brief list of UIDs representing the group
    std::optional<InstUidListType> uids;
    //! \brief string key look up for transformFunc mapping
    //!
    //! When used the transformName is the lookup key into an
    //! external map containing function objects to perform transforms.
    const std::string transformName;
    //! \brief handle for the transform function
    //!
    //! In previous implementations constraints checking and
    //! transformation were enforced as split operations. This is
    //! no longer required.
    std::optional<TransformFuncType> transformFunc = default_transform;
};
// ---------------------------------------------------------------
//! \brief FusionGroup parent
//!
//! opcs & asm statements are not supported yet, future
// ---------------------------------------------------------------
class FusionGroupBase
{
  public:
    //! \brief save typing
    using UidType = fusion::UidType;
    //! \brief save typing
    using HashType = fusion::HashType;
    //! \brief save typing
    using InstPtrListType = fusion::InstPtrListType;
    //! \brief save typing
    using InstUidListType = fusion::InstUidListType;

    //! \brief base ctor
    FusionGroupBase(std::string n = "",
                    InstUidListType u = InstUidListType(),
                    HashType h = 0) :
        name_(n),
        uids_(u),
        // Future feature
        // opcs_(fusion::OpcodeListType()),
        // asms_(fusion::AsmStmtListType()),
        hash_(h)
    {
    }

    virtual ~FusionGroupBase()
    {
    }

    //! \brief capture the UIDs and create the hash key
    virtual void setUids(InstUidListType & u)
    {
        uids_ = u;
        initHash();
    }

    //! \brief uids accessor
    virtual InstUidListType & uids() { return uids_; }

    // virtual OpcodeListType&  opcs()     { return opcs_; }
    // virtual AsmStmtListType& asms()     { return asms_; }

    //! \brief hash setter
    virtual void setHash(HashType hash) { hash_ = hash; }

    //! \brief refresh the hash from uids_
    virtual void initHash() { hash_ = jenkins_1aat(uids_); }

    //! \brief hash accessor
    virtual HashType hash() const { return hash_; }

    //! \brief convenience function
    virtual void setName(std::string n) { name_ = n; }

    //! \brief name accessor
    virtual std::string name() const { return name_; }

    //! \brief report fgroup state to stream
    friend std::ostream& operator<<(std::ostream& os,
                                    const FusionGroupBase& grp)
    {
        os << "X Name: "   << grp.name();
        os << " Hash: " << std::hex << " " << grp.hash();
        os << " Uids: "; 
        for (auto u : grp.uids_) 
        {
            os << " " << std::hex << std::setw(2) << std::setfill('0') << u;
        }
        return os;
    }

  public:
    //! \brief Method to calculate hash based on UIDs
    //!
    //! In future consider making this a user controlled functor.
    //! In hardware the adds are not desirable.
    static HashType jenkins_1aat(const std::vector<UidType> & v)
    {
        HashType hash = 0;

        for (auto i : v)
        {
            hash += i;
            hash += (hash << 10);
            hash ^= (hash >> 6);
        }

        hash += (hash << 3);
        hash ^= (hash >> 11);
        hash += (hash << 15);

        return hash;
    }

  private:
    //! \brief convenient name string
    std::string name_{""};
    //! \brief the instruction UIDs known to  mavis
    InstUidListType uids_;
    // Future feature
    // OpcodeListType  opcs_;
    // AsmStmtListType asms_;
    //! \brief uint32_t hash key
    HashType hash_{0};
};

// ---------------------------------------------------------------
//! \brief A fusion group is the basis for fusion detection and transformation
//!
//! A fusion group is a list of UIDs that represent data useful for
//! matching a group against incoming Inst::PtrTypes as well as
//! constraints checking.
//!
//! TransformFuncType transform_ is the functor handle. The default
//! is expected to be overridden externally.
// ---------------------------------------------------------------
template <typename MachineInfoType, typename FieldExtractorType>
class FusionGroup : public FusionGroupBase
{
    //! \brief convenient typedef
    using FusionGroupType =
        FusionGroup<MachineInfoType, FieldExtractorType>;

    //! \brief conform to convention used else where
    using MavisType = Mavis<Instruction<uArchInfo>, uArchInfo>;

    //! \brief convenient typedef
    using FusionGroupCfgType =
        FusionGroupCfg<MachineInfoType, FieldExtractorType>;

    //! \brief functor signature and typedef
    using TransformFuncType =
        bool (*)(FusionGroup<MachineInfoType, FieldExtractorType> &,
                 InstPtrListType &, InstPtrListType &);

  public:
    //! \brief conform to convention used elsewhere
    typedef typename std::shared_ptr<
        FusionGroup<MachineInfoType, FieldExtractorType>>
        PtrType;

    // --------------------------------------------------------------------
    //! \brief default ctor
    FusionGroup(std::string n = "", InstUidListType u = InstUidListType(),
                TransformFuncType t = nullptr) :
        FusionGroupBase(n, u),
        transform_(t)
    {
        initHash();
    }

    // --------------------------------------------------------------------
    //! \brief groupcfg ctor
    FusionGroup(const FusionGroupCfgType & cfg) :
        FusionGroupBase(cfg.name,
                        cfg.uids ? *cfg.uids : InstUidListType() //,
                        // cfg.opcs ? *cfg.opcs : OpcodeListType()
                        ),
        transform_(cfg.transformFunc ? *cfg.transformFunc : nullptr)
    {
        if (uids().size() == 0)
        {
            throw std::invalid_argument(
                "For " + cfg.name
                + " uids are required in this implementation");
        }
        initHash();
    }

    // Future support
    // void opc2uid(const OpcodeListType &opcs,
    //              const fusion::MavisType *mavis,
    //              InstUidListType &uid_vec)
    //{
    // }

    // Future support
    // void asm2uid(const AsmStmtListType &asm,
    //              const fusion::MavisType *mavis,
    //              InstUidListType &uid_vec)
    //{
    // }

    //! \brief transform elements of input to append to output
    //!
    //! transform() is called when a fusiongroup is selected by
    //! Fusion (fusion.h). The return value returns true if
    //! the transform function met the criterion. False if not.
    //! On false Fusion continues to search the context.
    //!
    //! The transform operation is expected to modify in if fusion
    //! occurs and also to append to out with the transformation
    //! results. All combinations of true/false, modifying/not modifying
    //! input and output are valid.
    bool transform(InstPtrListType & in, InstPtrListType & out)
    {
        if (transform_)
            return transform_(*this, in, out);
        return false;
    }

    //! \brief default transform functor
    //!
    //! The group is not fused, input is appended to out. input is cleared.
    static bool default_transform(FusionGroupType &, InstPtrListType & in,
                                  InstPtrListType & out)
    {
        out.insert(std::end(out), std::begin(in), std::end(in));
        in.clear();
        return true;
    }

    //! \brief user method for changing the default transform functor
    void setTransform(TransformFuncType func) { transform_ = func; }

    //! \brief tranform handle accessor
    TransformFuncType getTransform() { return transform_; }

    //! \brief machine info handle accessor
    MachineInfoType & mi() { return mi_; }

    //! \brief machine info handle accessor
    MachineInfoType & machineInfo() { return mi(); }

    //! \brief field extractor handle accessor
    FieldExtractorType & fe() { return fe_; }

    //! \brief field extractor handle accessor
    FieldExtractorType & fieldExtractor() { return fe(); }

  private:
    //! \brief MachineInfo provides access to uarch details
    MachineInfoType mi_;
    //! \brief FieldExtractor provides field access methods
    FieldExtractorType fe_;
    //! \brief handle to transform functor
    TransformFuncType transform_ = default_transform;
};

} // namespace fusion
