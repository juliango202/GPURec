
#ifndef _PHANTOM_H
#define _PHANTOM_H

#include <GL/glew.h>
#include <GL/glut.h>

#include "Volume.h"

namespace GPURec {

class VolumeProjectionSet;

enum PhantomType {HEMISPHERE, RANDOM_VALUES, IMAGES, PH_POINT, PH_LINE, PH_STRIPS};

class Phantom : public Volume {

public:

        // CONSTRUCTOR - DESTRUCTOR
        // ------------------------------------------
        void create( PhantomType type = HEMISPHERE, unsigned int _dim = 64 );
         

        void saveProjections( VolumeProjectionSet& projSet, unsigned int nbProjections );
};


} // end namespace GPURec

#endif  // _PHANTOM_H
