# Camkit configure script
# Copyright (c) Andy Huang
CMAKE_MINIMUM_REQUIRED(VERSION 2.8)
PROJECT(CAMKIT C) # CAmera toolKIT

# version
SET(CK_MAJOR 0)
SET(CK_MINOR 1)
SET(CK_BUILD 0)

SET(CK_VERSION ${CK_MAJOR}.${CK_MINOR}.${CK_BUILD})
SET(CK_NAME camkit)

# configuration options
OPTION(DEBUG "Build with debug output" OFF) # Release by default
SET(PLAT "PC" CACHE STRING "specify the PLAT to use: PC, FSL, RPI")      # RPI: raspberry pi, FSL: freescale imx, PC: pc desktop (Default)

# macro definitions
IF (DEBUG)
    ADD_DEFINITIONS("-D_DEBUG_")
    SET(CMAKE_BUILD_TYPE Debug)
ELSE (DEBUG)
    SET(CMAKE_BUILD_TYPE Release)
ENDIF()

SET(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -Wall -Wextra -Wno-write-strings -Wno-missing-field-initializers -g")
SET(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -O3 -Wall -Wextra -Wno-write-strings -Wno-missing-field-initializers")

# find dependence
SET(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/modules/")

SET (LIBS pthread)

IF(PLAT STREQUAL "RPI") # raspberry pi
    # find VideoCore libraries
    FIND_PACKAGE(VideoCore REQUIRED)
    IF (VIDEOCORE_FOUND)
        INCLUDE_DIRECTORIES(${VideoCore_INCLUDE_DIRS})
        SET (LIBS ${LIBS} ${VideoCore_LIBRARIES})
    ENDIF()
    
ELSEIF(PLAT STREQUAL "FSL") # freescale imx
    # find vpu and ipu
    FIND_PACKAGE(IPUVPU REQUIRED)
    IF (IPUVPU_FOUND)
        INCLUDE_DIRECTORIES(${IPUVPU_INCLUDE_DIRS})
        SET (LIBS ${LIBS} ${IPUVPU_LIBRARIES})
    ENDIF ()
    
ELSEIF (PLAT STREQUAL "PC")  # PC
    # find ffmpeg
    FIND_PACKAGE(FFMPEG REQUIRED)
    IF (FFMPEG_FOUND)
        INCLUDE_DIRECTORIES(${FFMPEG_INCLUDE_DIRS})
        SET(LIBS ${LIBS} ${FFMPEG_LIBRARIES})
    ENDIF()
    
ELSE ()
    MESSAGE(FATAL_ERROR "Unknown Platform: " ${PLAT} ", Supported ones are: RPI, FSL, PC.") 
ENDIF (PLAT STREQUAL "RPI")


# include directory
INCLUDE_DIRECTORIES(${PROJECT_SOURCE_DIR}/include/)

# public headers
SET(CK_IDX_HDR ${PROJECT_SOURCE_DIR}/include/camkit.h)
SET(CK_HDRS ${PROJECT_SOURCE_DIR}/include/camkit/convert.h 
    ${PROJECT_SOURCE_DIR}/include/camkit/capture.h ${PROJECT_SOURCE_DIR}/include/camkit/encode.h
    ${PROJECT_SOURCE_DIR}/include/camkit/pack.h ${PROJECT_SOURCE_DIR}/include/camkit/network.h 
    ${PROJECT_SOURCE_DIR}/include/camkit/comdef.h)

# configuration file
CONFIGURE_FILE(${PROJECT_SOURCE_DIR}/config.h.in ${PROJECT_SOURCE_DIR}/include/camkit/config.h)
SET(CK_HDRS ${CK_HDRS} ${PROJECT_SOURCE_DIR}/include/camkit/config.h)

# sub directories
ADD_SUBDIRECTORY(src)
ADD_SUBDIRECTORY(demo)
#ADD_SUBDIRECTORY(test)

# print out compile configurations
MESSAGE(STATUS "--------------------------")
MESSAGE(STATUS "Version: " ${CK_VERSION})
MESSAGE(STATUS "PLAT: " ${PLAT})
MESSAGE(STATUS "Build Type: " ${CMAKE_BUILD_TYPE}) 
MESSAGE(STATUS "C Compiler: " ${CMAKE_C_COMPILER})
MESSAGE(STATUS "Install prefix: " ${CMAKE_INSTALL_PREFIX})
MESSAGE(STATUS "--------------------------")
