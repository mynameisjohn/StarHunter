CMAKE_MINIMUM_REQUIRED(VERSION 2.8)

PROJECT(StarHunter)

SET(CMAKE_CXX_FLAGS "-std=c++14 -Wall")

# If on linux, make sure to use gcc-4.8
# (cuda can't handle new gcc apparently)
IF(UNIX)
    IF(SH_CUDA AND NOT APPLE)
        SET(CMAKE_C_COMPILER gcc-4.8)    
        SET(CMAKE_CXX_COMPILER g++-4.8)
    ENDIF(SH_CUDA AND NOT APPLE)
ENDIF(UNIX)

IF(UNIX)
    IF(${CMAKE_BUILD_TYPE} STREQUAL "Debug")
        SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g")
    ENDIF(${CMAKE_BUILD_TYPE} STREQUAL "Debug")
ENDIF(UNIX)

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

IF(SH_CAMERA)
    ADD_DEFINITIONS(-DSH_CAMERA=1)
ENDIF(SH_CAMERA)

# Windows build settings
IF(WIN32)
    # this wasn't always necessary... what changed?
    ADD_DEFINITIONS(-DWIN32=1)
    
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
        
        # EDSDK needs a window, so use SDL2 as part of cam lib
        FIND_PACKAGE(SDL2)
        
        # Canon EDSDK (no libgphoto2 on Windows)
        ADD_DEFINITIONS(-DSH_USE_EDSDK=1)
        SET(CAM_INC_DIR $ENV{LIBDIR}/EDSDK/include ${SDL2_INCLUDE_DIR})
        SET(CAM_LIB_DIR $ENV{LIBDIR}/EDSDK/lib/${WIN_PLATFORM})
        SET(CAM_LIB EDSDK.lib ${SDL2_LIBRARY})
        
        # We'll use OpenGL and GLEW to display incoming images
        FIND_PACKAGE(GLEW)
        FIND_PACKAGE(OpenGL)
        ADD_DEFINITIONS(-DGLEW_STATIC)
        SET(GLM $ENV{LIBDIR}/glm)
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
    ${CAM_LIB}
    # GLEW if we need it
    ${OPENGL_LIBRARIES} ${GLEW_LIBRARIES})

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
LINK_DIRECTORIES(
    ${OCV_LIB_DIR} 
    ${LIBRAW_LIB_DIR} 
    ${CAM_LIB_DIR})

# Include directories
# those that are undefined will be blank
INCLUDE_DIRECTORIES(
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${OCV_INC_DIR}
    ${LIBRAW_INC_DIR}
    ${CAM_INC_DIR}
    ${GLEW_INCLUDE_DIRS}
    ${GLM})

# cpp and header files
FILE(GLOB SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp)
FILE(GLOB HEADERS ${CMAKE_CURRENT_SOURCE_DIR}/include/*.h)

# Create source groups
SOURCE_GROUP("Source" FILES ${SOURCES})
SOURCE_GROUP("Include" FILES ${HEADERS})

# Make a macro for all input dependencies
SET(SH_INPUT ${SOURCES} ${HEADERS})

IF(SH_CAMERA AND WIN32)
    FILE(GLOB SHADERS ${CMAKE_CURRENT_SOURCE_DIR}/shaders/*)
    SOURCE_GROUP("Shaders" FILES ${SHADERS})
    SET(SH_INPUT ${SH_INPUT} ${SHADERS})
ENDIF(SH_CAMERA AND WIN32)

# Append source files (add .cu for cuda)
IF(SH_CUDA)
    FILE(GLOB KERNELS ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cu ${CMAKE_CURRENT_SOURCE_DIR}/include/*.cuh)
    SOURCE_GROUP("Kernels" FILES ${KERNELS})
    SET(SH_INPUT ${SH_INPUT} ${KERNELS})
ENDIF(SH_CUDA)

# If building with telescope control,
# add pyliaison, scripts, and append include/lib
IF(SH_TELESCOPE)
    # Add subdir
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/pyl)
    # include python and pyl
    INCLUDE_DIRECTORIES(
        ${CMAKE_CURRENT_SOURCE_DIR}/pyl
        ${PYTHON_INCLUDE_DIR})
    # link against python
    LINK_DIRECTORIES(${PYTHON_LIB_DIR})
    SET(SH_LIBS ${SH_LIBS} PyLiaison ${PYTHON_LIBRARY})
    # Add scripts to source
    FILE(GLOB SCRIPTS ${CMAKE_CURRENT_SOURCE_DIR}/scripts/*.py)
    SOURCE_GROUP("Scripts" FILES ${SCRIPTS})
    SET(SH_INPUT ${SH_INPUT} ${SCRIPTS})
    # Add definition
    ADD_DEFINITIONS(-DSH_TELESCOPE=1)
ENDIF(SH_TELESCOPE)

# Add regular or cuda executable
IF(SH_CUDA)
    CUDA_ADD_EXECUTABLE(StarHunter ${SH_INPUT})
ELSE(SH_CUDA)
    ADD_EXECUTABLE(StarHunter ${SH_INPUT})
ENDIF(SH_CUDA)

# Link libraries with executable
TARGET_LINK_LIBRARIES(StarHunter LINK_PUBLIC ${SH_LIBS})
