include(CTest)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD_INCLUDE_DIRECTORIES ${CMAKE_CXX_IMPLICIT_INCLUDE_DIRECTORIES})

find_package(Catch2 REQUIRED)
include(Catch)

add_executable(bell-test
  main.cpp
  BinaryStreamTest.cpp
)
target_compile_options(bell-test PRIVATE -Wall -Wextra -Werror)
target_link_libraries(bell-test bell Catch2::Catch2)

catch_discover_tests(bell-test)