
#include "common.h"

#include <GL/glew.h>
#include <GL/glut.h>
#define _USE_MATH_DEFINES
#include <cmath>
#include <iostream>
#include <sstream>

#include "GPUGaussianConv.h"
#include "GPURecOpenGL.h"
#include "GLutils.h"
#include "DBGutils.h"

using namespace GPURec;

#define PACK_BUG 1

GLuint GPUGaussianConv::programHConv = 0;
GLuint GPUGaussianConv::programVConv = 0;
GLuint GPUGaussianConv::programVConvBackProject = 0;
GLuint GPUGaussianConv::programHConvBackProject = 0;

std::map< int, std::vector<float> > GPUGaussianConv::volumeGaussianCoefs;
GLuint GPUGaussianConv::gaussianCoefsTex = 0; 
GLuint GPUGaussianConv::packedGaussianCoefsTex = 0; 

unsigned int GPUGaussianConv::convolutionRadius = 0;
unsigned int GPUGaussianConv::projWidth;
unsigned int GPUGaussianConv::projHeight;

GLuint GPUGaussianConv::bufferTex = 0; 
bool GPUGaussianConv::initialized = false;

void GPUGaussianConv::initialize( unsigned int dim, float pixelSize ) {

  assert( dim > 0 );
  
  // compute the convolution radius : use the last plane where sigma is maximum
  // the computations has to be done in PIXEL unit !
  float distMax = (CAMERA_ROTATION_RADIUS / pixelSize) + dim/2 + 0.5;
  float sigmaMax = sqrt( ((distMax * COLLIMATOR_HOLES_DIAMETER / COLLIMATOR_DEPTH) * (distMax * COLLIMATOR_HOLES_DIAMETER / COLLIMATOR_DEPTH) + pow(CAMERA_RESOLUTION / pixelSize, 2) ) / ( 8 * M_LN2 ) ); 
  convolutionRadius = (unsigned int)( CONVOLUTION_RADIUS_TRUNCATION_FACTOR * sigmaMax );
  std::cout <<  "Convolution radius: " << convolutionRadius << std::endl;

  // compute the gaussian coefficients : There is 1 gaussian by projection plane
  float sigma = 0;
  for( unsigned int vsliceNum = 0; vsliceNum < dim; vsliceNum++ ) {
    
    float dist = (CAMERA_ROTATION_RADIUS / pixelSize) - (dim/2 - 0.5) + vsliceNum; 
  
    // compute gaussian sigma
    float Rt2 = (dist * COLLIMATOR_HOLES_DIAMETER / COLLIMATOR_DEPTH) * (dist * COLLIMATOR_HOLES_DIAMETER / COLLIMATOR_DEPTH) + pow( CAMERA_RESOLUTION / pixelSize, 2 );
    float sigma = sqrt( Rt2 / ( 8 * M_LN2 ) );

    float sum=0.0;
    for( unsigned int iFilter = 0; iFilter <= convolutionRadius; iFilter++ ) {

      float coef = exp( iFilter*iFilter / ( -2.0 * sigma * sigma ) ) / ( sqrt(2.0*M_PI) * sigma );   
  
      sum += coef;
      volumeGaussianCoefs[vsliceNum].push_back( coef );
    }  
    
    // we have the sum of a demi-gaussian, this operation deduces the whole gaussian sum
    sum = 2 * sum - volumeGaussianCoefs[vsliceNum][0];

    // normalize coefficients according to the sum
    for( unsigned int i = 0; i < volumeGaussianCoefs[vsliceNum].size(); i++ )
      volumeGaussianCoefs[vsliceNum][i] /= sum;    
  }
  
  // create texture to store convolution intermediate results
  projWidth = dim;
  projHeight = dim/4; 
  glGenTextures( 1, &bufferTex );  GL_TEST_ERROR
  glBindTexture( GL_TEXTURE_2D, bufferTex );   GL_TEST_ERROR  
  glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA16F_ARB, projWidth, projHeight, 0, GL_RGBA, GL_FLOAT, NULL );  GL_TEST_ERROR  
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, FILTERING_METHOD);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, FILTERING_METHOD);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);   
    
  
  // Definition of the horizontal convolution Fragment Program
  glGenProgramsARB(1, &programHConv);

  ////////////////// PROGRAM VERIFIED ////////////////////
  std::stringstream hcProgramCode;
  hcProgramCode << 
      "!!ARBfp1.0\n"
      //"OPTION ARB_precision_hint_fastest;\n"  
      "TEMP center;\n"
      "TEMP leftNeighboor;\n"
      "TEMP rightNeighboor;\n"
      "TEMP currentPixelCoords;\n"
      "MOV currentPixelCoords, fragment.texcoord[0];\n"
      
  // center pixel contribution        
      "TEX center, currentPixelCoords, texture[0], 2D;\n" 
      "MUL center, center, program.local[0];\n"; 

  // neighboors contributions
  for( unsigned int iFilter = 1; iFilter <= convolutionRadius; iFilter++ ) {

      hcProgramCode <<   
      "ADD currentPixelCoords.x, fragment.texcoord[0].x, " << iFilter/(float)projWidth << ";\n" 
      "TEX rightNeighboor, currentPixelCoords, texture[0], 2D;\n"  
      "SUB currentPixelCoords.x, fragment.texcoord[0].x, " << iFilter/(float)projWidth << ";\n"
      "TEX leftNeighboor, currentPixelCoords, texture[0], 2D;\n"  
      "MAD center, rightNeighboor, program.local[" << iFilter << "], center;\n"     
      "MAD center, leftNeighboor, program.local[" << iFilter << "], center;\n";   
  }
  hcProgramCode << 
      "MOV result.color, center;\n"
      "END\n";
  ///////////////////////////////////////////////////////

  glBindProgramARB(GL_FRAGMENT_PROGRAM_ARB, programHConv);  
  glProgramStringARB(GL_FRAGMENT_PROGRAM_ARB, GL_PROGRAM_FORMAT_ASCII_ARB,
                      hcProgramCode.str().length(), hcProgramCode.str().c_str() );
  
  GLint error_pos;
  glGetIntegerv(GL_PROGRAM_ERROR_POSITION_ARB, &error_pos);
  if( error_pos != -1 ) {
    const GLubyte *error_string;
    error_string = glGetString(GL_PROGRAM_ERROR_STRING_ARB);
    std::cerr << "Program error at position(" << __LINE__ << "): " << error_pos << error_string << std::endl;
    throw std::exception();
  } 


  // Definition of the vertical convolution Fragment Program
  glGenProgramsARB(1, &programVConv);

  ////////////////// PROGRAM VERIFIED ////////////////////
  std::stringstream vcProgramCode;
  vcProgramCode << 
      "!!ARBfp1.0\n"
      //"OPTION ARB_precision_hint_fastest;\n"  
      "TEMP center;\n"
      "TEMP pixelContribution;\n"
      "TEMP topNeighboor;\n"
      "TEMP bottomNeighboor;\n"
      "TEMP currentPixelCoords;\n"
      "MOV currentPixelCoords, fragment.texcoord[0];\n"

    // center pixel contribution
      "TEX pixelContribution, currentPixelCoords, texture[0], 2D;\n" 
      "DP4 center.x, pixelContribution, program.local[0].xyzw;\n" 
      "DP4 center.y, pixelContribution, program.local[0].yxyz;\n" 
      "DP4 center.z, pixelContribution, program.local[0].zyxy;\n" 
      "DP4 center.w, pixelContribution, program.local[0].wzyx;\n";

  // neighboors contributions 
  int currentCoef = 1;
  unsigned int packedConvRadius = ((int)convolutionRadius-1)/4 +1;
  for( unsigned int iFilter = 1; iFilter <= packedConvRadius; iFilter++ ) {

      vcProgramCode <<  
      "ADD currentPixelCoords.y, fragment.texcoord[0].y, " << iFilter/(float)projHeight << ";\n"
      "TEX topNeighboor, currentPixelCoords, texture[0], 2D;\n"   
      "SUB currentPixelCoords.y, fragment.texcoord[0].y, " << iFilter/(float)projHeight << ";\n"
      "TEX bottomNeighboor, currentPixelCoords, texture[0], 2D;\n"  
      
      "DP4 pixelContribution.x, topNeighboor, program.local[" << currentCoef << "].wzyx;\n" 
      "DP4 pixelContribution.y, topNeighboor, program.local[" << currentCoef+1 << "].wzyx;\n" 
      "DP4 pixelContribution.z, topNeighboor, program.local[" << currentCoef+2 << "].wzyx;\n" 
      "DP4 pixelContribution.w, topNeighboor, program.local[" << currentCoef+3 << "].wzyx;\n" 
      "ADD center, center, pixelContribution;\n"                                                  

      "DP4 pixelContribution.w, bottomNeighboor, program.local[" << currentCoef << "];\n" 
      "DP4 pixelContribution.z, bottomNeighboor, program.local[" << currentCoef+1 << "];\n" 
      "DP4 pixelContribution.y, bottomNeighboor, program.local[" << currentCoef+2 << "];\n" 
      "DP4 pixelContribution.x, bottomNeighboor, program.local[" << currentCoef+3 << "];\n" 
      "ADD center, center, pixelContribution;\n"; 

      currentCoef += 4;
  }

  vcProgramCode << 
      "MOV result.color, center;\n"
      "END\n";
  /////////////////////////////////////////////////////////
  
  
  glBindProgramARB(GL_FRAGMENT_PROGRAM_ARB, programVConv);  
  glProgramStringARB(GL_FRAGMENT_PROGRAM_ARB, GL_PROGRAM_FORMAT_ASCII_ARB,
                      vcProgramCode.str().length(), vcProgramCode.str().c_str() );
  
  glGetIntegerv(GL_PROGRAM_ERROR_POSITION_ARB, &error_pos);
  if( error_pos != -1 ) {
    const GLubyte *error_string;
    error_string = glGetString(GL_PROGRAM_ERROR_STRING_ARB);
    std::cerr << "Program error at position(" << __LINE__ << "): " << error_pos << error_string << std::endl;
    throw std::exception();
  } 
 
  computeGaussianCoefsTextures( dim );

  initialized = true;
}


void GPUGaussianConv::computeGaussianCoefsTextures( unsigned int dim ) {
  
  // the width of the texture corresponds to the maximum gaussian radius +1 for the center coef
  unsigned int maxNbCoefs = convolutionRadius + 1;

  
  // BEGIN HORIZONTAL CONVOLUTION TEXTURE AND FRAGMENT PROGRAM
  {
    // create a texture containing all convolution coeff (one line per convolution radius)
    // this is for horizontal convolution
    float *allCoef = new float[ maxNbCoefs * dim * 4 ];  
  
    int index = 0; 
    for( unsigned int j = 0; j < dim; j++ )
    for( unsigned int i = 0; i < maxNbCoefs; i++ ) {
      
      allCoef[index++] = getCoef(j,i);  // R
      allCoef[index++] = getCoef(j,i);  // v
      allCoef[index++] = getCoef(j,i);  // B
      allCoef[index++] = getCoef(j,i);  // A
    }
      
    glGenTextures( 1, &gaussianCoefsTex );  GL_TEST_ERROR
    glBindTexture( GL_TEXTURE_2D, gaussianCoefsTex );   GL_TEST_ERROR  
    glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA16F_ARB, maxNbCoefs, dim, 0, GL_RGBA, GL_FLOAT, allCoef );  GL_TEST_ERROR  
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP); 
    
    delete [] allCoef;
  
   // fragment program for Horizontal Convolution and BackProjection
    glGenProgramsARB(1, &programHConvBackProject);
  
    ////////////////// PROGRAM VERIFIED ////////////////////
    std::stringstream programCode;
    programCode << 
        "!!ARBfp1.0\n"
        "TEMP center;\n"
        "TEMP pixelContribution;\n"
        "TEMP rightNeighboor;\n"
        "TEMP leftNeighboor;\n"
        "TEMP currentCoef;\n"
        "TEMP currentPixelCoords;\n"
        "TEMP currentCoefCoords;\n"
        "MOV currentPixelCoords, fragment.texcoord[0];\n"
        "MUL currentCoefCoords.y, fragment.position.y, " <<  1.0/dim << ";\n"; // lower-left origin!
  
    // center pixel contribution
    programCode <<  
        "TEX pixelContribution, currentPixelCoords, texture[0], 2D;\n"
        // fetch coefficients to multiply with center pixel
        "MOV currentCoefCoords.x, " << 0.5/maxNbCoefs << ";\n"   
        "TEX currentCoef, currentCoefCoords, texture[1], 2D;\n" 
        "MUL center, pixelContribution, currentCoef;\n";
        
    // for each neighboors in the convolution radius
    for( unsigned int iFilter = 1; iFilter < maxNbCoefs; iFilter++ ) {
  
        programCode <<  
        // fetch the right neighboor texel 
        "ADD currentPixelCoords.x, fragment.texcoord[0].x, " << iFilter/(float)projWidth << ";\n"
        "TEX rightNeighboor, currentPixelCoords, texture[0], 2D;\n"
        
        // fetch the left neighboor texel 
        "SUB currentPixelCoords.x, fragment.texcoord[0].x, " << iFilter/(float)projWidth << ";\n"
        "TEX leftNeighboor, currentPixelCoords, texture[0], 2D;\n"
        
        // fetch the coefficients to multiply 
        "MOV currentCoefCoords.x, " << (iFilter+0.5)/maxNbCoefs << ";\n"   
        "TEX currentCoef, currentCoefCoords, texture[1], 2D;\n" 
        // add contribution to right neighboor
        "MUL pixelContribution, rightNeighboor, currentCoef;\n"
        "ADD center, center, pixelContribution;\n"
        // add contribution to left neighboor
        "MUL pixelContribution, leftNeighboor, currentCoef;\n"
        "ADD center, center, pixelContribution;\n";   
    }
  
    programCode << 
        "MOV result.color, center;\n"
        "END\n"; 
    //////////////////////////////////////////////////////////
        
        
    glBindProgramARB(GL_FRAGMENT_PROGRAM_ARB, programHConvBackProject);  
    glProgramStringARB(GL_FRAGMENT_PROGRAM_ARB, GL_PROGRAM_FORMAT_ASCII_ARB,
                        programCode.str().length(), programCode.str().c_str() );
    
    GLint error_pos;
    glGetIntegerv(GL_PROGRAM_ERROR_POSITION_ARB, &error_pos);
    if( error_pos != -1 ) {
      const GLubyte *error_string;
      error_string = glGetString(GL_PROGRAM_ERROR_STRING_ARB);
      std::cerr << "Program error at position(" << __LINE__ << "): " << error_pos << error_string << std::endl;
      throw std::exception();
    } 
    
  } // END HORIZONTAL CONVOLUTION TEXTURE AND FRAGMENT PROGRAM
      
    
  // BEGIN VERTICAL CONVOLUTION TEXTURE AND FRAGMENT PROGRAM  
  {
    // create a texture containing all convolution coeff (one line per convolution radius)
    // there are organized for maximazing efficiency of the vertical convolution fragment program
    
    // due to the fragment program computations the width has to be a power of 4 (+1 for the center coef)
    unsigned int packedTexWidth = maxNbCoefs;
    while( packedTexWidth % 4 != 1 )
      packedTexWidth++;
      
      
    float* allCoef = new float[ packedTexWidth * dim * 4 ];  
      
    int index = 0;
    for( unsigned int j = 0; j < dim; j++ )
    for( unsigned int i = 0; i < packedTexWidth;  i++ ) {
      
      // pack the current coefficient with its followers 
      allCoef[index++] = getCoef(j,i);
      allCoef[index++] = getCoef(j,i+1);
      allCoef[index++] = getCoef(j,i+2);
      allCoef[index++] = getCoef(j,i+3);
    }
    
    assert( index ==  packedTexWidth * dim * 4 );
    
    glGenTextures( 1, &packedGaussianCoefsTex );  GL_TEST_ERROR
    glBindTexture( GL_TEXTURE_2D, packedGaussianCoefsTex );   GL_TEST_ERROR  
    glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA16F_ARB, packedTexWidth, dim, 0, GL_RGBA, GL_FLOAT, allCoef );  GL_TEST_ERROR  
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP); 
    
//     GLutils::memToPPM( allCoef, packedTexWidth, dim );
  
    delete [] allCoef;
    
    
    // fragment program for Vertical Convolution and BackProjection
    glGenProgramsARB(1, &programVConvBackProject);
 
#ifndef PACK_BUG
///////////////////////////////////////////////////////////////////////////////////////////
    ////////////////// PROGRAM VERIFIED ////////////////////
    std::stringstream programCode;
    programCode << 
        "!!ARBfp1.0\n"
        "TEMP center;\n"
        "TEMP pixelContribution;\n"
        "TEMP topNeighboor;\n"
        "TEMP bottomNeighboor;\n"
        "TEMP currentCoef;\n"
        "TEMP currentPixelCoords;\n"
        "TEMP currentCoefCoords;\n"
        "MOV currentPixelCoords, fragment.texcoord[0];\n"
        "MUL currentCoefCoords.y, fragment.position.y, " <<  1.0/dim << ";\n" // lower-left origin!
        "MOV center, 0;\n";

    // center pixel contribution
    programCode <<  
        "TEX pixelContribution, currentPixelCoords, texture[0], 2D;\n"
        // fetch coefficients to multiply with center pixel
        "MOV currentCoefCoords.x, " << 0.5/packedTexWidth << ";\n"   
        "TEX currentCoef, currentCoefCoords, texture[1], 2D;\n" 
        "DP4 center.x, pixelContribution, currentCoef.xyzw;\n"
        "DP4 center.y, pixelContribution, currentCoef.yxyz;\n"
        "DP4 center.z, pixelContribution, currentCoef.zyxy;\n"
        "DP4 center.w, pixelContribution, currentCoef.wzyx;\n";
    
    // for each right neighboor in the convolution radius
    float currentRadius = 1.5;
    unsigned int packedConvRadius = ((int)maxNbCoefs-2)/4 +1;
    for( unsigned int iFilter = 1; iFilter <= packedConvRadius; iFilter++ ) {
  
        programCode <<  
        // fetch the bottom neighboor texel 
        "ADD currentPixelCoords.y, fragment.texcoord[0].y, " << iFilter/(float)projHeight << ";\n"
        "TEX topNeighboor, currentPixelCoords, texture[0], 2D;\n"
        
        // fetch the top neighboor texel 
        "SUB currentPixelCoords.y, fragment.texcoord[0].y, " << iFilter/(float)projHeight << ";\n"
        "TEX bottomNeighboor, currentPixelCoords, texture[0], 2D;\n"
        
        // fetch coefficients 1..4
        "MOV currentCoefCoords.x, " << currentRadius++/packedTexWidth << ";\n"   
        "TEX currentCoef, currentCoefCoords, texture[1], 2D;\n" 
        "DP4 pixelContribution.w, bottomNeighboor, currentCoef;\n"    // ALPHA channel
        "DP4 pixelContribution.x, topNeighboor, currentCoef.wzyx;\n"  // RED channel
        
        // fetch coefficients 2..5
        "MOV currentCoefCoords.x, " << currentRadius++/packedTexWidth << ";\n" 
        "TEX currentCoef, currentCoefCoords, texture[1], 2D;\n" 
        "DP4 pixelContribution.z, bottomNeighboor, currentCoef;\n"    // BLUE channel
        "DP4 pixelContribution.y, topNeighboor, currentCoef.wzyx;\n"  // GREEN channel
        
        // add contribution
        "ADD center, center, pixelContribution;\n"
        
        // fetch coefficients 3..6
        "MOV currentCoefCoords.x, " << currentRadius++/packedTexWidth << ";\n" 
        "TEX currentCoef, currentCoefCoords, texture[1], 2D;\n" 
        "DP4 pixelContribution.y, bottomNeighboor, currentCoef;\n"    // GREEN channel
        "DP4 pixelContribution.z, topNeighboor, currentCoef.wzyx;\n"  // BLUE channel
        
        // fetch coefficients 4..7
        "MOV currentCoefCoords.x, " << currentRadius++/packedTexWidth << ";\n" 
        "TEX currentCoef, currentCoefCoords, texture[1], 2D;\n" 
        "DP4 pixelContribution.x, bottomNeighboor, currentCoef;\n"    // RED channel
        "DP4 pixelContribution.w, topNeighboor, currentCoef.wzyx;\n"  // ALPHA channel
        
        // add contribution
        "ADD center, center, pixelContribution;\n";        
    }
    
//     std::cout << "currentRadius: " << currentRadius << "packedTexWidth: " << packedTexWidth << "convRad: " << maxNbCoefs-1<< std::endl;   
    assert( currentRadius == packedTexWidth + 0.5);
  
    programCode << 
        "MOV result.color, center;\n"
        "END\n";
    ///////////////////////////////////////////////////////////////////////////////////////////
#else
    
    ////////////////// PROGRAM VERIFIED ////////////////////
    std::stringstream programCode;
    programCode << 
        "!!ARBfp1.0\n"
        "TEMP center;\n"
        "TEMP pixelContribution;\n"
        "TEMP topNeighboor;\n"
        "TEMP bottomNeighboor;\n"
        "TEMP currentCoef;\n"
        "TEMP currentPixelCoords;\n"
        "TEMP currentCoefCoords;\n"
        "MOV currentPixelCoords, fragment.texcoord[0];\n"
        "MUL currentCoefCoords.y, fragment.position.y, " <<  1.0/dim << ";\n" // lower-left origin!
        "MOV center, 0;\n";

    // center pixel contribution
    programCode <<  
        "TEX pixelContribution, currentPixelCoords, texture[0], 2D;\n"
        // fetch coefficients to multiply with center pixel
        "MOV currentCoefCoords.x, " << 0.5/packedTexWidth << ";\n"   
        "TEX currentCoef, currentCoefCoords, texture[1], 2D;\n" 
        "DP4 center.x, pixelContribution, currentCoef.xyzw;\n"
        "DP4 center.y, pixelContribution, currentCoef.yxyz;\n"
        "DP4 center.z, pixelContribution, currentCoef.zyxy;\n"
        "DP4 center.w, pixelContribution, currentCoef.wzyx;\n";
    
    // for each right neighboor in the convolution radius
    float currentRadius = 1.5;
    unsigned int packedConvRadius = (maxNbCoefs-2)/4 +1;
    for( unsigned int iFilter = 1; iFilter <= packedConvRadius; iFilter++ ) {
  
        programCode <<  
        // fetch the bottom neighboor texel 
        "ADD currentPixelCoords.y, fragment.texcoord[0].y, " << iFilter/(float)projHeight << ";\n"
        "TEX topNeighboor, currentPixelCoords, texture[0], 2D;\n"
        
        // fetch the top neighboor texel 
        "SUB currentPixelCoords.y, fragment.texcoord[0].y, " << iFilter/(float)projHeight << ";\n"
        "TEX bottomNeighboor, currentPixelCoords, texture[0], 2D;\n"
        
        // fetch coefficients 1..4
        "MOV currentCoefCoords.x, " << currentRadius++/packedTexWidth << ";\n"   
        "TEX currentCoef, currentCoefCoords, texture[1], 2D;\n" 
        "DP4 pixelContribution.x, bottomNeighboor, currentCoef;\n"    // ALPHA channel
        "DP4 pixelContribution.w, topNeighboor, currentCoef.wzyx;\n"  // RED channel
        
        // fetch coefficients 2..5
        "MOV currentCoefCoords.x, " << currentRadius++/packedTexWidth << ";\n" 
        "TEX currentCoef, currentCoefCoords, texture[1], 2D;\n" 
        "DP4 pixelContribution.y, bottomNeighboor, currentCoef;\n"    // BLUE channel
        "DP4 pixelContribution.z, topNeighboor, currentCoef.wzyx;\n"  // GREEN channel
        
        // add contribution
        "ADD center, center, pixelContribution;\n"
        
        // fetch coefficients 3..6
        "MOV currentCoefCoords.x, " << currentRadius++/packedTexWidth << ";\n" 
        "TEX currentCoef, currentCoefCoords, texture[1], 2D;\n" 
        "DP4 pixelContribution.z, bottomNeighboor, currentCoef;\n"    // GREEN channel
        "DP4 pixelContribution.y, topNeighboor, currentCoef.wzyx;\n"  // BLUE channel
        
        // fetch coefficients 4..7
        "MOV currentCoefCoords.x, " << currentRadius++/packedTexWidth << ";\n" 
        "TEX currentCoef, currentCoefCoords, texture[1], 2D;\n" 
        "DP4 pixelContribution.w, bottomNeighboor, currentCoef;\n"    // RED channel
        "DP4 pixelContribution.x, topNeighboor, currentCoef.wzyx;\n"  // ALPHA channel
        
        // add contribution
        "ADD center, center, pixelContribution;\n";        
    }
   
    assert( currentRadius == packedTexWidth + 0.5);
  
    programCode << 
        "MOV result.color, center;\n"
        "END\n";
    ///////////////////////////////////////////////////////////////////////////////////////////
#endif 
        
    glBindProgramARB(GL_FRAGMENT_PROGRAM_ARB, programVConvBackProject);  
    glProgramStringARB(GL_FRAGMENT_PROGRAM_ARB, GL_PROGRAM_FORMAT_ASCII_ARB,
                        programCode.str().length(), programCode.str().c_str() );
    
    GLint error_pos;
    glGetIntegerv(GL_PROGRAM_ERROR_POSITION_ARB, &error_pos);
    if( error_pos != -1 ) {
      const GLubyte *error_string;
      error_string = glGetString(GL_PROGRAM_ERROR_STRING_ARB);
      std::cerr << "Program error at position(" << __LINE__ << "): " << error_pos << error_string << std::endl;
      throw std::exception();
    } 
  } // END VERTICAL CONVOLUTION TEXTURE AND FRAGMENT PROGRAM
} 


void GPUGaussianConv::convolveTexture( GLuint inputTex, unsigned int vsliceNum ) {

  DBGutils::timerBegin("conv");
  assert( initialized ); // the initialize() method has to be called first!
  
  // save FBO attachment 
  GLint outputTex;
  glGetFramebufferAttachmentParameterivEXT( GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME_EXT, &outputTex);
  
  glPushAttrib( GL_ENABLE_BIT ); GL_TEST_ERROR
  
  // convolve the plane HORIZONTALLY 
  glFramebufferTexture2DEXT( GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_2D, bufferTex, 0 ); GL_TEST_ERROR
  glClear(GL_COLOR_BUFFER_BIT); 
  
  glBindTexture( GL_TEXTURE_2D, inputTex );

  glEnable( GL_FRAGMENT_PROGRAM_ARB );
  glBindProgramARB( GL_FRAGMENT_PROGRAM_ARB, programHConv );  GL_TEST_ERROR
  
  for( unsigned int iFilter = 0; iFilter < volumeGaussianCoefs[vsliceNum].size(); iFilter++ )
    glProgramLocalParameter4fARB( GL_FRAGMENT_PROGRAM_ARB, iFilter, getCoef(vsliceNum,iFilter), getCoef(vsliceNum,iFilter), getCoef(vsliceNum,iFilter), getCoef(vsliceNum,iFilter) );
     
  glBegin(GL_QUADS);
    glTexCoord2f( 0, 0 ); glVertex3f( -1, 0, -1); 
    glTexCoord2f( 1, 0 ); glVertex3f( 1, 0, -1); 
    glTexCoord2f( 1, 1 ); glVertex3f( 1, 0, 1); 
    glTexCoord2f( 0, 1 ); glVertex3f( -1, 0, 1);
  glEnd();
 
  glDisable( GL_FRAGMENT_PROGRAM_ARB );  

  // convolve the plane VERTICALY 
  glFramebufferTexture2DEXT( GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_2D, outputTex, 0 ); GL_TEST_ERROR

  glBindTexture( GL_TEXTURE_2D, bufferTex );
  
  glEnable( GL_FRAGMENT_PROGRAM_ARB );
  glBindProgramARB( GL_FRAGMENT_PROGRAM_ARB, programVConv );  GL_TEST_ERROR

  // due to the fragment program computations the width has to be a power of 4 (+1 for the center coef)
  unsigned int nbCoefs = convolutionRadius +1;
  while( nbCoefs % 4 != 1 )
    nbCoefs++;

  for( unsigned int iFilter = 0; iFilter <= nbCoefs; iFilter++ )     
    glProgramLocalParameter4fARB( GL_FRAGMENT_PROGRAM_ARB, iFilter, getCoef(vsliceNum,iFilter), getCoef(vsliceNum,iFilter+1), getCoef(vsliceNum,iFilter+2), getCoef(vsliceNum,iFilter+3) );
  
  glBegin(GL_QUADS);
    glTexCoord2f( 0, 0 ); glVertex3f( -1, 0, -1); 
    glTexCoord2f( 1, 0 ); glVertex3f( 1, 0, -1); 
    glTexCoord2f( 1, 1 ); glVertex3f( 1, 0, 1); 
    glTexCoord2f( 0, 1 ); glVertex3f( -1, 0, 1);
  glEnd();                            

  glPopAttrib();

  DBGutils::timerEnd("conv");
}


void GPUGaussianConv::convolveAndBackProject( GLuint inputTex, float angle, unsigned int hsliceNum ) {

  DBGutils::timerBegin("convback");

  assert( initialized ); // the init() method has to be called first!
  
  glPushAttrib( GL_ENABLE_BIT ); GL_TEST_ERROR

  glActiveTextureARB(GL_TEXTURE0_ARB);  GL_TEST_ERROR 
  glBindTexture( GL_TEXTURE_2D, inputTex ); GL_TEST_ERROR  
  
   glActiveTextureARB(GL_TEXTURE1_ARB);  GL_TEST_ERROR 
   glEnable( GL_TEXTURE_2D );
   glBindTexture( GL_TEXTURE_2D, packedGaussianCoefsTex ); GL_TEST_ERROR  
   
   glEnable( GL_FRAGMENT_PROGRAM_ARB );
   glBindProgramARB( GL_FRAGMENT_PROGRAM_ARB, programVConvBackProject );  GL_TEST_ERROR
  
  glFramebufferTexture2DEXT( GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_2D, GPURecOpenGL::tex1Dim, 0 ); GL_TEST_ERROR 
  glClear(GL_COLOR_BUFFER_BIT);
  
  glBegin(GL_QUADS);
    glTexCoord2f( 0, (0.5+hsliceNum)/projHeight ); glVertex3f(-1,-1,0); 
    glTexCoord2f( 1, (0.5+hsliceNum)/projHeight ); glVertex3f(1,-1,0); 
    glTexCoord2f( 1, (0.5+hsliceNum)/projHeight ); glVertex3f(1,1,0); 
    glTexCoord2f( 0, (0.5+hsliceNum)/projHeight ); glVertex3f(-1,1,0);
  glEnd();  
  
  glActiveTextureARB(GL_TEXTURE1_ARB);  GL_TEST_ERROR 
  glDisable( GL_TEXTURE_2D );
  glActiveTextureARB(GL_TEXTURE0_ARB);  GL_TEST_ERROR 
  glDisable( GL_FRAGMENT_PROGRAM_ARB );

  // HORIZONTAL CONVOLUTION
  glFramebufferTexture2DEXT( GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_2D, GPURecOpenGL::tex2Dim, 0 ); GL_TEST_ERROR 
  glActiveTextureARB(GL_TEXTURE0_ARB);  GL_TEST_ERROR 
  glBindTexture( GL_TEXTURE_2D, GPURecOpenGL::tex1Dim ); GL_TEST_ERROR  
  
   glActiveTextureARB(GL_TEXTURE1_ARB);  GL_TEST_ERROR 
   glEnable( GL_TEXTURE_2D );
   glBindTexture( GL_TEXTURE_2D, gaussianCoefsTex ); GL_TEST_ERROR  
    
   glEnable( GL_FRAGMENT_PROGRAM_ARB );
   glBindProgramARB( GL_FRAGMENT_PROGRAM_ARB, programHConvBackProject );  GL_TEST_ERROR
    
  glRotated( angle*180.0/M_PI,  0,0,1 );
  
  glBegin(GL_QUADS);
    glTexCoord2f( 0, 0 ); glVertex3f(-1,-1,0); 
    glTexCoord2f( 1, 0 ); glVertex3f(1,-1,0); 
    glTexCoord2f( 1, 1 ); glVertex3f(1,1,0); 
    glTexCoord2f( 0, 1 ); glVertex3f(-1,1,0);
  glEnd(); 
  
  glLoadIdentity();
  
  glActiveTextureARB(GL_TEXTURE1_ARB);  GL_TEST_ERROR 
  glDisable( GL_TEXTURE_2D );
  glActiveTextureARB(GL_TEXTURE0_ARB);  GL_TEST_ERROR 
  
  glPopAttrib(); 

  DBGutils::timerEnd("convback");
} 

void GPUGaussianConv::terminate( void ) {

  volumeGaussianCoefs.clear();
    
  if( programVConv ) {
    glDeleteProgramsARB( 1, &programVConv );
    programVConv = 0;
  }
    
  if( programHConv ) {
    glDeleteProgramsARB( 1, &programHConv );
    programHConv = 0;
  }
    
  if( programVConvBackProject ) {
    glDeleteProgramsARB( 1, &programVConvBackProject );   
    programVConvBackProject = 0;
  }
    
  if( programHConvBackProject ) {
    glDeleteProgramsARB( 1, &programHConvBackProject );   
    programHConvBackProject = 0;
  }
    
  if( bufferTex ) {
    glDeleteTextures(1, &bufferTex);
    bufferTex = 0;
  }
  
  if( gaussianCoefsTex ) {
    glDeleteTextures(1, &gaussianCoefsTex);
    gaussianCoefsTex = 0;
  }
  
  if( packedGaussianCoefsTex ) {
    glDeleteTextures(1, &packedGaussianCoefsTex);
    packedGaussianCoefsTex = 0;
  }
}
