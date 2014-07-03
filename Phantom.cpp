
#include "common.h"

#define _USE_MATH_DEFINES
#include <cmath>

#include "Phantom.h"
#include "VolumeProjectionSet.h"
#include "GPURecOpenGL.h"
#include "GLutils.h"

using namespace GPURec;


void Phantom::create( PhantomType type, unsigned int _dim ) {

  reset( _dim );
 
  if( type == HEMISPHERE ) {

    for(unsigned int i = 0; i < dim; i++ ) 
    for(unsigned int j = 0; j < dim; j++ )
    for(unsigned int k = 0; k < dim; k++ ) {
    
      float worldx = volumeToWorldCoord(i);
      float worldy = volumeToWorldCoord(j);
      float worldz = volumeToWorldCoord(k);
      float worldLength = sqrt( worldx*worldx + worldy*worldy + worldz*worldz ); 
      
      // default color : black
      float val = 0.0f;
        
      // put an hemisphere
      if( worldLength < 0.9 && worldy > 0.0 )
        val = 0.5f;
        
      // add a smaller sphere 
      if( worldLength < 0.6 && (worldy > 0.0 || (worldz < 0.0 && worldx < 0.0) ) )
        val = 0.8f;                
          
      if( worldLength < 0.4 )
        val = 0.95f;   
        
      value(i,j,k) = val;  
    }  
  }
  
  if( type == RANDOM_VALUES ) {     
    
    int firstReliableRec = (int)( dim * (sqrt(2.0)-1.0)/2.2 );

    for(unsigned int i = firstReliableRec; i < dim-firstReliableRec; i++ ) 
    for(unsigned int j = firstReliableRec; j < dim-firstReliableRec; j++ )
    for(unsigned int k = 0; k < dim; k++ )   
      value(i,j,k) = std::rand() / (float)RAND_MAX;
  }
  
  if( type == PH_POINT ) {
 
    for(unsigned int i = 0; i < dim; i++ ) 
    for(unsigned int j = 0; j < dim; j++ )
    for(unsigned int k = 0; k < dim; k++ )

      /*if( i == dim/2 && j == dim/2 && k == dim/2 )*/
      if( i == dim/2 && j == dim/2 && k == 0 )
        value( i, j, k ) = 1.0f;  
      else
        value(i,j,k) = 0.f; 
  }
  
  if( type == PH_LINE ) {

    for(unsigned int i = 0; i < dim; i++ ) 
    for(unsigned int j = 0; j < dim; j++ )
    for(unsigned int k = 0; k < dim; k++ ) {
      if( i == dim/2 && k == dim/2 ) 
        value(i,j,k) = 1.0;
      else
        if( i == (dim/2 -1) && k == dim/2 ) 
          value(i,j,k) = 0.6f;
        else
          value(i,j,k) = 0.0f;
    }
  }
  
  if( type == PH_STRIPS ) {

    for(unsigned int i = 0; i < dim; i++ ) 
    for(unsigned int j = 0; j < dim; j++ )
    for(unsigned int k = 0; k < dim; k++ ) {
      if( k % 2 == 0 ) 
        value(i,j,k) = 1.0;
      else
        value(i,j,k) = 0.0f;
    }
  }
  
  maxValue = 1.0;

  // take only values in the cylinder
  for(unsigned int i = 0; i < dim; i++ ) 
  for(unsigned int j = 0; j < dim; j++ )
  for(unsigned int k = 0; k < dim; k++ ) {
    
      float worldx = volumeToWorldCoord(i);
      float worldy = volumeToWorldCoord(j);
      float worldLength = sqrt( worldx*worldx + worldy*worldy ); 
  
      if( worldLength > 1 )
        value(i,j,k) = 0.0f;     
  }
  
  sendToGraphicMemory();
}
         

void Phantom::saveProjections( VolumeProjectionSet& projSet, unsigned int nbProjections ) {

  projSet.createEmpty( dim, nbProjections );
  
  glViewport( 0, 0, dim, dim/4 );
  
  // load projections textures
  float angle = projSet.getStartAngle();
  for( unsigned int p = 0; p < nbProjections; p++ ) {
    
    // render to texture
    glFramebufferTexture2DEXT( GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_2D, projSet.getTexId(p), 0 ); GL_TEST_ERROR

    projection( angle, false ); 
     
    angle += projSet.getRotationIncrement();
  }  
  
  // detach texture from FBO
  glFramebufferTexture2DEXT( GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_2D, GPURecOpenGL::texQuarterDim, 0 );  GL_TEST_ERROR
    
  glViewport( 0, 0, dim, dim );  

}
