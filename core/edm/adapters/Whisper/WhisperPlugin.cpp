#include "edm/EDMPluginABI.h"
#include "Whisper.hpp"

#include <exception>
#include <cstdio>

extern "C"
{
	__attribute__((visibility("default")))
	const EDMPluginInfo * edm_plugin_info()
	{
		static const EDMPluginInfo info{EDM_PLUGIN_ABI_VERSION, "whisper"};
		return &info;
	}

	__attribute__((visibility("default")))
	::olympia::edm::EDMInterface* edm_plugin_create(const char* config_file, const char* workload)
	{
		try
		{
			return ::olympia::edm::createWhisperAdapter(config_file ? config_file : "",
					workload ? workload : "");
		}
		catch (const std::exception & e)
		{
			std::fprintf(stderr, "edm_whisper_plugin: createWhisperAdapter threw: %s\n", e.what());
			return nullptr;
		}
	}
}
