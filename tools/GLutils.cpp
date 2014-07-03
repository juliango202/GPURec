
#include "GLutils.h"

#include <iostream>
#include <fstream>
#include <sstream>
#define _USE_MATH_DEFINES
#include <cmath>
#include <limits>


#define BUF_SIZE 512  


std::map<const std::string, unsigned int> GLutils::logs;

const char *glFormatToString(GLint format);
const char *glBlendFactorToString(GLenum factor);
const char *glTextureTargetToString(GLenum target);
void printTextureFormat( GLenum textureTarget, std::ostream& os );

GLboolean GLutils::isExtensionAvailable( char *extName ) {      
     // Search for extName in the extensions string.  Use of strstr()
     // is not sufficient because extension names can be prefixes of
     // other extension names.  Could use strtok() but the constant
     // string returned by glGetString can be in read-only memory.
         
     char *p = (char *) glGetString(GL_EXTENSIONS);
     char *end;
     int extNameLen;

     extNameLen = strlen(extName);
     end = p + strlen(p);
    
     while (p < end) {
         int n = strcspn(p, " ");
         if ((extNameLen == n) && (strncmp(extName, p, n) == 0)) {
             return GL_TRUE;
         }
         p += (n + 1);
     }
     return GL_FALSE;
}

GLint GLutils::getMaxTextureSize( void ) {
    // Compute the maximum texture size available
    GLint maxSize = 16;
    GLint width;
        
    do {
      maxSize *= 2;
    
      // use proxy_texture to check th current size is supported
      glTexImage2D(GL_PROXY_TEXTURE_2D, 0, 4, maxSize, maxSize, 0, GL_RGBA, GL_UNSIGNED_INT, NULL);
      
      // width is set to 0 in case of failure
      glGetTexLevelParameteriv(GL_PROXY_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &width);
    }
    while( width != 0 );
    
    return maxSize;
}

std::string GLutils::getBufferInfos( GLenum internalFormat, GLsizei width, GLsizei height, float divFactor ) {
  
  std::stringstream stream;
  
  float *pixels = new float[ width * height ];
  
  glReadPixels( 0, 0, width, height, internalFormat, GL_FLOAT, pixels );  GL_TEST_ERROR
    
  float min = 1000;//std::numeric_limits<float>::max();
  float max = 0;
  float avg = 0;
  int i;
  int iMax = (width * height);
  
  for( i = 0; i < iMax; i++ ) {
    if( i == iMax/2 ) stream << " middle: " << pixels[i];
    if( pixels[i] < min ) min = pixels[i];
    if( pixels[i] > max ) max = pixels[i];
    /*if( isnan(pixels[i]) ) {
      std::cerr << "un NaN a réussi à passer ! :(" << std::endl;
      exit(0);
    }*/
    avg += pixels[i];
  }
  
  delete [] pixels;
  
  stream << "  min: " << min/divFactor << "  max: " << max/divFactor;  

  if(i > 0)
    stream << "  avg: " << avg/(i*divFactor) << " ";
  
  return stream.str();                            
}

std::string GLutils::getTextureInfos( GLenum textureTarget ) {

  std::stringstream stream;
  
  if( textureTarget == GL_TEXTURE_3D ) {
    std::cerr << "cannot call getTexImage on a texture3D" << std::endl;
    return stream.str();
  }
  
  int width, height;
  glGetTexLevelParameteriv( textureTarget, 0, GL_TEXTURE_WIDTH, &width ); GL_TEST_ERROR
  glGetTexLevelParameteriv( textureTarget, 0, GL_TEXTURE_HEIGHT, &height ); GL_TEST_ERROR

  stream << " width: " << width << "  height: " << height;
  
  float *pixels = new float[ width * height ];

  glGetTexImage( textureTarget, 0, GL_LUMINANCE, GL_FLOAT, &pixels[0] );  GL_TEST_ERROR

  float min = 1000;//std::numeric_limits<float>::max();
  float max = 0;
  float avg = 0;
  int i, nb = 0;
  for( i = 0; i < width * height; i++ ) {
    if( pixels[i] < min ) min = pixels[i];
    if( pixels[i] > max ) max = pixels[i];
    if( pixels[i] != 0.0 ) {
      avg += pixels[i];
      nb++;
    }
  }
  
  delete [] pixels;
  
  stream << "  min: " << min << "  max: " << max;
   
  if(i > 0)
    stream << "  avg: " << avg/nb << " ";
  
  return stream.str();  
}

void GLutils::memToPPM( float* mem, GLsizei width, GLsizei height, float normalizationFactor ) {

  static bool done = false;
   
  if( !done ) {
  
    done = true;
    std::ofstream outFile( "membuffer.ppm", std::ios::out | std::ios::binary );
    
    outFile << "P6" << std::endl;
    outFile << "# Created by GLutils" << std::endl;
    outFile << width << " " << height << std::endl;
    outFile << "255" << std::endl;
    
    // write array of pixels
    char component;
    for( int i = 0; i <  width * height * 4 ; i+=4 ) {
      
      component = (char)(mem[i]*255.0*normalizationFactor);
      outFile.write( &component, sizeof(char) );
      outFile.write( &component, sizeof(char) );
      outFile.write( &component, sizeof(char) );
    }
    outFile.close();
  }
}

void GLutils::bufferToPPM( GLsizei width, GLsizei height, float normalizationFactor ) {
  
  static bool done = false;
   
  if( !done ) {
  
    done = true;
    std::ofstream outFile( "buffer.ppm", std::ios::out | std::ios::binary );
    
    outFile << "P6" << std::endl;
    outFile << "# Created by GLutils" << std::endl;
    outFile << width << " " << height << std::endl;
    outFile << "255" << std::endl;
    
    float *buffer = new float[ width * height * 4 ];
    
    glReadPixels( 0, 0, width, height, GL_LUMINANCE, GL_FLOAT, buffer );  GL_TEST_ERROR
    
    // write array of pixels
    char component;
    for( int i = 0; i <  width * height ; i++ ) {
      
      component = (char)(buffer[i]*255.0*normalizationFactor*0.3);
      outFile.write( &component, sizeof(char) );
      outFile.write( &component, sizeof(char) );
      outFile.write( &component, sizeof(char) );
    }
    
    delete [] buffer;  
    
    outFile.close();
  }
}

void GLutils::bufferToPGM( GLenum internalFormat, GLsizei width, GLsizei height, float normalizationFactor, unsigned int nbFile ) {

  static unsigned int currentNbFile = 0;
   
  if( currentNbFile++ < nbFile ) {
  
    std::stringstream sstr;
    sstr << "buffer" << char(currentNbFile+'a') << ".pgm";
    std::ofstream outFile( sstr.str().c_str(), std::ios::out | std::ios::binary );
    
    outFile << "P5" << std::endl;
    outFile << "# Created by Jul" << std::endl;
    outFile << width << " " << height << std::endl;
    outFile << "255" << std::endl;
    
    float *buffer = new float[ width * height ];
    
    glReadPixels( 0, 0, width, height, internalFormat, GL_FLOAT, buffer );  GL_TEST_ERROR
    
    // write array of pixels
    char value;
    for( int j = height-1; j >= 0; j-- )
    for( int i = 0; i < width; i ++ ) {
      
        value = (char)(buffer[i+j*width]*255.0*normalizationFactor);
        outFile.write( &value, sizeof(char) );
    }
   
    delete [] buffer;  
    
    outFile.close();
  }
}


void GLutils::packBufferToPGM( GLsizei width, GLsizei height, float normalizationFactor ) {
  
  static bool done = false;
   
  if( !done ) {
  
    done = true;
    std::ofstream outFile( "buffer.pgm", std::ios::out | std::ios::binary );
    
    outFile << "P5" << std::endl;
    outFile << "# Created by Jul" << std::endl;
    outFile << width << " " << height*4 << std::endl;
    outFile << "255" << std::endl;
    
    float *buffer = new float[ width * height * 4 ];
    
    glReadPixels( 0, 0, width, height, GL_RGBA, GL_FLOAT, buffer );  GL_TEST_ERROR
    
    // write array of pixels
    char value;
    for( int j = height-1; j >= 0; j-- ) {
    
      for( int i = (j*width*4) + 3; i <  (j*width*4) + width * 4; i += 4 ) {
      
        value = (char)(buffer[i]*255.0*normalizationFactor);
        outFile.write( &value, sizeof(char) );
      }
          
      for( int i = (j*width*4) + 2; i <  (j*width*4) + width * 4; i += 4 ) {
      
        value = (char)(buffer[i]*255.0*normalizationFactor);
        outFile.write( &value, sizeof(char) );
      }
      
      for( int i = (j*width*4) + 1; i <  (j*width*4) + width * 4; i += 4 ) {
      
        value = (char)(buffer[i]*255.0*normalizationFactor);
        outFile.write( &value, sizeof(char) );
      }
      
      for( int i = (j*width*4) + 0; i <  (j*width*4) + width * 4; i += 4 ) {
      
        value = (char)(buffer[i]*255.0*normalizationFactor);
        outFile.write( &value, sizeof(char) );
      }
    }    
    
    delete [] buffer;  
    
    outFile.close();
  }
}


/*void GLutils::postString( char* key, char* value ) {



}*/


// ========================================================================== //
//      DEBUG UTILS                                                           //
// ========================================================================== //

#define	checkImageWidth 64
#define	checkImageHeight 64
static GLubyte checkImage[checkImageHeight][checkImageWidth][4];
static GLubyte otherImage[checkImageHeight][checkImageWidth][4];
static GLuint checherTexId = 0;

void makeCheckImage(void)
{
    int i, j, c;

       for (i = 0; i < checkImageWidth; i++) {
        for (j = 0; j < checkImageHeight; j++) {
            c = ( ((i&0x8)==0)^((j&0x8)==0) )*255;
            checkImage[i][j][0] = (GLubyte) c;
            checkImage[i][j][1] = (GLubyte) c;
            checkImage[i][j][2] = (GLubyte) c;
            checkImage[i][j][3] = 255;
        }
    }
}

void GLutils::checkerTexture( void ) {

  static bool init = true;
  if( init ) {
    makeCheckImage();
    glGenTextures( 1, &checherTexId );  GL_TEST_ERROR  
    glBindTexture( GL_TEXTURE_2D, checherTexId ); GL_TEST_ERROR
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, checkImageWidth,
              checkImageHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE,
              checkImage);  GL_TEST_ERROR        
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);  GL_TEST_ERROR
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);  GL_TEST_ERROR
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);  GL_TEST_ERROR
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);  GL_TEST_ERROR
  
    init = false;
  }
    
  glBindTexture( GL_TEXTURE_2D, checherTexId ); GL_TEST_ERROR
}

void GLutils::displayColorBuffer() {

}
  
void GLutils::displayTexture() {


}

// =================================================================================================== //

void GLutils::drawAxis( float originX, float originY, float originZ ) {
   
  glPushAttrib( GL_ENABLE_BIT | GL_TRANSFORM_BIT );
  glDisable( GL_TEXTURE_3D );
  glDisable( GL_TEXTURE_2D );
  glDisable( GL_BLEND );
   
  glEnable(GL_DEPTH_TEST);
  glClear( GL_DEPTH_BUFFER_BIT );
  
  glMatrixMode( GL_MODELVIEW );
  glPushMatrix();
  glTranslatef( originX, originY, originZ );

  glLineWidth(2.0);
  glBegin (GL_LINES) ;
    glColor3f(1,0,0) ;
    glVertex3f(0,0,0); glVertex3f(0.5,0,0) ;
    glColor3f(0,1,0) ;
    glVertex3f(0,0,0); glVertex3f(0,0.5,0) ;
    glColor3f(0,0,1) ;
    glVertex3f(0,0,0); glVertex3f(0,0,0.5) ;
  glEnd () ;
  
  glPopMatrix();
  
  glPopAttrib();
}


void GLutils::logOpenglStates( const char* fileName, unsigned int nbLogs ) {

  // look if the number of log has been reach for this filename
  std::map<const std::string, unsigned int>::iterator log  = logs.find( std::string(fileName) );
  
  if( log != logs.end() ) {
   // the log exists
   
    if( (*log).second == nbLogs )
      return;
    
    (*log).second++;
  }
  else
    logs[std::string(fileName)] = 1;   // first request for this log 
    
  // create the file on disk
  std::stringstream sstr;
  sstr << fileName << logs[std::string(fileName)] << ".log";
  std::ofstream logFile( sstr.str().c_str() );
  
  // begin record states
  
  //-------- OpenGL activable states --------//
  logFile << "# OpenGL activable states" << std::endl << "#" << std::endl;
  
  if( glIsEnabled(GL_LIGHTING) )
    logFile << "GL_LIGHTING ON" << std::endl;
  else
    logFile << "GL_LIGHTING OFF" << std::endl;
    
  if( glIsEnabled(GL_CULL_FACE) )
    logFile << "GL_CULL_FACE ON" << std::endl;
  else
    logFile << "GL_CULL_FACE OFF" << std::endl;
    
  if( glIsEnabled(GL_DEPTH_TEST) )
    logFile << "GL_DEPTH_TEST ON" << std::endl;
  else
    logFile << "GL_DEPTH_TEST OFF" << std::endl;
    
  logFile << std::endl;

  float vector4[4];   // variable to store temporarly result from glGet() calls 
  int param;          // 
    
  //-------- Current values --------//
  logFile << "# Current values" << std::endl << "#" << std::endl;
  
  glGetFloatv( GL_CURRENT_COLOR, vector4 );
  logFile << "Color: " << vector4[0] << " " << vector4[1] << " " << vector4[2] << " " << vector4[3] << std::endl;
    
  glGetIntegerv( GL_RED_BITS, &param );
  logFile << "Nb red bits: " << param << std::endl;
  glGetIntegerv( GL_GREEN_BITS, &param );
  logFile << "Nb green bits: " << param << std::endl;
  glGetIntegerv( GL_BLUE_BITS, &param );
  logFile << "Nb blue bits: " << param << std::endl;
  glGetIntegerv( GL_ALPHA_BITS, &param );
  logFile << "Nb alpha bits: " << param << std::endl;
  
  logFile << std::endl;
  
  //--------  Texture --------//
  logFile << "# Texture" << std::endl << "#" << std::endl; 
  printTextureFormat( GL_TEXTURE_2D, logFile );  
  printTextureFormat( GL_TEXTURE_RECTANGLE_NV, logFile );
  printTextureFormat( GL_TEXTURE_3D, logFile );
  
  //--------  Blending --------//
  logFile << "# Blending" << std::endl << "#" << std::endl;
    
  if( glIsEnabled(GL_BLEND) )
    logFile << "GL_BLEND ON" << std::endl;
  else
    logFile << "GL_BLEND OFF" << std::endl;
  
  if( GLEW_EXT_blend_color ) {
    glGetFloatv( GL_BLEND_COLOR_EXT, vector4 );
    logFile << "Blend Color: " << vector4[0] << " " << vector4[1] << " " << vector4[2] << " " << vector4[3] << std::endl;
  }
  
  glGetIntegerv( GL_BLEND_SRC, &param );
  logFile << "Blend source: " << glBlendFactorToString(param) << std::endl; 
  glGetIntegerv( GL_BLEND_DST, &param );
  logFile << "Blend destination: " << glBlendFactorToString(param) << std::endl;
  logFile << std::endl;
  
  //--------  SHADERS --------//
  logFile << "# Shaders" << std::endl << "#" << std::endl;
  if( glIsEnabled(GL_FRAGMENT_PROGRAM_ARB) )
    logFile << "GL_FRAGMENT_PROGRAM ON" << std::endl;
  else
    logFile << "GL_FRAGMENT_PROGRAM OFF" << std::endl;
  
  glGetProgramivARB( GL_FRAGMENT_PROGRAM_ARB, GL_PROGRAM_BINDING_ARB, &param );   
  logFile << "Program nb: " << param << std::endl;
  logFile << std::endl; 
   
  //--------  FRAMEBUFFER OBJECTS --------//
  logFile << "# FRAMEBUFFER OBJECTS" << std::endl << "#" << std::endl;
  glGetFramebufferAttachmentParameterivEXT( GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE_EXT, &param); 
  if( param == GL_NONE )
    logFile << "Attachment 0: GL_NONE" << std::endl;
  if( param == GL_RENDERBUFFER_EXT )
    logFile << "Attachment 0: GL_RENDERBUFFER_EXT" << std::endl;
  
  if( param == GL_TEXTURE ) {
    logFile << "Attachment 0: GL_TEXTURE" << std::endl;
    glGetFramebufferAttachmentParameterivEXT( GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME_EXT, &param);
    logFile << "======  Texture ID: " << param << " ======" << std::endl;
    
    // save texture state
    GLint savTexId;
    glGetIntegerv( GL_TEXTURE_BINDING_2D, &savTexId );
    
    glBindTexture( GL_TEXTURE_2D, param); 
    printTextureFormat( GL_TEXTURE_2D, logFile );  
    
    // restore texture state
    glBindTexture( GL_TEXTURE_2D, savTexId);
    
    logFile << " ==============================" << std::endl;
  }
  
  logFile.close();
  
  std::cout << "Log File " << sstr.str().c_str() << " written." << std::endl;
  
  GL_TEST_ERROR
}

// =================================================================================================== //

void printTextureFormat( GLenum textureTarget, std::ostream& os ) {

  if( glIsEnabled(textureTarget) )
    os << glTextureTargetToString( textureTarget ) << " ON" << std::endl;
  else
    os << glTextureTargetToString( textureTarget ) << " OFF" << std::endl;
  
  int param;  
  
  if( textureTarget == GL_TEXTURE_2D ) {
    glGetIntegerv( GL_TEXTURE_BINDING_2D, &param ); 
    os << "Texture ID: " << param << std::endl;
  }
  if( textureTarget == GL_TEXTURE_3D ) {
    glGetIntegerv( GL_TEXTURE_BINDING_3D, &param );  
    os << "Texture ID: " << param << std::endl;
  }  
    
  glGetTexLevelParameteriv( textureTarget, 0, GL_TEXTURE_WIDTH, &param );
  os << "Width: " << param << std::endl;
    
  glGetTexLevelParameteriv( textureTarget, 0, GL_TEXTURE_HEIGHT, &param );
  os << "Height: " << param << std::endl;
    
  glGetTexLevelParameteriv( textureTarget, 0, GL_TEXTURE_INTERNAL_FORMAT, &param );
  os << "Internal format: " << glFormatToString(param) << std::endl;
    
  glGetTexLevelParameteriv( textureTarget, 0, GL_TEXTURE_RED_SIZE, &param );
  os << "Nb red bits: " << param << std::endl;    
    
  glGetTexLevelParameteriv( textureTarget, 0, GL_TEXTURE_GREEN_SIZE, &param );
  os << "Nb green bits: " << param << std::endl;
  glGetTexLevelParameteriv( textureTarget, 0, GL_TEXTURE_BLUE_SIZE, &param );
  os << "Nb blue bits: " << param << std::endl;
  glGetTexLevelParameteriv( textureTarget, 0, GL_TEXTURE_ALPHA_SIZE, &param );
  os << "Nb alpha bits: " << param << std::endl;
  glGetTexLevelParameteriv( textureTarget, 0, GL_TEXTURE_LUMINANCE_SIZE, &param );
  os << "Nb luminance bits: " << param << std::endl;
  glGetTexLevelParameteriv( textureTarget, 0, GL_TEXTURE_INTENSITY_SIZE, &param );
  os << "Nb intensity bits: " << param << std::endl;

  os << std::endl;
}

const char *glFormatToString(GLint format)
{

  switch(format) {
    case GL_ALPHA: return "GL_ALPHA";
    case GL_ALPHA8: return "GL_ALPHA8";
    case GL_ALPHA16: return "GL_ALPHA16";
    
    case GL_LUMINANCE: return "GL_LUMINANCE";
    case GL_LUMINANCE8: return "GL_LUMINANCE8";
    case GL_LUMINANCE12: return "GL_LUMINANCE12";
    case GL_LUMINANCE16: return "GL_LUMINANCE16";
    
    case GL_INTENSITY: return "GL_INTENSITY";
    case GL_INTENSITY8: return "GL_INTENSITY8";
    case GL_INTENSITY12: return "GL_INTENSITY12";
    case GL_INTENSITY16: return "GL_INTENSITY16";
    
    case GL_RGB: return "GL_RGB";
    case GL_RGB8: return "GL_RGB8";
    case GL_RGB12: return "GL_RGB12";
    case GL_RGB16: return "GL_RGB16";
    
    case GL_RGBA: return "GL_RGBA";
    case GL_RGBA8: return "GL_RGBA8";
    case GL_RGBA12: return "GL_RGBA12";
    case GL_RGBA16: return "GL_RGBA16";
    
    case GL_FLOAT_R16_NV: return "GL_FLOAT_R16_NV";
    case GL_FLOAT_R32_NV: return "GL_FLOAT_R32_NV";
    case GL_FLOAT_RGBA16_NV: return "GL_FLOAT_RGBA16_NV";
    case GL_FLOAT_RGBA32_NV: return "GL_FLOAT_RGBA32_NV";
    
    case GL_RGBA16F_ARB: return "GL_RGBA16F_ARB";
    case GL_RGBA32F_ARB: return "GL_RGBA32F_ARB";
    
    default: return "Unknown";
  }
}

const char *glBlendFactorToString(GLenum factor)
{
  switch(factor) {
    case GL_ZERO: return "GL_ZERO";
    case GL_ONE: return "GL_ONE";
    case GL_DST_COLOR: return "GL_DST_COLOR";
    case GL_ONE_MINUS_DST_COLOR: return "GL_ONE_MINUS_DST_COLOR";
    case GL_SRC_ALPHA: return "GL_SRC_ALPHA";
    case GL_ONE_MINUS_SRC_ALPHA: return "GL_ONE_MINUS_SRC_ALPHA";
    case GL_DST_ALPHA: return "GL_DST_ALPHA";
    case GL_ONE_MINUS_DST_ALPHA: return "GL_ONE_MINUS_DST_ALPHA";
    case GL_CONSTANT_COLOR_EXT: return "GL_CONSTANT_COLOR_EXT";
    case GL_ONE_MINUS_CONSTANT_COLOR_EXT: return "GL_ONE_MINUS_CONSTANT_COLOR_EXT";
    case GL_CONSTANT_ALPHA_EXT: return "GL_CONSTANT_ALPHA_EXT";
    case GL_ONE_MINUS_CONSTANT_ALPHA_EXT: return "GL_ONE_MINUS_CONSTANT_ALPHA_EXT";
    case GL_SRC_ALPHA_SATURATE: return "GL_SRC_ALPHA_SATURATE";
    
    default: return "Unknown";
  }
}

const char *glTextureTargetToString(GLenum target) {

  switch(target) {
    
    case GL_TEXTURE_2D: return "GL_TEXTURE_2D";
    case GL_TEXTURE_3D: return "GL_TEXTURE_3D";
    case GL_TEXTURE_RECTANGLE_NV: return "GL_TEXTURE_RECTANGLE_NV";
    
    default: return "Unknown";
  }
}








