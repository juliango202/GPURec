
#ifndef _GLUTILS_H
#define _GLUTILS_H

#include <GL/glew.h>
#include <GL/glut.h>

//#include <string>
#include <map>
#include <iostream>



#ifdef DEBUG

  // OpenGL test error macro
  // can’t use inside of glBegin() / glEnd() pair 
  #define GL_TEST_ERROR \
      {GLenum  error; \
      if ( (error = glGetError()) != GL_NO_ERROR) { \
          printf( "[%s:%d](! or before !) opengl command failed with error %s\n", \
            __FILE__, __LINE__, \
                  gluErrorString(error) ); \
      }}     
#else

  #define GL_TEST_ERROR           
#endif  // DEBUG

// The GLutils class provides static functions to query GL states, available extensions, to display string in an opengl window, etc...
class GLutils
{
public:

  // print available extensions on the standard output  
  static void printExt( void ) {
    std::cout << glGetString(GL_EXTENSIONS) << std::endl; 
  }
  
  // check that a given extension is available
  // taken from OpenGL red book
  static GLboolean isExtensionAvailable( char *extName );

  // return the maximum size for a texture on the current graphic hardware
  static GLint getMaxTextureSize( void );
              
  // return infos on current texture
  static std::string getTextureInfos( GLenum textureTarget );
  static std::string getBufferInfos(  GLenum internalFormat, GLsizei width, GLsizei height, float divFactor = 1.0 );
  
  static void bufferToPPM( GLsizei width, GLsizei height, float normalizationFactor = 1.0 );
  static void memToPPM( float* mem, GLsizei width, GLsizei height, float normalizationFactor = 1.0 );
  static void bufferToPGM( GLenum internalFormat, GLsizei width, GLsizei height, float normalizationFactor = 1.0, unsigned int nbFile = 1 );
  static void packBufferToPGM( GLsizei width, GLsizei height, float normalizationFactor = 1.0 );
                                                                                                              
  // store current opengl states in a logfile
  static  void logOpenglStates( const char* fileName, unsigned int nbLogs = 1 ); 
  
  // display string in current opengl window
  //static void postString( char* key, char* value );


  // =========================================== //
  //  DEBUG UTILS                                //
  // =========================================== //
  static void checkerTexture( void ); 
  
  static void drawAxis( float originX = 0.0, float originY = 0.0, float originZ = 0.0 );
  
  static void displayColorBuffer();
  static void displayTexture();
  
  // screenshot
  // cout once (comme logs)
  // LINE(x,y,z,x,y,z)
  
private:
  
  static std::map<const std::string, unsigned int> logs;

};

#endif  // _GLUTILS_H
