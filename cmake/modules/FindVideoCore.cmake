# Try to find VideoCore includes and libraries
# Once done this will define
# VIDEOCORE_FOUND 
# VideoCore_INCLUDE_DIRS
# VideoCore_LIBRARIES

FIND_PATH(BCM_HOST_INCLUDE_DIR bcm_host.h
    PATHS
    /opt/vc/include
    /usr/include
    /usr/local/include
    )
    
FIND_LIBRARY(VCOS_LIBRARY
    NAMES vcos
    PATHS
    /opt/vc/lib
    /usr/lib
    /usr/local/lib
    )

FIND_LIBRARY(VCSM_LIBRARY
    NAMES vcsm
    PATHS
    /opt/vc/lib
    /usr/lib
    /usr/local/lib
    )

FIND_LIBRARY(BCM_HOST_LIBRARY
    NAMES bcm_host
    PATHS
    /opt/vc/lib
    /usr/lib
    /usr/local/lib
    )

FIND_LIBRARY(OPENMAXIL_LIBRARY
    NAMES openmaxil
    PATHS
    /opt/vc/lib
    /usr/lib
    /usr/local/lib
    )

FIND_LIBRARY(VCHIQ_ARM_LIBRARY
    NAMES vchiq_arm
    PATHS
    /opt/vc/lib
    /usr/lib
    /usr/local/lib
    )
    
INCLUDE(FindPackageHandleStandardArgs)
find_package_handle_standard_args(VideoCore DEFAULT_MSG 
    BCM_HOST_INCLUDE_DIR VCOS_LIBRARY VCSM_LIBRARY BCM_HOST_LIBRARY OPENMAXIL_LIBRARY VCHIQ_ARM_LIBRARY
    )
    
SET (VideoCore_INCLUDE_DIRS ${BCM_HOST_INCLUDE_DIR} ${BCM_HOST_INCLUDE_DIR}/interface/vcos/pthreads/
    ${BCM_HOST_INCLUDE_DIR}/interface/vmcs_host/linux/)
SET (VideoCore_LIBRARIES ${VCOS_LIBRARY} ${VCSM_LIBRARY} ${BCM_HOST_LIBRARY} ${OPENMAXIL_LIBRARY} ${VCHIQ_ARM_LIBRARY})

MARK_AS_ADVANCED(VideoCore_INCLUDE_DIRS VideoCore_LIBRARIES)
    
