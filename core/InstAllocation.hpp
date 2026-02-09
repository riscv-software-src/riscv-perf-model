// <InstAllocation.hpp> -*- C++ -*-

//
// \file InstAllocation.hpp
// \brief Instruction and Instruction micro allocation classes
//
// For speed, these classes help allocate instruction and instruction
// microarchitecture classes.
//

#pragma once

#include <iostream>

#include "Inst.hpp"

#include "sparta/utils/SpartaSharedPointer.hpp"
#include "sparta/utils/SpartaSharedPointerAllocator.hpp"

namespace olympia
{
    // Required by mavis to allocate instructions. This is a wrapper
    // class or a delegate class that will convert Mavis allocations
    // requests to sparta shared pointer allocations
    template<typename InstAllocatorT>
    class InstPtrAllocator
    {
    public:
        ~InstPtrAllocator() {
            // For debug
            std::cout << "Inst Allocator: "
                      << inst_allocator_.getNumAllocated()
                      << " Inst objects allocated/created"
                      << std::endl;

        }

        explicit InstPtrAllocator(InstAllocatorT & inst_allocator) :
            inst_allocator_(inst_allocator)
        {}

        // Used by Mavis for the type allocated
        using InstTypePtr = sparta::SpartaSharedPointer<typename InstAllocatorT::element_type>;

        // Called by Mavis when creating a new instruction
        template<typename ...Args>
        InstTypePtr operator()(Args&&...args) {
            return sparta::allocate_sparta_shared_pointer
                <typename InstAllocatorT::element_type>(inst_allocator_,
                                                        std::forward<Args>(args)...);
        }
    private:
        InstAllocatorT & inst_allocator_;
    };

}
