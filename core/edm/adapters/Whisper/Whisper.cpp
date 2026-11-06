#include <array>
#include <boost/json/basic_parser_impl.hpp>
#include <cstring>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>

#include "Whisper.hpp"
#include "edm/EDMInterface.hpp"
#include "edm/EDMTypes.hpp"
#include "whisper/DecodedInst.hpp"
#include "whisper/Hart.hpp"
#include "whisper/HartConfig.hpp"
#include "whisper/InstEntry.hpp"
#include "whisper/PerfApi.hpp"
#include "whisper/System.hpp"

namespace olympia::edm
{
    // Some helper functions

    void bytesFromScalar(uint64_t v, size_t nbytes, std::vector<uint8_t> & out)
    {
        out.resize(nbytes);
        std::memcpy(out.data(), &v, nbytes);
    }

    void fillOperandValue(const TT_PERF::OpVal & opval, std::vector<uint8_t> & out)
    {
        if (!opval.vec.empty())
        {
            out = opval.vec;
        }
        else
        {
            bytesFromScalar(opval.scalar, sizeof(uint64_t), out);
        }
    }

    template <typename URV>
    void fillInstructionInfo(InstructionInfo & info, TT_PERF::InstrPac & packet,
                             WdRiscv::Hart<URV> & hart)
    {
        info.setIssUid(packet.tag());
        info.setPC(packet.instrVa());
        info.setFaulted(packet.trapped());

        if (!packet.decoded())
        {
            return;
        }

        const auto & di = packet.decodedInst();
        info.setOpcode(di.inst());
        info.setOpcodeSize(di.instSize());
        info.setBranch(di.isBranch());
        info.setLoad(di.isLoad());
        info.setStore(di.isStore());

        std::string dasm;
        hart.disassembleInst(di, dasm);
        info.setDasm(std::move(dasm));

        if (!packet.executed())
        {
            return;
        }

        info.setNextPC(packet.nextPc());

        if (di.isBranch())
        {
            info.setTaken(packet.isTakenBranch());

            const uint64_t fallThrough = packet.instrVa() + di.instSize();
            if (packet.isTakenBranch())
            {
                info.setAltNextPC(fallThrough);
            }
            else
            {
                info.setAltNextPC(packet.nextPc());
            }
        }

        // Source operands
        std::array<TT_PERF::Operand, 3> srcOps{};
        const unsigned srcCount = packet.getSourceOperands(srcOps);
        for (unsigned i = 0; i < srcCount; ++i)
        {
            const auto & op = srcOps[i];
            if (op.type == WdRiscv::OperandType::Imm || op.type == WdRiscv::OperandType::None)
            {
                continue;
            }

            RegAccess ra;
            ra.reg_id = op.number;
            fillOperandValue(op.value, ra.value);
            info.addRegRead(std::move(ra));
        }

        // Destination operands
        std::array<TT_PERF::Operand, 2> dstOps{};
        const unsigned dstCount = packet.getDestOperands(dstOps);
        for (unsigned i = 0; i < dstCount; ++i)
        {
            const auto & op = dstOps[i];
            if (op.type == WdRiscv::OperandType::None)
            {
                continue;
            }

            RegAccess ra;
            ra.reg_id = op.number;
            fillOperandValue(op.value, ra.value);
            fillOperandValue(op.prevValue, ra.prev_value);
            info.addRegWrite(std::move(ra));
        }

        // Getting the memory accesses
        if (packet.dataSize() > 0)
        {
            MemAccess ma;
            ma.vaddr = packet.dataVa();
            ma.paddr = packet.dataPa();
            ma.size = packet.dataSize();

            if (di.isStore())
            {
                ma.is_write = true;
                bytesFromScalar(packet.stData(), ma.size, ma.value);
                info.addMemWrite(std::move(ma));
            }
            else if (di.isLoad())
            {
                ma.is_write = false;
                info.addMemRead(std::move(ma));
            }
        }
    }

    template <typename URV>
    WhisperAdapter<URV>::WhisperAdapter(const std::string & config_file,
                                        const std::string & filename)
    {
        if (config_file.empty())
        {
            throw std::invalid_argument("Whisper EDM : Empty config file");
        }

        WdRiscv::HartConfig config;
        try
        {
            config.loadConfigFile(config_file);
        }
        catch (const std::exception & ex)
        {
            throw std::runtime_error(
                std::string("Failed to load your config file :") + config_file
                + std::string(" to the HartConfig. It failed with the exception: ") + ex.what());
        }

        unsigned xlen = sizeof(URV) * 8;
        config.getXlen(xlen);
        if (xlen != sizeof(URV) * 8)
        {
            throw std::runtime_error(
                "Whisper Backend expects XLEN vlaue : " + std::to_string(xlen)
                + " but this adapter was instantiated for XLEN = " + std::to_string(sizeof(URV) * 8)
                + " . Recommended to check the config file. Did it get changed ?");
        }

        // Baseline values that will be updated later from the config
        unsigned core = 1;
        unsigned hart = 1;
        unsigned hartIdOffset = 1;
        size_t memsize = size_t(1) * 1024 * 1024 * 1024;
        size_t pagesize = 4096;

        config.getCoreCount(core);
        config.getHartsPerCore(hart);
        config.getHartIdOffset(hartIdOffset);
        config.getMemorySize(memsize);
        config.getPageSize(pagesize);

        system_ =
            std::make_unique<WdRiscv::System<URV>>(core, hart, hartIdOffset, memsize, pagesize);

        bool userMode = config.userModeEnabled();
        bool verbose = true;
        bool unmappedElfOk = false;

        if (!config.configMemory(*system_, unmappedElfOk))
        {
            throw std::runtime_error("WhisperAdapter : failed to config the memory");
        }

        if (!config.configHarts(*system_, userMode, verbose))
        {
            throw std::runtime_error("WhisperAdapter : failed to config the harts");
        }

        std::string isa_str;
        config.getIsa(isa_str);
        const bool updateMisa = !config.hasCsrConfig("misa");
        for (unsigned i = 0; i < system_->hartCount(); ++i)
        {
            auto hartPtr = system_->ithHart(i);
            if (!hartPtr)
            {
                continue;
            }

            if (!isa_str.empty())
            {
                if (!hartPtr->configIsa(isa_str, updateMisa))
                {
                    throw std::runtime_error(
                        std::string("WhisperAdapter : failed to apply isa string : " + isa_str));
                }
            }

            hartPtr->reset();
        }

        try
        {
            system_->loadElfFiles({filename}, false, verbose);
        }
        catch (const std::exception & ex)
        {
            throw std::runtime_error(std::string("Failed to load the elf file ") + filename
                                     + std::string(" . The error is : ") + ex.what());
        }

        if (!config.finalizeCsrConfig(*system_))
        {
            throw std::runtime_error("Failed to finalize config");
        }

        std::vector<FILE*> traceFiles;
        try
        {
            system_->enablePerfApi(traceFiles);
        }
        catch (const std::exception & ex)
        {
            throw std::runtime_error(std::string("PerfAPi enable call failed with exception : ")
                                     + ex.what());
        }

        system_->setTohostSymbol("tohost");

        perfApi_ = system_->getPerfApi();
    }

    template <typename URV> WhisperAdapter<URV>::~WhisperAdapter() = default;

    template <typename URV>
    unsigned WhisperAdapter<URV>::hartIx_(CoreId core_id, HartId hart_id) const
    {
        return core_id * system_->hartsPerCore() + hart_id;
    }

    template <typename URV>
    bool WhisperAdapter<URV>::isFinished(CoreId core_id, HartId hart_id) const
    {
        const unsigned ix = hartIx_(core_id, hart_id);
        auto hart = system_->ithHart(ix);
        if (!hart)
        {
            return true;
        }

        return hart->hasTargetProgramFinished();
    }

    template <typename URV>
    Addr WhisperAdapter<URV>::peekNextPc(CoreId core_id, HartId hart_id) const
    {
        const unsigned ix = hartIx_(core_id, hart_id);
        auto it = nextPc_.find(ix);
        if (it != nextPc_.end())
        {
            return it->second;
        }

        auto hart = system_->ithHart(ix);
        if (!hart)
        {
            return 0;
        }

        return static_cast<uint64_t>(hart->peekPc());
    }

    template <typename URV>
    InstructionInfo WhisperAdapter<URV>::step(CoreId core_id, HartId hart_id)
    {
        const unsigned ix = hartIx_(core_id, hart_id);
        const Addr vpc = peekNextPc(core_id, hart_id);
        return doStep_(ix, vpc, false);
    }

    template <typename URV>
    InstructionInfo WhisperAdapter<URV>::stepWithOverridePc(CoreId core_id, HartId hart_id,
                                                            Addr override_pc)
    {
        const unsigned ix = hartIx_(core_id, hart_id);
        return doStep_(ix, override_pc, false);
    }

    template <typename URV>
    InstructionInfo WhisperAdapter<URV>::doStep_(unsigned hartIx, Addr vpc, bool is_wrong_path)
    {
        auto & tagRef = nextTag_[hartIx];
        if (tagRef == 0)
        {
            tagRef = 1;
        }

        const uint64_t currentTag = tagRef++;
        const uint64_t fetchTime = time_++;

        bool trap = false;
        WdRiscv::ExceptionCause cause = WdRiscv::ExceptionCause::NONE;
        uint64_t trapPc = 0;

        if (!perfApi_->fetch(hartIx, fetchTime, currentTag, vpc, is_wrong_path, cause, trapPc))
        {
            throw std::runtime_error("WhisperAdapter: PerfApi fetch failed for tag "
                                     + std::to_string(currentTag));
        }

        const uint64_t execTime = time_++;
        try
        {
            if (!perfApi_->execute(hartIx, execTime, currentTag))
            {
                throw std::runtime_error("WhipserAdapter: PerfApi execute failed for tag : "
                                         + std::to_string(currentTag));
            }
        }
        catch (const WdRiscv::CoreException & ex)
        {
            if (ex.type() == WdRiscv::CoreException::Stop
                || ex.type() == WdRiscv::CoreException::Exit)
            {
                auto hartPtr = system_->ithHart(hartIx);
                if (hartPtr)
                {
                    hartPtr->setTargetProgramFinished(true);
                }

                std::cerr << "WhisperAdapter: Program finished at execute (tag " << currentTag
                          << " , tohost = " << ex.value() << ")" << std::endl;

                InstructionInfo info;
                auto packet = perfApi_->getInstructionPacket(hartIx, currentTag);
                if (packet && hartPtr)
                {
                    fillInstructionInfo(info, *packet, *hartPtr);
                }

                info.setEndsSimulation(true);
                info.setWrongPath(is_wrong_path);
                return info;
            }
            else
            {
                throw;
            }
        }

        InstructionInfo info;
        auto packet = perfApi_->getInstructionPacket(hartIx, currentTag);
        auto hart = system_->ithHart(hartIx);
        if (packet && hart)
        {
            fillInstructionInfo(info, *packet, *hart);

            if (packet->executed())
            {
                nextPc_[hartIx] = packet->nextPc();
            }
            else if (trap)
            {
                nextPc_[hartIx] = trapPc;
            }
        }

        info.setWrongPath(is_wrong_path);
        return info;
    }

    template <typename URV>
    void WhisperAdapter<URV>::commitInstruction(CoreId core_id, HartId hart_id, uint64_t iss_uid)
    {
        const unsigned ix = hartIx_(core_id, hart_id);
        TT_PERF::RetireResult status = TT_PERF::RetireResult::Success;

        try
        {
            if (!perfApi_->retire(ix, time_++, iss_uid, &status))
            {
                std::cerr << "WhisperAdapter: retire failed for tag : " << iss_uid
                          << " reason : " << TT_PERF::retireResultToString(status);
            }
        }
        catch (const WdRiscv::CoreException & ex)
        {
            if (ex.type() == WdRiscv::CoreException::Stop
                || ex.type() == WdRiscv::CoreException::Exit)
            {
                auto hartPtr = system_->ithHart(ix);
                if (hartPtr)
                {
                    hartPtr->setTargetProgramFinished(true);
                }
                std::cerr << "WhisperAdapter: program finished at retire (tag " << iss_uid
                          << ", tohost = " << ex.what() << ")" << std::endl;
                return;
            }
            else
            {
                throw;
            }
        }
    }

    template <typename URV>
    void WhisperAdapter<URV>::commitStoreWrite(CoreId core_id, HartId hart_id, uint64_t iss_uid)
    {
        const unsigned ix = hartIx_(core_id, hart_id);

        try
        {
            if (!perfApi_->drainStore(ix, time_++, iss_uid))
            {
                std::cerr << "WhisperAdapter: drainStore failed for tag " << iss_uid << std::endl;
            }
        }
        catch (const WdRiscv::CoreException & ex)
        {
            if (ex.type() == WdRiscv::CoreException::Stop
                || ex.type() == WdRiscv::CoreException::Exit)
            {
                auto hartPtr = system_->ithHart(ix);
                if (hartPtr)
                {
                    hartPtr->setTargetProgramFinished(true);
                }

                std::cerr << "Whisper program finished at drainStore (tag " << iss_uid
                          << ", tohost = " << ex.what() << ") " << std::endl;
                return;
            }
            else
            {
                throw;
            }
        }
    }

    template <typename URV>
    void WhisperAdapter<URV>::dropStoreWrite(CoreId core_id, HartId hart_id, uint64_t iss_uid)
    {
        (void)core_id;
        (void)hart_id;
        (void)iss_uid;
    }

    template <typename URV>
    void WhisperAdapter<URV>::flush(CoreId core_id, HartId hart_id,
                                    const EDMCheckpoint & checkpoint)
    {
        const unsigned ix = hartIx_(core_id, hart_id);

        // The PerfApi::flush removes the tag recieved from the checkpoint's iss_uid and all the
        // newer tags.

        const uint64_t flushTag =
            (checkpoint.iss_uid != std::numeric_limits<uint64_t>::max()) ? checkpoint.iss_uid : 0;

        if (!perfApi_->flush(ix, time_++, flushTag))
        {
            std::cerr << "WhisperAdapter: flush failed for tag " << flushTag << std::endl;
            return;
        }

        const bool packetMapEmpty = perfApi_->getInstructionPacketCount(ix) == 0;
        if (packetMapEmpty)
        {
            if (flushTag + 1 > nextTag_[ix])
            {
                nextTag_[ix] = flushTag + 1;
            }
        }
        else
        {
            nextTag_[ix] = flushTag;
        }

        nextPc_[ix] = checkpoint.correct_path_pc;
    }

    EDMInterface* createWhisperAdapter(const std::string & config_file,
                                       const std::string & filename)
    {
        if (config_file.empty())
        {
            throw std::invalid_argument("WhisperAdapter: empty config file path");
        }

        unsigned xlen = 64;

        WdRiscv::HartConfig probe;
        if (!probe.loadConfigFile(config_file))
        {
            throw std::runtime_error(
                std::string("WhisperAdapter: failed to parse config file for XLEN probe: ")
                + config_file);
        }
        probe.getXlen(xlen);

        if (xlen == 32)
        {
            return new WhisperAdapter<uint32_t>(config_file, filename);
        }
        else if (xlen == 64)
        {
            return new WhisperAdapter<uint64_t>(config_file, filename);
        }
        else
        {
            throw std::runtime_error(std::string("WhisperAdapter: unsupported XLEN = ")
                                     + std::to_string(xlen) + (" ( only 32 and 64 are supported)"));
        }
    }

    template class WhisperAdapter<uint32_t>;
    template class WhisperAdapter<uint64_t>;

} // namespace olympia::edm