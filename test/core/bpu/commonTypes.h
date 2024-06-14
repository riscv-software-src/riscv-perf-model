#pragma once
#include "Inst.hpp"

using ThreadID = uint32_t;
//    using PtrType = sparta::SpartaSharedPointer<Inst>;
//    using InstPtr = Inst::PtrType;

enum BpuCommand
{
    NONE,
    SQUASH,
    UPDATE,
    LOOKUP,  //like PREDICT but for cond_branch = true
    PREDICT
};

struct BpuRequestInfo
{
    BpuRequestInfo() {}
    BpuCommand command{BpuCommand::NONE};
    uint32_t reqId{0}; // for matching responses, reqId=0 should be reserved
    ThreadID tid{0};       // we have only 1 thread
    olympia::InstPtr inst; // the instruction
    friend std::ostream& operator<<(std::ostream&,const BpuRequestInfo&);
};

struct BpuResponseInfo
{
    BpuResponseInfo() {}
    BpuCommand response{BpuCommand::NONE};
    uint32_t reqId{0};  // for matching responses, reqId=0 should be reserved
    ThreadID tid{0};        // we have only 1 thread
    olympia::InstPtr inst;  // the original instruction
    bool taken{false};      // prediction return
    bool ack{false};        // for non-prediction commands
    friend std::ostream& operator<<(std::ostream&,const BpuResponseInfo&);
};
