// <Execute.hpp> -*- C++ -*-
#pragma once

#include "sparta/simulation/Unit.hpp"
#include "sparta/simulation/ParameterSet.hpp"
#include "sparta/simulation/ResourceFactory.hpp"

#include "Inst.hpp"
#include "execute/ExecutePipe.hpp"
#include "execute/IssueQueue.hpp"

namespace olympia
{
    /*!
     * \class Execute
     * \brief Class that creates multiple execution pipes
     *
     * This unit will create the pipes in simulation and be a conduit
     * between pipes.  The pipes it will create: ALU, FPU, amd BR
     * pipes.  This class will not create the LSU pipes
     */
    class Execute : public sparta::Unit
    {

      public:
        //! \brief Parameters for Execute model
        class ExecuteParameterSet : public sparta::ParameterSet
        {
          public:
            ExecuteParameterSet(sparta::TreeNode* n) : sparta::ParameterSet(n) {}
        };

        /**
         * @brief Constructor for Execute
         *
         * @param node The node that represents (has a pointer to) the Execute
         * @param p The Execute's parameter set
         */
        Execute(sparta::TreeNode* node, const ExecuteParameterSet* p);

        //! \brief Name of this resource. Required by sparta::UnitFactory
        static constexpr char name[] = "execute";
    };

    //! Execute's factory class.  Don't create Execute without it
    class ExecuteFactory : public sparta::ResourceFactory<Execute, Execute::ExecuteParameterSet>
    {
      public:
        void onConfiguring(sparta::ResourceTreeNode* node) override;
        void bindLate(sparta::TreeNode* node) override;
        void deleteSubtree(sparta::ResourceTreeNode*) override;

        ~ExecuteFactory() = default;

      private:
        // The order of these two members is VERY important: you
        // must destroy the tree nodes _before_ the factory since
        // the factory is used to destroy the nodes!
        ExecutePipeFactory exe_pipe_fact_;
        std::vector<std::unique_ptr<sparta::ResourceTreeNode>> exe_pipe_tns_;

        IssueQueueFactory issue_queue_fact_;
        std::vector<std::vector<std::string>> issue_queue_to_pipe_map_;
        std::vector<std::unique_ptr<sparta::ResourceTreeNode>> issue_queues_;
    };
} // namespace olympia
