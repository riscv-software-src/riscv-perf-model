#pragma once


#include "edm/EDMInterface.hpp"
#include "edm/EDMTypes.hpp"
#include "edm/EDMPluginABI.h"

#include "whisper/PerfApi.hpp"

#include <memory>
#include <map>


namespace olympia::edm
{
    template <typename URV>
    class WhisperAdapter : public EDMInterface
    {
    public:
        WhisperAdapter(const std::string& config_file, const std::string & filename);

        ~WhisperAdapter() override;

        bool isFinished(CoreId core_id, HartId hart_id) const override;

        Addr peekNextPc(CoreId core_id, HartId hart_id) const override;

        InstructionInfo step(CoreId core_id, HartId hart_id) override;

        InstructionInfo stepWithOverridePc(CoreId core_id, HartId hart_id, Addr override_pc) override;

        void commitInstruction(CoreId core_id, HartId hart_id, uint64_t iss_uid) override;

        void commitStoreWrite(CoreId core_id, HartId hart_id, uint64_t iss_uid) override;

        void dropStoreWrite(CoreId core_id, HartId hart_id, uint64_t iss_uid) override;

        void flush(CoreId core_id, HartId hart_id, const EDMCheckpoint & checkpoint) override;

    private:
        InstructionInfo doStep_(unsigned hartIx, Addr vpc, bool is_wrong_path);

        unsigned hartIx_(CoreId core_id, HartId hart_id) const;

        std::unique_ptr<WdRiscv::System<URV>> system_;
        std::shared_ptr<TT_PERF::PerfApi<URV>> perfApi_;

        uint64_t time_ = 0;
        std::map<unsigned, uint64_t> nextTag_;
        std::map<unsigned, uint64_t> nextPc_;
    };

    EDMInterface* createWhisperAdapter(const std::string & config_file, const std::string & filename);

    extern template class WhisperAdapter<uint32_t>;
    extern template class WhisperAdapter<uint64_t>;
}