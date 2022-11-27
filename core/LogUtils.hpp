// <LogUtils.hpp> -*- C++ -*-

#pragma once

// Macro to simplify sending messages to info_logger_
#define ILOG(msg) \
    if (SPARTA_EXPECT_FALSE(info_logger_)) { \
        info_logger_ << __func__ <<  ": " << msg; \
    }

// Macro to simplify sending messages to warn_logger_
#define WLOG(msg) \
    if (SPARTA_EXPECT_FALSE(warn_logger_)) { \
        warn_logger_ << __func__ <<  ": " << msg; \
    }

// Macro to simplify sending messages to debug_logger_
#define DLOG(msg) \
    if (SPARTA_EXPECT_FALSE(debug_logger_)) { \
        debug_logger_ << __func__ <<  ": " << msg; \
    }
