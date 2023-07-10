file(GLOB AACDEC_SOURCES "src/*.c")
file(GLOB AACDEC_HEADERS "src/*.h" "oscl/*.h" "include/*.h")

add_library(opencore-aacdec SHARED ${AACDEC_SOURCES})
add_definitions(-DAAC_PLUS -DHQ_SBR -DPARAMETRICSTEREO -DC_EQUIVALENT)
target_compile_options(opencore-aacdec PRIVATE -Wno-array-parameter)
target_include_directories(opencore-aacdec PUBLIC "src/" "oscl/" "include/")