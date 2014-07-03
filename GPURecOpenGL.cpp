
#include "common.h"
#include <GL/glew.h>
#include <GL/glut.h>

#include "GPURecOpenGL.h"
#include "GLutils.h"

#include <iostream>
#define _USE_MATH_DEFINES
#include <cmath>
#include <limits>


using namespace GPURec;

GLuint GPURecOpenGL::dim = 0;

GLuint GPURecOpenGL::textureDivProgram = 0;
GLuint GPURecOpenGL::textureDivPackProgram = 0;
GLuint GPURecOpenGL::textureAlphaProgram = 0;
GLuint GPURecOpenGL::texture3DAlphaProgram = 0;
GLuint GPURecOpenGL::updateSliceProgram = 0;
GLuint GPURecOpenGL::unpackProgram = 0;


GLuint GPURecOpenGL::fboId = 0;
GLuint GPURecOpenGL::texQuarterDim = 0; 
GLuint GPURecOpenGL::tex1Dim = 0; 
GLuint GPURecOpenGL::tex2Dim = 0; 
GLuint GPURecOpenGL::unpackTex = 0; 


bool GPURecOpenGL::initialized = false;


void GPURecOpenGL::terminate( void ) {

  if( texQuarterDim )  {
    glDeleteTextures( 1, &texQuarterDim );
    texQuarterDim = 0;
  }
  
  if( tex1Dim ) {
    glDeleteTextures( 1, &tex1Dim ); 
    tex1Dim = 0;
  }
  
  if( tex2Dim ) {
    glDeleteTextures( 1, &tex2Dim );
    tex2Dim = 0;
  }
  
  if( unpackTex ) {
    glDeleteTextures( 1, &unpackTex );
    unpackTex = 0;
  }
  
  if( textureAlphaProgram ) {
    glDeleteProgramsARB(1, &textureAlphaProgram);
    textureAlphaProgram = 0;
  }
  
  if( textureDivProgram ) {
    glDeleteProgramsARB(1, &textureDivProgram);
    textureDivProgram = 0;
  }
  
  if( textureDivPackProgram ) {
    glDeleteProgramsARB(1, &textureDivPackProgram);
    textureDivPackProgram = 0;
  }
  
  if( updateSliceProgram ) {
    glDeleteProgramsARB(1, &updateSliceProgram);
    updateSliceProgram = 0;
  }
  
  if( unpackProgram ) {
    glDeleteProgramsARB(1, &unpackProgram);
    unpackProgram = 0;
  }
  
  // BUG DRIVER !!
  /*if( fboId ) {
    glDeleteFramebuffersEXT(1, &fboId);   
    fboId = 0;
  }*/
  
  initialized = false;
}

void GPURecOpenGL::initialize( unsigned int _dim ) {

  dim = _dim;
  initGLEW();              
  initFBO();        
  initFragmentPrograms();        
  initGLStatesInCurrentContext();
            
  initialized = true;
}

void GPURecOpenGL::reset( unsigned int _dim) { 
  
  if( !initialized ) {
    
    initialize(_dim);
    return;
  }

  // reset FBO settings with the new size
  if( fboId ) {
    glDeleteFramebuffersEXT(1, &fboId);   
    fboId = 0;
  }
  
  if( texQuarterDim )  {
    glDeleteTextures( 1, &texQuarterDim );
    texQuarterDim = 0;
  }
  
  if( tex1Dim ) {
    glDeleteTextures( 1, &tex1Dim ); 
    tex1Dim = 0;
  }
  
  if( tex2Dim ) {
    glDeleteTextures( 1, &tex2Dim );
    tex2Dim = 0;
  }
  
  if( unpackTex ) {
    glDeleteTextures( 1, &unpackTex );
    unpackTex = 0;
  }

  dim = _dim;
  initFBO();  
}

void GPURecOpenGL::initGLEW() {

  GLenum err = glewInit();
  if (GLEW_OK != err)
  {
    /* Problem: glewInit failed, something is seriously wrong. */
    std::cerr << "GLEW Error: " << glewGetErrorString(err) << std::endl;
    throw std::exception();	
  }
}
        
void GPURecOpenGL::initFBO() {

  glGenFramebuffersEXT(1, &fboId);
  glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, fboId);
  
  // initialize color texture
  glGenTextures( 1, &texQuarterDim );  GL_TEST_ERROR
  glBindTexture( GL_TEXTURE_2D, texQuarterDim );   GL_TEST_ERROR
  
  glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA16F_ARB, dim, dim/4, 0, GL_RGBA, GL_FLOAT, NULL );  GL_TEST_ERROR
  
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);                        
  glFramebufferTexture2DEXT( GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_2D, texQuarterDim, 0 );  GL_TEST_ERROR

  // initialize color texture 2
  glGenTextures( 1, &tex1Dim );  GL_TEST_ERROR
  glBindTexture( GL_TEXTURE_2D, tex1Dim );   GL_TEST_ERROR
  glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA16F_ARB, dim, dim, 0, GL_RGBA, GL_FLOAT, NULL );  GL_TEST_ERROR
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);   
  
  // initialize color texture 3
  glGenTextures( 1, &tex2Dim );  GL_TEST_ERROR
  glBindTexture( GL_TEXTURE_2D, tex2Dim );   GL_TEST_ERROR
  glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA16F_ARB, dim, dim, 0, GL_RGBA, GL_FLOAT, NULL );  GL_TEST_ERROR
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);        
  
  // initialize unpack texture
  float *buffer = new float[ dim * dim * 4 ];
  int index = 0;
  for( unsigned int j = 0; j < dim; j++ )
  for( unsigned int i = 0; i < dim; i++ ) {
  
    buffer[index++] = (j%4 == 3); // RED
    buffer[index++] = (j%4 == 2); // GREEN
    buffer[index++] = (j%4 == 1); // BLUE
    buffer[index++] = (j%4 == 0); // ALPHA
  }
  glGenTextures( 1, &unpackTex );  GL_TEST_ERROR
  glBindTexture( GL_TEXTURE_2D, unpackTex );   GL_TEST_ERROR
  glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA, dim, dim, 0, GL_RGBA, GL_FLOAT, buffer );  GL_TEST_ERROR
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP); 
  delete [] buffer;                      
     
  glBindTexture( GL_TEXTURE_2D, 0 );   GL_TEST_ERROR
        
  // check that initialization is OK
  GLenum fbstatus;
  fbstatus = glCheckFramebufferStatusEXT(GL_FRAMEBUFFER_EXT);
  switch(fbstatus) {
    case GL_FRAMEBUFFER_COMPLETE_EXT:
            //All OK
            break;
    case GL_FRAMEBUFFER_UNSUPPORTED_EXT:
            /* Have to use other formats on this hardware/OS/driver combination */
            std::cerr << "I am sorry but this format is not supported on your hardware/OS/driver combination." << std::endl;
            throw std::exception();
            break;
    default:
            std::cerr << "Framebuffer object not initialized the right way, problem in sourcecode!" << std::endl;
            throw std::exception();
  }
  
  glDrawBuffer( GL_COLOR_ATTACHMENT0_EXT );
}

void GPURecOpenGL::initFragmentPrograms() {
  
  glGenProgramsARB(1, &textureAlphaProgram);
  glGenProgramsARB(1, &texture3DAlphaProgram);
  glGenProgramsARB(1, &textureDivProgram);
  glGenProgramsARB(1, &textureDivPackProgram);
  glGenProgramsARB(1, &updateSliceProgram);
  glGenProgramsARB(1, &unpackProgram);
     
  const char* textureAlphaCode = "!!ARBfp1.0\n"
                                 "PARAM alpha = program.local[0];\n"
                                 "TEMP tex0;\n"
                                 "TEX tex0, fragment.texcoord[0], texture[0], 2D;\n"
                                 "MUL result.color, tex0, alpha;\n"
                                 "END\n"; 
                                 
  const char* texture3DAlphaCode = "!!ARBfp1.0\n"
                                 "PARAM alpha = program.local[0];\n"
                                 "TEMP tex0;\n"
                                 "TEX tex0, fragment.texcoord[0], texture[0], 3D;\n"
                                 "MUL result.color, tex0, alpha;\n"
                                 "END\n"; 
       
  const char* textureDivCode =
        "!!ARBfp1.0\n"
        "TEMP tex0;\n"
        "TEMP tex1;\n"
        "TEX tex0, fragment.texcoord[0], texture[0], 2D;\n"
        "TEX tex1, fragment.texcoord[0], texture[1], 2D;\n"
        "MAX tex1.x, tex1.x, 1;\n"
        "RCP tex1, tex1.x;\n"
        "MUL result.color, tex0, tex1.x;\n"
        "END\n";  
        
    const char* updateSliceCode =
        "!!ARBfp1.0\n"
        "OPTION ARB_precision_hint_nicest;\n"  
        "PARAM normalizationFactor = program.local[0];\n"
        "TEMP tex0;\n"
        "TEMP tex1;\n"
        "TEX tex0, fragment.texcoord[0], texture[0], 3D;\n"
        "TEX tex1, fragment.texcoord[1], texture[1], 2D;\n"
        "MUL tex0, tex0, tex1;\n"
        "MUL tex0, tex0, normalizationFactor;\n"        
        "MIN tex0, tex0, 60000;\n"        
        "MOV result.color, tex0;\n"
        "END\n";  
        
    const char* textureDivPackCode =
        "!!ARBfp1.0\n"
        "TEMP tex0;\n"
        "TEMP tex1;\n"
        "TEMP tex11;\n"
        "TEMP tex12;\n"
        "TEMP tex13;\n"
        "TEMP tex14;\n"
        "TEX tex0, fragment.texcoord[0], texture[0], 2D;\n"
        "TEX tex1, fragment.texcoord[0], texture[1], 2D;\n"
        "MAX tex1, tex1, 0.1;\n"
        "RCP tex11, tex1.x;\n"
        "RCP tex12, tex1.y;\n"
        "RCP tex13, tex1.z;\n"
        "RCP tex14, tex1.w;\n"
        "MUL tex1.x, tex0.x, tex11.x;\n"
        "MUL tex1.y, tex0.y, tex12.x;\n"
        "MUL tex1.z, tex0.z, tex13.x;\n"
        "MUL tex1.w, tex0.w, tex14.x;\n"
        "MOV result.color, tex1;\n"
        "END\n"; 
              
       
  const char *unpackCode =  "!!ARBfp1.0\n"
                            "PARAM alpha = program.local[0];\n"
                            "TEMP tex0;\n"
                            "TEMP tex1;\n"
                            "TEX tex0, fragment.texcoord[0], texture[0], 2D;\n"
                            "TEX tex1, fragment.texcoord[1], texture[1], 2D;\n"
                            "DP4 tex0, tex0, tex1;\n"
                            "MUL result.color, tex0, alpha;\n"
                            "END\n";                        
    
  GLint error_pos;       
  
  glBindProgramARB(GL_FRAGMENT_PROGRAM_ARB, textureAlphaProgram);
  glProgramStringARB(GL_FRAGMENT_PROGRAM_ARB, GL_PROGRAM_FORMAT_ASCII_ARB,
                       strlen(textureAlphaCode), textureAlphaCode);
   
  glGetIntegerv(GL_PROGRAM_ERROR_POSITION_ARB, &error_pos);
  if( error_pos != -1 ) {
    const GLubyte *error_string;
    error_string = glGetString(GL_PROGRAM_ERROR_STRING_ARB);
    std::cerr << "Program error at position(" << __LINE__ << "): " << error_pos << error_string << std::endl;
    throw std::exception();
  }
  
  glBindProgramARB(GL_FRAGMENT_PROGRAM_ARB, texture3DAlphaProgram);
  glProgramStringARB(GL_FRAGMENT_PROGRAM_ARB, GL_PROGRAM_FORMAT_ASCII_ARB,
                       strlen(texture3DAlphaCode), texture3DAlphaCode);
   
  glGetIntegerv(GL_PROGRAM_ERROR_POSITION_ARB, &error_pos);
  if( error_pos != -1 ) {
    const GLubyte *error_string;
    error_string = glGetString(GL_PROGRAM_ERROR_STRING_ARB);
    std::cerr << "Program error at position(" << __LINE__ << "): " << error_pos << error_string << std::endl;
    throw std::exception();
  }

    
  glBindProgramARB(GL_FRAGMENT_PROGRAM_ARB, textureDivProgram);
  glProgramStringARB(GL_FRAGMENT_PROGRAM_ARB, GL_PROGRAM_FORMAT_ASCII_ARB,
                       strlen(textureDivCode), textureDivCode);
   
  glGetIntegerv(GL_PROGRAM_ERROR_POSITION_ARB, &error_pos);
  if( error_pos != -1 ) {
    const GLubyte *error_string;
    error_string = glGetString(GL_PROGRAM_ERROR_STRING_ARB);
    std::cerr << "Program error at position(" << __LINE__ << "): " << error_pos << error_string << std::endl;
    throw std::exception();
  }
  
  
  glBindProgramARB(GL_FRAGMENT_PROGRAM_ARB, textureDivPackProgram);
  glProgramStringARB(GL_FRAGMENT_PROGRAM_ARB, GL_PROGRAM_FORMAT_ASCII_ARB,
                       strlen(textureDivPackCode), textureDivPackCode);
   
  glGetIntegerv(GL_PROGRAM_ERROR_POSITION_ARB, &error_pos);
  if( error_pos != -1 ) {
    const GLubyte *error_string;
    error_string = glGetString(GL_PROGRAM_ERROR_STRING_ARB);
    std::cerr << "Program error at position(" << __LINE__ << "): " << error_pos << error_string << std::endl;
    throw std::exception();
  }
  
  
  glBindProgramARB(GL_FRAGMENT_PROGRAM_ARB, updateSliceProgram);
  glProgramStringARB(GL_FRAGMENT_PROGRAM_ARB, GL_PROGRAM_FORMAT_ASCII_ARB,
                       strlen(updateSliceCode), updateSliceCode);
   
  glGetIntegerv(GL_PROGRAM_ERROR_POSITION_ARB, &error_pos);
  if( error_pos != -1 ) {
    const GLubyte *error_string;
    error_string = glGetString(GL_PROGRAM_ERROR_STRING_ARB);
    std::cerr << "Program error at position(" << __LINE__ << "): " << error_pos << error_string << std::endl;
    throw std::exception();
  }
  
  
  glBindProgramARB(GL_FRAGMENT_PROGRAM_ARB, unpackProgram);
  glProgramStringARB(GL_FRAGMENT_PROGRAM_ARB, GL_PROGRAM_FORMAT_ASCII_ARB,
                       strlen(unpackCode), unpackCode);
   
  glGetIntegerv(GL_PROGRAM_ERROR_POSITION_ARB, &error_pos);
  if( error_pos != -1 ) {
    const GLubyte *error_string;
    error_string = glGetString(GL_PROGRAM_ERROR_STRING_ARB);
    std::cerr << "Program error at position(" << __LINE__ << "): " << error_pos << error_string << std::endl;
    throw std::exception();
  }
}

void GPURecOpenGL::initGLStatesInCurrentContext( void )
{
  // disable the OpenGL features we're not using
  glDisable( GL_LIGHTING );          
  glDisable( GL_POLYGON_SMOOTH ) ;    
  glDisable( GL_COLOR_MATERIAL ); 
  glDisable( GL_ALPHA_TEST );  
  glDisable( GL_DEPTH_TEST );
  
  glClearColor(0,0,0,0); 
  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
  glPixelStorei(GL_PACK_ALIGNMENT, 1);    
  
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAX_LEVEL,0);
  glTexParameteri(GL_TEXTURE_3D,GL_TEXTURE_MAX_LEVEL,0);
  
  glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE); 
  
  // default states that can change
  glDisable(GL_FRAGMENT_PROGRAM_ARB);
  glDisable( GL_BLEND );  
  glDisable( GL_TEXTURE_3D );
  glEnable( GL_TEXTURE_2D );    
 
  glViewport(0,0,dim,dim);  
}


// In OpenGL texture coordinates : bottom left corner is at (0,0) and the top right corner is at (1,1)
//
//        t
//        |
//        o- s
// --------------------------------------------------------------------------------------------
void GPURecOpenGL::draw8bitsTexture( int windowSizeX, int windowSizeY ) {
  
  glPushAttrib( GL_ENABLE_BIT );
  glEnable(GL_TEXTURE_2D);
  glDisable(GL_BLEND);  // IMPORTANT: display the texture without blending
  
  glClear( GL_COLOR_BUFFER_BIT );

  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();
     
  glBegin(GL_QUADS) ;
    glTexCoord2f(0,0);glVertex2i( -1, -1 );
    glTexCoord2f(1,0);glVertex2i( 1, -1 ); 
    glTexCoord2f(1,1);glVertex2i( 1, 1 ); 
    glTexCoord2f(0,1);glVertex2i( -1, 1 ); 
  glEnd();  
  
  glPopAttrib();
}

void GPURecOpenGL::draw16bitsTexture( int windowSizeX, int windowSizeY, float alpha ) {
  
  glViewport(0,0,windowSizeX,windowSizeY);
  glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);  GL_TEST_ERROR
  
  glPushAttrib( GL_ENABLE_BIT );
  glEnable( GL_TEXTURE_2D );
  glDisable(GL_BLEND);  // IMPORTANT: display the texture without blending
  
  glClear( GL_COLOR_BUFFER_BIT );

  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();
  
  // save current fragment_program states
  GLboolean isFragmentProgramEnabled = glIsEnabled( GL_FRAGMENT_PROGRAM_ARB );
  int savedProgram;
  glGetProgramivARB( GL_FRAGMENT_PROGRAM_ARB, GL_PROGRAM_BINDING_ARB, &savedProgram ); 
    
  if( alpha != 1.0) {
    glBindProgramARB(GL_FRAGMENT_PROGRAM_ARB, textureAlphaProgram);
    glProgramLocalParameter4fARB( GL_FRAGMENT_PROGRAM_ARB, 0, alpha, alpha, alpha, alpha );
    glEnable(GL_FRAGMENT_PROGRAM_ARB);
  }
  
  glBegin(GL_QUADS) ;
    glTexCoord2f(0,0);glVertex2i( -1, -1 );
    glTexCoord2f(1,0);glVertex2i( 1, -1 ); 
    glTexCoord2f(1,1);glVertex2i( 1, 1 ); 
    glTexCoord2f(0,1);glVertex2i( -1, 1 ); 
  glEnd(); 
  
  // restore old fragment_program states
  if( !isFragmentProgramEnabled ) glDisable(GL_FRAGMENT_PROGRAM_ARB);
  glBindProgramARB(GL_FRAGMENT_PROGRAM_ARB, savedProgram); 
  
  glPopAttrib();
   
   // restore window states
  glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, fboId);  GL_TEST_ERROR
  glViewport(0,0,dim,dim);
}


void GPURecOpenGL::drawFBO( bool unpack, int windowSizeX, int windowSizeY, float alpha ) {
  
  // bind FBO texture 
  glViewport(0,0,windowSizeX,windowSizeY);
  glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);  GL_TEST_ERROR
  glBindTexture(GL_TEXTURE_2D, texQuarterDim);  GL_TEST_ERROR
  
  glPushAttrib( GL_ENABLE_BIT );
  glDisable(GL_BLEND);  // IMPORTANT: display the texture without blending
  
  glEnable(GL_FRAGMENT_PROGRAM_ARB);
  if( unpack ) {
    glActiveTextureARB(GL_TEXTURE1_ARB);  GL_TEST_ERROR 
    glEnable( GL_TEXTURE_2D );
    glBindTexture( GL_TEXTURE_2D, unpackTex ); GL_TEST_ERROR  
    glBindProgramARB(GL_FRAGMENT_PROGRAM_ARB, unpackProgram);  GL_TEST_ERROR
  }
  else {
    glBindProgramARB(GL_FRAGMENT_PROGRAM_ARB, textureAlphaProgram);  GL_TEST_ERROR
  }
  glProgramLocalParameter4fARB( GL_FRAGMENT_PROGRAM_ARB, 0, alpha, alpha, alpha, alpha );
  
  glClear( GL_COLOR_BUFFER_BIT );

  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();
     
  glDisable(GL_TEXTURE_2D);
  glColor3d(0,1,0);
  glBegin(GL_QUADS) ;
    glMultiTexCoord2f(GL_TEXTURE0,0,0);glMultiTexCoord2f(GL_TEXTURE1,0,0);glVertex2i( -1, -1 );
    glMultiTexCoord2f(GL_TEXTURE0,1,0);glMultiTexCoord2f(GL_TEXTURE1,1,0);glVertex2i( 1, -1 ); 
    glMultiTexCoord2f(GL_TEXTURE0,1,1);glMultiTexCoord2f(GL_TEXTURE1,1,1);glVertex2i( 1, 1 ); 
    glMultiTexCoord2f(GL_TEXTURE0,0,1);glMultiTexCoord2f(GL_TEXTURE1,0,1);glVertex2i( -1, 1 ); 
  glEnd();  
  
  glDisable(GL_FRAGMENT_PROGRAM_ARB);
  
  if( unpack ) {
    glActiveTextureARB(GL_TEXTURE1_ARB);  GL_TEST_ERROR 
    glDisable( GL_TEXTURE_2D );
    glActiveTextureARB(GL_TEXTURE0_ARB);  GL_TEST_ERROR 
  }

  glPopAttrib();
  
  // restore window states
  glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, fboId);  GL_TEST_ERROR
  glViewport(0,0,dim,dim);
}



// ============================================================================================
// --------------------------------------------------------------------------------------------
//  VIEW MANAGEMENT
// ============================================================================================
//  Volume centered on (0,0,0)
//  Axis:
//             z y
//             |/
//             ---x
// --------------------------------------------------------------------------------------------

void GPURecOpenGL::sideView() {

  assert( initialized );

  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  glOrtho( -1, 1, -1, 1, 0, 4 );  // be careful to not clip the cube in the z direction      
  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();
  
  gluLookAt(  0, -2, 0,   // eye point
              0, 1, 0,    // focus point
              0, 0, 1);   // up vector    
}

void GPURecOpenGL::topView() {

  assert( initialized );

  glMatrixMode(GL_PROJECTION);
  glLoadIdentity(); // default projection is glOrtho( -1, 1,  -1, 1,   1, -1 ); 
        
  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();
  
  gluLookAt(  0, 0, 1,    // eye point
              0, 0, -1,   // focus point
              0, 1, 0);   // up vector    
}


// ===============================================================================================
   
