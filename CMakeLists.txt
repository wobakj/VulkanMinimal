cmake_minimum_required (VERSION 3.0.2)
# allows access to environment variables with the name 
project (VulkanMinimal)

#set the build type if its not set
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING "Choose the type of build, options are: None Debug Release RelWithDebInfo MinSizeRel." FORCE )
endif()

#if user didnt set install dir, override it and write it to the cache -> Type and description necessary,
#overwrite variable, not just write it to cache
if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
    set(CMAKE_INSTALL_PREFIX "${PROJECT_SOURCE_DIR}/install" CACHE STRING "Install path prefix, prepended onto install directories." FORCE)
endif(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)

# installation
SET(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)

#before adding libraries, define the output paths
# MSVC & Xcode automatically create the build-type folders
if(MSVC OR CMAKE_GENERATOR STREQUAL Xcode)
  SET(EXECUTABLE_OUTPUT_PATH ${PROJECT_BINARY_DIR})
  SET(LIBRARY_OUTPUT_PATH	 ${PROJECT_BINARY_DIR})
else()
  SET(EXECUTABLE_OUTPUT_PATH ${PROJECT_BINARY_DIR}/${CMAKE_BUILD_TYPE})
  SET(LIBRARY_OUTPUT_PATH	 ${PROJECT_BINARY_DIR}/${CMAKE_BUILD_TYPE})
endif()

# Location where cmake first looks for modules.
list(APPEND CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake/modules)
set(GLOBAL_EXT_DIR ${CMAKE_SOURCE_DIR}/external)

include(AddSubLibrary)

# vulkan SDK
find_package(Vulkan REQUIRED)
include_directories(SYSTEM ${VULKAN_INCLUDE_DIR})

# include glm, as system header to suppress warnings
include_directories(SYSTEM external/glm-0.9.8.4)

# get lodepng components
set(LODEPNG_DIR "${CMAKE_CURRENT_SOURCE_DIR}/external/lodepng-20161127")
file(GLOB LODEPNG_SOURCES "${LODEPNG_DIR}/*.cpp")
set(LODEPNG_INCLUCE_DIR ${LODEPNG_DIR})
# suppress al warnings for sources
if(NOT MSVC)
  set_source_files_properties(${LODEPNG_SOURCES} PROPERTIES COMPILE_FLAGS -w)
else()
  set_source_files_properties(${LODEPNG_SOURCES} PROPERTIES COMPILE_FLAGS /w)
endif()

# glfw
# configure glfw building
option(GLFW_BUILD_DOCS     OFF)
option(GLFW_BUILD_EXAMPLES OFF)
option(GLFW_BUILD_TESTS    OFF)
option(GLFW_INSTALL        OFF)

# add glfw build system
add_subdirectory(external/glfw-3.2.1)

# create framework library 
file(GLOB_RECURSE FRAMEWORK_SOURCES framework/source/*.cpp)
add_library(framework STATIC ${FRAMEWORK_SOURCES} ${LODEPNG_SOURCES})
target_include_directories(framework PUBLIC framework/include)
target_include_directories(framework SYSTEM PUBLIC ${LODEPNG_DIR})
target_link_libraries(framework glfw ${GLFW_LIBRARIES} ${VULKAN_LIBRARY})

include(GenerateExecutables)
generate_executables("./applications" LIBRARIES framework)

# set build type dependent flags
if(UNIX)
    set(CMAKE_CXX_FLAGS_RELEASE "-O2")
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG}")
elseif(MSVC)
	set(CMAKE_CXX_FLAGS_RELEASE "/MD /O2")
	set(CMAKE_CXX_FLAGS_DEBUG "/MDd /Zi")
endif()

# set general options
if(NOT MSVC)
    add_definitions(-std=c++11)
    # show all warnings
    add_definitions(-Wall -Wconversion)
    # force clang to link with c++11 lib
    if(APPLE)
        set(CMAKE_XCODE_ATTRIBUTE_CLANG_CXX_LANGUAGE_STANDARD "c++0x")
        add_definitions(-stdlib=libc++)
    endif()
else()
    # build in parallel, show warnings
    add_definitions(/MP /W3)
endif()

# remove external configuration vars from cmake gui
mark_as_advanced(GLFW_BUILD_DOCS GLFW_INSTALL GLFW_BUILD_EXAMPLES GLFW_BUILD_TESTS
 GLFW_VULKAN_STATIC GLFW_DOCUMENT_INTERNALS GLFW_USE_EGL GLFW_USE_MIR 
 GLFW_USE_WAYLAND GLFW_LIBRARIES LIB_SUFFIX BUILD_SHARED_LIBS)

# generate custom target to compile shaders
include(CompileShaders)
compile_shaders("shaders/")
