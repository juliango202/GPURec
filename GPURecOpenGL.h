
#ifndef _GPURECOPENGL_H
#define _GPURECOPENGL_H

namespace GPURec {


// this class encapsulates all commands related to opengl and gpu programming
// the goal is to separate Reconstruction Algorithms and Technical/Hardware specific commands
class GPURecOpenGL {

public:	
      
      static void initialize( unsigned int _dim );
      static void terminate( void );
      static void reset( unsigned int _dim);
              
      // draw the Rendered texture to current viewport(for DEBUG)
      static void draw16bitsTexture( int windowSizeX, int windowSizeY, float alpha = 1.0 );
      static void draw8bitsTexture( int windowSizeX, int windowSizeY );
      
      static void drawFBO( bool unpack, int windowSizeX, int windowSizeY, float alpha = 1.0 );
          
      static void sideView();
      static void topView();
      
      static int poisson_sample ( double a );
      
      //static blending( list of textureID );
      
      
protected:
      
      static void initGLStatesInCurrentContext( void );
      static void initGLEW();  
      static void initFBO();
      static void initFragmentPrograms( void );
      
public:
      static GLuint dim;

      static GLuint textureDivProgram;
      static GLuint textureDivPackProgram;
      static GLuint textureAlphaProgram;
      static GLuint texture3DAlphaProgram;
      static GLuint updateSliceProgram;
      static GLuint unpackProgram;

      static GLuint fboId;
      static GLuint texQuarterDim; 
      static GLuint tex1Dim;
      static GLuint tex2Dim;
      static GLuint unpackTex;
      
      static bool initialized;
};


} // end namespace GPURec

#endif  // _GPURECOPENGL_H
