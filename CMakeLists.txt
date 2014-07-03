PROJECT (GPURec)

# Search for libraries
#
INCLUDE(${CMAKE_ROOT}/Modules/FindOpenGL.cmake)
INCLUDE(${CMAKE_ROOT}/Modules/FindGLUT.cmake)

# Initialization
#
SET(  SOURCES 
      main.cpp 
      GPURecOpenGL.cpp
      GPUGaussianConv.cpp
      Volume.cpp
      VolumeProjectionSet.cpp
      Phantom.cpp
      io/HdrFile.cpp
      tools/GLutils.cpp
      tools/DBGutils.cpp)

SET_SOURCE_FILES_PROPERTIES(${SOURCES} COMPILE_FLAGS -DDEBUG)

# Project setup
#
SET(EXECUTABLE_OUTPUT_PATH "..")
SET(INCLUDE_DIRS .. ../tools ../io)

# GLEW setup
#
SET(INCLUDE_DIRS ${INCLUDE_DIRS} /usr/local/glew/include)
SET(GLEW_LIBRARY /usr/local/glew/lib/libGLEW.so)


# Build and Link
#
INCLUDE_DIRECTORIES(${INCLUDE_DIRS})
ADD_EXECUTABLE( GPURec ${SOURCES})                          
TARGET_LINK_LIBRARIES(GPURec ${OPENGL_LIBRARIES} ${GLUT_LIBRARY} ${GLEW_LIBRARY} )
