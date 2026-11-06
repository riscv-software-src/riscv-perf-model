#include <sparta/utils/SpartaAssert.hpp>

#include "EDMFactory.hpp"
#include "EDMPluginABI.h"

#include <cstdlib>
#include <cstring>
#include <dirent.h>
#include <dlfcn.h>
#include <sys/stat.h>

#ifdef PEGASUS_AVAILABLE
#include "Pegasus.hpp"
#endif

namespace olympia::edm
{
    std::map<std::string, EDMBackendFactory::BackendCreator> & EDMBackendFactory::getRegistry_()
    {
        static std::map<std::string, BackendCreator> registry;
        return registry;
    }

    std::vector<void*> & EDMBackendFactory::getPluginHandles_()
    {
        static std::vector<void*> handles;
        return handles;
    }

    std::string & EDMBackendFactory::getDefault()
    {
        static std::string default_backend;
        return default_backend;
    }

    void EDMBackendFactory::registerBackend(const std::string & name, BackendCreator creator)
    {
        auto & reg = getRegistry_();
        if (reg.empty())
        {
            getDefault() = name;
        }
        reg.emplace(name, std::move(creator));
    }

    std::string EDMBackendFactory::loadPlugin(const std::string & so_path)
    {
        void* handle = dlopen(so_path.c_str(), RTLD_NOW | RTLD_LOCAL);
        sparta_assert(handle != nullptr, "EDM: dlopen failed for " << so_path << dlerror());

        dlerror(); // This is clearing any stale error state before we do the dlsym

        auto info_fn = reinterpret_cast<edm_plugin_info_fn>(
            dlsym(handle, EDM_PLUGIN_INFO_SYMBOL)
        );
        const char* err = dlerror();
        sparta_assert(info_fn != nullptr && err == nullptr, "EDM: plugin " << so_path << " missing " << EDM_PLUGIN_INFO_SYMBOL << ": " << (err ? err : "(null)"));

        auto create_fn =reinterpret_cast<edm_plugin_create_fn>(dlsym(handle, EDM_PLUGIN_CREATE_SYMBOL));
        err = dlerror();
        sparta_assert(create_fn != nullptr && err == nullptr, "EDM: plugin " << so_path << " missing " << EDM_PLUGIN_CREATE_SYMBOL << " : " << (err ? err : "(null)"));

        const EDMPluginInfo* info = info_fn();
        sparta_assert(info != nullptr, "EDM: plgin " << so_path << " returned null info block");
        sparta_assert(info->abi_version == EDM_PLUGIN_ABI_VERSION, "EDM : plugin " << so_path << " ABI version " << info->abi_version << " != host " << EDM_PLUGIN_ABI_VERSION);
        sparta_assert(info->backend_name != nullptr && info->backend_name[0] != '\0', "EDM: plugin " << so_path << " has empty backend name");

        const std::string backend_name = info->backend_name;

        registerBackend(backend_name, [create_fn](const std::string & cfg, const std::string & fn){
            auto* raw = create_fn(cfg.c_str(), fn.c_str());
            sparta_assert(raw != nullptr, " EDM plugin: returned a null adapter - no adapter configured");

            return std::unique_ptr<EDMInterface>(raw);
        });

        getPluginHandles_().push_back(handle);
        return backend_name;
    }

    void EDMBackendFactory::loadPluginFromDir(const std::string & dir)
    {
        struct stat st {};
        if (stat(dir.c_str(), &st) != 0 || !S_ISDIR(st.st_mode))
        {
            return; // We are missing the plugin directory - we will get his when we have just pegasus
        }

        DIR* d = opendir(dir.c_str());
        if(d == nullptr)
        {
            return;
        }

        while (auto* ent = readdir(d))
        {
            const std::string name = ent->d_name;
            constexpr const char* prefix = "libedm_";
            constexpr const char* suffix = "_plugin.so";
            if(name.rfind(prefix, 0) != 0)
            {
                continue;
            }

            if(name.size() < std::strlen(suffix) || name.compare(name.size() - std::strlen(suffix), std::strlen(suffix), suffix) != 0)
            {
                continue;
            }

            loadPlugin(dir + "/" + name);
        }
        closedir(d);
    }

    std::unique_ptr<EDMInterface> EDMBackendFactory::create(const std::string & backend_name,
                                                            const std::string & config_file,
                                                            const std::string & filename)
    {

        static bool link = [](){
        #ifdef PEGASUS_AVAILABLE
            EDMBackendFactory::registerBackend("pegasus", [](const std::string & config_file,
            const std::string& filename) {
                return std::make_unique<PegasusAdapter>(config_file, filename);
            });
        #endif

        #ifdef EDM_PLUGIN_DIR
        // The EDM_PLUGIN_DIR is set from the core/edm/CMakeLists.txt
            EDMBackendFactory::loadPluginFromDir(EDM_PLUGIN_DIR);
        #endif


        if (const char* env = std::getenv("EDM_PLUGIN_DIR"))
        {
            std::string list = env;
            size_t start = 0;
            while (start <= list.size())
            {
                size_t end = list.find(":", start);
                if(end == std::string::npos)
                {
                    end = list.size();
                }   
                if (end > start)
                {
                    EDMBackendFactory::loadPlugin(list.substr(start, end - start));
                }
                start = end + 1;
            }
        }
            return true;
        }();
        (void)link;

        

        auto & reg = getRegistry_();
        auto it = reg.find(backend_name);
        sparta_assert(it != reg.end(),
                      "No backend with the name "
                          << backend_name
                          << "found. Ensure the backend is registered and the edm mode is enabled");
        return it->second(config_file, filename);
    }

    std::string & EDMBackendFactory::getDefaultBackend() { return getDefault(); }
} // namespace olympia::edm
