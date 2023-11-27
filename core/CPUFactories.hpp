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
#include "MMU.hpp"
#include "SimpleTLB.hpp"
#include "BIU.hpp"
#include "MSS.hpp"
#include "ROB.hpp"
#include "FlushManager.hpp"
#include "Preloader.hpp"
#include "MavisUnit.hpp"

namespace olympia{

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
        sparta::ResourceFactory<olympia::Core,
                                olympia::Core::CoreParameterSet> core_rf;

        //! \brief Resouce Factory to build a Fetch Unit
        FetchFactory fetch_rf;

        //! \brief Resouce Factory to build a Decode Unit
        sparta::ResourceFactory<olympia::Decode,
                                olympia::Decode::DecodeParameterSet> decode_rf;

        //! \brief Resouce Factory to build a Rename Unit
        RenameFactory rename_rf;

        //! \brief Resouce Factory to build a Dispatch Unit
        DispatchFactory dispatch_rf;

        //! \brief Resouce Factory to build a Execute Unit
        ExecuteFactory  execute_rf;


        //! \brief Resouce Factory to build a MMU Unit
        sparta::ResourceFactory<olympia::DCache,
                olympia::DCache::CacheParameterSet> dcache_rf;

        //! \brief Resouce Factory to build a TLB Unit
        sparta::ResourceFactory<olympia::SimpleTLB,
                olympia::SimpleTLB::TLBParameterSet> tlb_rf;

        //! \brief Resouce Factory to build a MMU Unit
        sparta::ResourceFactory<olympia::MMU,
                                olympia::MMU::MMUParameterSet> mmu_rf;

        //! \brief Resouce Factory to build a LSU Unit
        sparta::ResourceFactory<olympia::LSU,
                                olympia::LSU::LSUParameterSet> lsu_rf;


        //! \brief Resouce Factory to build a BIU Unit
        sparta::ResourceFactory<olympia_mss::BIU,
                                olympia_mss::BIU::BIUParameterSet> biu_rf;

        //! \brief Resouce Factory to build a MSS Unit
        sparta::ResourceFactory<olympia_mss::MSS,
                                olympia_mss::MSS::MSSParameterSet> mss_rf;

        //! \brief Resouce Factory to build a ROB Unit
        sparta::ResourceFactory<olympia::ROB,
                                olympia::ROB::ROBParameterSet> rob_rf;

        //! \brief Resouce Factory to build a Flush Unit
        sparta::ResourceFactory<olympia::FlushManager,
                                olympia::FlushManager::FlushManagerParameters> flushmanager_rf;

        //! \brief Resouce Factory to build a Preloader Unit
        sparta::ResourceFactory<olympia::Preloader,
                                olympia::Preloader::PreloaderParameterSet> preloader_rf;

        //! \brief Set up the Mavis Decode functional unit
        MavisFactoy  mavis_rf;
    }; // struct CPUFactories
}  // namespace olympia
