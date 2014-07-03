
#ifndef _COMMON_H
#define _COMMON_H

#include <GL/glew.h>
#include <GL/glut.h>

namespace GPURec {


#define GPUREC_PROGRAM_DATE "August 2005"
const float GPUREC_PROGRAM_VERSION = 0.75f;

// =================== PARAMETERS =========================== //

#define DEFAULT_CONFIG_FILENAME "config.txt"
const float DEFAULT_PIXEL_SIZE = 0.002f;

const GLenum FILTERING_METHOD = GL_NEAREST;

// #define PACK_BUG 1

const float CONVOLUTION_RADIUS_TRUNCATION_FACTOR = 3.5f;

const unsigned int START_ANGLE_SHIFT = 90;  // in DEGREES


// =================== EXTERN PARAMETERS =========================== //

// CAMERA PARAMATERS
extern float CAMERA_ROTATION_RADIUS;
extern float CAMERA_RESOLUTION;
extern float COLLIMATOR_HOLES_DIAMETER;
extern float COLLIMATOR_DEPTH;

// RECONSTRUCTION PARAMETERS
extern bool USE_OSEM3D;
extern unsigned int NB_SUBSETS;
extern unsigned int NB_ITERATIONS;


} // end namespace GPURec

#ifdef _WIN32
#define isnan _isnan
#endif

#include <assert.h>

#ifdef _DEBUG
#define DEBUG
#endif


#endif  //  _COMMON_H
