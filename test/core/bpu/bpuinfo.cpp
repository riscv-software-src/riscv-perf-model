#include "commonTypes.h"
// These are working examples, add additional fields as needed

//! \brief BpuRequestInfo stream overload
std::ostream& operator<<(std::ostream& os, const BpuRequestInfo& req) {
    os << "BPU request("<<req.reqId<<")";
    return os;
}
//! \brief BpuResponseInfo stream overload
std::ostream& operator<<(std::ostream& os, const BpuResponseInfo& resp) {
    os << "BPU request("<<resp.reqId<<")";
    return os;
}

