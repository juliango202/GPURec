
#include "DBGutils.h"

// ============================================================================================ //
// -------------------------------------------------------------------------------------------- //
//  TIMER FUNCTIONS
// -------------------------------------------------------------------------------------------- //
// ============================================================================================ //

std::map<const std::string, DBGutils::Timer> DBGutils::timers;

#ifdef _WIN32
LARGE_INTEGER DBGutils::Timer::freq;
#else
struct timezone DBGutils::Timer::tz;
#endif

void DBGutils::timerBegin( char* const name ) {

#ifdef _DBG_OPENGL
  glFinish();
#endif
  timers[std::string(name)].start();   // first request for this log 
} 
  
void DBGutils::timerEnd( char* const name ) {
#ifdef _DBG_OPENGL
  glFinish();
#endif
  timers[std::string(name)].stop();
}

void DBGutils::timersInfo( std::ostream& os ) {

  os << std::endl << "===== Timers =====" << std::endl;
  
  for( std::map<const std::string, DBGutils::Timer>::iterator itTimer = timers.begin(); itTimer != timers.end(); itTimer++ ) {
  
    os << std::endl << " - " << itTimer->first << " - " << std::endl;
    os << "|  min| " << (itTimer->second).min << std::endl;
    os << "|  max| " << (itTimer->second).max << std::endl;
    os << "|  avg| " << ( (itTimer->second).sum / (itTimer->second).count ) << std::endl; 
    os << "|total| " << ( (itTimer->second).sum ) << std::endl; 
    os << "|count| " << (itTimer->second).count << std::endl;
  }

  os << std::endl;
}
