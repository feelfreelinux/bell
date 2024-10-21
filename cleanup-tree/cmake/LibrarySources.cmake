# Enable testing
if(NOT BELL_DISABLE_TESTS)
    enable_testing()
    add_subdirectory(test)
endif()

set(BELL_IO_DIR "${CMAKE_CURRENT_SOURCE_DIR}/main/io")

list(APPEND BELL_INCLUDES "main/io/include")

# Main library sources
file(GLOB BELL_SOURCES
    "main/io/*.cpp" # bell::io
)