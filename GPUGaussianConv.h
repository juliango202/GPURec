
#ifndef _GPUGAUSSIANCONV_H
#define _GPUGAUSSIANCONV_H

#include <vector>
#include <map>


namespace GPURec {


// This class compute the gaussian convolution of an image on the GPU
class GPUGaussianConv {

public:

  static void initialize( unsigned int dim, float pixelSize );
  static void terminate( void );  
  static void reset( unsigned int dim, float pixelSize ) { terminate(); initialize(dim, pixelSize); }
 
  static void convolveTexture       ( GLuint inputTex, unsigned int vsliceNum ); 
  static void convolveAndBackProject( GLuint inputTex, float angle, unsigned int hsliceNum ); 
    
private:

  // create textures storing the gaussian coefficients
  static void computeGaussianCoefsTextures( unsigned int dim ); 
  
  // return the value of the gaussian coef or zero if outside the domain definition
  static float getCoef( unsigned int vsliceNum, unsigned int index ) { return (index < volumeGaussianCoefs[vsliceNum].size()) ? volumeGaussianCoefs[vsliceNum][index] : 0.0; }
  
  
  // container and textures to store gaussian coefficients
  static std::map<int /*vsliceNum*/, std::vector<float> /*gaussianCoefs*/> volumeGaussianCoefs;
  static GLuint gaussianCoefsTex; 
  static GLuint packedGaussianCoefsTex; 
  
  // fragment programs to perform convolutions
  static GLuint programHConv;
  static GLuint programVConv;
  static GLuint programHConvBackProject;
  static GLuint programVConvBackProject; 
  
  // convolution size variables
  static unsigned int convolutionRadius;
  static unsigned int projWidth;
  static unsigned int projHeight;
  
  static GLuint bufferTex; 
  static bool initialized;
  
};


} // end namespace GPURec

#endif  // _GPUGAUSSIANCONV_H
