// <CPUFactories.h> -*- C++ -*-


#pragma once

#include "sparta/simulation/ResourceFactory.hpp"
#include "Core.hpp"
#include "Fetch.hpp"
#include "Decode.hpp"
#include "Rename.hpp"
#include "Dispatch.hpp"
#include "Execute.hpp"
#include "LSU.hpp"
#include "SimpleTLB.hpp"
#include "BIU.hpp"
#include "MSS.hpp"
#include "ROB.hpp"
#include "FlushManager.hpp"
#include "Preloader.hpp"
#include "MavisUnit.hpp"

namespace olympia_core{

    /**
     * @file  CPUFactories.h
     * @brief CPUFactories will act as the place which contains all the
     *        required factories to build sub-units of the CPU.
     *
     * CPUFactories unit will
     * 1. Contain resource factories to build each core of the CPU
     * 2. Contain resource factories to build microarchitectural units in each core
     */
    struct CPUFactories{

        //! \brief Resouce Factory to build a Core Unit
        sparta::ResourceFactory<olympia_core::Core,
                                olympia_core::Core::CoreParameterSet> core_rf;

        //! \brief Resouce Factory to build a Fetch Unit
        sparta::ResourceFactory<olympia_core::Fetch,
                                olympia_core::Fetch::FetchParameterSet> fetch_rf;

        //! \brief Resouce Factory to build a Decode Unit
        sparta::ResourceFactory<olympia_core::Decode,
                                olympia_core::Decode::DecodeParameterSet> decode_rf;

        //! \brief Resouce Factory to build a Rename Unit
        sparta::ResourceFactory<olympia_core::Rename,
                                olympia_core::Rename::RenameParameterSet> rename_rf;

        //! \brief Resouce Factory to build a Dispatch Unit
        sparta::ResourceFactory<olympia_core::Dispatch,
                                olympia_core::Dispatch::DispatchParameterSet> dispatch_rf;

        //! \brief Resouce Factory to build a Execute Unit
        sparta::ResourceFactory<olympia_core::Execute,
                                olympia_core::Execute::ExecuteParameterSet> execute_rf;

        //! \brief Resouce Factory to build a LSU Unit
        sparta::ResourceFactory<olympia_core::LSU,
                                olympia_core::LSU::LSUParameterSet> lsu_rf;

        //! \brief Resouce Factory to build a TLB Unit
        sparta::ResourceFactory<olympia_core::SimpleTLB,
                                olympia_core::SimpleTLB::TLBParameterSet> tlb_rf;

        //! \brief Resouce Factory to build a BIU Unit
        sparta::ResourceFactory<olympia_mss::BIU,
                                olympia_mss::BIU::BIUParameterSet> biu_rf;

        //! \brief Resouce Factory to build a MSS Unit
        sparta::ResourceFactory<olympia_mss::MSS,
                                olympia_mss::MSS::MSSParameterSet> mss_rf;

        //! \brief Resouce Factory to build a ROB Unit
        sparta::ResourceFactory<olympia_core::ROB,
                                olympia_core::ROB::ROBParameterSet> rob_rf;

        //! \brief Resouce Factory to build a Flush Unit
        sparta::ResourceFactory<olympia_core::FlushManager,
                                olympia_core::FlushManager::FlushManagerParameters> flushmanager_rf;

        //! \brief Resouce Factory to build a Preloader Unit
        sparta::ResourceFactory<olympia_core::Preloader,
                                olympia_core::Preloader::PreloaderParameterSet> preloader_rf;

        //! \brief Set up the Mavis Decode functional unit
        sparta::ResourceFactory<olympia_core::MavisUnit,
                                olympia_core::MavisUnit::MavisParameters> mavis_rf;
    }; // struct CPUFactories
}  // namespace olympia_core
