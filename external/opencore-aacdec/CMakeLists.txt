file(GLOB AACDEC_SOURCES "src/*.c")
file(GLOB AACDEC_HEADERS "src/*.h" "oscl/*.h" "include/*.h")

add_library(opencore-aacdec STATIC ${AACDEC_SOURCES})
if(NOT MSVC)
	target_compile_options(opencore-aacdec PRIVATE -Wno-parentheses-equality -Wno-implicit-const-int-float-conversion -Wno-extern-initializer -Wno-array-parameter -Wno-cast-align)
endif()	
add_definitions(-DAAC_PLUS -DHQ_SBR -DPARAMETRICSTEREO -DC_EQUIVALENT)
target_include_directories(opencore-aacdec SYSTEM PUBLIC "src/" "oscl/" "include/")