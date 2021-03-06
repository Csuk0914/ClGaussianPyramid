INCLUDE_DIRECTORIES(${OPENCL_INCLUDE_DIR})
INCLUDE_DIRECTORIES(../include)

SET(clgp_CL_SRCS downsampledgauss5x5.cl downscaledgauss5x5.cl gauss9x9.cl)
FOREACH(CL_SRC ${clgp_CL_SRCS})
  # Check CL_SRC syntax a first time at compilation time, then build a C source
  # file that will contains a string with the program source from CL_SRC
  GET_FILENAME_COMPONENT(PROGRAM_NAME ${CL_SRC} NAME_WE)
  ADD_CUSTOM_COMMAND(
      OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/${CL_SRC}.c
      COMMAND clc ${CMAKE_CURRENT_SOURCE_DIR}/${CL_SRC} 
      COMMAND cl2c -n clgp_${PROGRAM_NAME}_src -o ${CMAKE_CURRENT_BINARY_DIR}/${CL_SRC}.c ${CMAKE_CURRENT_SOURCE_DIR}/${CL_SRC}
      DEPENDS clc cl2c ${CMAKE_CURRENT_SOURCE_DIR}/${CL_SRC}
      COMMENT "Generating ressource C file for OpenCL program ${PROGRAM_NAME}")
  LIST(APPEND clgp_CL_C_SRCS "${CL_SRC}.c")
ENDFOREACH(CL_SRC ${CL_SRCS})
ADD_CUSTOM_TARGET(generate_clgp_CL_C_SRCS DEPENDS ${clgp_CL_C_SRCS})

SET(clgp_SRCS 
    clgp.c
    downsampledgauss5x5.c
    downsampledgauss5x5.h
    downscaledgauss5x5.c
    downscaledgauss5x5.h
    gauss9x9.c
    gauss9x9.h
    utils.c
    ${clgp_CL_C_SRCS})
ADD_LIBRARY(clgp SHARED ${clgp_SRCS})
ADD_DEPENDENCIES(clgp generate_clgp_CL_C_SRCS)
TARGET_LINK_LIBRARIES(clgp ${OPENCL_LIBRARIES})

INSTALL(TARGETS clgp
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib${LIB_SUFFIX}
    ARCHIVE DESTINATION lib${LIB_SUFFIX})

