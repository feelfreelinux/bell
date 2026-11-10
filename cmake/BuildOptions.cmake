option(BELL_DISABLE_TESTS "Disable bell unit tests" ON)
option(BELL_RUN_CLANGTIDY "Run clang-tidy static analysis" OFF)
option(BELL_DISABLE_SANITIZERS "Disable sanitizers" ON)

# Audio containers
option(BELL_DISABLE_CONTAINERS "Disable the entire audio container wrapper" OFF)
option(BELL_CONTAINER_OGG "Support OGG container" ON)

# Audio codecs
option(BELL_DISABLE_CODECS "Disable the entire audio codec wrapper" OFF)
option(BELL_CODEC_AAC "Support fdk-aac codec" ON)
option(BELL_CODEC_MP3 "Support libhelix-mp3 codec" ON)
option(BELL_CODEC_TREMOR "Support tremor Vorbis codec" ON)
option(BELL_CODEC_OPUS "Support Opus codec" ON)
option(BELL_CODEC_LC3PLUS "Support LC3plus codec" ON)
option(BELL_CODEC_HQLC "Support HQLC codec" ON)

# Vorbis
set(BELL_EXTERNAL_VORBIS "" CACHE STRING "External Vorbis library target name, optional")
option(BELL_VORBIS_FLOAT "Use floating point Vorbis API" OFF)

# Extras
option(BELL_DISABLE_MQTT "Disable the built-in MQTT wrapper" OFF)

# Audio sinks
option(BELL_BACKEND_PORTAUDIO "Enable PortAudio output backend" OFF)

# DSP
option(BELL_DSP_FLOAT_FILTERS "Run DSP filter sections in single-precision float (needs a hardware FPU)" OFF)

# Misc
option(BELL_DISABLE_TAOJSON "Don't include TaoJSON" OFF)
option(BELL_DISABLE_MBEDTLS "Don't include MbedTLS" OFF)
option(BELL_DISABLE_MDNS "Don't include MDNS (Bonjour, Avahi) wrappers" OFF)

set(BELL_EXTERNAL_MBEDTLS "" CACHE STRING "External mbedtls library target name, optional")

message(STATUS "Bell options:")
message(STATUS "    Disable unit tests: ${BELL_DISABLE_TESTS}")
message(STATUS "    Run clang-tidy: ${BELL_RUN_CLANGTIDY}")
message(STATUS "    Disable sanitizers: ${BELL_DISABLE_SANITIZERS}")
message(STATUS "    Disable all containers: ${BELL_DISABLE_CONTAINERS}")

if(NOT BELL_DISABLE_CONTAINERS)
    message(STATUS "  * OGG audio container: ${BELL_CONTAINER_OGG}")
endif()

message(STATUS "    Disable all codecs: ${BELL_DISABLE_CODECS}")

if(NOT BELL_DISABLE_CODECS)
    message(STATUS "  * AAC audio codec: ${BELL_CODEC_AAC}")
    message(STATUS "  * MP3 audio codec: ${BELL_CODEC_MP3}")
    message(STATUS "  * Tremor (vorbis) audio codec: ${BELL_CODEC_TREMOR}")
    message(STATUS "    Use Vorbis float version: ${BELL_VORBIS_FLOAT}")
    message(STATUS "  * Opus audio codec: ${BELL_CODEC_OPUS}")
    message(STATUS "  * LC3plus audio codec: ${BELL_CODEC_LC3PLUS}")
    message(STATUS "  * HQLC audio codec: ${BELL_CODEC_HQLC}")
endif()

message(STATUS " Enable PortAudio backend: ${BELL_PORTAUDIO_BACKEND}")

message(STATUS "  DSP float filters: ${BELL_DSP_FLOAT_FILTERS}")
message(STATUS "  Disable TaoJSON: ${BELL_DISABLE_TAOJSON}")
message(STATUS "  Disable MQTT: ${BELL_DISABLE_MQTT}")
message(STATUS "  Disable MbedTLS: ${BELL_DISABLE_MBEDTLS}")
message(STATUS "  Disable MDNS: ${BELL_DISABLE_MDNS}")
