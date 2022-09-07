// <OlympiaSim.hpp> -*- C++ -*-

#pragma once

#include "sparta/app/Simulation.hpp"
#include "sparta/trigger/ExpiringExpressionTrigger.hpp"

namespace olympia{ class CPUFactory; }

/*!
 * \brief OlympiaSim which builds the model and configures it
 */
class OlympiaSim : public sparta::app::Simulation
{
public:

    /*!
     * \brief Construct OlympiaSim
     * \param topology  The CPU topology to instantiate
     * \param scheduler The Scheduler used in simulation
     * \param num_cores Number of cores to instantiate any nodes
     *                  created which match the description as they
     *                  are created
     * \param workload The workload to run (JSON, stf trace)
     * \param instruction_limit The maximum number of instructions to
     *                          run.  0 means no limit
     * \param show_factories Print the registered factories to stdout
     */
    OlympiaSim(const std::string& topology,
               sparta::Scheduler & scheduler,
               const uint32_t num_cores,
               const std::string workload,
               const uint64_t instruction_limit=0,
               const bool show_factories = false);

    // Tear it down
    virtual ~OlympiaSim();

private:

    //////////////////////////////////////////////////////////////////////
    // Setup

    //! Build the tree with tree nodes, but does not instantiate the
    //! unit yet
    void buildTree_() override;

    //! Configure the tree and apply any last minute parameter changes
    void configureTree_() override;

    //! The tree is now configured, built, and instantiated.  We need
    //! to bind things together.
    void bindTree_() override;

    //! This method is used to support command line options like
    //! --report-warmup-icount and -i
    const sparta::CounterBase* findSemanticCounter_(CounterSemantic sem) const override;

    //////////////////////////////////////////////////////////////////////
    // Runtime

    //! Name of the topology to build
    std::string cpu_topology_;

    //! Number of cores in this simulator. Temporary startup option
    const uint32_t num_cores_;

    //! Workload
    const std::string workload_;

    //! Instruction limit (set up -i option on command line)
    const uint64_t instruction_limit_;

    /*!
     * \brief Get the factory for topology build
     */
    auto getCPUFactory_() -> olympia::CPUFactory*;

    /*!
     * \brief Optional flag to print registered factories to console
     */
    const bool show_factories_;
};
