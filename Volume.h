
#ifndef _VOLUME_H
#define _VOLUME_H

#include <string>
#include <GL/glew.h>
#include <GL/glut.h>

namespace GPURec {


enum MajorAxis {X_AXIS, Y_AXIS, Z_AXIS};

class Volume {

public:

        // CONSTRUCTOR - DESTRUCTOR
        // ------------------------------------------
        Volume();
        ~Volume() { terminate(); }
        void initialize( unsigned int _dim );   // allocate ressources and assign default values
        void terminate();                       // free all allocated ressources
        void reset( unsigned int dim ) { terminate(); initialize( dim ); }
                
        
        // IO methods
        // ------------------------------------------        
        void createEmpty( unsigned int _dim = 64 );
        void saveToRAW( const std::string& fileName, bool append = false ) const;


        //  MATHEMATICAL TRANSFORMS
        // ------------------------------------------
        void projection( double angle, bool convolve = true ) const;
        
        void downSample( Volume &destVolume );
        
        //  GRAPHIC MEMORY TRANSFERS
        // ------------------------------------------
        void sendToGraphicMemory();         // create a 3D texture with the data and load it to graphic memory
        void retrieveFromGraphicMemory();   // copy back the texture to the data array in main memory


        // GETTERS
        // ------------------------------------------
        GLuint getVolumeTex() { return volumeTex; }
        unsigned int getDim( void ) const { return dim; }; 
        float getMaxValue() { return maxValue; }
        float voxelSize( void ) const { return 2.0 / dim; };
        
        float& value( unsigned int i, unsigned int j, unsigned int k ) {  
          return data[ i + j*dim + k*dim*dim ];     
        }
        
        float value( unsigned int i, unsigned int j, unsigned int k ) const {  
          return data[ i + j*dim + k*dim*dim ];     
        }

        
        // DEBUG methods
        // ------------------------------------------ 
        void draw( double angleH, double angleV ) const;
        void drawBoundingBox() const;
        void drawGrid() const;
        void loadSliceAsTexture( MajorAxis axis, int sliceNumber ) const;   // load a slice of the volume in the current texture 
        friend float diff( const Volume& v1, const Volume& v2 );  // compute the Relative Mean Error between 2 volumes 

        
protected:

  // volume coordinate correspond to the index of the array 
  // in world coordinates the volume coords ranges between -1 and 1 (same for all direction)
  float volumeToWorldCoord( float coord ) const {
    return ( -1 + voxelSize()/2.0 + coord * voxelSize() );
  }


  float *data;         // volume values    
  unsigned int dim;    // volume dimension: for non-cubic dimensions split into several cubic volumes 
  
  GLuint volumeTex;    // texture object IDs
  GLuint vsliceTex;

  float maxValue;
};


} // end namespace GPURec

#endif  // _VOLUME_H
