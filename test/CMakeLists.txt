include(CTest)

# Set the C++ standard and other options
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD_INCLUDE_DIRECTORIES ${CMAKE_CXX_IMPLICIT_INCLUDE_DIRECTORIES})

include(../external/doctest/scripts/cmake/doctest.cmake)

# Add the executable for the tests
add_executable(bell-test
  main.cpp
  BinaryStreamTest.cpp
  VectorStreamTest.cpp
  SocketsTest.cpp
  URIParserTest.cpp
  HTTPReaderTest.cpp
  HTTPWriterTest.cpp
  SemaphoreTest.cpp
  TaskTest.cpp
  IpAddressTest.cpp
  RadixRouterTest.cpp
  HTTPClientTest.cpp
)

if(NOT BELL_DISABLE_MDNS)
    # Only include the test if the MDNS wrappers are enabled
    target_sources(bell-test PRIVATE MDNSWrappersTest.cpp)
endif()

# Set compile options for the test executable
target_compile_options(bell-test PRIVATE -Wall -Wextra -Werror)

# Link the doctest library to the test executable
target_link_libraries(bell-test PRIVATE bell doctest::doctest trompeloeil::trompeloeil)

# Discover and register tests using doctest's module
# doctest_discover_tests(bell-test)
