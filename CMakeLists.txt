project(bell)

cmake_minimum_required(VERSION 2.8.9)
set (CMAKE_CXX_STANDARD 17)

file(GLOB SOURCES "src/*.cpp" "src/*.c")

# Add platform specific sources

if(${ESP_PLATFORM})
    file(GLOB ESP_PLATFORM_SOURCES "src/platform/esp/*.cpp" "src/platform/esp/*.c")
    set(SOURCES ${SOURCES} ${ESP_PLATFORM_SOURCES} )
endif()

if(UNIX)
    file(GLOB UNIX_PLATFORM_SOURCES "src/platform/unix/*.cpp" "src/platform/unix/*.c")
    set(SOURCES ${SOURCES} ${UNIX_PLATFORM_SOURCES} )
endif()

if(APPLE)
    file(GLOB APPLE_PLATFORM_SOURCES "src/platform/apple/*.cpp" "src/platform/apple/*.c")
    set(SOURCES ${SOURCES} ${APPLE_PLATFORM_SOURCES} )
endif()

if(UNIX AND NOT APPLE)
    file(GLOB LINUX_PLATFORM_SOURCES "src/platform/linux/*.cpp" "src/platform/linux/*.c")
    set(SOURCES ${SOURCES} ${LINUX_PLATFORM_SOURCES} )
endif()

if(${ESP_PLATFORM})
    list(REMOVE_ITEM SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/src/CryptoOpenSSL.cpp) # use MBedTLS 
    idf_build_set_property(COMPILE_DEFINITIONS "-DBELL_USE_MBEDTLS" APPEND)
    set(EXTRA_REQ_LIBS idf::mbedtls idf::pthread idf::mdns)
    add_definitions(-Wunused-const-variable -Wchar-subscripts -Wunused-label -Wmaybe-uninitialized -Wmisleading-indentation)
else()
    list(REMOVE_ITEM SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/src/CryptoMbedTLS.cpp) # use OpenSSL
    find_package(OpenSSL REQUIRED)
    if(OPENSSL_FOUND)
        set(OPENSSL_USE_STATIC_LIBS TRUE)
    endif()
    set(EXTRA_REQ_LIBS OpenSSL::Crypto Threads::Threads)
    set(THREADS_PREFER_PTHREAD_FLAG ON)
    find_package(Threads REQUIRED)
endif()

if(UNIX AND NOT APPLE)
    set(EXTRA_REQ_LIBS ${EXTRA_REQ_LIBS} dns_sd) # add apple bonjur compatibility library for linux
    # TODO: migrate from this to native linux mDNS
endif()

include_directories("include")
include_directories("tremor")
include_directories("bell")
include_directories("libhelix-aac")
include_directories("${CMAKE_CURRENT_BINARY_DIR}")

set(SOURCES ${SOURCES} "cJSON/cJSON.c" "tremor/mdct.c" "tremor/dsp.c" "tremor/info.c" "tremor/misc.c" "tremor/floor1.c" "tremor/floor0.c" "tremor/vorbisfile.c" "tremor/res012.c" "tremor/mapping0.c" "tremor/codebook.c" "tremor/framing.c" "tremor/bitwise.c" "tremor/floor_lookup.c")
set(SOURCES ${SOURCES} "libhelix-aac/aacdec.c" "libhelix-aac/aactabs.c" "libhelix-aac/bitstream.c" "libhelix-aac/buffers.c" "libhelix-aac/dct4.c" "libhelix-aac/decelmnt.c" "libhelix-aac/dequant.c" "libhelix-aac/fft.c" "libhelix-aac/filefmt.c" "libhelix-aac/huffman.c" "libhelix-aac/hufftabs.c" "libhelix-aac/imdct.c" "libhelix-aac/noiseless.c" "libhelix-aac/pns.c" "libhelix-aac/sbr.c" "libhelix-aac/sbrfft.c" "libhelix-aac/sbrfreq.c" "libhelix-aac/sbrhfadj.c" "libhelix-aac/sbrhfgen.c" "libhelix-aac/sbrhuff.c" "libhelix-aac/sbrimdct.c" "libhelix-aac/sbrmath.c" "libhelix-aac/sbrqmf.c" "libhelix-aac/sbrside.c" "libhelix-aac/sbrtabs.c" "libhelix-aac/stproc.c" "libhelix-aac/tns.c" "libhelix-aac/trigtabs.c")

add_library(bell STATIC ${SOURCES})
target_link_libraries(bell PRIVATE ${EXTRA_REQ_LIBS})

target_include_directories(bell PUBLIC "include" "protos" "tremor" "cJSON" ${EXTRA_REQ_LIBS} ${CMAKE_CURRENT_BINARY_DIR})
