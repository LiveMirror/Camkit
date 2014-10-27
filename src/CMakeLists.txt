# build library
SET(COM_SRC v4l_capture.c rtp_pack.c network.c)
IF (PLATFORM STREQUAL "RPI")        ## raspberry pi
  SET (CK_SRC ffmpeg_convert.c omx_encode.c ${COM_SRC})
  SET(DEPLIBS /opt/vc/lib/libilclient.a /opt/vc/lib/libvcos.so /opt/vc/lib/libvcsm.so
    /opt/vc/lib/libbcm_host.so /opt/vc/lib/libopenmaxil.so ${ROOTSYS}/usr/lib/libavcodec.so 
    ${ROOTSYS}/usr/lib/libswscale.so -lpthread)
  INCLUDE_DIRECTORIES(/opt/vc/include /opt/vc/include/interface/vcos/pthreads/ /opt/vc/include/interface/vmcs_host/linux/)
ELSEIF (PLATFORM STREQUAL "FSL")        ## freescale
  SET(CK_SRC ipu_convert.c vpu_encode.c ${COM_SRC})
  SET(DEPLIBS ${ROOTSYS}/usr/lib/libipu.so ${ROOTSYS}/usr/lib/libvpu.so -lpthread)
ELSE (PLATFORM STREQUAL "RPI")      ## ffmpeg as default
  SET(CK_SRC ffmpeg_convert.c ffmpeg_encode.c ${COM_SRC})
  SET(DEPLIBS ${ROOTSYS}/usr/lib/libavcodec.so ${ROOTSYS}/usr/lib/libswscale.so -lpthread)
ENDIF (PLATFORM STREQUAL "RPI")

ADD_LIBRARY(${CK_NAME} SHARED ${CK_SRC})
TARGET_LINK_LIBRARIES(${CK_NAME} ${DEPLIBS})

# install prefix
SET(PREFIX  ${CMAKE_INSTALL_PREFIX})

# install header files
SET(INCLUDEDIR  ${CMAKE_INSTALL_PREFIX}/include/)
INSTALL(FILES ${CK_IDX_HDR} DESTINATION ${INCLUDEDIR})
SET(INCLUDE_CK_DIR  ${CMAKE_INSTALL_PREFIX}/include/camkit)
INSTALL(FILES ${CK_HDRS} DESTINATION ${INCLUDE_CK_DIR})

# install library
SET(LIB_PREFIX ${CMAKE_INSTALL_PREFIX}/lib)
INSTALL(TARGETS ${CK_NAME} LIBRARY DESTINATION ${LIB_PREFIX})
