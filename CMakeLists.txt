CMAKE_MINIMUM_REQUIRED(VERSION 2.8)

PROJECT(StarHunter)

SET(CMAKE_CXX_FLAGS "-std=c++11 -Wall")

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

    # OCV 3.1, handle debug and release
    SET(OCV_INC_DIR $ENV{LIBDIR}/OpenCV/31/include)
    SET(OCV_LIB_DIR $ENV{LIBDIR}/OpenCV/31/lib/${WIN_PLATFORM})

    # Set up debug/release lib suffixes (suffices?)
    # I may phase this out by creating some links
    # and link against release on Windows
    IF (${CMAKE_BUILD_TYPE} STREQUAL "Debug")
        SET(LIBTYPE "Debug")
        SET(OCV_LIB_SUFFIX "310d${LIB_SUFFIX}")
    ELSE (${CMAKE_BUILD_TYPE} STREQUAL "Debug")
        SET(LIBTYPE "Release")
        SET(OCV_LIB_SUFFIX "310${LIB_SUFFIX}")
    ENDIF (${CMAKE_BUILD_TYPE} STREQUAL "Debug")

    # Do they want a camera SDK?
    IF(${SH_USE_CAMERA})
        # LibRaw
        SET(LIBRAW_INC_DIR $ENV{LIBDIR}/LibRaw/include)
        SET(LIBRAW_LIB $ENV{LIBDIR}/LibRaw/lib/${WIN_PLATFORM}/${EDSDK_LIB_DIR}/EDSDK.lib)
        # Canon EDSDK (no libgphoto2 on Windows)
        SET(CAM_INC_DIR $ENV{LIBDIR}/EDSDK/include)
        SET(CAM_LIB $ENV{LIBDIR}/EDSDK/lib/${WIN_PLATFORM})
    ENDIF(${SH_USE_CAMERA})
ELSE(WIN32)
    # OCV 3.1, handle debug and release
    SET(OCV_INC_DIR /usr/local//include)
    SET(OCV_LIB_DIR /usr/local/lib)

    # Either dylib or static lib... not sure about libRaw
    IF(APPLE)
        SET(LIB_SUFFIX ".dylib")
    ELSE(APPLE)
        SET(LIB_SUFFIX ".a")
    ENDIF(APPLE)

    # No need for this on unix unless I build debug
    SET(OCV_LIB_SUFFIX ${LIB_SUFFIX})

    # Do they want a camera SDK?
    IF(${SH_USE_CAMERA})
        # LibRaw
        SET(LIBRAW_INC_DIR /usr/local/include)
        SET(LIBRAW_LIB /usr/local/lib/libRaw${LIB_SUFFIX})
        # libgphoto2
        SET(CAM_INC_DIR /usr/local/include)
        SET(CAM_LIB /usr/local/lib/libghoto2${LIB_SUFFIX})
    ENDIF(${SH_USE_CAMERA})
ENDIF(WIN32)

# All libraries in one macro
SET(SH_LIBS
    # OpenCV libs
    ${OCV_LIB_DIR}/opencv_core${OCV_LIB_SUFFIX}
    ${OCV_LIB_DIR}/opencv_imgproc${OCV_LIB_SUFFIX}
    ${OCV_LIB_DIR}/opencv_imgcodecs${OCV_LIB_SUFFIX}
    ${OCV_LIB_DIR}/opencv_highgui${OCV_LIB_SUFFIX}
    # Camera libs
    ${LIBRAW_LIB}
    ${CAM_LIB})

# Append CUDA libraries if needed
IF(SH_CUDA)
    SET(SH_LIBS ${SH_LIBS}
        # opencv cuda libs
        ${OCV_LIB_DIR}/opencv_cudaimgproc${OCV_LIB_SUFFIX}
        ${OCV_LIB_DIR}/opencv_cudaarithm${OCV_LIB_SUFFIX}
        ${OCV_LIB_DIR}/opencv_cudafilters${OCV_LIB_SUFFIX}
        # cuda runtime libs
        ${CUDA_LIBRARIES})
ENDIF(SH_CUDA)

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
TARGET_LINK_LIBRARIES(StarHunter LINK_PUBLIC
${SH_LIBS})
