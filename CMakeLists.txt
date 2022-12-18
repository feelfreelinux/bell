cmake_minimum_required(VERSION 2.8.12)
cmake_policy(SET CMP0077 NEW)

project(bell)

# Configurable options
option(BELL_DISABLE_CODECS "Disable the entire audio codec wrapper" OFF)
option(BELL_CODEC_AAC "Support libhelix-aac codec" ON)
option(BELL_CODEC_MP3 "Support libhelix-mp3 codec" ON)
option(BELL_CODEC_VORBIS "Support tremor Vorbis codec" ON)
option(BELL_CODEC_ALAC "Support Apple ALAC codec" ON)
option(BELL_CODEC_OPUS "Support Opus codec" ON)
option(BELL_DISABLE_SINKS "Disable all built-in audio sink implementations" OFF)

# These are default OFF, as they're OS-dependent (ESP32 sinks are always enabled - no external deps)
option(BELL_SINK_ALSA "Enable ALSA audio sink" OFF)
option(BELL_SINK_PORTAUDIO "Enable PortAudio sink" OFF)

# cJSON wrapper
option(BELL_DISABLE_CJSON "Disable cJSON and JSONObject completely" OFF)
set(BELL_EXTERNAL_CJSON "" CACHE STRING "External cJSON library target name, optional")

# disable json tests
set(JSON_BuildTests OFF CACHE INTERNAL "")

if(BELL_EXTERNAL_MBEDTLS)
    set(MbedTLS_DIR ${BELL_EXTERNAL_MBEDTLS})
    message(STATUS "Setting local mbedtls ${MbedTLS_DIR}")
endif()

# Backwards compatibility with deprecated options
if(BELL_USE_ALSA)
    message(WARNING "Deprecated Bell options used, replace BELL_USE_ALSA with BELL_SINK_ALSA")
    set(BELL_SINK_ALSA ${BELL_USE_ALSA})
endif()

if(BELL_USE_PORTAUDIO)
    message(WARNING "Deprecated Bell options used, replace BELL_USE_PORTAUDIO with BELL_SINK_PORTAUDIO")
    set(BELL_SINK_PORTAUDIO ${BELL_USE_PORTAUDIO})
endif()

message(STATUS "Bell options:")
message(STATUS "    Disable all codecs: ${BELL_DISABLE_CODECS}")

if(NOT BELL_DISABLE_CODECS)
    message(STATUS "    - AAC audio codec: ${BELL_CODEC_AAC}")
    message(STATUS "    - MP3 audio codec: ${BELL_CODEC_MP3}")
    message(STATUS "    - Vorbis audio codec: ${BELL_CODEC_VORBIS}")
    message(STATUS "    - Opus audio codec: ${BELL_CODEC_OPUS}")
    message(STATUS "    - ALAC audio codec: ${BELL_CODEC_ALAC}")
endif()

message(STATUS "    Disable built-in audio sinks: ${BELL_DISABLE_SINKS}")

if(NOT BELL_DISABLE_SINKS)
    message(STATUS "    - ALSA sink: ${BELL_SINK_ALSA}")
    message(STATUS "    - PortAudio sink: ${BELL_SINK_PORTAUDIO}")
endif()

message(STATUS "    Disable cJSON and JSONObject: ${BELL_DISABLE_CJSON}")

# Include nanoPB library
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_CURRENT_SOURCE_DIR}/external/nanopb/extra")
find_package(Nanopb REQUIRED)
message(${NANOPB_INCLUDE_DIRS})
list(APPEND EXTRA_INCLUDES ${NANOPB_INCLUDE_DIRS})

# CMake options
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

set(AUDIO_CODEC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/main/audio-codec")
set(AUDIO_CONTAINERS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/main/audio-containers")
set(AUDIO_DSP_DIR "${CMAKE_CURRENT_SOURCE_DIR}/main/audio-dsp")
set(AUDIO_SINKS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/main/audio-sinks")
set(IO_DIR "${CMAKE_CURRENT_SOURCE_DIR}/main/io")
set(PLATFORM_DIR "${CMAKE_CURRENT_SOURCE_DIR}/main/platform")
set(UTILITIES_DIR "${CMAKE_CURRENT_SOURCE_DIR}/main/utilities")

add_definitions("-DUSE_DEFAULT_STDLIB=1")

# Main library sources
file(GLOB SOURCES
    "external/nanopb/*.c"
    "main/utilities/*.cpp" "main/utilities/*.c"
    "main/io/*.cpp" "main/io/*.c"
    "main/audio-codec/*.cpp" "main/audio-codec/*.c"
    "main/audio-dsp/*.cpp" "main/audio-dsp/*.c"
)

list(APPEND EXTRA_INCLUDES "main/audio-codec/include")
list(APPEND EXTRA_INCLUDES "main/audio-dsp/include")
list(APPEND EXTRA_INCLUDES "main/audio-sinks/include")
list(APPEND EXTRA_INCLUDES "main/io/include")
list(APPEND EXTRA_INCLUDES "main/utilities/include")
list(APPEND EXTRA_INCLUDES "main/platform")

# Add platform specific sources
if(ESP_PLATFORM)
    file(GLOB ESP_PLATFORM_SOURCES "main/platform/esp/*.cpp" "main/platform/esp/*.c" "main/asm/biquad_f32_ae32.S")
    list(APPEND SOURCES ${ESP_PLATFORM_SOURCES})
endif()

if(APPLE)
    file(GLOB APPLE_PLATFORM_SOURCES "main/platform/apple/*.cpp" "main/platform/apple/*.c")
    list(APPEND SOURCES ${APPLE_PLATFORM_SOURCES})
    list(APPEND EXTRA_INCLUDES "/usr/local/opt/mbedtls@3/include")
endif()

if(UNIX AND NOT APPLE)
    file(GLOB LINUX_PLATFORM_SOURCES "main/platform/linux/*.cpp" "main/platform/linux/*.c")
    list(APPEND SOURCES ${LINUX_PLATFORM_SOURCES})
endif()

if(WIN32)
    file(GLOB WIN32_PLATFORM_SOURCES "main/platform/win32/*.cpp" "main/platform/win32/*.c")
    list(APPEND SOURCES ${WIN32_PLATFORM_SOURCES})
    list(APPEND EXTRA_INCLUDES "main/platform/include/win32")
endif()

# A hack to make Opus keep quiet
function(message)
    if(NOT MESSAGE_QUIET)
        _message(${ARGN})
    endif()
endfunction()


if(ESP_PLATFORM)
    list(APPEND EXTRA_LIBS idf::mbedtls idf::pthread idf::mdns)
    add_definitions(-Wunused-const-variable -Wchar-subscripts -Wunused-label -Wmaybe-uninitialized -Wmisleading-indentation)
else()
    find_package(Threads REQUIRED)
    find_package(MbedTLS REQUIRED)
    list(APPEND EXTRA_INCLUDES ${MBEDTLS_INCLUDE_DIRS})
    set(THREADS_PREFER_PTHREAD_FLAG ON)
    list(APPEND EXTRA_LIBS ${MBEDTLS_LIBRARIES} Threads::Threads)

    # try to handle mbedtls when not system-wide installed
    if(BELL_EXTERNAL_MBEDTLS)
        if(MSVC)
            set(MBEDTLS_RELEASE "RELEASE" CACHE STRING "local mbedtls version")
        else()
            set(MBEDTLS_RELEASE "NOCONFIG" CACHE STRING "local mbedtls version")
        endif()

        message(STATUS "using local mbedtls version ${MBEDTLS_RELEASE}")
        get_target_property(MBEDTLS_INFO MbedTLS::mbedtls IMPORTED_LOCATION_${MBEDTLS_RELEASE})
        list(APPEND EXTRA_LIBS ${MBEDTLS_INFO})
        get_target_property(MBEDTLS_INFO MbedTLS::mbedx509 IMPORTED_LOCATION_${MBEDTLS_RELEASE})
        list(APPEND EXTRA_LIBS ${MBEDTLS_INFO})
        get_target_property(MBEDTLS_INFO MbedTLS::mbedcrypto IMPORTED_LOCATION_${MBEDTLS_RELEASE})
        list(APPEND EXTRA_LIBS ${MBEDTLS_INFO})
    else()
        find_package(MbedTLS REQUIRED)
        list(APPEND EXTRA_INCLUDES ${MBEDTLS_INCLUDE_DIRS})
        list(APPEND EXTRA_LIBS ${MBEDTLS_LIBRARIES})
    endif()

    if(MSVC)
        add_compile_definitions(NOMINMAX _CRT_SECURE_NO_WARNINGS)
        add_definitions(/wd4068 /wd4244 /wd4018 /wd4101 /wd4102 /wd4142)
    endif()
endif()

if(NOT BELL_DISABLE_CODECS)
    file(GLOB EXTRA_SOURCES "main/audio-decoders/*.cpp")

    list(APPEND SOURCES "${EXTRA_SOURCES}")
    list(APPEND SOURCES "${AUDIO_CODEC_DIR}/DecoderGlobals.cpp")
    list(APPEND SOURCES "${AUDIO_CODEC_DIR}/BaseCodec.cpp")
    list(APPEND SOURCES "${AUDIO_CODEC_DIR}/AudioCodecs.cpp")
    list(APPEND EXTRA_INCLUDES "main/audio-containers/include")

    # AAC-LC codec
    if(BELL_CODEC_AAC)
        file(GLOB LIBHELIX_AAC_SOURCES "external/libhelix-aac/*.c")
        list(APPEND LIBHELIX_SOURCES ${LIBHELIX_AAC_SOURCES})
        list(APPEND EXTRA_INCLUDES "external/libhelix-aac")
        list(APPEND SOURCES "${AUDIO_CODEC_DIR}/AACDecoder.cpp")
        list(APPEND CODEC_FLAGS "-DBELL_CODEC_AAC")
    endif()

    # MP3 codec
    if(BELL_CODEC_MP3)
        file(GLOB LIBHELIX_MP3_SOURCES "external/libhelix-mp3/*.c")
        list(APPEND LIBHELIX_SOURCES ${LIBHELIX_MP3_SOURCES})
        list(APPEND EXTRA_INCLUDES "external/libhelix-mp3")
        list(APPEND SOURCES "${AUDIO_CODEC_DIR}/MP3Decoder.cpp")
        list(APPEND CODEC_FLAGS "-DBELL_CODEC_MP3")
    endif()

    # MP3 codec
    # if(BELL_CODEC_ALAC)
    #     file(GLOB ALAC_SOURCES "external/alac/*.c" "external/alac/*.cpp")
    #     list(APPEND ALAC_SOURCES ${ALAC_SOURCES})
    #     list(APPEND EXTRA_INCLUDES "external/alac")

    #     # list(APPEND SOURCES "${AUDIO_DIR}/codec/ALACDecoder.cpp")
    #     list(APPEND CODEC_FLAGS "-DBELL_CODEC_ALAC")
    # endif()

    # libhelix Cygwin workaround
    if(CYGWIN)
        # Both Cygwin and ESP are Unix-like so this seems to work (or, at least, compile)
        set_source_files_properties("${AUDIO_CODEC_DIR}/DecoderGlobals.cpp" ${LIBHELIX_SOURCES} PROPERTIES COMPILE_FLAGS "-DESP_PLATFORM")
    endif()

    list(APPEND SOURCES ${LIBHELIX_SOURCES})
    list(APPEND SOURCES ${ALAC_SOURCES})

    # Vorbis codec
    if(BELL_CODEC_VORBIS)
        file(GLOB TREMOR_SOURCES "external/tremor/*.c")
        list(REMOVE_ITEM TREMOR_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/external/tremor/ivorbisfile_example.c")
        list(APPEND SOURCES ${TREMOR_SOURCES})
        list(APPEND EXTRA_INCLUDES "external/tremor")
        list(APPEND SOURCES "${AUDIO_CODEC_DIR}/VorbisDecoder.cpp")
        list(APPEND CODEC_FLAGS "-DBELL_CODEC_VORBIS")
    endif()

    # Opus codec
    if(BELL_CODEC_OPUS)
        set(OPUS_INSTALL_CMAKE_CONFIG_MODULE OFF CACHE BOOL "")
        set(OPUS_INSTALL_CMAKE_CONFIG_MODULE OFF)
        set(OPUS_INSTALL_PKG_CONFIG_MODULE OFF CACHE BOOL "")
        set(OPUS_INSTALL_PKG_CONFIG_MODULE OFF)
        set(MESSAGE_QUIET ON)
        add_subdirectory("external/opus")
        unset(MESSAGE_QUIET)
        target_compile_options(opus PRIVATE "-O3")
        list(APPEND EXTRA_LIBS Opus::opus)
        list(APPEND SOURCES "${AUDIO_CODEC_DIR}/OPUSDecoder.cpp")
        list(APPEND CODEC_FLAGS -DBELL_CODEC_OPUS)
    endif()

    # Enable global codecs
    string(REPLACE ";" " " CODEC_FLAGS "${CODEC_FLAGS}")
    set_source_files_properties("${AUDIO_CODEC_DIR}/AudioCodecs.cpp" PROPERTIES COMPILE_FLAGS "${CODEC_FLAGS}")
elseif(BELL_EXTERNAL_TREMOR)
    list(APPEND EXTRA_LIBS ${BELL_EXTERNAL_TREMOR})
endif()

if(NOT BELL_DISABLE_SINKS)
    set(PLATFORM "unix")

    if(ESP_PLATFORM)
        set(PLATFORM "esp")
    endif()

    # Add all built-in audio sinks
    file(GLOB SINK_SOURCES "${AUDIO_SINKS_DIR}/${PLATFORM}/*.cpp" "${AUDIO_SINKS_DIR}/${PLATFORM}/*.c")
    list(APPEND EXTRA_INCLUDES "main/audio-sinks/include/${PLATFORM}")

    # Find ALSA if required, else remove the sink
    if(BELL_SINK_ALSA)
        find_package(ALSA REQUIRED)
        list(APPEND EXTRA_INCLUDES ${ALSA_INCLUDE_DIRS})
        list(APPEND EXTRA_LIBS ${ALSA_LIBRARIES})
    else()
        list(REMOVE_ITEM SINK_SOURCES "${AUDIO_SINKS_DIR}/unix/ALSAAudioSink.cpp")
    endif()

    # Find PortAudio if required, else remove the sink
    if(BELL_SINK_PORTAUDIO)
        if(WIN32)
            list(APPEND EXTRA_INCLUDES "external/portaudio/include")

            if(NOT "${CMAKE_GENERATOR}" MATCHES "(Win64|IA64)")
                list(APPEND EXTRA_LIBS "${CMAKE_CURRENT_SOURCE_DIR}/external/portaudio/portaudio_win32.lib")
            else()
                list(APPEND EXTRA_LIBS "${CMAKE_CURRENT_SOURCE_DIR}/external/portaudio/portaudio_x64.lib")
            endif()
        else()
            find_package(portaudio REQUIRED)
            list(APPEND EXTRA_INCLUDES ${PORTAUDIO_INCLUDE_DIRS})
            list(APPEND EXTRA_LIBS ${PORTAUDIO_LIBRARIES})
        endif()
    else()
        list(REMOVE_ITEM SINK_SOURCES "${AUDIO_SINKS_DIR}/unix/PortAudioSink.cpp")
    endif()

    list(APPEND SOURCES ${SINK_SOURCES})
endif()

if(BELL_DISABLE_CJSON)
    list(REMOVE_ITEM SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/main/io/JSONObject.cpp")
else()
    add_subdirectory(external/nlohmann_json)
    list(APPEND EXTRA_LIBS nlohmann_json::nlohmann_json)
    if(BELL_EXTERNAL_CJSON)
        list(APPEND EXTRA_LIBS ${BELL_EXTERNAL_CJSON})
    else()
        list(APPEND SOURCES "external/cJSON/cJSON.c")
        list(APPEND EXTRA_INCLUDES "external/cJSON")
    endif()
endif()

if (BELL_DISABLE_FMT)
else()
    add_subdirectory(external/fmt)
    list(APPEND EXTRA_LIBS fmt::fmt)
endif()

file(GLOB CIVET_SRC "external/civetweb/*.c" "external/civetweb/*.inl" "external/civetweb/*.cpp")
file(GLOB COREHTTP_SRC "external/coreHTTP/*.c")

list(APPEND SOURCES ${CIVET_SRC} ${COREHTTP_SRC})
list(APPEND EXTRA_INCLUDES "external/civetweb/include" "external/coreHTTP")


add_library(bell STATIC ${SOURCES})

# PUBLIC to propagate esp-idf includes to bell dependents
target_link_libraries(bell PUBLIC ${EXTRA_LIBS})
target_include_directories(bell PUBLIC ${EXTRA_INCLUDES} ${CMAKE_CURRENT_BINARY_DIR})
target_compile_definitions(bell PUBLIC PB_ENABLE_MALLOC)

if(WIN32)
    target_compile_definitions(bell PUBLIC PB_NO_STATIC_ASSERT)
endif()
