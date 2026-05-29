
#pragma once

#include <iostream>
#include <cinttypes>
#include "sparta/pairs/SpartaKeyPairs.hpp"
#include "sparta/simulation/State.hpp"
#include "sparta/utils/SpartaSharedPointer.hpp"
#include "sparta/utils/ValidValue.hpp"

#include "Inst.hpp"

namespace olympia {

class MemoryAccessInfo;
using MemoryAccessInfoPtr = sparta::SpartaSharedPointer<MemoryAccessInfo>;

// Forward declaration of the Pair Definition class is must as we are friending it.
class MemoryAccessInfoPairDef;

// Keep record of memory access information in LSU
class MemoryAccessInfo {
public:

    // The modeler needs to alias a type called "SpartaPairDefinitionType" to the Pair Definition class of itself
    using SpartaPairDefinitionType = MemoryAccessInfoPairDef;

    enum class UnitName : std::uint32_t {
        NO_ACCESS = 0,
        __FIRST = NO_ACCESS,
        DCACHE,
        ICACHE,
        LSU,
        L2CACHE,
        L3CACHE,
        L2TLB,
        BIU,
        NUM_UNITS,
        __LAST = NUM_UNITS
    };

    enum class RequestType : std::uint32_t {
        NO_ACCESS = 0,
        __FIRST = NO_ACCESS,
        READ,
        WRITE,
        PREFETCH,
        NUM_STATES,
        __LAST = NUM_STATES
    };

    MemoryAccessInfo()
    {}

    MemoryAccessInfo(const uint64_t& paddr) :
        paddr_(paddr)
    {}

    MemoryAccessInfo(const uint64_t& vaddr,
                     const uint64_t& paddr) :
        MemoryAccessInfo(paddr)
    {
        vaddr_ = vaddr;
    }

    MemoryAccessInfo(const uint64_t& vaddr,
                     const uint64_t& paddr,
                     const InstPtr& inst_ptr) :
        MemoryAccessInfo(vaddr, paddr)
    {
        inst_ptr_ = inst_ptr;
    }

    MemoryAccessInfo(const InstPtr& inst_ptr) :
        MemoryAccessInfo(inst_ptr->getTargetVAddr(), inst_ptr->getTargetPAddr(), inst_ptr)
    {
        inst_ptr_ = inst_ptr;
    }

    virtual ~MemoryAccessInfo() {}

    void setInstPtr(const InstPtr& inst_ptr) { inst_ptr_ = inst_ptr; }
    const InstPtr& getInstPtr() const { return inst_ptr_; }
    uint64_t getInstUniqueID() const { return inst_ptr_ ? inst_ptr_->getUniqueID() : 0; }

    void setVAddr(sparta::memory::addr_t vAddr) { vaddr_ = vAddr; }
    sparta::memory::addr_t getVAddr() const { return vaddr_; }

    void setPAddr(sparta::memory::addr_t pAddr) { paddr_ = pAddr; }
    sparta::memory::addr_t getPAddr() const { return paddr_; }

    void setModified(bool modified) { modified_ = modified; }
    bool getModified() const { return modified_; }

    void setSrcUnit(UnitName src_unit) { src_ = src_unit; }
    const UnitName & getSrcUnit() const { return src_; }

    void setDestUnit(UnitName dest_unit) { dest_ = dest_unit; }
    const UnitName & getDestUnit() const { return dest_; }

    void setOwnerUnit(UnitName owner_unit) { owner_ = owner_unit; }
    const UnitName & getOwnerUnit() const { return owner_; }

    void setReqType(RequestType req_type) { req_type_ = req_type; }
    const RequestType & getReqType() const { return req_type_; }

    void setDataReady(bool data_ready) { is_data_ready_ = data_ready; }
    const bool & isDataReady() const { return is_data_ready_; }

private:

    // load/store instruction pointer
    InstPtr inst_ptr_;

    // Target virtual address
    uint64_t vaddr_ = 0;

    // Target physical address
    uint64_t paddr_ = 0;

    // Is the data modified
    bool modified_ = false;

    // Request Type READ or WRITE
    RequestType req_type_ = RequestType::NO_ACCESS;

    bool is_data_ready_ = false;

    // src/destination for multiple master/slaves
    UnitName src_ = UnitName::NO_ACCESS;
    UnitName dest_ = UnitName::NO_ACCESS;

    // Owner of the MemAccessInfo creation, mostly for debug purposes currently.
    UnitName owner_ = UnitName::NO_ACCESS;
};  // class MemoryAccessInfo

/*!
 * \class MemoryAccessInfoPairDef
 * \brief Pair Definition class of the Memory Access Information that flows through the example/CoreModel
 */

// This is the definition of the PairDefinition class of MemoryAccessInfo.
// This PairDefinition class could be named anything but it needs to inherit
// publicly from sparta::PairDefinition templatized on the actual class MemoryAcccessInfo.
class MemoryAccessInfoPairDef : public sparta::PairDefinition<MemoryAccessInfo>{
public:

    // The SPARTA_ADDPAIRs APIs must be called during the construction of the PairDefinition class
    MemoryAccessInfoPairDef() : PairDefinition<MemoryAccessInfo>(){
        SPARTA_INVOKE_PAIRS(MemoryAccessInfo);
    }
    SPARTA_REGISTER_PAIRS(SPARTA_ADDPAIR("DID",   &MemoryAccessInfo::getInstUniqueID),
                          SPARTA_ADDPAIR("uid",   &MemoryAccessInfo::getInstUniqueID),
                          SPARTA_ADDPAIR("va",    &MemoryAccessInfo::getVAddr, std::ios::hex),
                          SPARTA_ADDPAIR("pa",    &MemoryAccessInfo::getPAddr, std::ios::hex))
    //                    SPARTA_FLATTEN(         &MemoryAccessInfo::getInstPtr))
};

inline std::ostream & operator<<(std::ostream& os,
                                 const MemoryAccessInfo::RequestType& req) {
    switch(req) {
    case MemoryAccessInfo::RequestType::NO_ACCESS:
        os << "NO_ACCESS";
        break;
    case MemoryAccessInfo::RequestType::READ:
        os << "READ";
        break;
    case MemoryAccessInfo::RequestType::WRITE:
        os << "WRITE";
        break;
    case MemoryAccessInfo::RequestType::PREFETCH:
        os << "PREFETCH";
        break;
    case MemoryAccessInfo::RequestType::NUM_STATES:
        os << "NUM_STATES";
        break;
    }
    return os;
}

inline std::ostream & operator<<(std::ostream& os,
                                 const MemoryAccessInfo::UnitName& unit) {
    switch(unit) {
    case MemoryAccessInfo::UnitName::NO_ACCESS:
        os << "NO_ACCESS";
        break;
    case MemoryAccessInfo::UnitName::DCACHE:
        os << "DCACHE";
        break;
    case MemoryAccessInfo::UnitName::ICACHE:
        os << "ICACHE";
        break;
    case MemoryAccessInfo::UnitName::LSU:
        os << "LSU";
        break;
    case MemoryAccessInfo::UnitName::L2TLB:
        os << "L2TLB";
        break;
    case MemoryAccessInfo::UnitName::L2CACHE:
        os << "L2CACHE";
        break;
    case MemoryAccessInfo::UnitName::L3CACHE:
        os << "L3CACHE";
        break;
    case MemoryAccessInfo::UnitName::BIU:
        os << "BIU";
        break;
    case MemoryAccessInfo::UnitName::NUM_UNITS:
        os << "NUM_UNITS";
        break;
    }
    return os;
}

inline std::ostream & operator<<(std::ostream& os,
                                 const MemoryAccessInfo& ma)
{
    os << " inst: "        << ma.getInstPtr()
       << " va: "          << HEX16(ma.getVAddr())
       << " pa: "          << HEX16(ma.getPAddr())
       << " src: "         << ma.getSrcUnit()
       << " dest: "        << ma.getDestUnit()
       << " req_type: "    << ma.getReqType()
       << " data_ready: "  << ma.isDataReady()
       << " modified: "    << ma.getModified();
    return os;
}

inline std::ostream & operator<<(std::ostream& os,
                                 const MemoryAccessInfoPtr& ma_ptr)
{
    if (ma_ptr) {
        os << *ma_ptr;
    } else {
        os << "nullptr";
    }
    return os;
}

// MemoryAccessInfo allocators
using MemoryAccessInfoAllocator = sparta::SpartaSharedPointerAllocator<MemoryAccessInfo>;
extern MemoryAccessInfoAllocator memory_access_info_allocator;

} // namespace olympia
