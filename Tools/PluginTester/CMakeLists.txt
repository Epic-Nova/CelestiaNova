cmake_minimum_required(VERSION 3.15)
project(PluginTester)

# PluginTester is a thin test runner that links to the shared NovaCore library
add_executable(PluginTester ${CMAKE_SOURCE_DIR}/Tools/PluginTester/main.cpp)
target_include_directories(PluginTester PRIVATE ${CMAKE_SOURCE_DIR}/Source/Public ${CMAKE_SOURCE_DIR}/Source/Public/Core ${CMAKE_SOURCE_DIR}/Source/ThirdParty)

# Link the shared core library if present
if(TARGET NovaCore)
  target_link_libraries(PluginTester PRIVATE NovaCore)
endif()

if (UNIX)
  find_library(DL_LIB dl)
  if (DL_LIB)
    target_link_libraries(PluginTester PRIVATE ${DL_LIB})
  endif()
endif()

# Link FTXUI static/import libraries from source
target_link_libraries(PluginTester PRIVATE screen dom component)

# Export executable symbols so dynamically-loaded plugins can resolve them at runtime
if (UNIX)
  target_link_options(PluginTester PRIVATE "-Wl,--export-dynamic")
endif()

set_target_properties(PluginTester PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})

if (WIN32)
  set_target_properties(PluginTester PROPERTIES OUTPUT_NAME "PluginTester-${BUILD_CONFIGURATION}")
endif()

if(CMAKE_CONFIGURATION_TYPES)
  foreach(_cfg IN LISTS CMAKE_CONFIGURATION_TYPES)
    string(TOUPPER "${_cfg}" _cfg_up)
    set_target_properties(PluginTester PROPERTIES RUNTIME_OUTPUT_DIRECTORY_${_cfg_up} ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})
  endforeach()
endif()

# Provide PROJECT_SOURCE_DIR to allow runtime helpers (like NovaLog) to
# resolve repository-root resources when PluginTester is launched from
# `Binaries/`.
target_compile_definitions(PluginTester PRIVATE PROJECT_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
