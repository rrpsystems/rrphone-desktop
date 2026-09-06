cmake_minimum_required(VERSION 3.21)
project(RRPSoftphone VERSION 1.0.0 LANGUAGES CXX)

# Identifies the app to the SIP server (User-Agent header) and in the About
# information. Bump the project() version above to change it.
add_compile_definitions(
    RRP_APP_NAME="RRP Softphone"
    RRP_APP_VERSION="${PROJECT_VERSION}"
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

# Svg is needed so QIcon can render the .svg toolbar icons in resources/.
find_package(Qt6 REQUIRED COMPONENTS Widgets Network Xml Svg)

# --- liblinphone SDK -------------------------------------------------------
# This project links against the prebuilt liblinphone Desktop SDK (Belledonne
# Communications). Download it and point LINPHONESDK_DIR at the extracted
# folder (it must contain include/linphone/core.h and lib/linphone.lib).
# See README.md for download instructions.
set(LINPHONESDK_DIR "" CACHE PATH "Path to the extracted liblinphone Desktop SDK")

find_path(LINPHONE_INCLUDE_DIR
    NAMES linphone/core.h
    HINTS "${LINPHONESDK_DIR}/include"
)
find_library(LINPHONE_LIBRARY
    # The prebuilt Windows SDK ships the import lib as liblinphone.lib (not
    # linphone.lib) — keep both names since this has varied across SDK
    # packages/versions.
    NAMES liblinphone linphone
    HINTS "${LINPHONESDK_DIR}/lib" "${LINPHONESDK_DIR}/bin"
)
# bctbx_list_* (the linked-list helpers liblinphone's C API uses everywhere,
# e.g. iterating codecs) live in bctoolbox, a separate import lib from
# liblinphone itself — without it, linking fails with unresolved externs.
find_library(BCTOOLBOX_LIBRARY
    NAMES bctoolbox
    HINTS "${LINPHONESDK_DIR}/lib" "${LINPHONESDK_DIR}/bin"
)

# AES-GCM + PBKDF2 for the .rrpprofile file (src/profile/ProfileCipher.cpp).
# mbedTLS comes with the SDK and its DLL is already shipped alongside the app,
# so this adds no new dependency to install or redistribute.
find_library(MBEDCRYPTO_LIBRARY
    NAMES mbedcrypto
    HINTS "${LINPHONESDK_DIR}/lib" "${LINPHONESDK_DIR}/bin"
)

if(NOT LINPHONE_INCLUDE_DIR OR NOT LINPHONE_LIBRARY OR NOT BCTOOLBOX_LIBRARY OR NOT MBEDCRYPTO_LIBRARY)
    message(WARNING
        "liblinphone SDK not fully found (LINPHONESDK_DIR='${LINPHONESDK_DIR}'). "
        "Configure will continue but the build will fail at the compile/link "
        "step for src/core/SipCoreManager.cpp. See README.md.")
endif()

add_library(linphone_sdk INTERFACE)
if(LINPHONE_INCLUDE_DIR)
    target_include_directories(linphone_sdk INTERFACE "${LINPHONE_INCLUDE_DIR}")
endif()
if(LINPHONE_LIBRARY)
    target_link_libraries(linphone_sdk INTERFACE "${LINPHONE_LIBRARY}")
endif()
if(BCTOOLBOX_LIBRARY)
    target_link_libraries(linphone_sdk INTERFACE "${BCTOOLBOX_LIBRARY}")
endif()
if(MBEDCRYPTO_LIBRARY)
    target_link_libraries(linphone_sdk INTERFACE "${MBEDCRYPTO_LIBRARY}")
endif()

# --- Application ------------------------------------------------------------
add_executable(RRPSoftphone WIN32
    src/main.cpp
    src/core/SipCoreManager.h
    src/core/SipCoreManager.cpp
    src/core/CodecInfo.h
    src/core/CodecInfo.cpp
    src/core/Logger.h
    src/core/Logger.cpp
    src/contacts/Contact.h
    src/contacts/ContactsXmlFetcher.h
    src/contacts/ContactsXmlFetcher.cpp
    src/contacts/LocalContactsStore.h
    src/contacts/LocalContactsStore.cpp
    src/ui/ContactEditDialog.h
    src/ui/ContactEditDialog.cpp
    src/profile/ProfileStore.h
    src/profile/ProfileStore.cpp
    src/profile/ProfileCipher.h
    src/profile/ProfileCipher.cpp
    src/profile/CredentialStore.h
    src/profile/CredentialStore.cpp
    src/profile/SettingsStore.h
    src/profile/SettingsStore.cpp
    src/ui/Theme.h
    src/ui/Theme.cpp
    src/ui/MainWindow.h
    src/ui/MainWindow.cpp
    src/ui/DialPadWidget.h
    src/ui/DialPadWidget.cpp
    src/ui/DialKeyButton.h
    src/ui/DialKeyButton.cpp
    src/ui/SettingsDialog.h
    src/ui/SettingsDialog.cpp
    src/ui/ContactsPanel.h
    src/ui/ContactsPanel.cpp
    src/ui/HistoryPanel.h
    src/ui/HistoryPanel.cpp
    src/history/CallHistoryStore.h
    src/history/CallHistoryStore.cpp
    resources/resources.qrc
)

target_include_directories(RRPSoftphone PRIVATE src)

target_link_libraries(RRPSoftphone PRIVATE
    Qt6::Widgets
    Qt6::Network
    Qt6::Xml
    Qt6::Svg
    linphone_sdk
)

if(WIN32)
    # CredWriteW/CredReadW (Windows Credential Manager) live in advapi32.
    target_link_libraries(RRPSoftphone PRIVATE advapi32)
endif()

# windeployqt copies the Qt runtime (Qt6Core.dll, platform/imageformat/svg
# plugins, ...) next to the executable, so the build folder can just be
# double-clicked or zipped and handed to someone — no Qt on PATH required.
if(WIN32)
    get_target_property(_qt_qmake_exe Qt6::qmake IMPORTED_LOCATION)
    get_filename_component(_qt_bin_dir "${_qt_qmake_exe}" DIRECTORY)
    find_program(WINDEPLOYQT_EXECUTABLE windeployqt HINTS "${_qt_bin_dir}")
    if(WINDEPLOYQT_EXECUTABLE)
        add_custom_command(TARGET RRPSoftphone POST_BUILD
            COMMAND "${WINDEPLOYQT_EXECUTABLE}"
                    --no-translations
                    --no-system-d3d-compiler
                    --no-opengl-sw
                    "$<TARGET_FILE:RRPSoftphone>"
            COMMENT "Copiando runtime do Qt com windeployqt"
        )
    else()
        message(WARNING "windeployqt não encontrado — o .exe só vai rodar com o Qt no PATH.")
    endif()
endif()

# --- Standalone registration smoke test (not part of the shipped app) -----
# Exercises SipCoreManager's registration path from the command line, so
# real SIP credentials can be tested without typing them into the GUI or
# committing them anywhere. See tools/sip_register_test.cpp.
add_executable(sip_register_test
    tools/sip_register_test.cpp
    src/core/SipCoreManager.h
    src/core/SipCoreManager.cpp
    src/core/CodecInfo.h
    src/core/CodecInfo.cpp
)
target_include_directories(sip_register_test PRIVATE src)
target_link_libraries(sip_register_test PRIVATE Qt6::Core linphone_sdk)

# Copy liblinphone runtime DLLs next to the executables on Windows so they
# can actually start without manually fiddling with PATH.
if(WIN32 AND LINPHONESDK_DIR)
    file(GLOB LINPHONE_RUNTIME_DLLS "${LINPHONESDK_DIR}/bin/*.dll")
    if(LINPHONE_RUNTIME_DLLS)
        foreach(target_name RRPSoftphone sip_register_test)
            add_custom_command(TARGET ${target_name} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    ${LINPHONE_RUNTIME_DLLS}
                    $<TARGET_FILE_DIR:${target_name}>
            )
        endforeach()
    endif()
endif()

# liblinphone needs its "top resources dir" at runtime (belr grammars for
# SIP/SDP/vCard parsing, sound/ring files, root CA bundle, etc.) — without
# it, linphone_factory_create_core_3() aborts the process (bctbx-fatal) the
# moment it tries to parse a vCard grammar. SipCoreManager::start() points
# liblinphone at "<exe dir>/share", so that folder needs to exist there too.
if(LINPHONESDK_DIR AND EXISTS "${LINPHONESDK_DIR}/share")
    foreach(target_name RRPSoftphone sip_register_test)
        add_custom_command(TARGET ${target_name} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_directory
                "${LINPHONESDK_DIR}/share"
                "$<TARGET_FILE_DIR:${target_name}>/share"
        )
    endforeach()
endif()

# mediastreamer plugins — this is where the Windows audio driver lives
# (libmswasapi) along with the echo canceller (libmswebrtc). They are NOT in
# the SDK's bin/ folder, so without copying them the app has no sound card
# driver at all. SipCoreManager::start() points liblinphone at this folder.
# Only the audio plugins are shipped: libmsopenh264 is a video codec (out of
# scope for this app) and it fails to load anyway without Cisco's separately
# distributed openh264.dll, which just produces a startup error.
set(MS_PLUGINS_TO_SHIP
    "${LINPHONESDK_DIR}/lib/mediastreamer/plugins/libmswasapi.dll"
    "${LINPHONESDK_DIR}/lib/mediastreamer/plugins/libmswebrtc.dll"
)
foreach(plugin_path ${MS_PLUGINS_TO_SHIP})
    if(NOT EXISTS "${plugin_path}")
        list(REMOVE_ITEM MS_PLUGINS_TO_SHIP "${plugin_path}")
    endif()
endforeach()

if(LINPHONESDK_DIR AND MS_PLUGINS_TO_SHIP)
    foreach(target_name RRPSoftphone sip_register_test)
        add_custom_command(TARGET ${target_name} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:${target_name}>/plugins"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                ${MS_PLUGINS_TO_SHIP}
                "$<TARGET_FILE_DIR:${target_name}>/plugins"
        )
    endforeach()
endif()
