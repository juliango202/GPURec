
#include "common.h"

#include <iostream>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cctype>
#define _USE_MATH_DEFINES
#include <cmath>

#include "HdrFile.h"
#include "Volume.h"
#include "VolumeProjectionSet.h"

using namespace GPURec;

HdrFile::HdrFile( const std::string& fileName ) {

  // get system time
  time( &openTime );

  // extract the path from HDR fileName if any
  path = "";
  if( fileName.find("/") != std::string::npos )
    path = fileName.substr( 0, fileName.rfind("/") +1 );
  else if( fileName.find("\\") != std::string::npos )
    path = fileName.substr( 0, fileName.rfind("\\") +1 );   
  
  std::ifstream file( fileName.c_str(), std::ios::in );
      
  if( !file ) {
    std::cerr << "error opening file " << fileName << std::endl;
    throw std::exception();
  }
  
  std::string ln;
  while( file.good() )
  {
    // read the next line
    std::getline( file, ln );
   
    // retrieve the key (in HDR separation sign is ":=")    
    std::string key = ln.substr( 0, ln.find(":=") );
    
    // remove blanks
    key.erase( std::remove(key.begin(), key.end(), ' '), key.end() );    
    key.erase( std::remove(key.begin(), key.end(), '\t'), key.end() );
    // make it lower case
    std::transform( key.begin(), key.end(), key.begin(), tolower);
   
    // retrieve the value if any
    std::string value;
    if( ln.size() > (ln.find(":=") +2) )  {
      value = ln.substr( ln.find(":=")+2 );
        
      // remove begining and ending spaces (trim function)
      value.erase( value.find_last_not_of(' ') + 1 );
      value.erase( 0, value.find_first_not_of(' ') );
      // remove special characters
      value.erase( std::remove(value.begin(), value.end(), '\r'), value.end() );
      value.erase( std::remove(value.begin(), value.end(), '\n'), value.end() );
      value.erase( std::remove(value.begin(), value.end(), '\t'), value.end() );
      // make it lowercase except for the data filename
      if( key != "nameofdatafile" )
        std::transform( value.begin(), value.end(), value.begin(), tolower);
    }
    
    values[key] = value;
  }  

  file.close();

  checkGPURecCompatibility();
}


void HdrFile::loadVolumeProjectionSet( VolumeProjectionSet& projectionSet, unsigned int num ) const {

  unsigned int nbProjections = (unsigned int)getNumericValue("numberofprojections");
  unsigned int dim = (unsigned int)getNumericValue("matrixsize[1]");
  
  // compute rotation increment
  float rotationIncrement;
  float rotationExtent = getNumericValue("extentofrotation") * M_PI / 180.0f; // radian unit
  std::string rotationDirection = getStringValue("directionofrotation");
  if( rotationDirection == "ccw" )
    rotationIncrement = (rotationExtent / nbProjections);   
  else
    rotationIncrement = (-rotationExtent / nbProjections);  
 
  projectionSet.createFromRAW(  dim, 
                                getNumericValue("scalingfactor(mm/pixel)[1]") / 1000.f,   // METER unit
                                nbProjections,  
                                (getNumericValue("startangle") + START_ANGLE_SHIFT) * M_PI / 180.0f,
                                rotationIncrement,
                                path + getStringValue("nameofdatafile"), 
                                num * nbProjections * dim * dim * sizeof(unsigned short) );  
}


void HdrFile::saveVolume( std::string fileName, const Volume& volume, unsigned int num ) const {  

  // construct RAW filename
  std::string rawFileName = "";
    // remove the file extension if any
  if( fileName.find(".") != std::string::npos )
    rawFileName = fileName.substr(0, fileName.rfind("."));
  else
    rawFileName = fileName;
  rawFileName += ".raw";  
    
  volume.saveToRAW( rawFileName, (num > 0) );  
    
  // remove path from RAW filename before writing it in HDR
  if( rawFileName.find("/") != std::string::npos )
    rawFileName = rawFileName.substr( rawFileName.rfind("/") +1 );
  else if( rawFileName.find("\\") != std::string::npos )
    rawFileName = rawFileName.substr( rawFileName.rfind("\\") +1 );
 
    
  std::ofstream hdrFile( fileName.c_str(), std::ios::out );

  if( !hdrFile ) {
    std::cerr << "error creating file " << fileName << std::endl;
    throw std::exception();
  }

  // create a study ID string containing the reconstruction algorithm used
  std::stringstream studyId;
  studyId << "OSEM";
  if( USE_OSEM3D )
    studyId << "3D";
  studyId << "-GPU(" << NB_ITERATIONS << "it," << NB_SUBSETS << "sub)";
  
  // create a string with reconstruction parameters
  std::stringstream recParameters;
  recParameters <<  "CAMERA_ROTATION_RADIUS(" << CAMERA_ROTATION_RADIUS <<  "), CAMERA_RESOLUTION(" << CAMERA_RESOLUTION <<
                    "), COLLIMATOR_HOLES_DIAMETER(" << COLLIMATOR_HOLES_DIAMETER <<
                    "), COLLIMATOR_DEPTH(" << COLLIMATOR_DEPTH << ")";

  // get system time
  time_t currentTime;
  time( &currentTime );
  
  unsigned int dim = (unsigned int)getNumericValue("matrixsize[1]");
  
  // create HDR file
  hdrFile << 
	"!INTERFILE :=\n"
	"!imaging modality := " << getStringValue("imagingmodality") << "\n"
	"!originating system := " << getStringValue("originatingsystem") << "\n"
	"!version of keys := 3.3\n"
	"conversion program := GPURec\n"
	"program date := " << GPUREC_PROGRAM_DATE << "\n"
  "program version := " << GPUREC_PROGRAM_VERSION << "\n"
  ";\n"
	"!GENERAL DATA :=\n"
	"!data starting block := 0\n"
	"!name of data file := " << rawFileName << "\n"
	"patient name := " << getStringValue("patientname") << "\n"
	"!patient ID := " << getStringValue("patientID") << "\n"
	"patient dob := " << getStringValue("patientdob") << "\n"
	"patient sex := " << getStringValue("patientsex") << "\n"
	"!study ID := " << studyId.str() << "\n"
	"data compression := none\n"
	"data encode := none\n"
  ";\n"
	"!GENERAL IMAGE DATA :=\n"
	"!type of data := " << getStringValue("typeofdata") << "\n"
	"!total number of images := "<< dim * (num+1) << "\n"
	";reference study date := " << getStringValue("studydate") << "\n"
	"imagedata byte order := LITTLEENDIAN\n"
	"number of energy windows := 1\n"
  ";\n"
	"!SPECT STUDY (general) :=\n"
	"number of images/energy window := "<< dim << "\n"
	"!process status := reconstructed\n"
	"!matrix size [1] := "<< dim << "\n"
	"!matrix size [2] := "<< dim << "\n"
	"!matrix size [3] := "<< dim << "\n"
  "!matrix size [4] := " << num+1 << "\n"
	"!number format := unsigned integer\n"
	"!number of bytes per pixel := 2\n"
	"scaling factor (mm/pixel) [1] := " << getStringValue("scalingfactor(mm/pixel)[1]") << "\n"
	"scaling factor (mm/pixel) [2] := " << getStringValue("scalingfactor(mm/pixel)[1]")  << "\n"
	"scaling factor (mm/pixel) [3] := " << getStringValue("scalingfactor(mm/pixel)[1]")  << "\n"
	"!number of projections :=\n"
	"!extent of rotation :=\n"
	"!time per projection (sec) :=\n"
	"!maximum pixel count :=\n"
	"patient orientation := " << getStringValue("patientorientation") << "\n"
	"patient rotation := " << getStringValue("patientrotation") << "\n"
  ";\n"
	"!SPECT STUDY (reconstructed data) :=\n" 
  "method of reconstruction := " << studyId.str() << "\n"
  "reconstruction date := " << ctime( &currentTime ) <<
  "reconstruction duration := " << difftime( currentTime, openTime ) << " seconds\n"
  "reconstruction parameters := " << recParameters.str() << "\n"
	"!number of slices := "<< dim << "\n"
	"!END OF INTERFILE :=\n";
  hdrFile.close();  
}


void HdrFile::saveVolumeProjectionSet( std::string fileName, const VolumeProjectionSet& projSet, unsigned int num ) const {

  // construct RAW filename
  std::string rawFileName = "";
    // remove the file extension if any
  if( fileName.find(".") != std::string::npos )
    rawFileName = fileName.substr(0, fileName.rfind("."));
  else
    rawFileName = fileName;
  rawFileName += ".raw";  
    
//   volume.saveToRAW( rawFileName, (num > 0) );  


}


void HdrFile::checkGPURecCompatibility() const {
  
  unsigned int sizeX = (unsigned int)getNumericValue("matrixsize[1]");
  unsigned int sizeY = (unsigned int)getNumericValue("matrixsize[2]");
  if( sizeX == 0 || sizeX != sizeY ) {

    std::cerr << "Error in HDR file: invalid (or absent) matrixsize" << std::endl;
    if( sizeX != sizeY ) std::cerr << "image width and height have to be identical" << std::endl;
    throw std::exception();
  }

  unsigned int nbProjection = (unsigned int)getNumericValue("numberofprojections");
  if( nbProjection == 0 ) {

    std::cerr << "Error in HDR file: invalid (or absent) numberofprojections" << std::endl;
    throw std::exception();
  }
  
  unsigned int numberOfImages =  (unsigned int)getNumericValue("totalnumberofimages");
  if( numberOfImages == 0 ) {

    std::cerr << "Error in HDR file: invalid (or absent) totalnumberofimages" << std::endl;
    throw std::exception();
  }
  
  if( numberOfImages / (float)nbProjection != numberOfImages / nbProjection ) {
  
    std::cerr << "Error in HDR file: the number of images has to be a multiple of the number of projections." << std::endl;
    throw std::exception();
  }

  float pixelSizeX  =  getNumericValue("scalingfactor(mm/pixel)[1]");
  float pixelSizeY  =  getNumericValue("scalingfactor(mm/pixel)[2]");
  if( pixelSizeX == 0 ) {

    std::cerr << "Error in HDR file: invalid (or absent) scaling factor" << std::endl;
    throw std::exception();
  }
  
  if( pixelSizeX != pixelSizeY )      
    std::cerr << "Warning: in HDR file: GPURec doesn't support different x and y scaling factors: only x value will be used" << std::endl;
  
  if( getNumericValue("numberofbytesperpixel") != 2 || getStringValue("numberformat") != "unsigned integer" ) {
  
    std::cerr << "Error in HDR file: at present GPURec only support 2 bytes unsigned integer data" << std::endl;
    throw std::exception();
  }

  std::string rotationDirection = getStringValue("directionofrotation");
  if( rotationDirection != "cw" && rotationDirection != "ccw" ) {

    std::cerr << "Error in HDR file: invalid (or absent) direction of rotation" << std::endl;
    throw std::exception();
  }
}


const std::string HdrFile::getStringValue( const std::string& key ) const {

  if( values.find(key) == values.end() ) {

    if( values.find( "!" + key ) == values.end() )
      return "";
    else
     return (values.find( "!" + key ))->second;
  }
  else
    return (values.find( key ))->second;
}


float HdrFile::getNumericValue( const std::string& key ) const {
  
  std::stringstream value( getStringValue(key) );
      
  float result = 0.0;
  value >> result;

  return result;
}

