project(bell)

cmake_minimum_required(VERSION 2.8.9)
set (CMAKE_CXX_STANDARD 17)

file(GLOB SOURCES "src/*.cpp" "src/*.c")

# Add platform specific sources

if(${ESP_PLATFORM})
    file(GLOB ESP_PLATFORM_SOURCES "src/platform/esp/*.cpp" "src/platform/esp/*.c" "src/asm/biquad_f32_ae32.S")
    set(SOURCES ${SOURCES} ${ESP_PLATFORM_SOURCES} )
endif()

if(UNIX)
    file(GLOB UNIX_PLATFORM_SOURCES "src/platform/unix/*.cpp" "src/platform/linux/TLSSocket.cpp" "src/platform/unix/*.c")
    set(SOURCES ${SOURCES} ${UNIX_PLATFORM_SOURCES} )
endif()

if(APPLE)
    file(GLOB APPLE_PLATFORM_SOURCES "src/platform/apple/*.cpp" "src/platform/linux/TLSSocket.cpp"  "src/platform/apple/*.c")
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
    set(EXTRA_REQ_LIBS OpenSSL::Crypto OpenSSL::SSL Threads::Threads)
    set(THREADS_PREFER_PTHREAD_FLAG ON)
    find_package(Threads REQUIRED)
endif()

if (BELL_DISABLE_CODECS) 
    list(REMOVE_ITEM SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/src/DecoderGlobals.cpp) # use OpenSSL
	add_definitions(-DBELL_DISABLE_CODECS)
else()
	set(EXTRA_INC ${EXTRA_INC} "libhelix-aac" "libhelix-mp3")
	set(SOURCES ${SOURCES} "libhelix-aac/aacdec.c" "libhelix-aac/aactabs.c" "libhelix-aac/bitstream.c" "libhelix-aac/buffers.c" "libhelix-aac/dct4.c" "libhelix-aac/decelmnt.c" "libhelix-aac/dequant.c" "libhelix-aac/fft.c" "libhelix-aac/filefmt.c" "libhelix-aac/huffman.c" "libhelix-aac/hufftabs.c" "libhelix-aac/imdct.c" "libhelix-aac/noiseless.c" "libhelix-aac/pns.c" "libhelix-aac/sbr.c" "libhelix-aac/sbrfft.c" "libhelix-aac/sbrfreq.c" "libhelix-aac/sbrhfadj.c" "libhelix-aac/sbrhfgen.c" "libhelix-aac/sbrhuff.c" "libhelix-aac/sbrimdct.c" "libhelix-aac/sbrmath.c" "libhelix-aac/sbrqmf.c" "libhelix-aac/sbrside.c" "libhelix-aac/sbrtabs.c" "libhelix-aac/stproc.c" "libhelix-aac/tns.c" "libhelix-aac/trigtabs.c")
	set(SOURCES ${SOURCES} "libhelix-mp3/bitstream.c" "libhelix-mp3/buffers.c" "libhelix-mp3/dct32.c" "libhelix-mp3/dequant.c" "libhelix-mp3/dqchan.c" "libhelix-mp3/huffman.c" "libhelix-mp3/hufftabs.c" "libhelix-mp3/imdct.c" "libhelix-mp3/mp3dec.c" "libhelix-mp3/mp3tabs.c" "libhelix-mp3/polyphase.c" "libhelix-mp3/scalfact.c" "libhelix-mp3/stproc.c" "libhelix-mp3/subband.c" "libhelix-mp3/trigtabs.c")
endif()

if(UNIX AND NOT APPLE)
    set(EXTRA_REQ_LIBS ${EXTRA_REQ_LIBS} dns_sd) # add apple bonjur compatibility library for linux
    # TODO: migrate from this to native linux mDNS
endif()
add_definitions( -DUSE_DEFAULT_STDLIB=1)

if (BELL_CJSON_EXTERNAL) 
	message("Using external cJSON")
	set(EXTRA_REQ_LIBS ${EXTRA_REQ_LIBS} ${BELL_CJSON_EXTERNAL})
else() 
	set(EXTRA_INC ${EXTRA_INC} "cJSON")
	set(SOURCES ${SOURCES} "cJSON/cJSON.c")
endif()

if (BELL_TREMOR_EXTERNAL) 
	message("Using external TREMOR")
	set(EXTRA_REQ_LIBS ${EXTRA_REQ_LIBS} ${BELL_TREMOR_EXTERNAL})
else()
	set(EXTRA_INC ${EXTRA_INC} "tremor")
	set(SOURCES ${SOURCES} "tremor/mdct.c" "tremor/dsp.c" "tremor/info.c" "tremor/misc.c" "tremor/floor1.c" "tremor/floor0.c" "tremor/vorbisfile.c" "tremor/res012.c" "tremor/mapping0.c" "tremor/codebook.c" "tremor/framing.c" "tremor/bitwise.c" "tremor/floor_lookup.c")
endif()

add_library(bell STATIC ${SOURCES})
target_link_libraries(bell PRIVATE ${EXTRA_REQ_LIBS})
target_include_directories(bell PUBLIC "include" "protos" ${EXTRA_INC} ${EXTRA_REQ_LIBS} ${CMAKE_CURRENT_BINARY_DIR})

