aux_source_directory("." GETARG_SRCS)
set(EXECUTABLE_OUTPUT_PATH "${PROJECT_BINARY_DIR}/bin")
set(LIBRARY_OUTPUT_PATH "${PROJECT_BINARY_DIR}/lib")
add_library("getarg" ${GETARG_SRCS})
