#pragma once

#include "edm/EDMInterface.hpp"
#include <cstdint>

namespace olympia::edm
{
    class EDMInterface;
}

extern "C"
{

#define EDM_PLUGIN_ABI_VERSION 1u

    struct  EDMPluginInfo
    {
        uint32_t abi_version;
        const char* backend_name;
    };


    typedef const EDMPluginInfo* (*edm_plugin_info_fn)();

    // We are passing a pointer to the interface
    typedef ::olympia::edm::EDMInterface* (*edm_plugin_create_fn)(const char* config_file, const char* workload);

#define EDM_PLUGIN_INFO_SYMBOL "edm_plugin_info"
#define EDM_PLUGIN_CREATE_SYMBOL "edm_plugin_create"
}
