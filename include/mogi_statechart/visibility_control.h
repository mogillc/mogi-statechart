#ifndef MOGI_STATECHART__VISIBILITY_CONTROL_H_
#define MOGI_STATECHART__VISIBILITY_CONTROL_H_

// This logic was borrowed (then namespaced) from the examples on the gcc wiki:
//     https://gcc.gnu.org/wiki/Visibility

#if defined _WIN32 || defined __CYGWIN__
  #ifdef __GNUC__
    #define MOGI_STATECHART_EXPORT __attribute__ ((dllexport))
    #define MOGI_STATECHART_IMPORT __attribute__ ((dllimport))
  #else
    #define MOGI_STATECHART_EXPORT __declspec(dllexport)
    #define MOGI_STATECHART_IMPORT __declspec(dllimport)
  #endif
  #ifdef MOGI_STATECHART_BUILDING_LIBRARY
    #define MOGI_STATECHART_PUBLIC MOGI_STATECHART_EXPORT
  #else
    #define MOGI_STATECHART_PUBLIC MOGI_STATECHART_IMPORT
  #endif
  #define MOGI_STATECHART_PUBLIC_TYPE MOGI_STATECHART_PUBLIC
  #define MOGI_STATECHART_LOCAL
#else
  #define MOGI_STATECHART_EXPORT __attribute__ ((visibility("default")))
  #define MOGI_STATECHART_IMPORT
  #if __GNUC__ >= 4
    #define MOGI_STATECHART_PUBLIC __attribute__ ((visibility("default")))
    #define MOGI_STATECHART_LOCAL  __attribute__ ((visibility("hidden")))
  #else
    #define MOGI_STATECHART_PUBLIC
    #define MOGI_STATECHART_LOCAL
  #endif
  #define MOGI_STATECHART_PUBLIC_TYPE
#endif

#endif  // MOGI_STATECHART__VISIBILITY_CONTROL_H_
