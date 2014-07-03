
#ifndef _VOLUMEPROJECTIONSET_H
#define _VOLUMEPROJECTIONSET_H

#include <string>
#include <GL/glew.h>
#include <GL/glut.h>

namespace GPURec {

class Volume;

struct VolumeProjection {

  VolumeProjection() : angle(0.f), texture(0), data(0) {}
  
  float angle;
  GLuint texture;
  float *data;  
};


// This class contains the projections of a volume
//
class VolumeProjectionSet {

public:	

        // CONSTRUCTOR - DESTRUCTOR
        // ------------------------------------------
        VolumeProjectionSet()   { initialize(); }
        ~VolumeProjectionSet()  { terminate(); }   
        void initialize();      // assign default values
        void terminate();       // free all allocated ressources
        void reset()            { terminate(); initialize(); }
             

        // IO methods
        // ------------------------------------------     
        void createEmpty( unsigned int _dim, unsigned int _nbProjection, float _startAngle = 0.0, float _rotationIncrement = 0.0 );
        void createFromRAW(  unsigned int _dim, float _pixelSize, unsigned int _nbProjection, float _startAngle, float _rotationIncrement, const std::string& fileName, unsigned int offset = 0 );

        
        //  GRAPHIC MEMORY TRANSFERS
        // ------------------------------------------
        void sendToGraphicMemory();         // create 2D textures of the projections and load them in graphic memory
        void retrieveFromGraphicMemory();   // copy back the textures to the data arrays in main memory

        //  MATHEMATICAL TRANSFORMS
        // ------------------------------------------
        void backProjection( Volume& volume ) const;
        void backProjectSlice( unsigned int sliceNum ) const;
        

        //  RECONSTRUCTION methods
        //
        void osemIteration( Volume& volume, const VolumeProjectionSet& scan );
          

        // GETTERS
        // ------------------------------------------
        unsigned int    getDim( void ) const { return dim; };  
        unsigned int    getNbProjection( void ) const { return nbProjection; };  
        float           getStartAngle( void ) const { return startAngle; };
        float           getRotationIncrement( void ) const { return rotationIncrement; };
        float           getPixelSize( void ) const { return pixelSize; };
        GLuint          getTexId( unsigned int projNum ) { return projections[projNum].texture; }


        //  DEBUG methods
        //
        void loadProjectionAsTexture( unsigned int projectNum ) const;
        
private:
    
  VolumeProjection *projections;  // array of volume projection for each angle
  unsigned int nbProjection;      // number of projections in the set
  unsigned int dim;               // volume dimensions 
  float startAngle;               // angle of the first projection 
  float rotationIncrement;        // angle between 2 projections
  float pixelSize;                // dimension, in meters, of a pixel

  unsigned int currentSubset;     // store active subset during reconstruction
};


} // end namespace GPURec

#endif  // _VOLUMEPROJECTIONSET_H
