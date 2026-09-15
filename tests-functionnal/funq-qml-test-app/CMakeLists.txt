cmake_minimum_required(VERSION 3.5...3.19)
if(${CMAKE_VERSION} VERSION_LESS 3.12)
  cmake_policy(VERSION ${CMAKE_MAJOR_VERSION}.${CMAKE_MINOR_VERSION})
endif()

project(funq-qml-test-app LANGUAGES CXX)

set(default_build_type "Release")
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  message(STATUS "Setting build type to '${default_build_type}' as none was specified.")
  set(CMAKE_BUILD_TYPE "${default_build_type}" CACHE STRING "Choose the type of build." FORCE)
  set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release" "MinSizeRel" "RelWithDebInfo")
endif()

set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT QT_MAJOR_VERSION)
  find_package(Qt NAMES Qt6 Qt5 REQUIRED COMPONENTS Core)
  set(QT_MAJOR_VERSION ${Qt_VERSION_MAJOR})
endif()
set(QT "Qt${QT_MAJOR_VERSION}")

if(MSVC)
  add_compile_options(/W4 /permissive-)
else()
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

find_package(${QT} REQUIRED COMPONENTS Core Gui Qml Quick)
message(STATUS "Building funq-qml-test-app with Qt ${${QT}_VERSION}")

set(CMAKE_AUTOMOC ON)

add_executable(funq-qml-test-app main.cpp)
target_link_libraries(funq-qml-test-app PRIVATE
  ${QT}::Core
  ${QT}::Gui
  ${QT}::Qml
  ${QT}::Quick
)
