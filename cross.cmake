SET(CMAKE_SYSTEM_NAME Linux)
SET(CMAKE_SYSTEM_VERSION 1)

# specify the cross compiler, change this if needed
SET(CMAKE_C_COMPILER "cross_complier_cc")     # Change this
SET(CMAKE_CXX_COMPILER "cross_complier_c++")   # Change this

# where is the target environment, change this to the root system for cross compiling 
SET(CMAKE_FIND_ROOT_PATH "location_of_cross_compile_rootsys")   # Change this

# search for programs in the build host directories
SET(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
# for libraries and headers in the target directories
SET(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
SET(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)

# indicate it's cross compiled
SET(IS_CROSS ON)
