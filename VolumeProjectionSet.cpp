
#include "common.h"

#define _USE_MATH_DEFINES
#include <cmath>
#include <fstream>
#include <sstream>
#include <algorithm>

#include "VolumeProjectionSet.h"
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

void VolumeProjectionSet::initialize() {
  
  projections = 0;
  nbProjection = 0;
  dim = 0;
  pixelSize = 0;
  startAngle = 0;
  rotationIncrement = 0;
  currentSubset = 0;
}


void VolumeProjectionSet::terminate() {
  
  if( projections ) {
  
    for( unsigned int p = 0; p < nbProjection; p++ ) {
      
      glDeleteTextures( 1, &(projections[p].texture) ); GL_TEST_ERROR      
      
      if( projections[p].data )
        delete [] projections[p].data;
    }
    
    delete [] projections; 
    projections = 0;
  }
}



// ============================================================================================ //
// -------------------------------------------------------------------------------------------- //
//  IO methods
// ============================================================================================ //
// -------------------------------------------------------------------------------------------- //

void VolumeProjectionSet::createEmpty( unsigned int _dim, unsigned int _nbProjection, float _startAngle, float _rotationIncrement ) {

  reset();
  
  // initialize data members
  nbProjection = _nbProjection;
  projections = new VolumeProjection[ nbProjection ];
    
  dim = _dim;
  pixelSize = DEFAULT_PIXEL_SIZE;
  startAngle = _startAngle;
  rotationIncrement = _rotationIncrement;
  
  if( rotationIncrement == 0.0 )
    rotationIncrement = (2.0f * M_PI) / nbProjection;

  float* emptyTab = new float[ dim * dim ];
  for( unsigned int i = 0; i < dim * dim; i++ )
    emptyTab[i] = 1.0;

  // load projections textures
  float angle = startAngle;
  for( unsigned int p = 0; p < nbProjection; p++ ) {
   
    // for each angle store the projection into a texture object
    projections[p].angle = angle;  
            
    glGenTextures( 1, &(projections[p].texture) ); GL_TEST_ERROR     
    glBindTexture( GL_TEXTURE_2D, projections[p].texture );  GL_TEST_ERROR 
    
    glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA16F_ARB, dim, dim/4, 0, GL_RGBA, GL_FLOAT, emptyTab ); GL_TEST_ERROR         
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER,FILTERING_METHOD);   GL_TEST_ERROR 
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,FILTERING_METHOD);   GL_TEST_ERROR 
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);  GL_TEST_ERROR 
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);   GL_TEST_ERROR 
        
    angle += rotationIncrement;
  }
  
  delete [] emptyTab;
}


void VolumeProjectionSet::createFromRAW(  unsigned int _dim, float _pixelSize, unsigned int _nbProjection, float _startAngle, float _rotationIncrement, 
                                          const std::string& fileName, unsigned int offset ) {
     
  reset();

  std::ifstream file( fileName.c_str(), std::ios::in | std::ios::binary );

  if( !file ) {
    std::cerr << "error opening DATA file " << fileName << std::endl;
    throw std::exception();
  }
  
  nbProjection = _nbProjection;

  projections = new VolumeProjection[ nbProjection ];
  
  dim = _dim;
  pixelSize = _pixelSize;
  startAngle = _startAngle;
  rotationIncrement = _rotationIncrement;
  
  unsigned short *textureBuffer =  new unsigned short[dim*dim];
  float *floatTextureBuffer =  new float[dim*dim];
   
   // go to the scan position
  file.seekg( offset, std::ios::beg );
  
  if( file.bad() ) {
  
    std::cerr << "Error when offseting:" << offset << " in file:" << fileName << std::endl;
    throw std::exception();
  }
  
  float angle = 0.0;
  float sum = 0.0;
  for( unsigned int p = 0; p < nbProjection; p++ ) {
      
    // for each angle store the projection into a texture object
    projections[p].angle = angle;   
    
    file.read( (char*)textureBuffer, dim * dim * sizeof(unsigned short) );

    int index = 0;
    for( unsigned int j = 0; j < dim/4; j++ )
    for( unsigned int i = 0; i < dim; i++ ) {
      
      floatTextureBuffer[index++] = textureBuffer[i+(dim-j*4-4)*dim];
      floatTextureBuffer[index++] = textureBuffer[i+(dim-j*4-3)*dim];
      floatTextureBuffer[index++] = textureBuffer[i+(dim-j*4-2)*dim];
      floatTextureBuffer[index++] = textureBuffer[i+(dim-j*4-1)*dim];

      sum = sum + textureBuffer[i+(dim-j*4-4)*dim] + textureBuffer[i+(dim-j*4-3)*dim] + textureBuffer[i+(dim-j*4-2)*dim] + textureBuffer[i+(dim-j*4-1)*dim];
    }  
      
    glGenTextures( 1, &(projections[p].texture) ); GL_TEST_ERROR     

    glBindTexture( GL_TEXTURE_2D, projections[p].texture);  GL_TEST_ERROR 
    

    glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA16F_ARB, dim, dim/4, 0, GL_RGBA, GL_FLOAT, floatTextureBuffer); GL_TEST_ERROR   
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER,FILTERING_METHOD);   GL_TEST_ERROR 
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,FILTERING_METHOD);   GL_TEST_ERROR 
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);  GL_TEST_ERROR 
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);   GL_TEST_ERROR 
     
    angle += rotationIncrement;
  }  
 
  delete [] textureBuffer;
  delete [] floatTextureBuffer;

  std::cout << "Scan sum: " << sum / nbProjection << std::endl;

  file.close();
}


// ============================================================================================ //
// -------------------------------------------------------------------------------------------- //
//  MATHEMATICAL TRANSFORMS
// ============================================================================================ //
// -------------------------------------------------------------------------------------------- //

void VolumeProjectionSet::backProjectSlice( unsigned int sliceNum ) const {
  
  glClear(GL_COLOR_BUFFER_BIT);
 
  GPURecOpenGL::topView();
  
  glEnable(GL_BLEND);
  glBlendFunc(GL_ONE,GL_ONE);  GL_TEST_ERROR    
  glMatrixMode( GL_MODELVIEW ); 

  for( unsigned int p = currentSubset; p < nbProjection; p += NB_SUBSETS ) {
    
    if( USE_OSEM3D )
      GPUGaussianConv::convolveAndBackProject( projections[p].texture, projections[p].angle, sliceNum );      
    else {
      float imageHeight = dim/4;

      glBindTexture( GL_TEXTURE_2D, projections[p].texture ); GL_TEST_ERROR  

      glLoadIdentity();
      glRotated( projections[p].angle*180.0/M_PI,  0,0,1 );

      glBegin(GL_QUADS);
        glTexCoord2f( 0, (0.5+sliceNum)/imageHeight ); glVertex3f(-1,-1,0); 
        glTexCoord2f( 1, (0.5+sliceNum)/imageHeight ); glVertex3f(1,-1,0); 
        glTexCoord2f( 1, (0.5+sliceNum)/imageHeight ); glVertex3f(1,1,0); 
        glTexCoord2f( 0, (0.5+sliceNum)/imageHeight ); glVertex3f(-1,1,0);
      glEnd();
    }
  }
}


void VolumeProjectionSet::backProjection( Volume& volume ) const {

  static int nbBackproj = 0;

  DBGutils::timerBegin("VolumeBackprojection");

  assert( dim != 0 && volume.getDim() == dim );
  
  glClear( GL_COLOR_BUFFER_BIT );

  for( unsigned int k = 0; k < dim/4; k++ ) {

    glFramebufferTexture2DEXT( GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_2D, GPURecOpenGL::tex2Dim, 0 ); GL_TEST_ERROR

    backProjectSlice( k );  
      
    glFramebufferTexture2DEXT( GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_2D, GPURecOpenGL::tex1Dim, 0 ); GL_TEST_ERROR 
    
    glActiveTextureARB(GL_TEXTURE0_ARB);  GL_TEST_ERROR 
    glEnable( GL_TEXTURE_3D );
    glBindTexture( GL_TEXTURE_3D, volume.getVolumeTex() );
    
    // load measuredProj texture 
    glActiveTextureARB(GL_TEXTURE1_ARB);  GL_TEST_ERROR
    glEnable( GL_TEXTURE_2D );  GL_TEST_ERROR
    glBindTexture( GL_TEXTURE_2D, GPURecOpenGL::tex2Dim );  GL_TEST_ERROR   
    
    // perform multiplication
    
    ////////////////////////////////////////////////////////////
    
    glEnable(GL_FRAGMENT_PROGRAM_ARB);    
    glBindProgramARB(GL_FRAGMENT_PROGRAM_ARB, GPURecOpenGL::updateSliceProgram);
    float normalizationFactor = NB_SUBSETS/(float)nbProjection;
    glProgramLocalParameter4fARB( GL_FRAGMENT_PROGRAM_ARB, 0, normalizationFactor, normalizationFactor, normalizationFactor, normalizationFactor );
    
    glClear(GL_COLOR_BUFFER_BIT);
    glDisable( GL_BLEND );
    
    GPURecOpenGL::sideView();
    
    glBegin(GL_QUADS);    
      glMultiTexCoord3f(GL_TEXTURE0,0,0,(k+0.5)/(float)(dim/4));glMultiTexCoord2f(GL_TEXTURE1,0,0); glVertex3f( -1, 0, -1); 
      glMultiTexCoord3f(GL_TEXTURE0,1,0,(k+0.5)/(float)(dim/4));glMultiTexCoord2f(GL_TEXTURE1,1,0); glVertex3f( 1, 0, -1); 
      glMultiTexCoord3f(GL_TEXTURE0,1,1,(k+0.5)/(float)(dim/4));glMultiTexCoord2f(GL_TEXTURE1,1,1); glVertex3f( 1, 0, 1); 
      glMultiTexCoord3f(GL_TEXTURE0,0,1,(k+0.5)/(float)(dim/4));glMultiTexCoord2f(GL_TEXTURE1,0,1); glVertex3f( -1, 0, 1);
    glEnd();  GL_TEST_ERROR    
      
    glDisable(GL_FRAGMENT_PROGRAM_ARB);
  
    glActiveTextureARB(GL_TEXTURE1_ARB);  GL_TEST_ERROR    
    glDisable( GL_TEXTURE_2D );  GL_TEST_ERROR
     
    glActiveTextureARB(GL_TEXTURE0_ARB);  GL_TEST_ERROR
    glDisable( GL_TEXTURE_3D );  GL_TEST_ERROR

    DBGutils::timerBegin("glCopyTexSubImage3D");
    
    glCopyTexSubImage3D( GL_TEXTURE_3D, 0, 0, 0, k, 0, 0, dim, dim ); GL_TEST_ERROR
    
    DBGutils::timerEnd("glCopyTexSubImage3D");
  }
  
  glFramebufferTexture2DEXT( GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_2D, GPURecOpenGL::texQuarterDim, 0 );  
  
  glDisable( GL_BLEND );
  
  DBGutils::timerEnd("VolumeBackprojection");
}



// ============================================================================================ //
// -------------------------------------------------------------------------------------------- //
//  GRAPHIC MEMORY TRANSFERS
// ============================================================================================ //
// -------------------------------------------------------------------------------------------- //

void VolumeProjectionSet::sendToGraphicMemory() {
  
  float *textureBuffer =  new float[ dim * dim/4 * 4 ];
  
  for( unsigned int p = 0; p < nbProjection; p++ ) {
       
    glDeleteTextures( 1, &(projections[p].texture) ); GL_TEST_ERROR  
    if( projections[p].data == 0 ) projections[p].data = new float[dim *dim];      
    
    glGenTextures( 1, &(projections[p].texture) ); GL_TEST_ERROR     
    glBindTexture( GL_TEXTURE_2D, projections[p].texture );  GL_TEST_ERROR 
       
    int index = 0;
    // for each group of 4 lines
    for( unsigned int j = 0; j < dim; j += 4 ) {
    
      // loop through the line
      for( unsigned int i = 0; i < dim; i++ ) {
      
        // axis from bottom to top -> ABGR ordering               
        textureBuffer[index++] = projections[p].data[ i + (j+3)*dim ];  // R channel
        textureBuffer[index++] = projections[p].data[ i + (j+2)*dim ];  // G channel 
        textureBuffer[index++] = projections[p].data[ i + (j+1)*dim ];  // B channel 
        textureBuffer[index++] = projections[p].data[ i + (j+0)*dim ];  // A channel 
      }
    }
    assert( index == dim * dim/4 );

    glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA16F_ARB, dim, dim/4, 0, GL_RGBA, GL_FLOAT, textureBuffer); GL_TEST_ERROR     
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, FILTERING_METHOD);    GL_TEST_ERROR 
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, FILTERING_METHOD);    GL_TEST_ERROR 
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);        GL_TEST_ERROR 
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);        GL_TEST_ERROR
  }  
  
  delete [] textureBuffer;
}

        
void VolumeProjectionSet::retrieveFromGraphicMemory() {
        
  glPushAttrib( GL_ENABLE_BIT );
  glDisable( GL_BLEND ); 
  
  float *textureBuffer = new float[dim * dim/4 * 4];
  
  glFramebufferTexture2DEXT( GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_2D, GPURecOpenGL::texQuarterDim, 0 ); GL_TEST_ERROR
  glViewport(0,0,dim,dim/4);  
  
  float sum = 0.0;
  for( unsigned int p = 0; p < nbProjection; p++ ) {
  
    glBindTexture( GL_TEXTURE_2D, projections[p].texture ); GL_TEST_ERROR  
      
    // draw the projection because glGetTexImage seems not working
    glClear(GL_COLOR_BUFFER_BIT); 
      
    GPURecOpenGL::sideView();
        
    glBegin(GL_QUADS);    
      glTexCoord2f(0,0); glVertex3f( -1, 0, -1); 
      glTexCoord2f(1,0); glVertex3f( 1, 0, -1); 
      glTexCoord2f(1,1); glVertex3f( 1, 0, 1); 
      glTexCoord2f(0,1); glVertex3f( -1, 0, 1);
    glEnd();  GL_TEST_ERROR   
    
    glReadPixels( 0, 0, dim, dim/4, GL_RGBA, GL_FLOAT, textureBuffer ); GL_TEST_ERROR   
      
    int index = 0;
    for( unsigned int j = 0; j < dim; j+=4 )
    for( unsigned int i = 0; i < dim; i++ ) {
    
      /*assert ( textureBuffer[index+0] >= 0 && !isnan(textureBuffer[index+0]) );
      assert ( textureBuffer[index+1] >= 0 && !isnan(textureBuffer[index+1]) );
      assert ( textureBuffer[index+2] >= 0 && !isnan(textureBuffer[index+2]) );
      assert ( textureBuffer[index+3] >= 0 && !isnan(textureBuffer[index+3]) );*/
      
      sum += textureBuffer[index] + textureBuffer[index+1] + textureBuffer[index+2] + textureBuffer[index+3];
      
      // axis from bottom to top -> ABGR order
      projections[p].data[ i + (j+0)*dim ] = textureBuffer[index++];
      projections[p].data[ i + (j+1)*dim ] = textureBuffer[index++];
      projections[p].data[ i + (j+2)*dim ] = textureBuffer[index++];
      projections[p].data[ i + (j+3)*dim ] = textureBuffer[index++];
    }  
  }  
  
  glFramebufferTexture2DEXT( GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_2D, GPURecOpenGL::texQuarterDim, 0 ); GL_TEST_ERROR
  glViewport(0,0,dim,dim);  
  
  delete [] textureBuffer;
  
  glPopAttrib();      
  
  std::cout << "Scan sum: " << sum << std::endl;    
}



// ============================================================================================ //
// -------------------------------------------------------------------------------------------- //
//  RECONSTRUCTION methods
// ============================================================================================ //
// -------------------------------------------------------------------------------------------- //

class SubsetsIterator {

  public:
    SubsetsIterator() : current(0), last(false) {}
    int first() { current = 0; done.push_back( current ); return current; }
    
    int next() { 
    
      // if all subsets have been done, set the LAST flag and return
      if( done.size() == NB_SUBSETS ) {
      
        last = true;
        return 0;
      }
      
      // the next subset should be at a distance of NB_SUBSETS/2 to maximize the cyclic distance 
      current = ( current + NB_SUBSETS/2 ) % NB_SUBSETS;
      
      // but if it has been already processed, take the next
      std::vector<int>::iterator it = std::find( done.begin(), done.end(), current );
      while( it != done.end() ) {
      
        current = (current+1)  % NB_SUBSETS;       
        it = std::find( done.begin(), done.end(), current );                
      }
      
      done.push_back( current );      
      return current;
    }
    
    bool isLast() { return last; }
    
  protected:
    int current;
    bool last;
    std::vector<int> done;
};


void VolumeProjectionSet::osemIteration( Volume& volume, const VolumeProjectionSet& scan ) {

  if( (scan.getNbProjection()/(float)NB_SUBSETS) != (scan.getNbProjection()/NB_SUBSETS) ) {
	  std::cerr << "Parameters error: The number of subsets has to be a divisor of the number of projections.\n";
	  std::cerr << "Number of subsets:" << NB_SUBSETS << " Number of projections:" << scan.getNbProjection() << std::endl;
	  throw std::exception();
  }
 
  // for each subset
  SubsetsIterator itSubsets;
  std::cout << "Nouvelle Iteration" << std::endl;
  for( currentSubset = itSubsets.first(); ! itSubsets.isLast(); currentSubset = itSubsets.next() ) {
  
    std::cout << "subset: " << ((currentSubset<10) ? "0" : "") << currentSubset << "\r";
    
    glViewport( 0, 0, dim, dim/4 );
       
    // apply MLEM iteration to the subset
    // each subset contains (nbProjections/NB_SUBSETS) projections evenly distributed among the set
    for( unsigned int p = currentSubset; p < scan.getNbProjection(); p += NB_SUBSETS ) {
    
      // store volume projection in a texture    
      glFramebufferTexture2DEXT( GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_2D, GPURecOpenGL::texQuarterDim, 0 ); GL_TEST_ERROR
      DBGutils::timerBegin("1 Projection");    
      volume.projection( projections[p].angle, USE_OSEM3D ); 
      DBGutils::timerEnd("1 Projection");     
      
      // perform division and store the result in the projectionsSet texture 
      glFramebufferTexture2DEXT( GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_2D, projections[p].texture, 0 ); GL_TEST_ERROR
      
      // load volume projection texture 
      glActiveTextureARB(GL_TEXTURE1_ARB);  GL_TEST_ERROR
      glEnable( GL_TEXTURE_2D );  GL_TEST_ERROR
      glBindTexture( GL_TEXTURE_2D, GPURecOpenGL::texQuarterDim );  GL_TEST_ERROR      
             
      // load scanner projection texture 
      glActiveTextureARB(GL_TEXTURE0_ARB);  GL_TEST_ERROR 
      scan.loadProjectionAsTexture( p );  GL_TEST_ERROR    
  
             
      glEnable(GL_FRAGMENT_PROGRAM_ARB);      
      glBindProgramARB(GL_FRAGMENT_PROGRAM_ARB, GPURecOpenGL::textureDivPackProgram);
      
      glClear(GL_COLOR_BUFFER_BIT);
      glDisable( GL_BLEND );
      
      GPURecOpenGL::sideView();
      
      glBegin(GL_QUADS);    
        glTexCoord2f(0,0); glVertex3f( -1, 0, -1); 
        glTexCoord2f(1,0); glVertex3f( 1, 0, -1); 
        glTexCoord2f(1,1); glVertex3f( 1, 0, 1); 
        glTexCoord2f(0,1); glVertex3f( -1, 0, 1);
      glEnd();  GL_TEST_ERROR   
      
      glDisable(GL_FRAGMENT_PROGRAM_ARB);
    
      glActiveTextureARB(GL_TEXTURE1_ARB);  GL_TEST_ERROR    
      glDisable( GL_TEXTURE_2D );  GL_TEST_ERROR
          
      glActiveTextureARB(GL_TEXTURE0_ARB);  GL_TEST_ERROR              
    }
        
    glFramebufferTexture2DEXT( GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_2D, GPURecOpenGL::texQuarterDim, 0 ); GL_TEST_ERROR
    glViewport( 0, 0, dim, dim );
    
    backProjection( volume ); 
  }      
}



// ============================================================================================ //
// -------------------------------------------------------------------------------------------- //
//  DEBUG methods
// ============================================================================================ //
// -------------------------------------------------------------------------------------------- //

void VolumeProjectionSet::loadProjectionAsTexture( unsigned int projectNum ) const {
  
  glBindTexture( GL_TEXTURE_2D, projections[projectNum].texture ); GL_TEST_ERROR       
}

