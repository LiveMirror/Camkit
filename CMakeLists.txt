# Camkit configure script
# Copyright (c) Andy Huang
CMAKE_MINIMUM_REQUIRED(VERSION 2.8)
PROJECT(CAMKIT C) # CAmera toolKIT

# version
SET(CK_MAJOR 0)
SET(CK_MINOR 0)
SET(CK_BUILD 1)

SET(CK_VERSION ${CK_MAJOR}.${CK_MINOR}.${CK_BUILD})
SET(CK_NAME camkit)

#configuration options
OPTION(DEBUG "Build with debug output" OFF)
OPTION(PLATFORM "specify the platform to use" OFF)      # FSL: freescale, FFMPEG: ffmpeg software, RPI: raspberry pi
OPTION(CC "specify the C Compiler" OFF)
OPTION(ROOTSYS "The root file system for the cross-compiling" OFF)

# macro definitions
IF (DEBUG)
    ADD_DEFINITIONS("-D_DEBUG_")
    SET(CMAKE_BUILD_TYPE Debug)
ENDIF()

IF (CC)
    SET(CMAKE_C_COMPILER ${CC})
ENDIF (CC)

IF (NOT ROOTSYS)
    SET(ROOTSYS "")
ENDIF (NOT ROOTSYS)

SET(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -Wall -Wextra -Wno-write-strings -Wno-missing-field-initializers -g")
SET(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -O3 -Wall -Wextra -Wno-write-strings -Wno-missing-field-initializers")

# compile flags, Release by default
IF (NOT CMAKE_BUILD_TYPE)
    SET(CMAKE_BUILD_TYPE Release)
ENDIF()

# include directory
INCLUDE_DIRECTORIES(${PROJECT_SOURCE_DIR}/include/ ${ROOTSYS}/usr/include)   #FIXME

# public headers
SET(CK_IDX_HDR ${PROJECT_SOURCE_DIR}/include/camkit.h)
SET(CK_HDRS ${PROJECT_SOURCE_DIR}/include/camkit/convert.h 
    ${PROJECT_SOURCE_DIR}/include/camkit/capture.h ${PROJECT_SOURCE_DIR}/include/camkit/encode.h
    ${PROJECT_SOURCE_DIR}/include/camkit/pack.h ${PROJECT_SOURCE_DIR}/include/camkit/network.h 
    ${PROJECT_SOURCE_DIR}/include/camkit/comdef.h)

# configuration file
CONFIGURE_FILE(${PROJECT_SOURCE_DIR}/config.h.in ${PROJECT_SOURCE_DIR}/include/camkit/config.h)
SET(CK_HDRS ${CK_HDRS} ${PROJECT_SOURCE_DIR}/include/camkit/config.h)

# install prefix
IF (CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
    SET (CMAKE_INSTALL_PREFIX ${ROOTSYS}/usr)     # install location by default, this is used in cross.cmake
ENDIF ()

# sub directories
ADD_SUBDIRECTORY(src)
ADD_SUBDIRECTORY(demo)
#ADD_SUBDIRECTORY(test)

# print out compile configurations
MESSAGE(STATUS "--------------------------")
MESSAGE(STATUS "CK Version: " ${CK_VERSION})
MESSAGE(STATUS "Platform: " ${PLATFORM})
MESSAGE(STATUS "Build Type: " ${CMAKE_BUILD_TYPE}) 
MESSAGE(STATUS "C Compiler: " ${CMAKE_C_COMPILER})
MESSAGE(STATUS "ROOT System: " ${ROOTSYS})
MESSAGE(STATUS "Install prefix: " ${CMAKE_INSTALL_PREFIX})
MESSAGE(STATUS "--------------------------")
