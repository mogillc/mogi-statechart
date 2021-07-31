// Copyright 2021 Mogi LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

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
