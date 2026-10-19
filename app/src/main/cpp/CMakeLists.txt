cmake_minimum_required(VERSION 3.22.1)

project("Doom3Android")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ---------------------------------------------------------------------------
# Android target
# ---------------------------------------------------------------------------
if(NOT ANDROID)
    message(FATAL_ERROR "This project is currently configured for Android only.")
endif()

# Prevent host (Termux/AndroidIDE) pkg-config from injecting system include
# paths (dbus, etc.) into the NDK build — that breaks libc++ headers.
set(ENV{PKG_CONFIG_PATH} "")
set(ENV{PKG_CONFIG_LIBDIR} "")
set(PKG_CONFIG_EXECUTABLE "" CACHE FILEPATH "Disabled on Android NDK cross-build" FORCE)

set(D3_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/dhewm3/neo")
set(D3_USE_GL4ES ON CACHE BOOL "Use GL4ES as the OpenGL compatibility layer" FORCE)
set(D3_BUILD_D3XP OFF CACHE BOOL "Build Resurrection of Evil initially" FORCE)

set(CORE ON CACHE BOOL "" FORCE)
set(BASE ON CACHE BOOL "" FORCE)
set(D3XP ${D3_BUILD_D3XP} CACHE BOOL "" FORCE)
set(HARDLINK_GAME ON CACHE BOOL "" FORCE)
set(DEDICATED OFF CACHE BOOL "" FORCE)
set(IMGUI OFF CACHE BOOL "" FORCE)
set(SDL2 ON CACHE BOOL "" FORCE)
set(SDL3 OFF CACHE BOOL "" FORCE)
set(TOOLS OFF CACHE BOOL "" FORCE)
set(ONATIVE OFF CACHE BOOL "" FORCE)
set(TRACY OFF CACHE BOOL "" FORCE)
set(ID_ENABLE_CURL OFF CACHE BOOL "" FORCE)

# ---------------------------------------------------------------------------
# SDL2
# ---------------------------------------------------------------------------
if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/SDL2/CMakeLists.txt")
    message(FATAL_ERROR "SDL2 sources not found at ${CMAKE_CURRENT_SOURCE_DIR}/SDL2")
endif()
if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/SDL2/include/SDL_config.h.cmake")
    message(FATAL_ERROR
        "SDL2 tree is incomplete: missing include/SDL_config.h.cmake\n"
        "Copy a full SDL2 release into cpp/SDL2/")
endif()

set(SDL2_DISABLE_INSTALL ON CACHE BOOL "" FORCE)
set(SDL2_DISABLE_INSTALL_DOCS ON CACHE BOOL "" FORCE)
set(SDL2_DISABLE_INSTALL_HEADERS ON CACHE BOOL "" FORCE)
set(SDL2_DISABLE_INSTALL_CMAKE ON CACHE BOOL "" FORCE)
set(SDL2_TESTS OFF CACHE BOOL "" FORCE)
set(SDL2_TESTS_ENABLED_BY_DEFAULT OFF CACHE BOOL "" FORCE)

add_subdirectory(
    ${CMAKE_CURRENT_SOURCE_DIR}/SDL2
    ${CMAKE_CURRENT_BINARY_DIR}/SDL2
)

# ---------------------------------------------------------------------------
# GL4ES (local preferred)
# ---------------------------------------------------------------------------
set(GL4ES_INCLUDE_DIR "")

if(D3_USE_GL4ES)
    set(GL4ES_LOCAL "${CMAKE_CURRENT_SOURCE_DIR}/gl4es")

    set(ANDROID ON CACHE BOOL "" FORCE)
    set(NOX11 ON CACHE BOOL "" FORCE)
    set(NOEGL ON CACHE BOOL "" FORCE)
    set(STATICLIB ON CACHE BOOL "" FORCE)
    set(NO_LOADER OFF CACHE BOOL "" FORCE)
    set(NO_INIT_CONSTRUCTOR ON CACHE BOOL "" FORCE)
    set(DEFAULT_ES 2 CACHE STRING "" FORCE)
    set(USE_ANDROID_LOG ON CACHE BOOL "" FORCE)
    set(EGL_WRAPPER OFF CACHE BOOL "" FORCE)
    set(GBM OFF CACHE BOOL "" FORCE)

    if(EXISTS "${GL4ES_LOCAL}/CMakeLists.txt")
        message(STATUS "Using local GL4ES at ${GL4ES_LOCAL}")
        add_subdirectory(${GL4ES_LOCAL} ${CMAKE_CURRENT_BINARY_DIR}/gl4es)
        set(GL4ES_INCLUDE_DIR "${GL4ES_LOCAL}/include")
    else()
        message(FATAL_ERROR
            "cpp/gl4es not found.\n"
            "Download https://github.com/ptitSeb/gl4es and place it at cpp/gl4es/")
    endif()

    if(NOT TARGET GL)
        message(FATAL_ERROR "GL4ES did not create the expected GL static target")
    endif()
    message(STATUS "GL4ES include dir: ${GL4ES_INCLUDE_DIR}")
endif()

# ---------------------------------------------------------------------------
# OpenAL Soft (local preferred)
# Only Android-friendly backends — no host pkg-config deps.
# ---------------------------------------------------------------------------
set(OPENAL_LOCAL "${CMAKE_CURRENT_SOURCE_DIR}/openal-soft")

set(ALSOFT_UTILS OFF CACHE BOOL "" FORCE)
set(ALSOFT_EXAMPLES OFF CACHE BOOL "" FORCE)
set(ALSOFT_TESTS OFF CACHE BOOL "" FORCE)
set(ALSOFT_INSTALL OFF CACHE BOOL "" FORCE)
set(ALSOFT_INSTALL_CONFIG OFF CACHE BOOL "" FORCE)
set(ALSOFT_INSTALL_HRTF_DATA OFF CACHE BOOL "" FORCE)
set(ALSOFT_INSTALL_AMBDEC_PRESETS OFF CACHE BOOL "" FORCE)
set(ALSOFT_INSTALL_EXAMPLES OFF CACHE BOOL "" FORCE)
set(ALSOFT_INSTALL_UTILS OFF CACHE BOOL "" FORCE)
set(ALSOFT_EAX ON CACHE BOOL "" FORCE)
set(LIBTYPE "STATIC" CACHE STRING "" FORCE)

# Disable every backend that pulls host libraries via pkg-config
set(ALSOFT_BACKEND_PIPEWIRE OFF CACHE BOOL "" FORCE)
set(ALSOFT_BACKEND_PULSEAUDIO OFF CACHE BOOL "" FORCE)
set(ALSOFT_BACKEND_ALSA OFF CACHE BOOL "" FORCE)
set(ALSOFT_BACKEND_OSS OFF CACHE BOOL "" FORCE)
set(ALSOFT_BACKEND_SOLARIS OFF CACHE BOOL "" FORCE)
set(ALSOFT_BACKEND_SNDIO OFF CACHE BOOL "" FORCE)
set(ALSOFT_BACKEND_JACK OFF CACHE BOOL "" FORCE)
set(ALSOFT_BACKEND_COREAUDIO OFF CACHE BOOL "" FORCE)
set(ALSOFT_BACKEND_WASAPI OFF CACHE BOOL "" FORCE)
set(ALSOFT_BACKEND_DSOUND OFF CACHE BOOL "" FORCE)
set(ALSOFT_BACKEND_WINMM OFF CACHE BOOL "" FORCE)
set(ALSOFT_BACKEND_OTHERIO OFF CACHE BOOL "" FORCE)
set(ALSOFT_BACKEND_PORTAUDIO OFF CACHE BOOL "" FORCE)
set(ALSOFT_BACKEND_SDL3 OFF CACHE BOOL "" FORCE)
set(ALSOFT_BACKEND_SDL2 OFF CACHE BOOL "" FORCE)
set(ALSOFT_BACKEND_WAVE ON CACHE BOOL "" FORCE)
set(ALSOFT_BACKEND_NULL ON CACHE BOOL "" FORCE)
set(ALSOFT_BACKEND_OPENSL ON CACHE BOOL "" FORCE)
set(ALSOFT_BACKEND_OBOE OFF CACHE BOOL "" FORCE)
set(ALSOFT_REQUIRE_OPENSL OFF CACHE BOOL "" FORCE)

if(EXISTS "${OPENAL_LOCAL}/CMakeLists.txt")
    message(STATUS "Using local OpenAL Soft at ${OPENAL_LOCAL}")
    add_subdirectory(${OPENAL_LOCAL} ${CMAKE_CURRENT_BINARY_DIR}/openal-soft)
else()
    message(FATAL_ERROR
        "cpp/openal-soft not found.\n"
        "Download https://github.com/kcat/openal-soft and place it at cpp/openal-soft/")
endif()

if(NOT TARGET OpenAL::OpenAL)
    message(FATAL_ERROR "OpenAL Soft did not create OpenAL::OpenAL")
endif()

# ---------------------------------------------------------------------------
# dhewm3
# ---------------------------------------------------------------------------
set(DHEWM3BINARY "doom3" CACHE STRING "" FORCE)

# NOTE: do NOT use global include_directories(GL4ES) — it pollutes OpenAL/SDL
# and can break libc++ header resolution on NDK.
if(D3_USE_GL4ES)
    add_compile_definitions(DHEWM3_USE_GL4ES=1)
endif()

find_library(ZLIB_LIBRARY z)

set(CMAKE_POSITION_INDEPENDENT_CODE ON)

add_subdirectory(
    ${D3_ROOT}
    ${CMAKE_CURRENT_BINARY_DIR}/dhewm3
)

# ---------------------------------------------------------------------------
# Final Android shared library linkage
# dhewm3 CMake creates the target named "dhewm3" (DHEWM3BINARY is only the
# output name on desktop; the CMake target stays "dhewm3").
#
# IMPORTANT: dhewm3/neo/CMakeLists.txt calls target_link_libraries() on this
# target using the PLAIN signature (no PRIVATE/PUBLIC/INTERFACE keywords).
# CMake forbids mixing plain and keyword signatures on the same target, so
# every call below MUST also be plain (no keywords).
# ---------------------------------------------------------------------------
if(TARGET dhewm3)
    set(_D3_TGT dhewm3)
elseif(TARGET doom3)
    set(_D3_TGT doom3)
else()
    message(FATAL_ERROR "Neither dhewm3 nor doom3 target was created")
endif()

message(STATUS "Linking Android deps onto target: ${_D3_TGT}")

# SDLActivity loads System.loadLibrary("doom3") → needs libdoom3.so
# (CMake target is still "dhewm3", only the output filename changes)
set_target_properties(${_D3_TGT} PROPERTIES OUTPUT_NAME "doom3")

target_link_libraries(
    ${_D3_TGT}
    SDL2::SDL2
    OpenAL::OpenAL
    android
    log
    GLESv2
    EGL
    dl
    m
    OpenSLES
)

if(TARGET GL)
    target_link_libraries(${_D3_TGT} GL)
endif()

if(ZLIB_LIBRARY)
    target_link_libraries(${_D3_TGT} ${ZLIB_LIBRARY})
endif()

# These are NOT target_link_libraries, so PRIVATE is fine here.
target_compile_definitions(
    ${_D3_TGT}
    PRIVATE
    __ANDROID__
    ANDROID
    DHEWM3_ANDROID=1
    DHEWM3_USE_GL4ES=1
)

target_include_directories(
    ${_D3_TGT}
    PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/SDL2/include
    ${CMAKE_CURRENT_SOURCE_DIR}/dhewm3/neo
)

if(GL4ES_INCLUDE_DIR)
    target_include_directories(${_D3_TGT} PRIVATE ${GL4ES_INCLUDE_DIR})
    message(STATUS "GL4ES includes for ${_D3_TGT}: ${GL4ES_INCLUDE_DIR}")
endif()

if(TARGET idlib)
    target_compile_definitions(idlib PRIVATE DHEWM3_USE_GL4ES=1 __ANDROID__ ANDROID)
    if(GL4ES_INCLUDE_DIR)
        target_include_directories(idlib PRIVATE ${GL4ES_INCLUDE_DIR})
    endif()
endif()

# ---------------------------------------------------------------------------
# Linker workaround: ld.lld aborts on-device when linking the huge debug
# binary. Single-threaded link + stripped debug info lowers memory use.
# (target_link_options is NOT target_link_libraries, so no style conflict.)
# ---------------------------------------------------------------------------
target_link_options(${_D3_TGT} PRIVATE
    -Wl,--threads=1
    -Wl,--strip-debug
)
