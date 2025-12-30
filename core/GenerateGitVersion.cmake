execute_process(
    COMMAND git describe --always --dirty
    WORKING_DIRECTORY ${SOURCE_DIR}
    OUTPUT_VARIABLE GIT_COMMIT_HASH
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
)

if(NOT GIT_COMMIT_HASH OR GIT_COMMIT_HASH STREQUAL "")
    set(GIT_COMMIT_HASH "unknown")
endif()

file(WRITE ${GIT_VERSION_FILE} 
"// Auto-generated file - do not edit
#ifndef GIT_VERSION_H
#define GIT_VERSION_H

#define GIT_COMMIT_HASH \"${GIT_COMMIT_HASH}\"

#endif // GIT_VERSION_H
")