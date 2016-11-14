cmake_minimum_required(VERSION 2.8)

project(StarHunter)

set(CMAKE_CXX_FLAGS "-std=c++11 -Wall")

if(WIN32)
  # CUDA requirse x64 on Windows
  set(WIN_PLATFORM "x64")

  # OCV 3.1, handle debug and release
  set(OCV_INC_DIR $ENV{LIBDIR}/OpenCV/31/include)
  set(OCV_LIB_DIR $ENV{LIBDIR}/OpenCV/31/lib/${WIN_PLATFORM})
  if (${CMAKE_BUILD_TYPE} STREQUAL "Debug")
    set(LIBTYPE "Debug")
    set(OCV_LIB_SUFFIX "310d.lib")
  else (${CMAKE_BUILD_TYPE} STREQUAL "Debug")
    set(OCV_LIB_SUFFIX "310.lib")
    set(LIBTYPE "Release")
  endif (${CMAKE_BUILD_TYPE} STREQUAL "Debug")

  # LibRaw
  set(LIBRAW_INC_DIR $ENV{LIBDIR}/LibRaw/include)
  set(LIBRAW_LIB_DIR $ENV{LIBDIR}/LibRaw/lib/${WIN_PLATFORM})

  # Canon EDSDK (no libgphoto2 on Windows)
  set(EDSDK_INC_DIR $ENV{LIBDIR}/EDSDK/include)
  set(EDSDK_LIB_DIR $ENV{LIBDIR}/EDSDK/lib/${WIN_PLATFORM})

  # All libraries in one macro
  set(SH_LIBS
    ${OCV_LIB_DIR}/opencv_core${OCV_LIB_SUFFIX}
    ${OCV_LIB_DIR}/opencv_imgproc${OCV_LIB_SUFFIX}
    ${OCV_LIB_DIR}/opencv_imgcodecs${OCV_LIB_SUFFIX}
    ${OCV_LIB_DIR}/opencv_highgui${OCV_LIB_SUFFIX}
    ${OCV_LIB_DIR}/opencv_cudaimgproc${OCV_LIB_SUFFIX}
    ${OCV_LIB_DIR}/opencv_cudaarithm${OCV_LIB_SUFFIX}
    ${OCV_LIB_DIR}/opencv_cudafilters${OCV_LIB_SUFFIX}
	${OCV_LIB_DIR}/opencv_cudaoptflow${OCV_LIB_SUFFIX}
    ${LIBRAW_LIB_DIR}/${LIBTYPE}/LibRaw.lib
    ${EDSDK_LIB_DIR}/EDSDK.lib)
endif(WIN32)

# let CMake find CUDA
find_package(CUDA REQUIRED)

# Set up sm arch (jetson is 32, but try 50 for now)
set(CUDA_NVCC_FLAGS "${CUDA_NVCC_FLAGS};-arch sm_50")

include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}/include
  ${OCV_INC_DIR}
  ${LIBRAW_INC_DIR}
  ${EDSDK_INC_DIR})

  file(GLOB SOURCE
    ${CMAKE_CURRENT_SOURCE_DIR}/src/*
    ${CMAKE_CURRENT_SOURCE_DIR}/include/*)

# Add cuda executable
cuda_add_executable(StarHunter ${SOURCE})
target_link_libraries(StarHunter LINK_PUBLIC
  ${SH_LIBS}
  ${CUDA_LIBRARIES})
