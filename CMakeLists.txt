cmake_minimum_required(VERSION 3.22)

if(APPLE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS deployment target" FORCE)
    set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "Build Universal 2 binary (Apple Silicon + Intel)" FORCE)
endif()

project(DAroom VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.6
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(JUCE)

juce_add_binary_data(DAroomAssets
    SOURCES
        Assets/DAroom_GUI.png
)

juce_add_plugin(DAroom
    COMPANY_NAME "D.A Production"
    BUNDLE_ID "audio.daroom.reverb"
    PLUGIN_MANUFACTURER_CODE DrAm
    PLUGIN_CODE DArm
    FORMATS VST3 AU Standalone
    PRODUCT_NAME "D.A Room"
    DESCRIPTION "D.A Room algorithmic reverb"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
)

juce_generate_juce_header(DAroom)

target_sources(DAroom PRIVATE
    Source/FactoryPresets.h
    Source/PluginProcessor.cpp
    Source/PluginProcessor.h
    Source/PluginEditor.cpp
    Source/PluginEditor.h
)

target_compile_definitions(DAroom PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
)

target_link_libraries(DAroom PRIVATE
    DAroomAssets
    juce::juce_audio_utils
    juce::juce_dsp
    PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags
)

# -----------------------------------------------------------------------------
# Native Windows 64-bit Custom GUI Installer (.EXE -> C:\Program Files\Common Files\VST3)
# -----------------------------------------------------------------------------
if(WIN32 AND TARGET DAroom_VST3)
    add_executable(PackPayload
        Installer/PackPayload.cpp
    )
    target_link_libraries(PackPayload PRIVATE Cabinet)

    get_target_property(DAROOM_VST3_ARTEFACT DAroom_VST3 JUCE_PLUGIN_ARTEFACT_FILE)
    set(INSTALLER_PAYLOAD_BIN "${CMAKE_CURRENT_BINARY_DIR}/installer_payload.bin")

    add_custom_command(
        OUTPUT "${INSTALLER_PAYLOAD_BIN}"
        COMMAND $<TARGET_FILE:PackPayload>
                "${DAROOM_VST3_ARTEFACT}"
                "${CMAKE_CURRENT_SOURCE_DIR}/Presets"
                "${INSTALLER_PAYLOAD_BIN}"
        DEPENDS DAroom_VST3 PackPayload
        COMMENT "Packing D.A Room.vst3 and 50 Factory Presets into compressed installer payload..."
        VERBATIM
    )
    add_custom_target(GenerateInstallerPayload DEPENDS "${INSTALLER_PAYLOAD_BIN}")

    file(TO_CMAKE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/Installer/Assets/installer.ico" INSTALLER_ICON_PATH)
    file(TO_CMAKE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/Installer/Assets/installer_welcome.png" INSTALLER_WELCOME_PATH)
    file(TO_CMAKE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/Installer/Assets/installer_base.png" INSTALLER_BASE_PATH)
    file(TO_CMAKE_PATH "${INSTALLER_PAYLOAD_BIN}" INSTALLER_PAYLOAD_PATH)

    set(GENERATED_INSTALLER_RC "${CMAKE_CURRENT_BINARY_DIR}/installer_generated.rc")
    configure_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/Installer/installer.rc.in"
        "${GENERATED_INSTALLER_RC}"
        @ONLY
    )

    add_executable(DA_Production_Installer WIN32
        Installer/InstallerMain.cpp
        Installer/resource.h
        "${GENERATED_INSTALLER_RC}"
    )
    add_dependencies(DA_Production_Installer GenerateInstallerPayload)
    target_include_directories(DA_Production_Installer PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/Installer")
    target_link_libraries(DA_Production_Installer PRIVATE
        gdiplus
        Cabinet
        ole32
        shell32
        user32
        gdi32
        advapi32
    )
    set_target_properties(DA_Production_Installer PROPERTIES
        OUTPUT_NAME "D.A Production Installer"
    )
    if(MSVC)
        target_link_options(DA_Production_Installer PRIVATE
            /MANIFEST:EMBED
            "/MANIFESTUAC:level='requireAdministrator' uiAccess='false'"
        )
    endif()
endif()
