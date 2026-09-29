# Discover release/debug libraries without mixing CRT configurations on Windows.
find_path(UNIMEMORY_JEMALLOC_INCLUDE_DIR jemalloc/jemalloc.h REQUIRED)
get_filename_component(_unimemory_jemalloc_prefix
    "${UNIMEMORY_JEMALLOC_INCLUDE_DIR}" DIRECTORY)
find_library(UNIMEMORY_JEMALLOC_RELEASE_LIBRARY NAMES jemalloc jemalloc_s
    PATHS "${_unimemory_jemalloc_prefix}/lib" NO_DEFAULT_PATH)
if(NOT UNIMEMORY_JEMALLOC_RELEASE_LIBRARY)
    find_library(UNIMEMORY_JEMALLOC_RELEASE_LIBRARY NAMES jemalloc jemalloc_s REQUIRED)
endif()
find_library(UNIMEMORY_JEMALLOC_DEBUG_LIBRARY NAMES jemalloc jemalloc_s
    PATHS "${_unimemory_jemalloc_prefix}/debug/lib" NO_DEFAULT_PATH)

# A je_ prefix does not suppress jemalloc's global C++ new/delete replacement.
# Inspect native symbols without running target binaries, including cross builds.
if(UNIX)
    if(NOT CMAKE_NM)
        message(FATAL_ERROR "jemalloc validation requires the target toolchain's nm")
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
        if(NOT _unimemory_je_nm_result EQUAL 0)
            message(FATAL_ERROR "Cannot inspect jemalloc symbols: ${_unimemory_je_nm_error}")
        endif()
        if(_unimemory_je_symbols MATCHES "[ \t][TW][ \t]+__?Zn[wa][jml]")
            message(FATAL_ERROR
                "jemalloc replaces global C++ new/delete. Build with --with-jemalloc-prefix=je_ --disable-cxx for an explicit backend.")
        endif()
    endforeach()
    unset(_unimemory_je_library)
    unset(_unimemory_je_nm_flags)
    unset(_unimemory_je_nm_result)
    unset(_unimemory_je_symbols)
    unset(_unimemory_je_nm_error)
endif()
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
unset(_unimemory_jemalloc_prefix)
