// <CamLogUtils.hpp> -*- C++ -*-

#pragma once

#include "sparta/utils/LogUtils.hpp"

// Note:  we pass in InstPtr, not Inst
// We append a space to UID to make it easier to grep for a specific UID, e.g.:
//   grep 'uid:1633 ' <info_log>
#ifndef ILOG_UID
#define ILOG_UID(inst_ptr, msg) ILOG("uid:" << inst_ptr->getUniqueID() << " " << msg)
#endif
