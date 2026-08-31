#include "Pegasus.hpp"
#include "EDMTypes.hpp"
#include <cstdint>
#include <iostream>
#include <map>
#include <memory>
#include "pegasus/cosim/PegasusCoSim.hpp"
#include "pegasus/cosim/EventAccessor.hpp"
#include "edm/EDMFactory.hpp"
#include <sparta/utils/SpartaAssert.hpp>
#include <yaml-cpp/yaml.h>

namespace olympia::edm
{
    PegasusAdapter::PegasusAdapter(const std::string & config_file, const std::string & filename)

    {
        if (config_file.empty())
        {
            throw;
        }

        YAML::Node config = YAML::LoadFile(config_file);
        const uint64_t ilimit = config["ilimit"].as<uint64_t>();
        const std::map<std::string, std::string> pegasus_params =
            config["params"].as<std::map<std::string, std::string>>();
        const std::vector<std::vector<std::string>> pegasus_loggers =
            config["pegasus_loggers"].as<std::vector<std::vector<std::string>>>();
        const std::string db_file = config["db_file"].as<std::string>();
        const uint64_t snapshot_threshold = config["snapshot_threshold"].as<uint64_t>();

        // read the config file and handle it
        cosim_ = std::make_unique<pegasus::cosim::PegasusCoSim>(
            ilimit, filename, pegasus_params,
            std::vector<std::vector<std::string>>{}, // empty pegasus_loggers
            db_file, snapshot_threshold);
    }

    PegasusAdapter::~PegasusAdapter()
    {
        try
        {
           cosim_->finish();
        }
        catch (...)
        {
            std::cerr << "There was an error ending the simmulation on the pegasus side" << std::endl;
        }
    }

    bool PegasusAdapter::isFinished(CoreId core_id, HartId hart_id) const
    {
        return cosim_->isSimulationFinished(core_id, hart_id);
    }

    Addr PegasusAdapter::peekNextPc(CoreId core_id, HartId hart_id) const
    {
        return cosim_->getPc(core_id, hart_id);
    }

    InstructionInfo PegasusAdapter::step(CoreId core_id, HartId hart_id)
    {
        pegasus::cosim::EventAccessor evt = cosim_->step(core_id, hart_id);
        InstructionInfo info(evt);
        if (const auto* event = evt.get())
        {
            info.setFaulted(event->getExceptionType() != pegasus::ExcpType::INVALID);
        }
        pending_events_.emplace(info.getIssUid(), std::move(evt));
        return info;
    }

    InstructionInfo PegasusAdapter::stepWithOverridePc(CoreId core_id, HartId hart_id,
                                                       Addr override_pc)
    {
        // TODO: Integrate the checkpoint mechanism
        pegasus::cosim::EventAccessor evt = cosim_->step(core_id, hart_id, override_pc);
        InstructionInfo info(evt);
        if (const auto* event = evt.get())
        {
            info.setFaulted(event->getExceptionType() != pegasus::ExcpType::INVALID);
        }
        info.setWrongPath(true);
        pending_events_.emplace(info.getIssUid(), std::move(evt));
        return info;
    }

    void PegasusAdapter::commitInstruction(CoreId /*core_id*/, HartId /*hart_id*/, uint64_t iss_uid)
    {
        auto it = pending_events_.find(iss_uid);
        if (it != pending_events_.end())
        {
            cosim_->commit(it->second);
            pending_events_.erase(it);
        }
    }

    void PegasusAdapter::commitStoreWrite(CoreId /*core_id*/, HartId /*hart_id*/, uint64_t iss_uid)
    {
        /*TODO: This method has not been implemented in pegasus*/
        (void)iss_uid;
    }

    void PegasusAdapter::dropStoreWrite(CoreId /*core_id*/, HartId /*hart_id*/, uint64_t iss_uid)
    {
        /*TODO: This method has not been implemented in pegasus*/
        (void)iss_uid; 
    }

    void PegasusAdapter::flush(CoreId /*core_id*/, HartId /*hart_id*/,
                               const EDMCheckpoint & checkpoint)
    {
        if (pending_events_.empty())
        {
            return;
        }

        if (checkpoint.iss_uid != std::numeric_limits<uint64_t>::max())
        {
            auto it = pending_events_.find(checkpoint.iss_uid);

            if (it == pending_events_.end())
            {
                auto & oldest = pending_events_.begin()->second;
                cosim_->flush(oldest, false);
                pending_events_.clear();
                return;
            }

            cosim_->flush(it->second, false);
            pending_events_.erase(it, pending_events_.end());
        }
        else
        {
            auto & oldest = pending_events_.begin()->second;
            cosim_->flush(oldest, false);
            pending_events_.clear();
        }
    }

} // namespace olympia::edm
