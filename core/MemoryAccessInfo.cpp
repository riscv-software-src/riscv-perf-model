#include "MemoryAccessInfo.hpp"

#include "Inst.hpp"

namespace olympia
{
    MemoryAccessInfo::MemoryAccessInfo(const InstPtr & inst_ptr) :
        ldst_inst_ptr_(inst_ptr),
        phy_addr_ready_(false),
        mmu_access_state_(MMUState::NO_ACCESS),
        cache_access_state_(CacheState::NO_ACCESS),
        cache_data_ready_(false),
        is_refill_(false),
        src_(ArchUnit::NO_ACCESS),
        dest_(ArchUnit::NO_ACCESS),
        vaddr_(inst_ptr->getTargetVAddr()),
        paddr_(inst_ptr->getRAdr())
    {}

    MemoryAccessInfo::~MemoryAccessInfo() = default;

    std::string MemoryAccessInfo::getMnemonic() const
    {
        return ldst_inst_ptr_ != nullptr ? ldst_inst_ptr_->getMnemonic() : "<unassoc>";
    }

    uint64_t MemoryAccessInfo::getInstUniqueID() const
    {
        return ldst_inst_ptr_ != nullptr ? ldst_inst_ptr_->getUniqueID() : 0;
    }

    std::ostream & operator<<(std::ostream & os, const MemoryAccessInfo & mem)
    {
        os << "memptr: " << std::hex << mem.getPhyAddr() << std::dec;
        if (mem.getInstPtr() != nullptr)
        {
            os << " " << mem.getInstPtr();
        }
        return os;
    }

    std::ostream & operator<<(std::ostream & os, const MemoryAccessInfoPtr & mem_ptr)
    {
        os << *mem_ptr;
        return os;
    }
} // namespace olympia
