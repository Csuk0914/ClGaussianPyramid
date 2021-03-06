CMAKE_MINIMUM_REQUIRED(VERSION 2.8.4)

FIND_PACKAGE(Java REQUIRED)
FIND_PACKAGE(JNI REQUIRED)
INCLUDE(UseJava)

FIND_PROGRAM(UNZIP_EXECUTABLE unzip)
IF(NOT UNZIP_EXECUTABLE)
  MESSAGE(FATAL_ERROR "unzip NOT found")
ENDIF(NOT UNZIP_EXECUTABLE)
MESSAGE(STATUS "Downloading JOCL archive")
FILE(DOWNLOAD 
    http://www.jocl.org/downloads/JOCL-0.1.7-bin.zip 
    ${CMAKE_CURRENT_BINARY_DIR}/downloads/JOCL-0.1.7-bin.zip)
MESSAGE(STATUS "Extracting JOCL jar")
EXECUTE_PROCESS(
    COMMAND ${UNZIP_EXECUTABLE} -j -o ${CMAKE_CURRENT_BINARY_DIR}/downloads/JOCL-0.1.7-bin.zip JOCL-0.1.7-bin/JOCL-0.1.7.jar
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    OUTPUT_QUIET)
     
SET(CMAKE_JAVA_INCLUDE_PATH ${CMAKE_CURRENT_BINARY_DIR}/JOCL-0.1.7.jar)
SET(CLGP_SRCS CLGP.java)
ADD_JAR(CLGP ${CLGP_SRCS})

SET(jclgp_JNIHDR ${CMAKE_CURRENT_BINARY_DIR}/CLGP.java.h)
ADD_CUSTOM_COMMAND(
    OUTPUT ${jclgp_JNIHDR}
    COMMAND ${Java_JAVAH_EXECUTABLE} -force -classpath ${CMAKE_CURRENT_BINARY_DIR}/JOCL-0.1.7.jar:${CMAKE_CURRENT_BINARY_DIR}/CLGP.jar -o ${jclgp_JNIHDR} clgp.CLGP
    DEPENDS CLGP CLGP.java
    COMMENT "Generating JNI header CLGP.java.h")
ADD_CUSTOM_TARGET(generate_jclgp_JNIHDR DEPENDS ${jclgp_JNIHDR})

INCLUDE_DIRECTORIES(
    ${JAVA_INCLUDE_PATH}
    ${JAVA_INCLUDE_PATH2}
    ${ClGaussianPyramid_SOURCE_DIR}/clgp/include
    ${CMAKE_CURRENT_BINARY_DIR})
SET(jclgp_SRCS jclgp.c)
ADD_LIBRARY(jclgp SHARED ${jclgp_SRCS})
ADD_DEPENDENCIES(jclgp generate_jclgp_JNIHDR)
TARGET_LINK_LIBRARIES(jclgp clgp)

SET(CMAKE_JAVA_INCLUDE_PATH ${CMAKE_CURRENT_BINARY_DIR}/CLGP.jar ${CMAKE_CURRENT_BINARY_DIR}/JOCL-0.1.7.jar)
ADD_JAR(GaussianPyramidJavaDemo GaussianPyramidJavaDemo.java)

