#pragma once

#include "EDMTypes.hpp"

namespace olympia::edm
{
    class EDMInterface
    {
      public:
        virtual ~EDMInterface() = default;

        virtual bool isFinished(CoreId core_id, HartId hart_id) const = 0;

        virtual Addr peekNextPc(CoreId core_id, HartId hart_id) const = 0;

        virtual InstructionInfo step(CoreId core_id, HartId hart_id) = 0;

        virtual InstructionInfo stepWithOverridePc(CoreId core_id, HartId hart_id,
                                                   Addr override_pc) = 0;

        virtual void commitInstruction(CoreId core_id, HartId hart_id, uint64_t iss_uid) = 0;

        virtual void commitStoreWrite(CoreId core_id, HartId hart_id, uint64_t iss_uid) = 0;

        virtual void dropStoreWrite(CoreId core_id, HartId hart_id, uint64_t iss_uid) = 0;

        virtual void flush(CoreId core_id, HartId hart_id, const EDMCheckpoint & checkpoint) = 0;
    };

} // namespace olympia::edm
