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

    :--- HOW TO ADD A NEW BACKEND ---:

    1. Implement the adapter
        Create a class ( e.g. WhisperAdapter) that inherits from EDMInterface and lives under core/edm/adapters/Whisper.

        Its constructor must accept (const std::string& config_file, const std::string& filename )
        
        Here, config_file -: path to the config file for the backend, and
        filename -: workload path

    2. Register it inside EDMFactory.cpp

        In the static lamdba EDMBackendFactory::create(), add:

        EDMBackendFactory::registerBackend(
            "whisper",
            [](const std::string & cfg, const std::string& fn){
                return std::make_unique<WhisperAdapter>(cfg, fn);
            }
        )

    3. Guard with a CMake feature flag (optional but recommended)
        Wrap the include and registration in something like #ifdef WHISPER_AVAILABLE so the build process is smooth

    4. Write it into CMakeLists.txt
        Add a FetchContent block or whatever is the method through which you would like to pull in the backend. Make sure to append the .cpp files of the adapter to the EDM_SOURCES

    5. Select the backend at runtime

        In arches YAML ( small_core, medium_core or large_core ) set:
        fetch.params.backend = "whisper"
        fetch.params.backend_config_file: "config/whisper.yaml"

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

        static std::string & getDefaultBackend();
        static std::string & getDefault();

      private:
        static std::map<std::string, BackendCreator> & getRegistry_();
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
