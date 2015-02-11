# - Try to find some used ffmpeg libraries (libavcodec, libavutil and libswscale)
# Once done this will define
#
# FFMPEG_FOUND - system has ffmpeg or libav
# FFMPEG_INCLUDE_DIRS - the ffmpeg include directory
# FFMPEG_LIBRARIES - Link these to use ffmpeg

# use pkg-config to get the directories and then use these values
# in the FIND_PATH() and FIND_LIBRARY() calls
FIND_PACKAGE(PkgConfig)
IF (PKG_CONFIG_FOUND)
  pkg_check_modules(_FFMPEG_AVCODEC libavcodec)
  pkg_check_modules(_FFMPEG_AVUTIL libavutil)
  pkg_check_modules(_FFMPEG_SWSCALE libswscale)
ENDIF (PKG_CONFIG_FOUND)

FIND_PATH(FFMPEG_AVCODEC_INCLUDE_DIR
  NAMES libavcodec/avcodec.h
  PATHS ${_FFMPEG_AVCODEC_INCLUDE_DIRS} /usr/include /usr/local/include
  PATH_SUFFIXES ffmpeg libav
  )

FIND_LIBRARY(FFMPEG_LIBAVCODEC
  NAMES avcodec
  PATHS ${_FFMPEG_AVCODEC_LIBRARY_DIRS} /usr/lib /usr/local/lib
  )

FIND_PATH(FFMPEG_AVUTIL_INCLUDE_DIR
  NAMES libavutil/common.h
  PATHS ${_FFMPEG_AVUTIL_INCLUDE_DIRS} /usr/include /usr/local/include
  PATH_SUFFIXES ffmpeg libav
  )

FIND_LIBRARY(FFMPEG_LIBAVUTIL
  NAMES avutil
  PATHS ${_FFMPEG_AVUTIL_LIBRARY_DIRS} /usr/lib /usr/local/lib
  )

FIND_PATH(FFMPEG_SWSCALE_INCLUDE_DIR
  NAMES libswscale/swscale.h
  PATHS ${_FFMPEG_SWSCALE_INCLUDE_DIRS} /usr/include /usr/local/include
  PATH_SUFFIXES ffmpeg libav
  )

FIND_LIBRARY(FFMPEG_LIBSWSCALE
  NAMES swscale
  PATHS ${_FFMPEG_SWSCALE_LIBRARY_DIRS} /usr/lib /usr/local/lib
  )

INCLUDE(FindPackageHandleStandardArgs)
find_package_handle_standard_args(FFMPEG DEFAULT_MSG
  FFMPEG_AVUTIL_INCLUDE_DIR FFMPEG_LIBAVCODEC FFMPEG_AVUTIL_INCLUDE_DIR FFMPEG_LIBAVCODEC FFMPEG_SWSCALE_INCLUDE_DIR FFMPEG_LIBSWSCALE)

SET (FFMPEG_INCLUDE_DIRS ${FFMPEG_AVUTIL_INCLUDE_DIR} ${FFMPEG_AVCODEC_INCLUDE_DIR} ${FFMPEG_SWSCALE_INCLUDE_DIR})
SET (FFMPEG_LIBRARIES ${FFMPEG_LIBAVUTIL} ${FFMPEG_LIBAVCODEC} ${FFMPEG_LIBSWSCALE})

MARK_AS_ADVANCED(FFMPEG_INCLUDE_DIRS FFMPEG_LIBRARIES)
