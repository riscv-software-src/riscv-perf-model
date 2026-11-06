#include <functional>
#include <map>
#include <memory>
#include <string>

#include "EDMInterface.hpp"
#include "EDMTypes.hpp"

namespace olympia::edm
{
    /**
    @brief Factory that creates the EDM backend instance by name.
    */

    class EDMBackendFactory
    {
      public:
      // function signature every backend creator must match
        using BackendCreator = std::function<std::unique_ptr<EDMInterface>(
            const std::string & config_file, const std::string & filename)>;
        
        /**
        @brief Instanctiates a backend by name
        @param backend_name must match the string passed to registerBackned()
        @param config_file Backend specific yaml file ( e.g. pegasus.yaml )
        @param filename Path to the workload (ELF binary)
         */

        static std::unique_ptr<EDMInterface> create(const std::string & backend_name,
                                                    const std::string & config_file,
                                                    const std::string & filename);
        
        /**
        @brief Register a backend creator function under a given name
         */
        static void registerBackend(const std::string & name, BackendCreator creator);

        static std::string loadPlugin(const std::string &plugin_path);

        /**
        Function to scan a directory for libedm_*_plugin.so and load each one.
        If no directory is given then we no-op
        */
        static void loadPluginFromDir(const std::string & dir);

        static std::string & getDefaultBackend();
        static std::string & getDefault();

      private:
        static std::map<std::string, BackendCreator> & getRegistry_();
        static std::vector<void*> & getPluginHandles_();
    };

    /**
     * The registrar - is global. This registrar uses RAII - Resource aquisition is initliazaiton.
     * This means that the different backends, like pegasus and whisper - are not declared and
     initialize globally. Instead - this registrar is global and when we acquire the backend by
       specifying it - then we have the backend.
    */

    template <typename Implementation> struct BackendRegistrar
    {
        BackendRegistrar(const std::string & name)
        {

            EDMBackendFactory::registerBackend(
                name, [](const std::string & config_file, const std::string & filename)
                { return std::make_unique<Implementation>(config_file, filename); });
        }
    };
} // namespace olympia::edm
