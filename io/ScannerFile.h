
#ifndef _SCANNERFILE_H
#define _SCANNERFILE_H

namespace GPURec {

class Volume;
class VolumeProjectionSet;

class ScannerFile {

public: 

  virtual void    loadVolumeProjectionSet ( VolumeProjectionSet& projectionSet, unsigned int num = 0 ) const = 0;
  virtual void    saveVolume              ( std::string fileName, const Volume& volume, unsigned int num = 0 ) const = 0;
  virtual void    saveVolumeProjectionSet ( std::string fileName, const VolumeProjectionSet& projSet, unsigned int num = 0 ) const = 0; 
  virtual void    checkGPURecCompatibility() const = 0;
};

} // end namespace GPURec


#endif  // _SCANNERFILE_H
