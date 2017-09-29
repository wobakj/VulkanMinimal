# compile shaders with glslangValidator from VulkanSDK
##############################################################################
#input variables
# DIR_IN - relative path to shader directory 
# DIR_INSTALL - install folder (if not specified, mirrors input dir)
#env variables
# VULKAN_SDK - path to SDK containing glslangvalidator
##############################################################################
cmake_minimum_required(VERSION 2.8)

MACRO(compile_shaders DIR_IN)
# get list of shaders
file(GLOB SHADERS "${DIR_IN}/*")

string(CONCAT _DIR_OUT ${PROJECT_BINARY_DIR} "/" ${DIR_IN} "/")
# create folder for compiled shaders
file(MAKE_DIRECTORY ${_DIR_OUT})
# add shader compilation command sand target
foreach(_SHADER ${SHADERS})
  # compute output file name
  get_filename_component(_EXT ${_SHADER} EXT)
  get_filename_component(_NAME ${_SHADER} NAME_WE)
  string(SUBSTRING ${_EXT} 1 -1 _EXT)
  string(CONCAT _NAME_OUT ${_NAME}_ ${_EXT} ".spv")
  string(CONCAT _SHADER_OUT ${_DIR_OUT} ${_NAME_OUT})
  # print info
  message(STATUS "Generating compilation command for ${_NAME}.${_EXT}")
  # add to output list
  list(APPEND SHADER_OUTPUTS ${_SHADER_OUT})
  add_custom_command(
    OUTPUT ${_SHADER_OUT}
    COMMAND "$ENV{VULKAN_SDK}/bin/glslangValidator" -V ${_SHADER} -o ${_SHADER_OUT}
    DEPENDS ${_SHADER}
    COMMENT "Compiling \"${_NAME}.${_EXT}\" to \"${_NAME_OUT}\"" 
  )
endforeach()
# add target depending on shaders to compile when building
add_custom_target(shaders ALL DEPENDS ${SHADER_OUTPUTS})

# Cannot use ARGN directly with list() command.
set(extra_macro_args ${ARGN})
list(GET extra_macro_args 0 _DIR_INSTALL)
# check if install override was set
if(NOT ${_DIR_INSTALL})
  string(CONCAT _DIR_INSTALL ${CMAKE_INSTALL_PREFIX} "/" ${DIR_IN})
endif(NOT ${_DIR_INSTALL})

# installation rules, copy compiled shaders
install(DIRECTORY ${_DIR_OUT}
  DESTINATION ${_DIR_INSTALL}
)

ENDMACRO(compile_shaders)
