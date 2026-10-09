function(_unimemory_find_jemalloc)
    set(UNIMEMORY_JEMALLOC_FOUND FALSE PARENT_SCOPE)
    set(UNIMEMORY_JEMALLOC_NOT_FOUND_MESSAGE "" PARENT_SCOPE)
    # Report status even if the caller enabled CMake's global required-find mode.
    # The source build and find_package(REQUIRED) decide whether failure is fatal.
    set(CMAKE_FIND_REQUIRED FALSE)

    # Discover release/debug libraries without mixing CRT configurations on Windows.
    find_path(UNIMEMORY_JEMALLOC_INCLUDE_DIR jemalloc/jemalloc.h)
    if(NOT UNIMEMORY_JEMALLOC_INCLUDE_DIR)
        set(UNIMEMORY_JEMALLOC_NOT_FOUND_MESSAGE
            "jemalloc header jemalloc/jemalloc.h was not found" PARENT_SCOPE)
        return()
    endif()
    get_filename_component(_unimemory_jemalloc_prefix
        "${UNIMEMORY_JEMALLOC_INCLUDE_DIR}" DIRECTORY)
    find_library(UNIMEMORY_JEMALLOC_RELEASE_LIBRARY NAMES jemalloc jemalloc_s
        PATHS "${_unimemory_jemalloc_prefix}/lib" NO_DEFAULT_PATH)
    if(NOT UNIMEMORY_JEMALLOC_RELEASE_LIBRARY)
        find_library(UNIMEMORY_JEMALLOC_RELEASE_LIBRARY NAMES jemalloc jemalloc_s)
    endif()
    if(NOT UNIMEMORY_JEMALLOC_RELEASE_LIBRARY)
        set(UNIMEMORY_JEMALLOC_NOT_FOUND_MESSAGE "jemalloc library was not found" PARENT_SCOPE)
        return()
    endif()
    find_library(UNIMEMORY_JEMALLOC_DEBUG_LIBRARY NAMES jemalloc jemalloc_s
        PATHS "${_unimemory_jemalloc_prefix}/debug/lib" NO_DEFAULT_PATH)

    # A je_ prefix does not suppress jemalloc's global C++ new/delete replacement.
    # Inspect native symbols without running target binaries, including cross builds.
    if(UNIX)
        if(NOT CMAKE_NM)
            set(UNIMEMORY_JEMALLOC_NOT_FOUND_MESSAGE
                "jemalloc validation requires the target toolchain's nm" PARENT_SCOPE)
            return()
        endif()
        foreach(_unimemory_je_library IN ITEMS
                "${UNIMEMORY_JEMALLOC_RELEASE_LIBRARY}" "${UNIMEMORY_JEMALLOC_DEBUG_LIBRARY}")
            if(NOT _unimemory_je_library)
                continue()
            endif()
            if(APPLE)
                set(_unimemory_je_nm_flags -gU)
            else()
                set(_unimemory_je_nm_flags -g --defined-only)
                if(_unimemory_je_library MATCHES "[.]so([.]|$)")
                    list(APPEND _unimemory_je_nm_flags -D)
                endif()
            endif()
            execute_process(COMMAND "${CMAKE_NM}" ${_unimemory_je_nm_flags} "${_unimemory_je_library}"
                RESULT_VARIABLE _unimemory_je_nm_result
                OUTPUT_VARIABLE _unimemory_je_symbols ERROR_VARIABLE _unimemory_je_nm_error)
            if(NOT _unimemory_je_nm_result STREQUAL "0")
                set(UNIMEMORY_JEMALLOC_NOT_FOUND_MESSAGE
                    "Cannot inspect jemalloc symbols (${_unimemory_je_nm_result}): ${_unimemory_je_nm_error}" PARENT_SCOPE)
                return()
            endif()
            if(_unimemory_je_symbols MATCHES "[ \t][TW][ \t]+__?Zn[wa][jml]")
                set(UNIMEMORY_JEMALLOC_NOT_FOUND_MESSAGE
                    "jemalloc replaces global C++ new/delete. Build with --with-jemalloc-prefix=je_ --disable-cxx for an explicit backend." PARENT_SCOPE)
                return()
            endif()
        endforeach()
    endif()
    # Only publish a target after every required discovery/validation step succeeds.
    if(NOT TARGET unimemory_jemalloc)
        add_library(unimemory_jemalloc UNKNOWN IMPORTED)
        set_target_properties(unimemory_jemalloc PROPERTIES
            IMPORTED_LOCATION "${UNIMEMORY_JEMALLOC_RELEASE_LIBRARY}"
            IMPORTED_CONFIGURATIONS RELEASE
            IMPORTED_LOCATION_RELEASE "${UNIMEMORY_JEMALLOC_RELEASE_LIBRARY}"
            MAP_IMPORTED_CONFIG_RELWITHDEBINFO Release
            MAP_IMPORTED_CONFIG_MINSIZEREL Release
            INTERFACE_INCLUDE_DIRECTORIES "${UNIMEMORY_JEMALLOC_INCLUDE_DIR}")
        if(UNIMEMORY_JEMALLOC_DEBUG_LIBRARY)
            set_property(TARGET unimemory_jemalloc APPEND PROPERTY IMPORTED_CONFIGURATIONS DEBUG)
            set_property(TARGET unimemory_jemalloc PROPERTY
                IMPORTED_LOCATION_DEBUG "${UNIMEMORY_JEMALLOC_DEBUG_LIBRARY}")
        else()
            set_property(TARGET unimemory_jemalloc PROPERTY MAP_IMPORTED_CONFIG_DEBUG Release)
        endif()
    endif()
    set(UNIMEMORY_JEMALLOC_FOUND TRUE PARENT_SCOPE)
endfunction()

_unimemory_find_jemalloc()
