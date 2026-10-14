project(edm)
include(FetchContent)

set(SUPPORTED_BACKENDS "pegasus" "whisper")

if(DEFAULT_BACKEND AND NOT DEFAULT_BACKEND IN_LIST SUPPORTED_BACKENDS AND NOT DEFAULT_BACKEND STREQUAL "all")
    message(FATAL_ERROR "Backend '${DEFAULT_BACKEND}' not found. Try: ${SUPPORTED_BACKENDS} or 'all'")
endif()

set(EDM_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/EDMFactory.cpp)

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
    file(CREATE_LINK ${CMAKE_SOURCE_DIR}/config ${CMAKE_BINARY_DIR}/config SYMBOLIC)

    add_dependencies(pegasuslibs softfloat_build)

    list(APPEND EDM_SOURCES ${PEGASUS_ADAPTER_DIR}/Pegasus.cpp)
endif()

## whisper
if(DEFAULT_BACKEND STREQUAL "all" OR DEFAULT_BACKEND STREQUAL "whisper" OR NOT DEFAULT_BACKEND)
    message(STATUS "EDM: Fetching Whisper into ${CMAKE_BINARY_DIR}/_deps/whisper-src")

    fetchcontent_declare(whisper
        GIT_REPOSITORY https://github.com/tenstorrent/whisper.git
        GIT_TAG master
        GIT_SHALLOW TRUE
        GIT_PROGRESS TRUE
    )
    fetchcontent_makeavailable(whisper)

    # list(APPEND EDM_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/adapters/Whisper/Whisper.cpp)
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
    target_link_libraries(edm PUBLIC whisper)
    target_compile_definitions(edm PUBLIC
        WHISPER_AVAILABLE
        DEFAULT_WHISPER
    )
endif()
