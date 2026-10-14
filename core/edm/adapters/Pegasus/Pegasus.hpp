#pragma once

#include "edm/EDMInterface.hpp"
#include "edm/EDMTypes.hpp"
#include <memory>
#include <map>

namespace pegasus::cosim
{
    class PegasusCoSim;
    class EventAccessor;
} // namespace pegasus::cosim

namespace olympia::edm
{
    class PegasusAdapter : public EDMInterface
    {
      public:
        PegasusAdapter(const std::string & config_file, const std::string & filename);

        ~PegasusAdapter();

        bool isFinished(CoreId core_id, HartId hart_id) const override;

        Addr peekNextPc(CoreId core_id, HartId hart_id) const override;

        InstructionInfo step(CoreId core_id, HartId hart_id) override;

        InstructionInfo stepWithOverridePc(CoreId core_id, HartId hart_id,
                                           Addr override_pc) override;

        void commitInstruction(CoreId core_id, HartId hart_id, uint64_t iss_uid) override;

        void commitStoreWrite(CoreId core_id, HartId hart_id, uint64_t iss_uid) override;

        void dropStoreWrite(CoreId core_id, HartId hart_id, uint64_t iss_uid) override;

        void flush(CoreId core_id, HartId hart_id, const EDMCheckpoint & checkpoint) override;

      private:
        std::unique_ptr<pegasus::cosim::PegasusCoSim> cosim_;
        std::map<uint64_t, pegasus::cosim::EventAccessor> pending_events_;
    };
} // namespace olympia::edm
