
#ifndef _HDRFILE_H
#define _HDRFILE_H

#include <string>
#include <map>
#include <ctime>

#include "ScannerFile.h"


namespace GPURec {


class HdrFile : public ScannerFile {

public:  
  // load the HDR file and fill the map with the key-value pairs
  HdrFile( const std::string& _fileName );
  
  void              loadVolumeProjectionSet( VolumeProjectionSet& projectionSet, unsigned int num = 0 ) const;
  void              saveVolume( std::string fileName, const Volume& volume, unsigned int num = 0 ) const;
  void              saveVolumeProjectionSet( std::string fileName, const VolumeProjectionSet& projSet, unsigned int num = 0 ) const;
  void              checkGPURecCompatibility() const;
   
  unsigned int      getNbScans() const { return (unsigned int)(getNumericValue("totalnumberofimages")/getNumericValue("numberofprojections")); }
 
   
private:  
  const std::string getStringValue( const std::string& key ) const;
  float             getNumericValue( const std::string& key ) const;
 
  std::map<std::string /*key*/, std::string /*value*/> values;
  std::string path;
  time_t openTime;
};



} // end namespace GPURec

#endif  // _HDRFILE_H
