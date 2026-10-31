project(edm)
include(FetchContent)

set(SUPPORTED_BACKENDS "pegasus" "whisper")

if(DEFAULT_BACKEND AND NOT DEFAULT_BACKEND IN_LIST SUPPORTED_BACKENDS AND NOT DEFAULT_BACKEND STREQUAL "all")
    message(FATAL_ERROR "Backend '${DEFAULT_BACKEND}' not found. Try: ${SUPPORTED_BACKENDS} or 'all'")
endif()

set(EDM_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/EDMFactory.cpp)

file(CREATE_LINK ${CMAKE_SOURCE_DIR}/config ${CMAKE_BINARY_DIR}/config SYMBOLIC)

## pegasus
if(DEFAULT_BACKEND STREQUAL "all" OR DEFAULT_BACKEND STREQUAL "pegasus" OR NOT DEFAULT_BACKEND)

    set(PEGASUS_ADAPTER_DIR ${CMAKE_SOURCE_DIR}/core/edm/adapters/Pegasus)
    set(PEGASUS_SOURCE_DIR ${PEGASUS_ADAPTER_DIR}/pegasus)

    message(STATUS "EDM: Fetching Pegasus into ${CMAKE_SOURCE_DIR}/core/edm/adapters/Pegasus/pegasus")

    set(PEGASUS_USE_PARENT_MAVIS TRUE CACHE BOOL "" FORCE)
    set(PEGASUS_USE_PARENT_STF_LIB TRUE CACHE BOOL "" FORCE)
    set(PEGASUS_USE_PARENT_SOFTFLOAT TRUE CACHE BOOL "" FORCE)
    set(FetchContent_Quiet FALSE)

    fetchcontent_declare(pegasus
        GIT_REPOSITORY https://github.com/sparcians/pegasus.git
        GIT_TAG f6231b7
        USES_TERMINAL_DOWNLOAD TRUE
        USES_TERMINAL_UPDATE TRUE
        SOURCE_DIR ${CMAKE_SOURCE_DIR}/core/edm/adapters/Pegasus/pegasus
    )

    fetchcontent_getproperties(pegasus)
    if(NOT pegasus_POPULATED)
        fetchcontent_populate(pegasus)
        message(STATUS "EDM: Pegasus source dir: ${pegasus_SOURCE_DIR}")
	set(NO_STF_LTO True)
        include("${pegasus_SOURCE_DIR}/stf_lib/cmake/lto.cmake")
        include_directories(${CMAKE_BINARY_DIR}/_deps/bitset2-src)
        add_compile_definitions(
            SOFTFLOAT_FAST_INT64
            SOFTFLOAT_FAST_DIV32TO16
            SOFTFLOAT_FAST_DIV64TO32
            SOFTFLOAT_ROUND_ODD
            SOFTFLOAT_BUILTIN_CLZ=1
            SOFTFLOAT_INTRINSIC_INT128=1
        )
        add_subdirectory(${pegasus_SOURCE_DIR} ${pegasus_BINARY_DIR})
    endif()

    add_compile_options(-isystem ${CMAKE_BINARY_DIR}/mavis/softfloat_build-prefix/src/softfloat_build/source/include)
    add_compile_options(-isystem ${CMAKE_BINARY_DIR}/mavis/softfloat_build-prefix/src/softfloat_build/source/RISCV)

    # Symlink pegasus sim artifacts into the build root
    file(CREATE_LINK ${CMAKE_BINARY_DIR}/_deps/pegasus-build/sim/mavis_json ${CMAKE_BINARY_DIR}/mavis_json SYMBOLIC)
    file(CREATE_LINK ${CMAKE_BINARY_DIR}/_deps/pegasus-build/sim/arch ${CMAKE_BINARY_DIR}/arch SYMBOLIC)

    add_dependencies(pegasuslibs softfloat_build)

    list(APPEND EDM_SOURCES ${PEGASUS_ADAPTER_DIR}/Pegasus.cpp)
endif()

## whisper
if(DEFAULT_BACKEND STREQUAL "all" OR DEFAULT_BACKEND STREQUAL "whisper" OR NOT DEFAULT_BACKEND)
    include(ExternalProject)

    set(WHISPER_ADAPTER_DIR ${CMAKE_SOURCE_DIR}/core/edm/adapters/Whisper)
    set(WHISPER_SOURCE_DIR ${WHISPER_ADAPTER_DIR}/whisper)

    if(NOT DEFINED WHISPER_BUILD_JOBS)
        include(ProcessorCount)
        ProcessorCount(WHISPER_BUILD_JOBS)
        if(WHISPER_BUILD_JOBS EQUAL 0)
            set(WHISPER_BUILD_JOBS 2)
        endif()
    endif()

    if(DEFINED Boost_INCLUDE_DIR)
        get_filename_component(WHISPER_BOOST_ROOT ${Boost_INCLUDE_DIR} DIRECTORY)
    elseif(DEFINED ENV{CONDA_PREFIX})
        set(WHISPER_BOOST_DIR $ENV{CONDA_PREFIX})
    else()
        message(FATAL_ERROR "EDM: cannot determine the boost root for whisper build")
    endif()


    set(WHISPER_LIB "${WHISPER_SOURCE_DIR}/build-Linux/librvcore.a")
    set(WHISPER_VIRTMEM_LIB "${WHISPER_SOURCE_DIR}/virtual_memory/libvirtual_memory.a")
    set(WHISPER_SOFTFLOAT_LIB "${WHISPER_SOURCE_DIR}/third_party/softfloat/build/RISCV-GCC/softfloat.a")
    set(WHISPER_PCI_LIB "${WHISPER_SOURCE_DIR}/pci/libpci.a")


    ExternalProject_Add(whisper_external
        GIT_REPOSITORY https://github.com/tenstorrent/whisper.git
        GIT_TAG f989924823fa6fb8c5c1116b8a81930a813a5a15
        GIT_SHALLOW TRUE
        GIT_PROGRESS TRUE 
        PREFIX ${CMAKE_BINARY_DIR}/whisper-build
        SOURCE_DIR ${WHISPER_SOURCE_DIR}
        BUILD_IN_SOURCE 1 
        BUILD_COMMAND ${CMAKE_MAKE_PROGRAM} -C ${WHISPER_SOURCE_DIR}
            -j ${WHISPER_BUILD_JOBS}
            CXX=${CMAKE_CXX_COMPILER}
            CXX_STD=c++20
            BOOST_ROOT=${WHISPER_BOOST_ROOT}
            MEM_CALLBACK=0
        INSTALL_COMMAND ""
        CONFIGURE_COMMAND ""
        UPDATE_COMMAND ""
        BUILD_BYPRODUCTS ${WHISPER_LIB} ${WHISPER_VIRTMEM_LIB} ${WHISPER_SOFTFLOAT_LIB} ${WHISPER_PCI_LIB}
        LOG_DOWNLOAD ON 
        LOG_BUILD ON
    )

    # Create the runtime plugin .so file
    add_library(edm_whisper_plugin SHARED
        ${WHISPER_ADAPTER_DIR}/Whisper.cpp
        ${WHISPER_ADAPTER_DIR}/WhisperPlugin.cpp
    )

    add_dependencies(edm_whisper_plugin whisper_external)

    set_target_properties(edm_whisper_plugin PROPERTIES 
        LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/plugins
        OUTPUT_NAME edm_whisper_plugin
        CXX_VISIBILITY_PRESENT hidden
        VISIBILITY_INLINES_HIDDEN ON
    )

    target_compile_options(edm_whisper_plugin PRIVATE -Wno-error=attributes)

    target_include_directories(edm_whisper_plugin PRIVATE 
        ${CMAKE_CURRENT_SORUCE_DIR}
        ${WHISPER_ADAPTER_DIR}
        ${WHISPER_SOURCE_DIR}
    )


    target_link_options(edm_whisper_plugin PRIVATE 
        -Wl,--whole-archive 
        ${WHISPER_LIB}
        ${WHISPER_VIRTMEM_LIB}
        ${WHISPER_SOFTFLOAT_LIB}
        ${WHISPER_PCI_LIB}
        -Wl,--no-whole-archive
        -Wl,-Bsymbolic 
        -Wl,--exclude-libs,ALL
    )

    target_link_libraries(edm_whisper_plugin PRIVATE 
        Boost::program_options 
        Threads::Threads 
        z dl rt util m
    )
endif()

add_library(edm STATIC ${EDM_SOURCES})

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
	target_compile_options(edm PRIVATE "-Qunused-arguments")
endif()

target_compile_features(edm PUBLIC cxx_std_17)
target_include_directories(edm PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

## pegasus
if(DEFAULT_BACKEND STREQUAL "all" OR DEFAULT_BACKEND STREQUAL "pegasus" OR NOT DEFAULT_BACKEND)
    target_include_directories(edm PUBLIC
        ${PEGASUS_ADAPTER_DIR}
        ${PEGASUS_SOURCE_DIR}
        ${PEGASUS_SOURCE_DIR}/cosim
        ${PEGASUS_SOURCE_DIR}/include
    )
    target_link_libraries(edm PUBLIC pegasuslibs pegasuscosimlib)
    target_compile_definitions(edm PUBLIC
        PEGASUS_AVAILABLE
        DEFAULT_PEGASUS
    )
endif()

## whisper
if(DEFAULT_BACKEND STREQUAL "all" OR DEFAULT_BACKEND STREQUAL "whisper" OR NOT DEFAULT_BACKEND)
    target_link_libraries(edm PUBLIC ${CMAKE_DL_LIBS})
    target_compile_definitions(edm PRIVATE 
        EDM_PLUGIN_DIR="${CMAKE_BINARY_DIR}/plugins"
    )
    target_compile_definitions(edm PUBLIC
        WHISPER_AVAILABLE  
    )
    add_dependencies(edm edm_whisper_plugin) 
endif()
