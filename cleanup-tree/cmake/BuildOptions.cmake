option(BELL_DISABLE_TESTS "Disable bell unit tests" ON)
option(BELL_RUN_CLANGTIDY "Run clang-tidy static analysis" OFF)

# Audio codecs
option(BELL_DISABLE_CODECS "Disable the entire audio codec wrapper" OFF)
option(BELL_CODEC_AAC "Support opencore-aac codec" ON)
option(BELL_CODEC_MP3 "Support libhelix-mp3 codec" ON)
option(BELL_CODEC_VORBIS "Support tremor Vorbis codec" ON)
option(BELL_CODEC_OPUS "Support Opus codec" ON)

# vorbis
set(BELL_EXTERNAL_VORBIS "" CACHE STRING "External Vorbis library target name, optional")
option(BELL_VORBIS_FLOAT "Use floating point Vorbis API" OFF)

# Extras
option(BELL_DISABLE_MQTT "Disable the built-in MQTT wrapper" ON)
option(BELL_DISABLE_WEBSERVER "Disable the built-in Web server" OFF)

# Audio sinks
option(BELL_DISABLE_SINKS "Disable all built-in audio sink implementations" OFF)
option(BELL_SINK_ALSA "Enable ALSA audio sink" OFF)
option(BELL_SINK_PORTAUDIO "Enable PortAudio sink" OFF)

# cJSON wrapper
option(BELL_ONLY_CJSON "Use only cJSON, not Nlohmann")
set(BELL_EXTERNAL_CJSON "" CACHE STRING "External cJSON library target name, optional")

# regex
option(BELL_DISABLE_REGEX "Don't use std::regex (saves space)" OFF)

message(STATUS "Bell options:")
message(STATUS "    Disable unit tests: ${BELL_DISABLE_TESTS}")
message(STATUS "    Disable all codecs: ${BELL_DISABLE_CODECS}")

if(NOT BELL_DISABLE_CODECS)
    message(STATUS "    - AAC audio codec: ${BELL_CODEC_AAC}")
    message(STATUS "    - MP3 audio codec: ${BELL_CODEC_MP3}")
    message(STATUS "    - Vorbis audio codec: ${BELL_CODEC_VORBIS}")
    message(STATUS "    - Opus audio codec: ${BELL_CODEC_OPUS}")
    message(STATUS "    - ALAC audio codec: ${BELL_CODEC_ALAC}")
endif()

message(STATUS "    Disable built-in audio sinks: ${BELL_DISABLE_SINKS}")
message(STATUS "    Use Vorbis float version: ${BELL_VORBIS_FLOAT}")

if(NOT BELL_DISABLE_SINKS)
    message(STATUS "    - ALSA sink: ${BELL_SINK_ALSA}")
    message(STATUS "    - PortAudio sink: ${BELL_SINK_PORTAUDIO}")
endif()

message(STATUS "    Use cJSON only: ${BELL_ONLY_CJSON}")
message(STATUS "    Disable Fmt: ${BELL_DISABLE_FMT}")
message(STATUS "    Disable Mqtt: ${BELL_DISABLE_MQTT}")
message(STATUS "    Disable Regex: ${BELL_DISABLE_REGEX}")
message(STATUS "    Disable Web server: ${BELL_DISABLE_WEBSERVER}")