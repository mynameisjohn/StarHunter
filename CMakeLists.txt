CMAKE_MINIMUM_REQUIRED(VERSION 2.8)

PROJECT(StarHunter)

SET(CMAKE_CXX_FLAGS "-std=c++11 -Wall")

# If on linux, make sure to use gcc-4.8
# (cuda can't handle new gcc apparently)
IF(UNIX AND SH_CUDA AND NOT APPLE)
    SET(CMAKE_C_COMPILER gcc-4.8)    
    SET(CMAKE_CXX_COMPILER g++-4.8)    
ENDIF(UNIX AND SH_CUDA AND NOT APPLE)

#CUDA flags
IF(SH_CUDA)
    # let CMake find CUDA
    FIND_PACKAGE(CUDA REQUIRED)
    # Set up sm arch (jetson is 32, but try 50 for now)
    SET(CUDA_NVCC_FLAGS "${CUDA_NVCC_FLAGS};-arch sm_50")
    # define SH_CUDA to be true
    ADD_DEFINITIONS(-DSH_CUDA=1)
# No CUDA? at least try to use openmp
ELSE(SH_CUDA)
    FIND_PACKAGE(OpenMP)
    IF(OPENMP_FOUND)
        SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${OpenMP_CXX_FLAGS}")
    ENDIF(OPENMP_FOUND)
ENDIF(SH_CUDA)

# Windows build settings
IF(WIN32)
    # CUDA requires x64 on Windows
    IF(SH_CUDA)
        SET(WIN_PLATFORM "x64")
    ELSE(SH_CUDA)
        SET(WIN_PLATFORM "Win32")
    ENDIF(SH_CUDA)
    
    # OCV 3.1
    SET(OCV_INC_DIR $ENV{LIBDIR}/OpenCV/31/include)
    SET(OCV_LIB_DIR $ENV{LIBDIR}/OpenCV/31/lib/${WIN_PLATFORM})

    # Do they want a camera SDK?
    IF(${SH_CAMERA})
        # LibRaw
        SET(LIBRAW_INC_DIR $ENV{LIBDIR}/LibRaw/include)
        SET(LIBRAW_LIB_DIR $ENV{LIBDIR}/LibRaw/lib/${WIN_PLATFORM})
        SET(LIBRAW_LIB LibRaw.lib)
        
        # Canon EDSDK (no libgphoto2 on Windows)
        # EDSDK needs a window, so use SDL2
        FIND_PACKAGE(SDL2)
        SET(CAM_INC_DIR $ENV{LIBDIR}/EDSDK/include ${SDL2_INCLUDE_DIR})
        SET(CAM_LIB_DIR $ENV{LIBDIR}/EDSDK/lib/${WIN_PLATFORM})
        SET(CAM_LIB EDSDK.lib ${SDL2_LIBRARY})
    ENDIF(${SH_CAMERA})
ELSE(WIN32)
    # OCV 3.1, handle debug and release
    SET(OCV_INC_DIR /usr/local//include)
    SET(OCV_LIB_DIR /usr/local/lib)

    # Do they want a camera SDK?
    IF(${SH_CAMERA})
        # LibRaw
        SET(LIBRAW_INC_DIR /usr/local/include)
        SET(LIBRAW_LIB_DIR  /usr/local/lib)
        SET(LIBRAW_LIB raw)
        # libgphoto2
        SET(CAM_INC_DIR /usr/include)
        # lib dir differs between ubuntu and os x
        IF(APPLE)
            SET(CAM_LIB_DIR /usr/local/lib)
        ELSE(APPLE)
            SET(CAM_LIB_DIR /usr/lib/x86_64-linux-gnu)
        ENDIF(APPLE)
        SET(CAM_LIB gphoto2)
    ENDIF(${SH_CAMERA})
ENDIF(WIN32)

# All libraries in one macro
SET(SH_LIBS
    # OpenCV libs
    opencv_core
    opencv_imgproc
    opencv_imgcodecs
    opencv_highgui
    # Camera libs
    ${LIBRAW_LIB}
    ${CAM_LIB})

# Append CUDA libraries if needed
IF(SH_CUDA)
    SET(SH_LIBS ${SH_LIBS}
        # opencv cuda libs
        opencv_cudaimgproc
        opencv_cudaarithm
        opencv_cudafilters
        # cuda runtime libs
        ${CUDA_LIBRARIES})
ENDIF(SH_CUDA)

# Set up link directories
LINK_DIRECTORIES(${OCV_LIB_DIR} ${LIBRAW_LIB_DIR} ${CAM_LIB_DIR})

# Include directories
# those that are undefined will be blank
INCLUDE_DIRECTORIES(
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${OCV_INC_DIR}
    ${LIBRAW_INC_DIR}
    ${CAM_INC_DIR})

# Add source files (add .cu for cuda)
IF(SH_CUDA)
    FILE(GLOB SOURCE
    ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cu
    ${CMAKE_CURRENT_SOURCE_DIR}/include/*)
ELSE(SH_CUDA)
    FILE(GLOB SOURCE
    ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/*)
ENDIF(SH_CUDA)

# Add regular or cuda executable
IF(SH_CUDA)
    CUDA_ADD_EXECUTABLE(StarHunter ${SOURCE})
ELSE(SH_CUDA)
    ADD_EXECUTABLE(StarHunter ${SOURCE})
ENDIF(SH_CUDA)

# Link libraries with executable
TARGET_LINK_LIBRARIES(StarHunter LINK_PUBLIC ${SH_LIBS})
