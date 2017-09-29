# compile shaders with glslangValidator from VulkanSDK
##############################################################################
#input variables
# DIR_IN - path holding executables
# LIBRARIES - libraries to link with
# INCLUDES - additional include dir
##############################################################################
#output variables
#one executable target for each cpp file
##############################################################################
cmake_minimum_required(VERSION 2.8.3)

function(generate_executables DIR_IN)
# get list of shaders
file(GLOB APPS "${DIR_IN}/*.c" "${DIR_IN}/*.cpp" "${DIR_IN}/*.cxx")
# get optional arguments
set(OPTIONS_ARGS)
set(ONE_VALUE_ARGS)
set(MULTI_VALUE_ARGS INCLUDES LIBRARIES)
cmake_parse_arguments(GEN_EXES "${OPTIONS_ARGS}" "${ONE_VALUE_ARGS}" "${MULTI_VALUE_ARGS}" ${ARGN})

foreach(_APP ${APPS})
  get_filename_component(_NAME ${_APP} NAME_WE)
  add_executable(${_NAME} ${_APP})
  target_link_libraries(${_NAME} ${GEN_EXES_LIBRARIES})
  target_include_directories(${_NAME} PUBLIC ${GEN_EXES_INCLUDES})
  install(TARGETS ${_NAME} DESTINATION .)
  message(STATUS "Generating executable target ${_NAME}")
endforeach()

endfunction(generate_executables)
