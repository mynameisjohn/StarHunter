CMAKE_MINIMUM_REQUIRED(VERSION 2.8)

PROJECT(StarHunter)

SET(CMAKE_CXX_FLAGS "-std=c++11 -Wall")

# Windows build settings
IF(WIN32)
    # CUDA requires x64 on Windows
    IF(SH_CUDA)
        ADD_DEFINITIONS(-DSH_CUDA=1)
        SET(WIN_PLATFORM "x64")
    ELSE(SH_CUDA)
        SET(WIN_PLATFORM "Win32")
    ENDIF(SH_CUDA)

    # OCV 3.1, handle debug and release
    SET(OCV_INC_DIR $ENV{LIBDIR}/OpenCV/31/include)
    SET(OCV_LIB_DIR $ENV{LIBDIR}/OpenCV/31/lib/${WIN_PLATFORM})
    IF (${CMAKE_BUILD_TYPE} STREQUAL "Debug")
        SET(LIBTYPE "Debug")
        SET(OCV_LIB_SUFFIX "310d.lib")
    ELSE (${CMAKE_BUILD_TYPE} STREQUAL "Debug")
        SET(OCV_LIB_SUFFIX "310.lib")
        SET(LIBTYPE "Release")
    ENDIF (${CMAKE_BUILD_TYPE} STREQUAL "Debug")

    # LibRaw
    SET(LIBRAW_INC_DIR $ENV{LIBDIR}/LibRaw/include)
    SET(LIBRAW_LIB_DIR $ENV{LIBDIR}/LibRaw/lib/${WIN_PLATFORM})

    # Canon EDSDK (no libgphoto2 on Windows)
    SET(EDSDK_INC_DIR $ENV{LIBDIR}/EDSDK/include)
    SET(EDSDK_LIB_DIR $ENV{LIBDIR}/EDSDK/lib/${WIN_PLATFORM})

    # All libraries in one macro
    SET(SH_LIBS
        # OpenCV libs
        ${OCV_LIB_DIR}/opencv_core${OCV_LIB_SUFFIX}
        ${OCV_LIB_DIR}/opencv_imgproc${OCV_LIB_SUFFIX}
        ${OCV_LIB_DIR}/opencv_imgcodecs${OCV_LIB_SUFFIX}
        ${OCV_LIB_DIR}/opencv_highgui${OCV_LIB_SUFFIX}
        ${LIBRAW_LIB_DIR}/${LIBTYPE}/LibRaw.lib
        ${EDSDK_LIB_DIR}/EDSDK.lib)

    # Append CUDA libraries if needed
    IF(SH_CUDA)
        SET(SH_LIBS ${SH_LIBS}
            ${OCV_LIB_DIR}/opencv_cudaimgproc${OCV_LIB_SUFFIX}
            ${OCV_LIB_DIR}/opencv_cudaarithm${OCV_LIB_SUFFIX}
            ${OCV_LIB_DIR}/opencv_cudafilters${OCV_LIB_SUFFIX}
            ${CUDA_LIBRARIES})
    ENDIF(SH_CUDA)
ELSE(WIN32)
    # TODO Unix stuff
ENDIF(WIN32)

#CUDA flags
IF(SH_CUDA)
    # let CMake find CUDA
    FIND_PACKAGE(CUDA REQUIRED)
    # Set up sm arch (jetson is 32, but try 50 for now)
    SET(CUDA_NVCC_FLAGS "${CUDA_NVCC_FLAGS};-arch sm_50")
# No CUDA? at least try to use openmp
ELSE(SH_CUDA)
    FIND_PACKAGE(OpenMP)
    IF(OPENMP_FOUND)
        SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${OpenMP_CXX_FLAGS}")
    ENDIF(OPENMP_FOUND)
ENDIF(SH_CUDA)

INCLUDE_DIRECTORIES(
${CMAKE_CURRENT_SOURCE_DIR}/include
${OCV_INC_DIR}
${LIBRAW_INC_DIR}
${EDSDK_INC_DIR})

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
