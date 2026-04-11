cmake_minimum_required(VERSION 3.15)

if(NOT DEFINED REPO_ROOT OR REPO_ROOT STREQUAL "")
  get_filename_component(_script_dir "${CMAKE_CURRENT_LIST_FILE}" DIRECTORY)
  get_filename_component(REPO_ROOT "${_script_dir}" DIRECTORY)
endif()

file(TO_CMAKE_PATH "${REPO_ROOT}" REPO_ROOT)
set(EXTENSIONS_ROOT "${REPO_ROOT}/Extensions")
set(TEMPLATE_FILE "${REPO_ROOT}/Utilities/Templates/GeneratedExtensionCMakeLists.txt.in")

if(NOT EXISTS "${EXTENSIONS_ROOT}")
  message(FATAL_ERROR "Extension CMake generation failed: Extensions root not found at '${EXTENSIONS_ROOT}'.")
endif()

if(NOT EXISTS "${TEMPLATE_FILE}")
  message(FATAL_ERROR "Extension CMake generation failed: template file not found at '${TEMPLATE_FILE}'.")
endif()

file(READ "${TEMPLATE_FILE}" TEMPLATE_CONTENT)

file(GLOB_RECURSE DESCRIPTOR_FILES RELATIVE "${EXTENSIONS_ROOT}" "${EXTENSIONS_ROOT}/*.json")
if(NOT DESCRIPTOR_FILES)
  message(STATUS "[ExtensionCMakeGen] No extension descriptor JSON files found under '${EXTENSIONS_ROOT}'.")
  return()
endif()

set(PLUGIN_ROOTS)
foreach(_descriptor_rel IN LISTS DESCRIPTOR_FILES)
  if(_descriptor_rel MATCHES "\\.schema\\.json$" OR 
     _descriptor_rel MATCHES "_MenuDefinitions\\.json$" OR 
     _descriptor_rel MATCHES "(^|/)MenuDefinitions(/|$)" OR 
     _descriptor_rel MATCHES "(^|/)Intermediate(/|$)" OR 
     _descriptor_rel MATCHES "(^|/)Binaries(/|$)" OR 
     _descriptor_rel MATCHES "(^|/)Install(/|$)")
    continue()
  endif()
  
  set(_descriptor_abs "${EXTENSIONS_ROOT}/${_descriptor_rel}")
  get_filename_component(_plugin_root "${_descriptor_abs}" DIRECTORY)
  
  # Skip the Extensions root itself (e.g. if a .json sits right in Extensions/)
  if("${_plugin_root}" STREQUAL "${EXTENSIONS_ROOT}")
    continue()
  endif()
  
  list(APPEND PLUGIN_ROOTS "${_plugin_root}")
endforeach()
list(REMOVE_DUPLICATES PLUGIN_ROOTS)

list(LENGTH DESCRIPTOR_FILES DESCRIPTOR_COUNT)
list(LENGTH PLUGIN_ROOTS PLUGIN_ROOT_COUNT)

set(GENERATED_COUNT 0)
set(UNCHANGED_COUNT 0)

foreach(_plugin_root IN LISTS PLUGIN_ROOTS)
  set(_source_dir "${_plugin_root}/Source")
  set(_output_file "${_source_dir}/CMakeLists.txt")

  file(MAKE_DIRECTORY "${_source_dir}")

  set(_needs_write ON)
  if(EXISTS "${_output_file}")
    file(READ "${_output_file}" _existing_content)
    if(_existing_content STREQUAL TEMPLATE_CONTENT)
      set(_needs_write OFF)
    endif()
  endif()

  if(_needs_write)
    file(WRITE "${_output_file}" "${TEMPLATE_CONTENT}")
    math(EXPR GENERATED_COUNT "${GENERATED_COUNT} + 1")
    message(STATUS "[ExtensionCMakeGen] Wrote '${_output_file}'")
  else()
    math(EXPR UNCHANGED_COUNT "${UNCHANGED_COUNT} + 1")
  endif()
endforeach()

message(STATUS "[ExtensionCMakeGen] Complete. descriptor_count=${DESCRIPTOR_COUNT}; plugin_root_count=${PLUGIN_ROOT_COUNT}; written=${GENERATED_COUNT}; unchanged=${UNCHANGED_COUNT}")
