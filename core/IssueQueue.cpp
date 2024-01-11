// <IssueQueue.cpp> -*- C++ -*-

#include "IssueQueue.hpp"
#include "ExecutePipe.cpp"
namespace olympia
{
    const char IssueQueue::name[] = "issue_queue";

    IssueQueue::IssueQueue(sparta::TreeNode * node, const IssueQueueParameterSet * p):
        sparta::Unit(node),
        reg_file_(determineRegisterFile(node->getGroup().substr(node->getGroup().find("_")+1))),
        issue_queue_size_(p->issue_queue_size)
    {
        in_execute_inst_.
            registerConsumerHandler(CREATE_SPARTA_HANDLER_WITH_DATA(IssueQueue, getInstsFromDispatch_,
                                                                    InstPtr));
        in_exe_pipe_done_.
            registerConsumerHandler(CREATE_SPARTA_HANDLER_WITH_DATA(IssueQueue, readyExeUnit_, uint32_t));
        in_reorder_flush_.
            registerConsumerHandler(CREATE_SPARTA_HANDLER_WITH_DATA(IssueQueue, flushInst_,
                                                                    FlushManager::FlushingCriteria));
        sparta::StartupEvent(node, CREATE_SPARTA_HANDLER(IssueQueue, setupIssueQueue_));
    }
    void IssueQueue::setupIssueQueue_(){
        std::vector<core_types::RegFile> reg_files = {core_types::RF_INTEGER, core_types::RF_FLOAT};
        for(const auto rf : reg_files)
        {
            scoreboard_views_[rf].reset(new sparta::ScoreboardView(getContainer()->getName(),
                                                                   core_types::regfile_names[rf],
                                                                   getContainer()));
        }
        out_scheduler_credits_.send(issue_queue_size_);
    }
    void IssueQueue::setExePipe(std::string exe_pipe_name, olympia::ExecutePipe* exe_pipe){
        exe_pipes_[exe_pipe_name] = exe_pipe;
    }
    void IssueQueue::setExePipeMapping(InstArchInfo::TargetPipe tgt_pipe, olympia::ExecutePipe* exe_pipe){
        // auto pipe_vector = pipe_exe_pipe_mapping_.find(tgt_pipe);
        // pipe_vector->second.emplace_back(exe_pipe);
        if(pipe_exe_pipe_mapping_.find(tgt_pipe) == pipe_exe_pipe_mapping_.end()){
            pipe_exe_pipe_mapping_[tgt_pipe] = std::vector<olympia::ExecutePipe*>();
        }
        auto pipe_vector = pipe_exe_pipe_mapping_.find(tgt_pipe);
        pipe_vector->second.emplace_back(exe_pipe);
        pipes_.push_back(exe_pipe);
        
    }
    
    std::unordered_map<std::string, olympia::ExecutePipe*> IssueQueue::getExePipes(){
        return exe_pipes_;
    }

    void IssueQueue::getInstsFromDispatch_(const InstPtr & ex_inst){
        const auto & src_bits = ex_inst->getSrcRegisterBitMask(reg_file_);
        if(scoreboard_views_[reg_file_]->isSet(src_bits)){
            // TODO: add assert to ensure that ready_queue_ size is not bigger than limit, we should be
            // checking in dispatch anyways that we have enough space b4 sending an instruction
            ready_queue_.emplace_back(ex_inst);
            // schedule event to issue an instruction to ExecutePipe if available
            ev_issue_ready_inst_.schedule(sparta::Clock::Cycle(0));
        }
        else{
            scoreboard_views_[reg_file_]->
                registerReadyCallback(src_bits, ex_inst->getUniqueID(),
                                      [this, ex_inst](const sparta::Scoreboard::RegisterBitMask&)
                                      {
                                          this->getInstsFromDispatch_(ex_inst);
                                      });
            ILOG("Instruction NOT ready: " << ex_inst << " Bits needed:" << sparta::printBitSet(src_bits));
        }
    }

    void IssueQueue::readyExeUnit_(const uint32_t & readyExe){
        /*
        have a map/mask to see which execution units are ready
        signal port

        mask concept -> value of the pipe, don't calculate every time, boolean ands everytime
        pipe -> has a specific mask and we and it, precalculate it

        LSU ROB changes -> sends notification that ends simulation
        - if ROB is not taking down simulation and there's stuff in Queue, add debug logic
        */
        if(!ready_queue_.empty()){
            ev_issue_ready_inst_.schedule(sparta::Clock::Cycle(0));
        }
    }

    void IssueQueue::sendReadyInsts_()
    {
        /*
        if instruction is ready, we can send
        but what do we do if an instruction ISN'T ready?
        we have a queue, we check each instruction if it's ready and there is a pipe is ready to accept
        */
        sparta_assert(ready_queue_.size() <= issue_queue_size_, "ready queue greater than issue queue size " << issue_queue_size_);
        for(auto inst_itr = ready_queue_.begin(); inst_itr != ready_queue_.end();){
            auto & inst = *inst_itr;
            auto & valid_exe_pipe = pipe_exe_pipe_mapping_[inst->getPipe()];
            bool sent = false;
            for(auto & exe_pipe : valid_exe_pipe){
                if(exe_pipe->canAccept()){
                    exe_pipe->execute(inst);
                    inst_itr = ready_queue_.erase(inst_itr);
                    out_scheduler_credits_.send(1, 0); // send credit back to dispatch, we now have more room in IQ
                    sent = true;
                    break;
                }
            }
            if(!sent){
                ++inst_itr;
            }
        }
        // InstPtr & ex_inst_ptr = ready_queue_.front();
        // auto & ex_inst = *ex_inst_ptr;
        // ex_inst.setStatus(Inst::Status::SCHEDULED);
    }


    void IssueQueue::flushInst_(const FlushManager::FlushingCriteria & criteria)
    {
        ILOG("Issue Queue, Got flush for criteria: " << criteria);
        // design -> store all instructions in flight, so we know which execute pipes have which instruction
        // on flush, we can then notify only relevant execute pipes
        // we also need a signal from execute pipe saying it's done (which we already have), we then are able to pop
        // from this mapping structure!
        // Flush instructions in the ready queue
        ReadyQueue::iterator it = ready_queue_.begin();
        uint32_t credits_to_send = 0;
        while(it != ready_queue_.end()) {
            if((*it)->getUniqueID() >= uint64_t(criteria)) {
                ready_queue_.erase(it++);
                ++credits_to_send;
            }
            else {
                ++it;
            }
        }
        if(credits_to_send) {
            // send credits back to dispatch
            out_scheduler_credits_.send(credits_to_send, 0);
        }
    }
}