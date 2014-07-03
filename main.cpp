
#include "common.h"
#include <GL/glew.h>
#include <GL/glut.h>

#include <iostream>
#include <sstream>
#include <fstream>
#include <algorithm>
#define _USE_MATH_DEFINES
#include <cmath>

#include "GLutils.h"
#include "DBGutils.h"

#include "Volume.h"
#include "Phantom.h"
#include "VolumeProjectionSet.h"
#include "HdrFile.h"
#include "GPURecOpenGL.h"
#include "GPUGaussianConv.h"

using namespace GPURec;

int thePlaneNum = 0;

Phantom phantom;
Phantom phantom8;
Volume volume;
Volume downSampledVolume;

VolumeProjectionSet scan;


float maxVal = 0.9f;

double viewAngle = 0.0;
double viewDistance = 2.0;        

int sliceNumber=0;
int projNumber = 1;

MajorAxis axis = Z_AXIS;

int viewType = 0;

int windowSizeX = 200;
int windowSizeY = 200;

void Reshape(int width, int height)
{
  windowSizeX = width;
  windowSizeY = height;
 
  std::cout << "resize: " << width << " " << height << std::endl;
}

int viewTex = 0;
bool volOrigin = true;

bool onePlane = false;
bool pleinplan = false;

void gestionClavierSpecial (int key, int x, int y)  
{  
    switch (key) 
    {  
            case GLUT_KEY_LEFT :  viewAngle -= 0.04; break ;
            case GLUT_KEY_RIGHT : viewAngle += 0.04; break ;
            case GLUT_KEY_UP :    if(viewDistance > 0.1) viewDistance -= 0.1; break ;
            case GLUT_KEY_DOWN :  viewDistance += 0.1; break ;
                                                    
            case GLUT_KEY_PAGE_UP: 
              break;
            case GLUT_KEY_PAGE_DOWN: 
              break;
    
            case GLUT_KEY_F8: viewTex++; if( viewTex == 3 ) viewTex = 0; break;
             case GLUT_KEY_F9: pleinplan = !pleinplan; break;   
            case GLUT_KEY_F10:onePlane = !onePlane; break;                      
            case GLUT_KEY_F11: if(viewType == 0) viewType = 1; else viewType = 0; break;
            case GLUT_KEY_F12 : volOrigin = !volOrigin; 
                                if( volOrigin )
                                  std::cout<< "downsample" <<std::endl;
                                else
                                  std::cout<< "normal" <<std::endl;
                                break;    
                                
           case GLUT_KEY_F7:
              if( axis == Z_AXIS ) axis = X_AXIS; 
              else if( axis == X_AXIS ) axis = Y_AXIS;
              else axis = Z_AXIS; 
              break;                                         
    }	
      
    std::cout << viewTex << std::endl;
        
    //glutSetWindow (theTextureWindow);
    //glutPostRedisplay ();
}

void gestionClavierNormal (unsigned char key, int x, int y)  
{
    static int logCount = 0;
    
    if (key == 27) {
        
        GPURecOpenGL::terminate();
        GPUGaussianConv::terminate();
        
        // create a log file
        std::ofstream logFile( "perf.log" );     
        
        /*std::cout << "Comment for the log file: ";
        std::string comment;
        std::cin >> comment;        
        logFile << comment << std::endl;   */
               
        DBGutils::timersInfo( logFile );
        
        logFile.close();
         
        exit(0);
    }
                
    if (key == 'l')
      GLutils::logOpenglStates( "manual", 50 );      

    if (key == 't')
      if( projNumber < 64 ) projNumber++;
    if (key == 'r')
      if( projNumber > 0 ) projNumber--;
      
    if (key == 'p')
      if( thePlaneNum < 64 ) thePlaneNum++;
    if (key == 'o')
      if( thePlaneNum > 0 ) thePlaneNum--;
    
    std::cout<< projNumber<<std::endl;
    
      
}

#define PHANTOM_SIZE 128



void Draw()
{   
  // ================= PROJECTION ===================== //
    glViewport( 0, 0, PHANTOM_SIZE, PHANTOM_SIZE/4 );  
 
    float normFact = projNumber/(float)(1*PHANTOM_SIZE);
    /*if( !onePlane && pleinplan ) normFact = projNumber/(float)(10*PHANTOM_SIZE);
    if( onePlane ) normFact = 1.0;   */
    
    if( volOrigin )
     phantom.projection( viewAngle, false );
    else
     volume.projection( viewAngle, false );
    GPURecOpenGL::drawFBO( true, windowSizeX, windowSizeY, normFact );
 
    glViewport( 0, 0, PHANTOM_SIZE, PHANTOM_SIZE ); 
  


  // ================= SLICE VIEW ===================== //
    //volume.loadSliceAsTexture( axis, projNumber );      
    //GPURecOpenGL::draw16bitsTexture( windowSizeX, windowSizeY, 0.9 );
    

  // ================= SCAN VIEW ===================== //
//   theProjectionSet.loadReferenceProjAsTexture(projNumber);
//   glViewport( 0, 0, scan.getDim(), scan.getDim()/4 );  
// 
//   glDisable(GL_BLEND);  // IMPORTANT: display the texture without blending
//   
//   glClear( GL_COLOR_BUFFER_BIT );
//  
//   glMatrixMode(GL_PROJECTION);
//   glLoadIdentity();
//   glMatrixMode(GL_MODELVIEW);
//   glLoadIdentity(); 
//   glBegin(GL_QUADS) ;
//    glTexCoord2f(0,0);glVertex2i( -1, -1 );
//    glTexCoord2f(1,0);glVertex2i( 1, -1 ); 
//    glTexCoord2f(1,1);glVertex2i( 1, 1 ); 
//    glTexCoord2f(0,1);glVertex2i( -1, 1 ); 
//   glEnd(); 
// 
//   GPURecOpenGL::drawFBO( true, windowSizeX, windowSizeY, thePlaneNum/(float)scan.getDim() );
//   glViewport( 0, 0, scan.getDim(), scan.getDim() );  

  glutSwapBuffers();
}


void idle (void)
{
  glutPostRedisplay ();
};

namespace GPURec {

bool USE_OSEM3D = true;
float CAMERA_ROTATION_RADIUS;
float CAMERA_RESOLUTION;
float COLLIMATOR_HOLES_DIAMETER;
float COLLIMATOR_DEPTH;
unsigned int NB_SUBSETS = 10;
unsigned int NB_ITERATIONS = 3;
std::string programPath = "";

} // end of namespace GPURec


void loadConfigFile( const char* fileName ) {

  std::ifstream file( fileName, std::ios::in );
      
  if( !file ) {
    std::cerr << "Initialization: error opening file " << fileName << std::endl;
    throw std::exception();
  }
 
  std::string ln;
  while( file.good() )
  {
    // read the next line
    std::getline( file, ln );
    
    // separate parameter name and value    
    std::string paramName = ln.substr( 0, ln.find("=") );    
      // remove blanks
    paramName.erase( std::remove(paramName.begin(), paramName.end(), ' '), paramName.end() );
   
    // retrieve the value if any
    std::stringstream paramValue("undefined");
    if( ln.size() > (ln.find("=") +1) )  {
      paramValue.str( ln.substr( ln.find("=")+1 ) );
    }
    
    if( paramName == ("USE_OSEM3D") )
    {       
      paramValue >> USE_OSEM3D;
    }
    else if( paramName == ("NB_SUBSETS") )
    {       
      paramValue >> NB_SUBSETS;
    }
    else if( paramName == ("NB_ITERATIONS") )
    {       
      paramValue >> NB_ITERATIONS;
    }
    else if( paramName == ("CAMERA_ROTATION_RADIUS") )
    {
      paramValue >> CAMERA_ROTATION_RADIUS;
    }
    else if( paramName == ("CAMERA_RESOLUTION") )
    {
      paramValue >> CAMERA_RESOLUTION;
    }
    else if( paramName == ("COLLIMATOR_HOLES_DIAMETER") )
    {            
      paramValue >> COLLIMATOR_HOLES_DIAMETER;
    }
    else if( paramName == ("COLLIMATOR_DEPTH") )
    {
      paramValue >> COLLIMATOR_DEPTH;
    }
  }
  
  file.close();
}


void reconstruction( const VolumeProjectionSet& scan, Volume& reconstructedVolume ) {
 
   GPURecOpenGL::reset( scan.getDim() );
   GPUGaussianConv::reset( scan.getDim(), scan.getPixelSize() );       
 
   reconstructedVolume.createEmpty( scan.getDim() ); 
   
   VolumeProjectionSet theProjectionSet;
   theProjectionSet.createEmpty( scan.getDim(), scan.getNbProjection(), scan.getStartAngle(), scan.getRotationIncrement() );  
  
   for( unsigned int i = 0; i < NB_ITERATIONS; i++ ) {                                                               
     theProjectionSet.osemIteration( reconstructedVolume, scan );
   }  
   
//    scan.backProjection( reconstructedVolume );
         
   reconstructedVolume.retrieveFromGraphicMemory();      
   std::cout << "Volume maximum value: " << reconstructedVolume.getMaxValue() << std::endl;
}
 

int main( int argc, char *argv[ ], char *envp[ ] )
{
  try {

      // CHECK ARGUMENTS 
      
      // check the number of parameters
      if( argc < 2 ) {
              
        std::cout << std::endl << "Usage: GPURec inputHdrFile [outputFile]" << std::endl;
        std::cout << "'inputHdrFile' is the interfile containing the scan to reconstruct." << std::endl;
        std::cout << "'outputFile' is the destination file to store the reconstructed volume." << std::endl << std::endl;
        return 4;
      }
          
      // extract program path to search for the config file in the same directory
      programPath = argv[0];
      programPath = programPath.substr( 0, programPath.rfind("\\") +1 );
      std::string configFile = programPath + DEFAULT_CONFIG_FILENAME;
      loadConfigFile( configFile.c_str() );

      std::string inputHdrFile( argv[1] );
      std::string outputFile;
      if( argc > 2 ) {
      
        // if no path is specified take the program path
        if( outputFile.find("/") != std::string::npos || outputFile.find("\\") != std::string::npos )
          outputFile = argv[2];
        else
          outputFile = programPath + argv[2];
      }
      else  {
      
        // DEFAULT OUTPUT FILE: add "_rec" to input file
        std::string extension = "";
        if( inputHdrFile.find(".") != std::string::npos ) {
          extension = inputHdrFile.substr( inputHdrFile.rfind(".") );
          outputFile = inputHdrFile.substr( 0, inputHdrFile.rfind(".") );
        }
        else
          outputFile = inputHdrFile;
          
        // remove path from input filename if any
        if( outputFile.find("/") != std::string::npos )
          outputFile = outputFile.substr( outputFile.rfind("/") +1 );
        else if( outputFile.find("\\") != std::string::npos )
          outputFile = outputFile.substr( outputFile.rfind("\\") +1 );
          
        outputFile = programPath + outputFile + "_rec" + extension;
      }
      
      
  // OPENGL SETUP
  
      int theMainWindow;
      glutInit(&argc,argv); 
      glutInitWindowSize(PHANTOM_SIZE, PHANTOM_SIZE);      
      glutInitDisplayMode( GLUT_RGBA | GLUT_DOUBLE );
      theMainWindow = glutCreateWindow("Volume Projection");  
      
      
  // RECONSTRUCTION

      DBGutils::timerBegin("TOTAL");
 
/*    HdrFile hdrFile( inputHdrFile );
       
      for( unsigned int s = 0; s < hdrFile.getNbScans(); s++ ) {
           
        std::cout << "Scan: " << s+1 << std::endl;
        hdrFile.loadVolumeProjectionSet( scan, s );  
        reconstruction( scan, volume );  
        hdrFile.saveVolume( outputFile, volume, s );
      }  */           
      
      GPURecOpenGL::reset( PHANTOM_SIZE );
      GPUGaussianConv::reset( PHANTOM_SIZE, DEFAULT_PIXEL_SIZE );
      phantom.create( HEMISPHERE, PHANTOM_SIZE );
      phantom.saveProjections( scan, 60 );
      
      
      reconstruction( scan, volume );  
//       downSampledVolume.createEmpty( PHANTOM_SIZE / 2 ); 
//       volume.downSample( downSampledVolume );
      
/*      GPURecOpenGL::reset( PHANTOM_SIZE / 2 );
      GPUGaussianConv::reset( PHANTOM_SIZE / 2, DEFAULT_PIXEL_SIZE * 2 );
      phantom8.create( HEMISPHERE, PHANTOM_SIZE / 2 );
      phantom8.saveProjections( scan, 60 );
//       scan.createFromVolume( phantom8, 60 );
      reconstruction( scan, volume ); */ 
       

      DBGutils::timerEnd("TOTAL");
      
  // OPENGL DISPLAY   
      
      glutReshapeFunc(Reshape);
      glutDisplayFunc(Draw);
      glutSpecialFunc (gestionClavierSpecial) ;
      glutKeyboardFunc (gestionClavierNormal) ;
      glutIdleFunc (idle);
      glutMainLoop();
  }
  catch( std::exception& e ) {
  
    // ABNORMAL TERMINATION
    std::cerr << e.what() << std::endl;
    GPURecOpenGL::terminate();
    GPUGaussianConv::terminate();
    return 4;
  }
        
  // NORMAL TERMINATION
  DBGutils::timersInfo( std::cout );
  //char u;
  //std::cin >> u;
  GPURecOpenGL::terminate();
  GPUGaussianConv::terminate();

  return 0;
}




