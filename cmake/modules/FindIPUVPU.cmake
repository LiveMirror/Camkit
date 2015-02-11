# Try to find Ipu and Vpu for Freescale platform
# Once done this will define
# IPUVPU_FOUND - System has IPU and VPU
# IPUVPU_INCLUDE_DIRS 
# IPUVPU_LIBRARIES

FIND_PATH(IPU_INCLUDE_DIR ipu.h
    PATHS
    /usr/include
    /usr/local/include
    PATH_SUFFIXES linux)

FIND_LIBRARY(IPU_LIBRARY
    NAMES ipu
    PATHS
    /usr/lib
    /usr/local/lib
    )
    
FIND_PATH(VPU_INCLUDE_DIR vpu_lib.h
    PATHS
    /usr/include
    /usr/local/include
    )

FIND_LIBRARY(VPU_LIBRARY
    NAMES vpu
    PATHS
    /usr/lib
    /usr/local/lib
    )

INCLUDE(FindPackageHandleStandardArgs)
find_package_handle_standard_args(IPUVPU DEFAULT_MSG 
    IPU_INCLUDE_DIR IPU_LIBRARY VPU_INCLUDE_DIR VPU_LIBRARY)
    
SET(IPUVPU_INCLUDE_DIRS ${IPU_INCLUDE_DIR} ${VPU_INCLUDE_DIR})
SET(IPUVPU_LIBRARIES ${IPU_LIBRARY} ${VPU_LIBRARY})

MARK_AS_ADVANCED(IPUVPU_INCLUDE_DIRS IPUVPU_LIBRARIES)