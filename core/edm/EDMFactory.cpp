#include <sparta/utils/SpartaAssert.hpp>

#include "EDMFactory.hpp"

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
            return true;
        }();
        (void)link;

        /**
        For future reference if you wish to add another adapter say Whisper, the above lamdba will
        be :
        //  static bool link = []() {
        //     EDMBackendFactory::registerBackend(
        //         "pegasus",
        //         [](const std::string & cfg, const std::string & fn) {
        //             return std::make_unique<PegasusAdapter>(cfg, fn);
        //         });
        //     EDMBackendFactory::registerBackend(
        //         "whisper",
        //         [](const std::string & cfg, const std::string & fn) {
        //             return std::make_unique<WhisperAdapter>(cfg, fn);
        //         });
        //     return true;
        // }();
        */

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
