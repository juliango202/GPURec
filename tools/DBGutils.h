
#ifndef _DBGUTILS_H
#define _DBGUTILS_H

#define _DBG_OPENGL 1

#ifdef _DBG_OPENGL
  #include <GL/glut.h>
#endif

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/time.h>
#endif

#include <map>
#include <string>
#include <iostream>

// The DBGutils class provides static functions to help debugging code such as asserts, timing functions, ...
class DBGutils
{
public:

  void initialize( void );
  
  // Measure the time, in seconds, between calls to timerBegin and timerEnd
  // given the possibility of being preempted by OS, and the time needed by the timer itself,
  // these functions shouldn't be used on statements of little execution time
  //  !! use glFinish() before stoping timer of opengl function !!
  // when using IO functions such as cout, timer func may be delayed ?
  struct Timer;
  static void timerBegin( char* const name );  
  static void timerEnd( char* const name );
  static void timersInfo( std::ostream& ); 
  
private:
 
  static std::map<const std::string, Timer> timers;  
};



#ifdef _WIN32

struct DBGutils::Timer {
    
    Timer() {
      QueryPerformanceFrequency(&freq);
      max = 0;
      min = 100000;
      count = 0;
      sum = 0;
    }
    
    void start() {

      QueryPerformanceCounter(&t1);
    }
    
    void stop() {

      QueryPerformanceCounter(&t2);
      
      static double diff;
      diff = ((double)t2.QuadPart - (double)t1.QuadPart)/((double)freq.QuadPart);
	  
	    if( diff < min ) min = diff;
      if( diff > max ) max = diff;
      sum += diff;
      
      count++;
    }
    
    // timer infos
    double min;
    double max;
    double sum;
    unsigned int count;

    // gettime variables
    static LARGE_INTEGER freq;
    LARGE_INTEGER t1, t2;
};

#else

struct DBGutils::Timer {
     
    Timer() {
      max = 0;
      min = 100000;
      count = 0;
      sum = 0;
    }
      
    void start() {
      gettimeofday(&t1, &tz);
    }
    
    void stop() {
      gettimeofday(&t2,&tz);
      
      static double diff;
      diff = (double)t2.tv_sec + (double)t2.tv_usec/(1000*1000) 
            -( (double)t1.tv_sec + (double)t1.tv_usec/(1000*1000) );

      if( diff < min ) min = diff;
      if( diff > max ) max = diff;
      sum += diff;
      
      count++;
    }
    
    // timer infos
    double min;
    double max;
    double sum;
    unsigned int count;

    // gettime variables
    struct timeval t1, t2;
    static struct timezone tz;
};
    


#endif

 
#ifdef DEBUG

  // macro for command executed in debug mode only
  #define DEBUG_ONLY(f)      f;      
#else

  #define DEBUG_ONLY(f)           
#endif  // DEBUG

#endif  // _DBGUTILS_H
