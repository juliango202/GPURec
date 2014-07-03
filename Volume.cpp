
#include "common.h"

#define _USE_MATH_DEFINES
#include <cmath>
#include <limits>
#include <climits>
#include <fstream>
#include <sstream>

#include "Volume.h"
#include "GPURecOpenGL.h"
#include "GPUGaussianConv.h"
#include "GLutils.h"
#include "DBGutils.h"

using namespace GPURec;

// ============================================================================================ //
// -------------------------------------------------------------------------------------------- //
//  CONSTRUCTOR - DESTRUCTOR
// ============================================================================================ //
// -------------------------------------------------------------------------------------------- //

Volume::Volume() {

  data = 0;
  dim = 0;
  volumeTex = 0;
}


void Volume::terminate(void) {
 
  delete [] data;
  data = 0;
        
  if( volumeTex ) {
    glDeleteTextures(1, &volumeTex);
    volumeTex = 0;
  } 
}


void Volume::initialize( unsigned int _dim ) {
    
  dim = _dim;
  data = new float[ dim * dim *dim ];
  
  // create texture to store convolution result 
  glGenTextures( 1, &vsliceTex );  GL_TEST_ERROR
  glBindTexture( GL_TEXTURE_2D, vsliceTex );   GL_TEST_ERROR  
  glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA16F_ARB, dim, dim/4, 0, GL_RGBA, GL_FLOAT, NULL );  GL_TEST_ERROR  
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, FILTERING_METHOD);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, FILTERING_METHOD);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);  
}



// ============================================================================================ //
// -------------------------------------------------------------------------------------------- //
//  IO methods
// ============================================================================================ //
// -------------------------------------------------------------------------------------------- //

void Volume::createEmpty( unsigned int _dim ) {
  
  reset( _dim );
 
  for(unsigned int i = 0; i < dim; i++ ) 
  for(unsigned int j = 0; j < dim; j++ )
  for(unsigned int k = 0; k < dim; k++ ) {
  
    value(i,j,k) = 1.0f;
    
    /*// drop values outside cylinder
    float worldx = volumeToWorldCoord(i);
    float worldy = volumeToWorldCoord(j);
    float worldLength = sqrt( worldx*worldx + worldy*worldy ); 
  
    if( worldLength > 1 )
      value(i,j,k) = 0.0f;     */  
  }
    
  maxValue = 1.0;

  sendToGraphicMemory();
}


void Volume::saveToRAW( const std::string& fileName, bool append ) const {

  std::ofstream file;
  
  if( append )
    file.open( fileName.c_str(), std::ios::app | std::ios::binary );
  else
    file.open( fileName.c_str(), std::ios::out | std::ios::binary );  
    
  if( !file ) {
    std::cerr << "error creating file " << fileName << std::endl;
    throw std::exception();
  }

  for( int k = dim-1; k >= 0; k-- )
  for( int j = dim-1; j >= 0; j-- )
  for( unsigned int i = 0; i < dim; i++ ) {
   
	  unsigned short uinb = (unsigned short)( (value(i,j,k)*1000) / (maxValue+1) );  // map the value in the unsigned short range
	  file.write( (char*)&uinb, sizeof(unsigned short) );
  }
  
  file.close();
}
  

// ============================================================================================ //
// -------------------------------------------------------------------------------------------- //
//  MATHEMATICAL TRANSFORMS
// ============================================================================================ //
// -------------------------------------------------------------------------------------------- //

extern int thePlaneNum;
extern bool onePlane;
extern bool pleinplan;

void Volume::projection( double angle, bool convolve ) const {

  glClear(GL_COLOR_BUFFER_BIT);
  
  // save current FBO attachment
  int projTex = 0;
  glGetFramebufferAttachmentParameterivEXT( GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME_EXT, &projTex);
  assert( projTex > 0 );
      
  // active texture and blending
  glPushAttrib( GL_ENABLE_BIT | GL_TRANSFORM_BIT ); GL_TEST_ERROR
  
  glEnable(GL_TEXTURE_3D); GL_TEST_ERROR
  glBindTexture( GL_TEXTURE_3D, volumeTex ); GL_TEST_ERROR
  
  glEnable(GL_BLEND); GL_TEST_ERROR
  glBlendFunc( GL_ONE, GL_ONE ); GL_TEST_ERROR
  
  // setup the view 
  GPURecOpenGL::sideView();  
  
  // active some clip planes to not draw outside the volume   
  glMatrixMode( GL_MODELVIEW );
  glPushMatrix();
  glRotated( angle*180.0/M_PI,  0, 0, -1);  
  static double eq0[] = { 1,0,0,1 };  // VOLUME LEFT PLANE
  glClipPlane( GL_CLIP_PLANE0, eq0 ); 
  static double eq1[] = { -1,0,0,1 }; // VOLUME RIGHT PLANE
  glClipPlane( GL_CLIP_PLANE1, eq1 ); 
  static double eq2[] = { 0,-1,0,1 }; // VOLUME BACK PLANE
  glClipPlane( GL_CLIP_PLANE2, eq2 ); 
  static double eq3[] = { 0,1,0,1 };  // VOLUME FRONT PLANE
  glClipPlane( GL_CLIP_PLANE3, eq3 );   
  glPopMatrix();     
  glEnable( GL_CLIP_PLANE0 );
  glEnable( GL_CLIP_PLANE1 );
  glEnable( GL_CLIP_PLANE2 );
  glEnable( GL_CLIP_PLANE3 );
  
  // view angle is generated by rotating the texture matrix
  glMatrixMode( GL_TEXTURE );
  glLoadIdentity(); 
  glTranslatef( 0.5, 0.5, 0.5 );   
  glRotated( angle*180.0/M_PI,  0, 0, 1);  // rotation of axis z make the rotation clockwise
  glTranslatef( -0.5, -0.5, -0.5 );  
      
  // the volume is sampled by *dim* planes perpendicular to the projection axis
  float deb =0;
  float fin = dim;
  if( onePlane ) {
  
    deb = thePlaneNum;
    fin = thePlaneNum + 0.1;
  }
  for( float vsliceNum = deb; vsliceNum < fin; vsliceNum+= (pleinplan?0.1:1) ) {

    glEnable(GL_TEXTURE_3D); GL_TEST_ERROR

    // save current plane in a 2D texture
    
    if( convolve ) {
    
      glFramebufferTexture2DEXT( GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_2D, vsliceTex, 0 ); GL_TEST_ERROR    
      glClear(GL_COLOR_BUFFER_BIT);
    }
    else {
       glFramebufferTexture2DEXT( GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_2D, projTex, 0 ); GL_TEST_ERROR     
    }
      
    glBegin(GL_QUADS);
        glTexCoord3f( 0, (0.5+vsliceNum)/(float)dim, 0 ); glVertex3f( -1, volumeToWorldCoord(vsliceNum), -1); 
        glTexCoord3f( 1, (0.5+vsliceNum)/(float)dim, 0 ); glVertex3f( 1, volumeToWorldCoord(vsliceNum), -1); 
        glTexCoord3f( 1, (0.5+vsliceNum)/(float)dim, 1 ); glVertex3f( 1, volumeToWorldCoord(vsliceNum), 1); 
        glTexCoord3f( 0, (0.5+vsliceNum)/(float)dim, 1 ); glVertex3f( -1, volumeToWorldCoord(vsliceNum), 1);
    glEnd();
    
    glDisable( GL_TEXTURE_3D );
    
    // reinit texture matrix
    glMatrixMode( GL_TEXTURE );
    glPushMatrix();
    glLoadIdentity(); 
    
    // convolve
    if( convolve ) {
      glFramebufferTexture2DEXT( GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_2D, projTex, 0 ); GL_TEST_ERROR     
      GPUGaussianConv::convolveTexture( vsliceTex, vsliceNum );
    }

    glMatrixMode( GL_TEXTURE );
    glPopMatrix();      
  }        

  glMatrixMode( GL_TEXTURE );
  glLoadIdentity();  
  glPopAttrib();

}


void Volume::downSample( Volume &destVolume ) {

  assert( destVolume.dim == dim / 2 );
  
  for( unsigned int i = 0; i < dim; i+=2 )
  for( unsigned int j = 0; j < dim; j+=2 )
  for( unsigned int k = 0; k < dim; k+=2 ) {
  
    float sum = 0.f;
    
    sum += value(i,j,k);  
    sum += value(i+1,j,k);  
    sum += value(i,j+1,k);
    sum += value(i+1,j+1,k);    
    
    sum += value(i,j,k+1);  
    sum += value(i+1,j,k+1);  
    sum += value(i,j+1,k+1);
    sum += value(i+1,j+1,k+1);    
  
    destVolume.value(i/2,j/2,k/2) = sum / 8.f;
  }
}

// ============================================================================================ //
// -------------------------------------------------------------------------------------------- //
//  GRAPHIC MEMORY TRANSFERS
// ============================================================================================ //
// -------------------------------------------------------------------------------------------- //

void Volume::sendToGraphicMemory() {

  if( volumeTex ) {
    glDeleteTextures(1, &volumeTex);
  }

  // reorganize data : one slice per RGBA channel
  float *textureBuffer = new float[dim*dim*dim];
  int indexBuffer = 0;
  
  // for each group of 4 slices
  for( unsigned int s = 0; s < dim; s += 4 ) {
  
    // loop through the image slice
    for( unsigned int y = 0; y < dim; y++ ) 
    for( unsigned int x = 0; x < dim; x++ ) {
    
      // axis from bottom to top -> ABGR ordering
      textureBuffer[indexBuffer++] = value(x,y,s+3);  // R channel 
      textureBuffer[indexBuffer++] = value(x,y,s+2);  // G channel 
      textureBuffer[indexBuffer++] = value(x,y,s+1);  // B channel 
      textureBuffer[indexBuffer++] = value(x,y,s+0);  // A channel  
    }
  }
  assert( indexBuffer == dim * dim * dim );
      
  // load 3d texture data
  glGenTextures( 1, &volumeTex ); GL_TEST_ERROR
  glBindTexture( GL_TEXTURE_3D, volumeTex ); GL_TEST_ERROR
  glTexImage3D( GL_TEXTURE_3D, 0, GL_RGBA16F_ARB, dim, dim, dim/4, 0, GL_RGBA, GL_FLOAT, textureBuffer ); GL_TEST_ERROR
  
  delete [] textureBuffer;

  glTexParameterf(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER,FILTERING_METHOD);   GL_TEST_ERROR 
  glTexParameterf(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER,FILTERING_METHOD);   GL_TEST_ERROR 
  glTexParameterf(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP);  GL_TEST_ERROR 
  glTexParameterf(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP);   GL_TEST_ERROR 
  glTexParameterf(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP);   GL_TEST_ERROR 

  glBindTexture( GL_TEXTURE_3D, 0 );   GL_TEST_ERROR  // force transfer to graphic memory
  glBindTexture( GL_TEXTURE_3D, volumeTex );

  // Check that the texture object is resident in graphic memory 
  GLboolean residence;
  if( ! glAreTexturesResident( 1, &volumeTex, &residence ) )
    std::cerr << "WARNING: 3D Texture is not resident" << std::endl;
}


void Volume::retrieveFromGraphicMemory() {

  glPushAttrib( GL_ENABLE_BIT );
  glEnable( GL_TEXTURE_3D );
  glDisable( GL_BLEND );
  glBindTexture( GL_TEXTURE_3D, volumeTex ); GL_TEST_ERROR
  
  float *textureBuffer = new float[dim*dim*4];
  
  glFramebufferTexture2DEXT( GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_2D, GPURecOpenGL::tex1Dim, 0 ); GL_TEST_ERROR
  
  maxValue = 0.0f;
  float sum = 0.0;
  for( unsigned int slice = 0; slice < dim/4; slice++ ) {
  
    // draw the slice 
    glClear(GL_COLOR_BUFFER_BIT); 
    
    GPURecOpenGL::sideView();
      
    glBegin(GL_QUADS);    
      glTexCoord3f(0,0,(slice+0.5)/(float)(dim/4)); glVertex3f( -1, 0, -1); 
      glTexCoord3f(1,0,(slice+0.5)/(float)(dim/4)); glVertex3f( 1, 0, -1); 
      glTexCoord3f(1,1,(slice+0.5)/(float)(dim/4)); glVertex3f( 1, 0, 1); 
      glTexCoord3f(0,1,(slice+0.5)/(float)(dim/4)); glVertex3f( -1, 0, 1);
    glEnd();  GL_TEST_ERROR   
    

    glReadPixels( 0, 0, dim, dim, GL_RGBA, GL_FLOAT, textureBuffer ); GL_TEST_ERROR   
      
    int index = 0;
    for( unsigned int indexY = 0; indexY < dim; indexY++ )
    for( unsigned int indexX = 0; indexX < dim; indexX++ ) {
     
      assert ( textureBuffer[index+0] >= 0 && !isnan(textureBuffer[index+0]) );
      assert ( textureBuffer[index+1] >= 0 && !isnan(textureBuffer[index+1]) );
      assert ( textureBuffer[index+2] >= 0 && !isnan(textureBuffer[index+2]) );
      assert ( textureBuffer[index+3] >= 0 && !isnan(textureBuffer[index+3]) );
        
      if( textureBuffer[index+0] > maxValue ) maxValue = textureBuffer[index+0];
      if( textureBuffer[index+1] > maxValue ) maxValue = textureBuffer[index+1];
      if( textureBuffer[index+2] > maxValue ) maxValue = textureBuffer[index+2];
      if( textureBuffer[index+3] > maxValue ) maxValue = textureBuffer[index+3];       
      
      sum += textureBuffer[index] + textureBuffer[index+1] + textureBuffer[index+2] + textureBuffer[index+3];
   
      // axis from bottom to top -> ABGR order
      value(indexX,indexY,4*slice+3) = textureBuffer[index++];
      value(indexX,indexY,4*slice+2) = textureBuffer[index++];
      value(indexX,indexY,4*slice+1) = textureBuffer[index++];
      value(indexX,indexY,4*slice+0) = textureBuffer[index++];
    }
  }
  
  glFramebufferTexture2DEXT( GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_2D, GPURecOpenGL::texQuarterDim, 0 ); GL_TEST_ERROR
  
  delete [] textureBuffer;
  
  glPopAttrib();
  
  std::cout << "Volume sum: " << sum << std::endl;
}



// ============================================================================================ //
// -------------------------------------------------------------------------------------------- //
//  DEBUG methods
// ============================================================================================ //
// -------------------------------------------------------------------------------------------- //

void Volume::draw( double angleH, double angleV ) const {
  
  glClear(GL_COLOR_BUFFER_BIT);
  
  // setup the projection matrix and the camera
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  gluPerspective(45,1,1,200); 
  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();
  
  gluLookAt( 0, -1*3.0, 0.2, 0,0,0, 0,0,1 ) ; 
   
  glEnable(GL_TEXTURE_3D);
  glEnable(GL_BLEND);
  //glBlendColorEXT(1.0/dim,1.0/dim,1.0/dim,1.0/dim);
  glBlendFunc( GL_ONE, GL_ONE );
  
  glMatrixMode( GL_TEXTURE );
  glLoadIdentity(); 
  glTranslatef( 0.5, 0.5, 0.5 );   
  glRotated( angleH*180.0/M_PI,  0, 0, 1);   
  glRotated( angleV*180.0/M_PI,  1, 0, 0); 
  glTranslatef( -0.5, -0.5, -0.5 );
  
  // draw the 3d texture
  glBindTexture( GL_TEXTURE_3D, volumeTex );  GL_TEST_ERROR 
  
  for( unsigned int sliceNum = 0; sliceNum < dim; sliceNum++ ) {

    glBegin(GL_QUADS);
      glTexCoord3f( 0, sliceNum/(float)dim, 0 ); glVertex3f( -1, volumeToWorldCoord(sliceNum), -1); 
      glTexCoord3f( 1, sliceNum/(float)dim, 0 ); glVertex3f( 1, volumeToWorldCoord(sliceNum), -1); 
      glTexCoord3f( 1, sliceNum/(float)dim, 1 ); glVertex3f( 1, volumeToWorldCoord(sliceNum), 1); 
      glTexCoord3f( 0, sliceNum/(float)dim, 1 ); glVertex3f( -1, volumeToWorldCoord(sliceNum), 1);
    glEnd();
  }           
}


void Volume::drawGrid() const {

  glPointSize(3.0);
  glBegin(GL_POINTS);
  
    for(unsigned int i = 0; i < dim; i++ ) 
    for(unsigned int j = 0; j < dim; j++ )
    for(unsigned int k = 0; k < dim; k++ ) {
    
      if( value(i,j,k) != 0 ) {
        glColor3f( value(i,j,k), value(i,j,k), value(i,j,k) );
        glVertex3f( volumeToWorldCoord(i), volumeToWorldCoord(j), volumeToWorldCoord(k) );
      }
    }

  glEnd();
}


void Volume::drawBoundingBox() const {

  glPushAttrib(GL_ALL_ATTRIB_BITS);
  glDisable(GL_TEXTURE_2D);
  glDisable(GL_BLEND);
  glColor3d(1,1,0);
  glPolygonMode( GL_FRONT_AND_BACK, GL_LINE );
  glDisable(GL_CULL_FACE) ;
  
  glBegin(GL_QUADS);
    glVertex3f(-1,-1,-1);glVertex3f(-1,1,-1);glVertex3f(-1,1,1);glVertex3f(-1,-1,1);
    glVertex3f(1,-1,-1);glVertex3f(1,1,-1);glVertex3f(1,1,1);glVertex3f(1,-1,1);
        
    glVertex3f(-1,-1,-1);glVertex3f(1,-1,-1); glVertex3f(1,-1,1);glVertex3f(-1,-1,1);
    glVertex3f(-1,1,-1);glVertex3f(1,1,-1); glVertex3f(1,1,1);glVertex3f(-1,1,1);

    glVertex3f(-1,-1,-1);glVertex3f(-1,1,-1);glVertex3f(1,1,-1);glVertex3f(1,-1,-1);
    glVertex3f(-1,-1,1);glVertex3f(-1,1,1);glVertex3f(1,1,1);glVertex3f(1,-1,1);
  glEnd();

  glLineWidth(2.0);
  glBegin (GL_LINES) ;
    glColor3f(1,0,0) ;
    glVertex3f(-1,-1,-1); glVertex3f(1,-1,-1) ;
    glColor3f(0,1,0) ;
    glVertex3f (-1,-1,-1); glVertex3f(-1,1,-1) ;
    glColor3f(0,0,1) ;
    glVertex3f(-1,-1,-1); glVertex3f(-1,-1,1) ;
  glEnd () ;
  
  glPopAttrib();
}


void Volume::loadSliceAsTexture( MajorAxis axis, int sliceNumber ) const {

  float* textureBuffer = new float[ dim * dim ];
  
  switch( axis ) {
  
    case Z_AXIS:  // slice on plane XY
    {
      int index = 0;
      for( unsigned int indexY = 0; indexY < dim; indexY++ )
      for( unsigned int indexX = 0; indexX < dim; indexX++ ) {
        textureBuffer[index++] = value(indexX,indexY,sliceNumber);
      }
      break;
    }
    
    case Y_AXIS:  // slice on plane XZ
    {
      int index = 0;
      for( unsigned int indexZ = 0; indexZ < dim; indexZ++ )
      for( unsigned int indexX = 0; indexX < dim; indexX++ ) {
        textureBuffer[index++] = value(indexX,sliceNumber,indexZ);
      }
      break;
    }
    
    case X_AXIS:  // slice on plane YZ
    {
      int index = 0;
      for( unsigned int indexZ = 0; indexZ < dim; indexZ++ ) {
        
        unsigned int indexY = dim;
        do {

          indexY--;
          textureBuffer[index++] = value(sliceNumber,indexY,indexZ);
        }
        while( indexY > 0 );
      }
      break;
    }
  }
  
  glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA16F_ARB, dim, dim, 0, GL_LUMINANCE, GL_FLOAT, textureBuffer ); GL_TEST_ERROR
  
  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER,GL_NEAREST); GL_TEST_ERROR 
  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,GL_NEAREST); GL_TEST_ERROR 
  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);  GL_TEST_ERROR 
  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);  GL_TEST_ERROR 
  
  delete [] textureBuffer;
}


float GPURec::diff( const Volume& v1, const Volume& v2 ) {

  assert( v1.dim == v2.dim );
  float relativeError = 0;
  
  // the error is not computed near borders since they can't be reconstructed precisly
  unsigned int firstReliableRec = (unsigned int)( v1.dim * (sqrt(2.0)-1.0)/2 );

  for(unsigned int i = firstReliableRec; i < v1.dim-firstReliableRec; i++ ) 
  for(unsigned int j = firstReliableRec; j < v1.dim-firstReliableRec; j++ )
  for(unsigned int k = 0; k < v1.dim; k++ ) {
    
    float diff = v1.value(i,j,k) - v2.value(i,j,k);

    //std::cout << val1 <<" " <<val2 <<" " <<(val1 - val2) << std::endl;
    relativeError += fabs(diff);
    //std::cout << (int)v1.value(i,j,k) <<" " <<(int)v2.value(i,j,k) <<" " << fabs(diff) / 255.0 << std::endl;
    
    /*if( v2.value(i,j,k).isNan() )
      std::cout << "ALERT: vol(" << i << "," << j << "," << k << ") NAN" << std::endl;
    if( v2.value(i,j,k).isInfinity() )
      std::cout << "ALERT: vol(" << i << "," << j << "," << k << ") INF" << std::endl;*/
  }
  
  return relativeError / (v1.dim*v1.dim*v1.dim);
}








