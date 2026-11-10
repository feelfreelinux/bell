# Enable testing
if(NOT BELL_DISABLE_TESTS)
    # Trompeloeil for mocking
    add_subdirectory(external/trompeloeil)
    # Doctest for unit testing
    add_subdirectory(external/doctest)
    enable_testing()
    add_subdirectory(test)
endif()

# All includes are referenced from the root directory as "bell/"
list(APPEND BELL_INCLUDES "include")

# Main library sources
file(GLOB BELL_SOURCES
    "main/io/*.cpp" # bell::io
    "main/net/*.cpp" # bell::net
    "main/http/*.cpp" # bell::http
    "main/utils/*.cpp" # bell::utils
    "main/audio/*.cpp" # bell::audio
    "main/dsp/*.cpp" # bell::dsp
    "main/dsp/nodes/*.cpp" # bell::dsp nodes
)

# Add platform-specific sources
if(APPLE)
    file(GLOB BELL_SOURCES_APPLE "main/platform/apple/*.cpp")
    list(APPEND BELL_SOURCES ${BELL_SOURCES_APPLE})
endif()

# Unix common includes
if(UNIX)
    file(GLOB BELL_SOURCES_POSIX "main/platform/posix/*.cpp")
    list(APPEND BELL_SOURCES ${BELL_SOURCES_POSIX})
endif()

# Linux includes
if (UNIX AND NOT APPLE)
    file(GLOB BELL_SOURCES_POSIX "main/platform/linux/*.cpp")
    list(APPEND BELL_SOURCES ${BELL_SOURCES_POSIX})
endif()

# Espressif includes
if (ESP_PLATFORM)
    file(GLOB BELL_SOURCES_ESP "main/platform/esp/*.cpp" "main/platform/esp/*.S")
    list(APPEND BELL_SOURCES ${BELL_SOURCES_ESP})
endif()
