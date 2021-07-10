 #############################################################################
 # Copyright 2021 Mogi LLC
 #
 # Licensed under the Apache License, Version 2.0 (the "License");
 # you may not use this file except in compliance with the License.
 # You may obtain a copy of the License at
 # 
 #     http://www.apache.org/licenses/LICENSE-2.0
 # 
 # Unless required by applicable law or agreed to in writing, software
 # distributed under the License is distributed on an "AS IS" BASIS,
 # WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 # See the License for the specific language governing permissions and
 # limitations under the License.
 #
 #############################################################################

cmake_minimum_required(VERSION 3.5)
project(mogi_statechart)

# Generate compile commands
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Default to C99
if(NOT CMAKE_C_STANDARD)
  set(CMAKE_C_STANDARD 99)
endif()

# Default to C++14
if(NOT CMAKE_CXX_STANDARD)
  set(CMAKE_CXX_STANDARD 14)
endif()

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

add_library(mogi_statechart SHARED
    src/chart.cpp
    src/event.cpp
    src/state.cpp
    src/transition.cpp
    )
target_link_libraries(mogi_statechart
    pthread)
target_include_directories(mogi_statechart PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>)
set_target_properties(mogi_statechart PROPERTIES
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN 1)
# Causes the visibility macros to use dllexport rather than dllimport,
# which is appropriate when building the dll but not consuming it.
target_compile_definitions(mogi_statechart PRIVATE "MOGI_STATECHART_BUILDING_LIBRARY")

install(
  DIRECTORY include/
  DESTINATION include
)
install(
  TARGETS mogi_statechart
  EXPORT export_${PROJECT_NAME}
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib
  RUNTIME DESTINATION bin
)

# Examples
add_executable(simple examples/simple.cpp)
target_include_directories(simple PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>)
target_link_libraries(simple mogi_statechart)
install(TARGETS
    simple
    DESTINATION lib/${PROJECT_NAME}
    )
add_executable(nestedChart examples/nestedChart.cpp)
target_include_directories(nestedChart PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>)
target_link_libraries(nestedChart mogi_statechart)
install(TARGETS
    nestedChart
    DESTINATION lib/${PROJECT_NAME}
    )

# Test
include(FetchContent)
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/609281088cfefc76f9d0ce82e1ff6c30cc3591e5.zip
)
# For Windows: Prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

enable_testing()

add_executable(${PROJECT_NAME}_test
    test/basic_test.cpp
    test/run_test.cpp
    test/event_test.cpp
    test/callback_test.cpp)
target_link_libraries(${PROJECT_NAME}_test
    gtest_main
    mogi_statechart)

include(GoogleTest)
gtest_discover_tests(${PROJECT_NAME}_test)
