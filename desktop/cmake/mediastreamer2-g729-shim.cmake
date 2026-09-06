# Shim injected into the mediastreamer2 build (via CMAKE_PROJECT_INCLUDE_BEFORE)
# so it can be rebuilt with G.729 against the *prebuilt* liblinphone SDK,
# instead of rebuilding the whole SDK from source.
#
# Why this exists: G.729 is compiled into mediastreamer2 itself (guarded by
# `if(BCG729_FOUND AND ENABLE_G729)`), it is not a loadable plugin — and the
# full SDK build needs ~25 dependency repos that live only on Belledonne's
# GitLab. Rebuilding just mediastreamer2 avoids all of that.
#
# See README.md ("Recompilando o mediastreamer2 com G.729") for the commands.
#
# Required cache variables:
#   RRP_SDK_DIR    - root of the extracted prebuilt SDK (contains include/, lib/, bin/)
#   RRP_BCG729_DIR - install prefix of our bcg729 build

if(NOT RRP_SDK_DIR)
    message(FATAL_ERROR "RRP_SDK_DIR must point at the prebuilt liblinphone SDK root.")
endif()

# Each dependency keeps its headers in its own subfolder (include/gsm/gsm.h,
# include/opus/opus.h, ...) but the sources include them unqualified, because
# upstream they are consumed from their build trees. Putting every subfolder
# on the include path reproduces that.
include_directories("${RRP_SDK_DIR}/include")
file(GLOB _rrp_include_subdirs LIST_DIRECTORIES true "${RRP_SDK_DIR}/include/*")
foreach(_inc_dir ${_rrp_include_subdirs})
    if(IS_DIRECTORY "${_inc_dir}")
        include_directories("${_inc_dir}")
    endif()
endforeach()

# Declares an imported target for a library shipped by the prebuilt SDK.
#
# Two reasons this is needed:
#
# 1. Belledonne's Find modules (FindBV16, FindGSM, FindSpeex, ...) have a
#    Windows bug in their "use a prebuilt copy" branch: they declare the
#    target as `UNKNOWN IMPORTED` but only set IMPORTED_IMPLIB, never
#    IMPORTED_LOCATION, which CMake rejects. That branch never runs in the
#    official build (there these libs are built in-tree, so `if(TARGET x)`
#    short-circuits first). Defining the targets up front restores that
#    short-circuit.
#
# 2. The target must resolve to the import library, not the DLL: parts of the
#    build consume imported targets as plain file paths, and handing a .dll to
#    the linker fails with "LNK1107: invalid or corrupt file".
function(_rrp_import_sdk_library target_name)
    if(TARGET ${target_name})
        return()
    endif()
    set(_implib "${RRP_SDK_DIR}/lib/${target_name}.lib")
    if(NOT EXISTS "${_implib}")
        return()
    endif()
    add_library(${target_name} UNKNOWN IMPORTED GLOBAL)
    set_target_properties(${target_name} PROPERTIES
        IMPORTED_LOCATION "${_implib}"
        INTERFACE_INCLUDE_DIRECTORIES "${RRP_SDK_DIR}/include"
    )
endfunction()

# bcg729 (the G.729 codec) is built separately by us; it is not in the SDK.
if(RRP_BCG729_DIR AND NOT TARGET bcg729 AND EXISTS "${RRP_BCG729_DIR}/lib/bcg729.lib")
    add_library(bcg729 UNKNOWN IMPORTED GLOBAL)
    set_target_properties(bcg729 PROPERTIES
        IMPORTED_LOCATION "${RRP_BCG729_DIR}/lib/bcg729.lib"
        INTERFACE_INCLUDE_DIRECTORIES "${RRP_BCG729_DIR}/include"
    )
endif()

# Import everything the SDK ships, except the libraries that come with proper
# CMake config packages (they resolve on their own, and predefining them would
# clash with their own add_library calls) and mediastreamer2 itself, which is
# what we are rebuilding.
#
# NOTE: the SDK's own Sqlite3Targets.cmake is deliberately NOT included — it
# points the target at sqlite3.dll and triggers the LNK1107 above. The generic
# importer defines the same target name against the .lib, which is all BZRTP's
# config needs.
set(_rrp_skip_libs
    mediastreamer2
    bctoolbox bctoolbox-tester bcunit
    ortp bzrtp belle-sip libbelle-sip-tester belr belcard
    liblinphone lime
)
file(GLOB _rrp_sdk_libs "${RRP_SDK_DIR}/lib/*.lib")
foreach(_lib_path ${_rrp_sdk_libs})
    get_filename_component(_lib_name "${_lib_path}" NAME_WE)
    if(NOT _lib_name IN_LIST _rrp_skip_libs)
        _rrp_import_sdk_library(${_lib_name})
    endif()
endforeach()
