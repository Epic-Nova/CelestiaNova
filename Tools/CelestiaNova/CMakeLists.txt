cmake_minimum_required(VERSION 3.15)
project(CelestiaNova)

add_executable(CelestiaNova ${CMAKE_SOURCE_DIR}/Source/Private/CelestiaNova.cpp)

target_include_directories(CelestiaNova PRIVATE
  ${CMAKE_SOURCE_DIR}/Source/Public
  ${CMAKE_SOURCE_DIR}/Source/Public/Core
  ${CMAKE_SOURCE_DIR}/Source/Public/Helpers
  ${CMAKE_SOURCE_DIR}/Source/Public/Menus
  ${CMAKE_SOURCE_DIR}/Source/ThirdParty)

# Provide a BUILD_CONFIGURATION macro for compatibility with the Makefile-based build
if(NOT DEFINED BUILD_CONFIGURATION)
  if(DEFINED CMAKE_BUILD_TYPE)
    set(BUILD_CONFIGURATION ${CMAKE_BUILD_TYPE})
  else()
    set(BUILD_CONFIGURATION "Development")
  endif()
endif()
target_compile_definitions(CelestiaNova PRIVATE BUILD_CONFIGURATION=\"${BUILD_CONFIGURATION}\")

if (UNIX)
  find_library(DL_LIB dl)
  if (DL_LIB)
    target_link_libraries(CelestiaNova PRIVATE ${DL_LIB})
  endif()
endif()

# Link FTXUI static libraries from source
target_link_libraries(CelestiaNova PRIVATE screen dom component)

# Link the shared core library if present
if(TARGET NovaCore)
  target_link_libraries(CelestiaNova PRIVATE NovaCore)
endif()

# Link Cpptrace for crash reporting support
target_link_libraries(CelestiaNova PRIVATE cpptrace-lib)
if(WIN32)
  target_link_libraries(CelestiaNova PRIVATE DbgHelp)
endif()

if (UNIX)
  # Export symbols so plugins can resolve against the host binary
  target_link_options(CelestiaNova PRIVATE "-Wl,--export-dynamic")
endif()

set_target_properties(CelestiaNova PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})

if (WIN32)
  set_target_properties(CelestiaNova PROPERTIES OUTPUT_NAME "CelestiaNova-${BUILD_CONFIGURATION}")
endif()

# Ensure multi-config generators emit the executable into the base Binaries
# folder rather than creating a per-config subfolder like 'Development/'.
if(CMAKE_CONFIGURATION_TYPES)
  foreach(_cfg IN LISTS CMAKE_CONFIGURATION_TYPES)
    string(TOUPPER "${_cfg}" _cfg_up)
    set_target_properties(CelestiaNova PROPERTIES RUNTIME_OUTPUT_DIRECTORY_${_cfg_up} ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})
  endforeach()
endif()

# Expose project source dir to the runtime so the application can locate
# repository-root resources (Content/, Extensions/, etc.) when launched
# from a Binaries folder. CelestiaNova will reference the real Extensions
# folder under PROJECT_SOURCE_DIR rather than copying it into the executable
# directory.
target_compile_definitions(CelestiaNova PRIVATE PROJECT_SOURCE_DIR="${CMAKE_SOURCE_DIR}")

# Note: no post-build copy of the Extensions folder is performed — the
# runtime will discover plugins via the JSON descriptors located in the
# repository `Extensions/` directories.
