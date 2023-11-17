file(GLOB OPUS_RESAMPLE_SOURCES "src/*.c")
file(GLOB OPUS_RESAMPLE_HEADERS "src/*.h")

add_library(opus-resample STATIC ${OPUS_RESAMPLE_SOURCES})

target_compile_definitions(opus-resample PUBLIC OUTSIDE_SPEEX RANDOM_PREFIX=bell)
target_include_directories(opus-resample PUBLIC "src/")