//
// Created by David Murrell on 11/7/19.
//
// JN: original file name was Inst.h

#ifndef DTABLEPROTO03_INST_H
#define DTABLEPROTO03_INST_H

#include <string>
#include <memory>
#include "mavis/OpcodeInfo.h"
#include "mavis/Extractor.h"

/**
 * EXAMPLE Instruction
 */
template <typename AnnotationType> class Instruction {
public:
  typedef typename std::shared_ptr<Instruction<AnnotationType>> PtrType;

public:
  // Instruction(const mavis::DecodedInstructionInfo::PtrType& dii, const
  // uint64_t icode, const mavis::ExtractorIF::PtrType& extractor, const
  // typename AnnotationType::PtrType& ui) :
  Instruction(const mavis::OpcodeInfo::PtrType &dinfo,
              const typename AnnotationType::PtrType &ui, uint32_t dummy)
      : dinfo_(dinfo), uinfo_(ui) {}

  // Copy construction (needed for cloning)
  Instruction(const Instruction<AnnotationType> &) = default;

  // Morph into a different instruction (new mavis info)
  void morph(const typename mavis::OpcodeInfo::PtrType &new_dinfo,
             const typename AnnotationType::PtrType &new_ui) {
    // dinfo_.reset(new_dinfo);
    dinfo_ = new_dinfo;
    // uinfo_.reset(new_ui);
    uinfo_ = new_ui;

    // TBD: Instruction user may need to reset any information cached with this
    // instruction
  }

  /**
   * User code to "recycle" an instruction (which DTable has cached and is
   * attempting to reuse)
   */
  void recycle() {}

  typename mavis::OpcodeInfo::PtrType getOpInfo() const { return dinfo_; }

  std::string getMnemonic() const { return dinfo_->getMnemonic(); }

  std::string dasmString() const { return dinfo_->dasmString(); }

  bool isInstType(mavis::InstMetaData::InstructionTypes itype) const {
    return dinfo_->isInstType(itype);
  }

  bool
  isExtInstType(mavis::DecodedInstructionInfo::ExtractedInstTypes itype) const {
    return dinfo_->isExtractedInstType(itype);
  }

  int64_t getSignedOffset() const { return dinfo_->getSignedOffset(); }

  mavis::DecodedInstructionInfo::BitMask getSourceAddressRegs() const {
    return dinfo_->getSourceAddressRegs();
  }

  mavis::DecodedInstructionInfo::BitMask getSourceDataRegs() const {
    return dinfo_->getSourceDataRegs();
  }

  mavis::DecodedInstructionInfo::BitMask getIntSourceRegs() const {
    return dinfo_->getIntSourceRegs();
  }

  mavis::DecodedInstructionInfo::BitMask getFloatSourceRegs() const {
    return dinfo_->getFloatSourceRegs();
  }

  mavis::DecodedInstructionInfo::BitMask getVectorSourceRegs() const {
    return dinfo_->getVectorSourceRegs();
  }

  mavis::DecodedInstructionInfo::BitMask getIntDestRegs() const {
    return dinfo_->getIntDestRegs();
  }

  mavis::DecodedInstructionInfo::BitMask getFloatDestRegs() const {
    return dinfo_->getFloatDestRegs();
  }

  mavis::DecodedInstructionInfo::BitMask getVectorDestRegs() const {
    return dinfo_->getVectorDestRegs();
  }

  uint64_t getSpecialField(mavis::OpcodeInfo::SpecialField sfid) const {
    return dinfo_->getSpecialField(sfid);
  }

  const mavis::OperandInfo &getSourceOpInfo() const {
    return dinfo_->getSourceOpInfo();
  }

  const mavis::OperandInfo &getDestOpInfo() const {
    return dinfo_->getDestOpInfo();
  }

  mavis::InstructionUniqueID getUID() const {
    return dinfo_->getInstructionUniqueID();
  }

  bool hasImmediate() const { return dinfo_->hasImmediate(); }

  const typename AnnotationType::PtrType &getuArchInfo() const {
    return uinfo_;
  }

//private:
  typename mavis::OpcodeInfo::PtrType dinfo_;
  typename AnnotationType::PtrType uinfo_;

  void print(std::ostream &os) const {
    std::ios_base::fmtflags os_state(os.flags());
    os << dinfo_->getMnemonic() << ", src[" << dinfo_->numSourceRegs()
       << "]: " << dinfo_->getSourceRegs()
       << " (addr: " << dinfo_->getSourceAddressRegs() << ")"
       << ", dst: " << dinfo_->getDestRegs() << ", data size: " << std::dec
       << dinfo_->getDataSize();
    if (uinfo_ != nullptr) {
      os << ", uInfo: " << *uinfo_;
    }
    os.flags(os_state);
  }

public:
  friend std::ostream &operator<<(std::ostream &os,
                                  const Instruction<AnnotationType> &i) {
    i.print(os);
    return os;
  }
};

#endif // DTABLEPROTO03_INST_H
