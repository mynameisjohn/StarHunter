cmake_minimum_required(VERSION 2.8)

# Project needs c++14 for std::integer_sequence 
set(CMAKE_CXX_FLAGS "-std=c++14 -Wall")

# These paths work for me on windows and unix, but they may need changing
if (WIN32)
	# This works for me on windows for python3.5
	# (though I occasionally link debug)
    set(PYTHON_LIB_DIR C:/Python35/libs)
	set(PYTHON_LIBRARY python35.lib)
	set(PYTHON_INCLUDE_DIR C:/Python35/include)
elseif(APPLE)
	# This is where brew put the python dev libraries
    set(PYTHON_LIB_DIR /usr/local/lib)
	set(PYTHON_LIBRARY python3.5m.a)
	set(PYTHON_INCLUDE_DIR /usr/local/include/python3.5m)
elseif(UNIX)
    # This is where I found the python3 libraries on ubuntu	(YMMV)
    set(PYTHON_LIB_DIR /usr/lib/x86_64-linux-gnu)
	set(PYTHON_LIBRARY python3.5m.so)
	set(PYTHON_INCLUDE_DIR /usr/include/python3.5m)
endif(WIN32)

# Several headers, just one source file
file(GLOB PYL_SRC ${CMAKE_CURRENT_SOURCE_DIR}/*)

# Move variables up to parent scope
set(PYTHON_LIB_DIR ${PYTHON_LIB_DIR} PARENT_SCOPE)
set(PYTHON_LIBRARY ${PYTHON_LIBRARY} PARENT_SCOPE)
set(PYTHON_INCLUDE_DIR ${PYTHON_INCLUDE_DIR} PARENT_SCOPE)
set(PYL_SRC ${PYL_SRC} PARENT_SCOPE)

# Declare PyLiaison library
add_library(PyLiaison ${PYL_SRC})

# We need the include path and the python header file location
target_include_directories(PyLiaison PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/pyl ${PYTHON_INCLUDE_DIR})
