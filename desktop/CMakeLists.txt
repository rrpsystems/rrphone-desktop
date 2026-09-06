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

# --- .rrpprofile encryption key ----------------------------------------------
# Kept out of the repository the way a web app keeps secrets out of the tree,
# but be clear about what that buys: this key ships inside every binary, on
# machines we do not control, so it raises the cost of reading a profile — it
# does not make one confidential. For that, export with a passphrase.
#
# To use a private key:
#   openssl rand -hex 32 > desktop/profile_key.txt     (gitignored)
# then reconfigure. Without the file, the published development key in
# ProfileCipher.cpp is used, so a fresh clone builds and runs with no setup.
#
# NOTE: profiles are only interchangeable between builds sharing a key. A build
# with RRP's private key cannot import a profile exported by a stock build, and
# vice versa — which is usually the point.
set(RRP_PROFILE_KEY_FILE "${CMAKE_CURRENT_SOURCE_DIR}/profile_key.txt"
    CACHE FILEPATH "File holding 64 hex chars used as the .rrpprofile key")

set(RRP_PROFILE_KEY_DEFINITION "")
if(EXISTS "${RRP_PROFILE_KEY_FILE}")
    file(READ "${RRP_PROFILE_KEY_FILE}" RRP_PROFILE_KEY_RAW)
    # Strip whitespace ourselves: string(STRIP) leaves the CR of a CRLF file
    # behind, so a key written on Windows would fail the hex check below with a
    # thoroughly confusing message. Only whitespace is removed — anything else
    # still has to survive the strict check.
    string(REGEX REPLACE "[ \t\r\n]" "" RRP_PROFILE_KEY_RAW "${RRP_PROFILE_KEY_RAW}")
    # Length checked separately because CMake's regex engine has no bounded
    # repetition: "{64}" is matched literally, so the obvious one-line check
    # would reject every key, valid or not.
    string(LENGTH "${RRP_PROFILE_KEY_RAW}" RRP_PROFILE_KEY_LENGTH)
    if(RRP_PROFILE_KEY_LENGTH EQUAL 64 AND RRP_PROFILE_KEY_RAW MATCHES "^[0-9a-fA-F]+$")
        set(RRP_PROFILE_KEY_DEFINITION "RRP_PROFILE_KEY_HEX=\"${RRP_PROFILE_KEY_RAW}\"")
        message(STATUS "RRP: chave privada do .rrpprofile carregada de ${RRP_PROFILE_KEY_FILE}")
    else()
        # Failing loudly beats silently falling back: a typo in the key file
        # would otherwise produce binaries that can't read the profiles the
        # previous build wrote, with nothing in the log to say why.
        message(FATAL_ERROR
            "${RRP_PROFILE_KEY_FILE} deve conter exatamente 64 caracteres hexadecimais "
            "(32 bytes). Gere com: openssl rand -hex 32")
    endif()
else()
    message(STATUS "RRP: usando a chave de desenvolvimento publicada do .rrpprofile "
                    "(crie ${RRP_PROFILE_KEY_FILE} para usar uma privada)")
endif()

if(RRP_PROFILE_KEY_DEFINITION)
    add_compile_definitions(${RRP_PROFILE_KEY_DEFINITION})
endif()

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
#
# Only three subfolders of the SDK's share/ are needed at runtime — copying the
# whole thing drags in 274 MB of API documentation and the liblinphone/
# mediastreamer test suites, which would end up in the installer:
#   belr/     as gramáticas SIP/SDP/vCard (a ausência delas aborta o processo)
#   linphone/ rootca.pem, para TLS
#   sounds/   ringback e o toque de reserva
set(RRP_SDK_SHARE_DIRS belr linphone sounds)
if(LINPHONESDK_DIR AND EXISTS "${LINPHONESDK_DIR}/share")
    foreach(target_name RRPSoftphone sip_register_test)
        foreach(share_dir ${RRP_SDK_SHARE_DIRS})
            if(EXISTS "${LINPHONESDK_DIR}/share/${share_dir}")
                add_custom_command(TARGET ${target_name} POST_BUILD
                    COMMAND ${CMAKE_COMMAND} -E copy_directory
                        "${LINPHONESDK_DIR}/share/${share_dir}"
                        "$<TARGET_FILE_DIR:${target_name}>/share/${share_dir}"
                )
            endif()
        endforeach()
    endforeach()
endif()

# Our own ringtone. liblinphone plays the ring from a real file path, so it
# cannot come from the Qt resource bundle like the icons do — it has to sit on
# disk next to the executable.
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/resources/sounds")
    add_custom_command(TARGET RRPSoftphone POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_CURRENT_SOURCE_DIR}/resources/sounds"
            "$<TARGET_FILE_DIR:RRPSoftphone>/sounds"
    )
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

# --- Empacotamento ----------------------------------------------------------
# `cmake --install build-msvc --prefix dist` monta uma árvore limpa e pronta
# para rodar, que é o que o instalador (installer/RRPSoftphone.iss) empacota.
#
# Não dá para simplesmente zipar a pasta de build: ela tem centenas de MB de
# CMakeFiles/, *_autogen/ e a ferramenta de teste de registro, nada disso
# distribuível.
#
# Os arquivos vêm da pasta de build porque é lá que os passos de POST_BUILD
# acima (windeployqt, DLLs do SDK, share/, plugins/, sounds/) depositam tudo —
# o gerador é single-config (Ninja), então o diretório é previsível.
install(TARGETS RRPSoftphone RUNTIME DESTINATION .)

# Subpastas de runtime. OPTIONAL porque quais existem depende do que o
# windeployqt decidiu copiar para esta versão do Qt.
foreach(runtime_dir plugins sounds platforms styles imageformats
                     iconengines networkinformation tls generic)
    install(DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/${runtime_dir}/"
            DESTINATION "${runtime_dir}" OPTIONAL)
endforeach()

# Os .mkv do SDK ficam de fora: são os toques que o pacote Windows não consegue
# decodificar (sem plugin Matroska), então são 8 MB de arquivos inúteis.
install(DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/share/"
        DESTINATION "share" OPTIONAL
        PATTERN "*.mkv" EXCLUDE)

# As DLLs são coletadas na hora da instalação, e não listadas aqui: a relação
# muda conforme a versão do Qt e do SDK, e uma lista fixa envelheceria em
# silêncio — o app só falharia ao abrir na máquina do cliente.
install(CODE "
    file(GLOB _rrp_runtime_dlls \"${CMAKE_CURRENT_BINARY_DIR}/*.dll\")
    file(INSTALL \${_rrp_runtime_dlls} DESTINATION \"\${CMAKE_INSTALL_PREFIX}\")
")

# O app é compilado com /MD, então depende do runtime do Visual C++. Ele não é
# copiado como DLL solta: o instalador executa este redistribuível, que é a
# forma suportada pela Microsoft e mantém as atualizações de segurança do
# runtime a cargo do Windows Update.
if(EXISTS "${CMAKE_CURRENT_BINARY_DIR}/vc_redist.x64.exe")
    install(FILES "${CMAKE_CURRENT_BINARY_DIR}/vc_redist.x64.exe" DESTINATION "redist")
endif()
