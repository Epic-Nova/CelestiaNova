cmake_minimum_required(VERSION 3.15)
cmake_policy(SET CMP0091 NEW)

# Modern MSVC runtime selection (enabled by CMP0091).
# Ensures that all dependencies (FTXUI, Cpptrace) pick up the correct CRT settings
# for our custom "Development" configuration before project() is called.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<OR:$<CONFIG:Debug>,$<CONFIG:Development>>:Debug>DLL")

project(CelestiaNova)
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

# Disable global Vcpkg integration to prevent include pollution from system-installed ftxui
set(CMAKE_VS_GLOBALS 
    "VcpkgEnabled=false" 
    "VcpkgAutoLink=false"
    "VcpkgConfiguration=Development"
)

# --- Build Configurations and Output Naming ---
if(NOT DEFINED BUILD_CONFIGURATION)
  set(BUILD_CONFIGURATION "Development" CACHE STRING "Main build configuration (Development, Testing, Production)")
endif()

# Map the custom "Development" configuration to "Debug" for all imported targets
# (like FTXUI and Cpptrace) so they use the correct debug libraries/CRT.
set(CMAKE_MAP_IMPORTED_CONFIG_DEVELOPMENT Debug)
set(CMAKE_MAP_IMPORTED_CONFIG_DEVELOPMENT Debug CACHE INTERNAL "Allow dependencies to map Development to Debug")

# Global build speed improvements for MSVC
if(MSVC)
  add_compile_options(/MP /Gm-) # Multi-processor compilation and disable minimal rebuild
  add_compile_options(/Zi)      # Generate complete debugging information
  add_compile_options(/utf-8)   # Explicitly set UTF-8 encoding
endif()

set(CMAKE_CXX_STANDARD 17)

# For MSVC avoid min/max macro collisions and CRT secure warnings
if(MSVC)
	add_compile_definitions(NOMINMAX _CRT_SECURE_NO_WARNINGS)
endif()

# Allow configuring where built artifacts go (set by compile script)
# Default to repo-root `Binaries/` so final outputs live at
# <repo>/Binaries rather than under Intermediate/.
if (NOT DEFINED BUILD_OUTPUT_DIR)
		set(BUILD_OUTPUT_DIR "${CMAKE_SOURCE_DIR}/Binaries")
endif()

# Normalize BUILD_OUTPUT_DIR to an absolute path when a relative path is provided.
if(NOT IS_ABSOLUTE "${BUILD_OUTPUT_DIR}")
	set(BUILD_OUTPUT_DIR "${CMAKE_SOURCE_DIR}/${BUILD_OUTPUT_DIR}")
endif()

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${BUILD_OUTPUT_DIR}")
# Use 'Libraries' as the folder name for import/static libs
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${BUILD_OUTPUT_DIR}/Libraries")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${BUILD_OUTPUT_DIR}/Libraries")

message(STATUS "BUILD_OUTPUT_DIR=${BUILD_OUTPUT_DIR}")

# Optimization: Ensure incremental linking is enabled for Development/Debug to speed up iteration
if(MSVC)
    set(CMAKE_EXE_LINKER_FLAGS_DEVELOPMENT "${CMAKE_EXE_LINKER_FLAGS_DEVELOPMENT} /INCREMENTAL")
    set(CMAKE_SHARED_LINKER_FLAGS_DEVELOPMENT "${CMAKE_SHARED_LINKER_FLAGS_DEVELOPMENT} /INCREMENTAL")
    set(CMAKE_EXE_LINKER_FLAGS_DEBUG "${CMAKE_EXE_LINKER_FLAGS_DEBUG} /INCREMENTAL")
    set(CMAKE_SHARED_LINKER_FLAGS_DEBUG "${CMAKE_SHARED_LINKER_FLAGS_DEBUG} /INCREMENTAL")
endif()

# Determine a per-platform intermediate folder under the repository's Intermediate/
# This keeps each OS/generator/config in its own subfolder so switching environments
# doesn't mix CMake caches or intermediate outputs.
if(NOT DEFINED INTERMEDIATE_PLATFORM_DIR)
	if(WIN32)
		set(_pf "Windows")
	elseif(APPLE)
		set(_pf "Darwin")
	elseif(UNIX)
		set(_pf "Linux")
	else()
		set(_pf "${CMAKE_SYSTEM_NAME}")
	endif()

	if(DEFINED CMAKE_GENERATOR)
		string(REPLACE " " "_" _gen "${CMAKE_GENERATOR}")
	else()
		set(_gen "native")
	endif()

	# Build type may be empty for multi-config generators; normalize to 'Development' fallback
	if(NOT DEFINED CMAKE_BUILD_TYPE OR CMAKE_BUILD_TYPE STREQUAL "")
		set(_build_type "Development")
	else()
		set(_build_type "${CMAKE_BUILD_TYPE}")
	endif()

	string(TOLOWER "${_pf}-${_gen}-${_build_type}" INTERMEDIATE_PLATFORM_DIR_NAME)
	set(INTERMEDIATE_PLATFORM_DIR "${CMAKE_SOURCE_DIR}/Intermediate/${INTERMEDIATE_PLATFORM_DIR_NAME}")
endif()

file(MAKE_DIRECTORY "${INTERMEDIATE_PLATFORM_DIR}")
message(STATUS "INTERMEDIATE_PLATFORM_DIR=${INTERMEDIATE_PLATFORM_DIR}")

# For multi-config generators on Windows, avoid the per-configuration
# subfolders (e.g., 'Development/') by forcing the per-config output
# directories to the same base paths. This keeps Windows outputs at
# ${BUILD_OUTPUT_DIR} and ${BUILD_OUTPUT_DIR}/Libraries instead of
# ${BUILD_OUTPUT_DIR}/<Config>.
if(WIN32 OR MSVC)
	if(CMAKE_CONFIGURATION_TYPES)
		foreach(_cfg IN LISTS CMAKE_CONFIGURATION_TYPES)
			string(TOUPPER "${_cfg}" _cfg_up)
			set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_${_cfg_up} "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
			set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_${_cfg_up} "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}")
			set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_${_cfg_up} "${CMAKE_ARCHIVE_OUTPUT_DIRECTORY}")
		endforeach()
	endif()
endif()

# Remove stale, empty per-configuration library folders left behind by older
# builds (e.g., Binaries/Libraries/Development). Only remove if the folder
# exists and is empty to avoid deleting user files.
if(CMAKE_CONFIGURATION_TYPES AND EXISTS "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}")
	foreach(_cfg IN LISTS CMAKE_CONFIGURATION_TYPES)
		# check both the literal config and a lowercase variant
		set(_paths_to_check
			"${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/${_cfg}"
			"${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/${_cfg}" # duplicate keeps intent clear
		)
		foreach(_p IN LISTS _paths_to_check)
			if(EXISTS "${_p}")
				file(GLOB _entries RELATIVE "${_p}" "${_p}/*")
				if(NOT _entries)
					file(REMOVE_RECURSE "${_p}")
					message(STATUS "Removed empty old library config dir: ${_p}")
				endif()
			endif()
		endforeach()
	endforeach()
endif()

include(FetchContent)
# Configure FetchContent to use an intermediate folder to avoid "in-source build" errors 
# in dependencies when the primary solution is generated at the repository root.
if(NOT DEFINED FETCHCONTENT_BASE_DIR)
  set(FETCHCONTENT_BASE_DIR "${CMAKE_SOURCE_DIR}/Intermediate/_deps")
endif()

FetchContent_Declare(
  cpptrace
  URL https://github.com/jeremy-rifkin/cpptrace/archive/refs/tags/v1.0.4.tar.gz
)

FetchContent_Declare(
  ftxui
  URL https://github.com/ArthurSonzogni/FTXUI/archive/refs/heads/main.tar.gz
)

# Populate cpptrace and patch its in-source build guard to allow 
# placing the Visual Studio solution at the repository root.
FetchContent_GetProperties(cpptrace)
if(NOT cpptrace_POPULATED)
  FetchContent_Populate(cpptrace)
  file(WRITE "${cpptrace_SOURCE_DIR}/cmake/PreventInSourceBuilds.cmake" "# Guard disabled by Celestia Nova for root-level solution support\n")
  add_subdirectory("${cpptrace_SOURCE_DIR}" "${cpptrace_BINARY_DIR}")
endif()

FetchContent_MakeAvailable(ftxui)

# Map ThirdParty dependencies to solution folders
set_target_properties(cpptrace-lib PROPERTIES FOLDER "ThirdParty/cpptrace")
set_target_properties(screen PROPERTIES FOLDER "ThirdParty/ftxui")
set_target_properties(dom PROPERTIES FOLDER "ThirdParty/ftxui")
set_target_properties(component PROPERTIES FOLDER "ThirdParty/ftxui")

# Collect public include directories and add them globally so headers
# like "BaseHelper.h" and "MainMenu.h" are discoverable by all targets.
set(_public_inc_dirs
	${CMAKE_SOURCE_DIR}/Source/Public
	${CMAKE_SOURCE_DIR}/Source/Public/Core
	${CMAKE_SOURCE_DIR}/Source/ThirdParty
)
file(GLOB _pub_entries RELATIVE ${CMAKE_SOURCE_DIR} ${CMAKE_SOURCE_DIR}/Source/Public/*)
foreach(_e IN LISTS _pub_entries)
	if(IS_DIRECTORY "${CMAKE_SOURCE_DIR}/${_e}")
		list(APPEND _public_inc_dirs "${CMAKE_SOURCE_DIR}/${_e}")
	endif()
endforeach()

include_directories(${_public_inc_dirs})


# If using a multi-config generator (Visual Studio/Xcode), ensure a 'Development'
# configuration exists so tools and CI that expect 'Development' can build it.
if(MSVC OR CMAKE_GENERATOR MATCHES "Visual Studio" OR CMAKE_GENERATOR MATCHES "Xcode")
	set(_cfgs "Development;Debug;Release;RelWithDebInfo;MinSizeRel")
	if(NOT CMAKE_CONFIGURATION_TYPES STREQUAL _cfgs)
		set(CMAKE_CONFIGURATION_TYPES "${_cfgs}" CACHE STRING "Available configurations" FORCE)
	endif()
	message(STATUS "Configured multi-config generator with CMAKE_CONFIGURATION_TYPES=${CMAKE_CONFIGURATION_TYPES}")
endif()

# For multi-config generators map our 'Development' configuration to the
# Debug flags to avoid runtime mismatches with third-party debug libraries
# (e.g., ftxui built with MDd). This makes 'Development' behave like Debug.
if(CMAKE_CONFIGURATION_TYPES)
	if(NOT DEFINED CMAKE_C_FLAGS_DEVELOPMENT)
		set(CMAKE_C_FLAGS_DEVELOPMENT "${CMAKE_C_FLAGS_DEBUG}")
	endif()
	if(NOT DEFINED CMAKE_CXX_FLAGS_DEVELOPMENT)
		set(CMAKE_CXX_FLAGS_DEVELOPMENT "${CMAKE_CXX_FLAGS_DEBUG}")
	endif()
	if(NOT DEFINED CMAKE_EXE_LINKER_FLAGS_DEVELOPMENT)
		set(CMAKE_EXE_LINKER_FLAGS_DEVELOPMENT "${CMAKE_EXE_LINKER_FLAGS_DEBUG}")
	endif()
	if(NOT DEFINED CMAKE_SHARED_LINKER_FLAGS_DEVELOPMENT)
		set(CMAKE_SHARED_LINKER_FLAGS_DEVELOPMENT "${CMAKE_SHARED_LINKER_FLAGS_DEBUG}")
	endif()
	if(NOT DEFINED CMAKE_STATIC_LINKER_FLAGS_DEVELOPMENT)
		set(CMAKE_STATIC_LINKER_FLAGS_DEVELOPMENT "${CMAKE_STATIC_LINKER_FLAGS_DEBUG}")
	endif()
endif()

# Provide a BUILD_CONFIGURATION macro (used by source code to detect build mode)
add_compile_definitions(BUILD_CONFIGURATION="${BUILD_CONFIGURATION}")

if(MSVC)
    if(BUILD_CONFIGURATION STREQUAL "Production")
        # Global high-performance Shipping optimizations
        add_compile_options(/O2 /GL /Gy)
        add_link_options(/LTCG /OPT:REF /OPT:ICF)
    else()
        # Development optimizations: Fast debug symbols and incremental linking
        add_compile_options(/Od /Zi)
        add_link_options(/DEBUG /INCREMENTAL)
    endif()
endif()

# FTXUI is now built from source via FetchContent, so no manual linking function is needed.

# Create a shared core library containing the project's private sources
# This provides an import library on Windows so plugins can link against
# the core symbols (resolving IModuleInterface, NovaLog, etc.).
file(GLOB_RECURSE CORE_PRIVATE_SOURCES RELATIVE ${CMAKE_SOURCE_DIR} ${CMAKE_SOURCE_DIR}/Source/Private/*.cpp)
# Exclude the main entry (CelestiaNova.cpp) from the core library; the
# executable will be a small launcher that links to the shared core.
list(REMOVE_ITEM CORE_PRIVATE_SOURCES Source/Private/CelestiaNova.cpp)
if(CORE_PRIVATE_SOURCES)
	# Build a filtered list of sources — include platform-specific files only
	# when they match the current platform (avoid compiling Windows-only
	# impls on Linux, and vice-versa).
	set(_core_sources)
	foreach(_s IN LISTS CORE_PRIVATE_SOURCES)
		string(REGEX MATCH "_Windows\\.cpp$" _is_win "${_s}")
		string(REGEX MATCH "_Linux\\.cpp$" _is_linux "${_s}")
		string(REGEX MATCH "_Darwin\\.cpp$" _is_darwin "${_s}")

		if(_is_win)
			if(WIN32)
				list(APPEND _core_sources "${CMAKE_SOURCE_DIR}/${_s}")
			endif()
		elseif(_is_linux)
			if(UNIX AND NOT APPLE)
				list(APPEND _core_sources "${CMAKE_SOURCE_DIR}/${_s}")
			endif()
		elseif(_is_darwin)
			if(APPLE)
				list(APPEND _core_sources "${CMAKE_SOURCE_DIR}/${_s}")
			endif()
		else()
			list(APPEND _core_sources "${CMAKE_SOURCE_DIR}/${_s}")
		endif()
	endforeach()

	if(_core_sources)
		add_library(NovaCore SHARED ${_core_sources})

		# Use the public include dirs we computed earlier for this target too.
		target_include_directories(NovaCore PUBLIC ${_public_inc_dirs})
		target_compile_definitions(NovaCore PRIVATE NOVA_CORE_EXPORTS)
		# Ensure consistent output locations
		set_target_properties(NovaCore PROPERTIES
			RUNTIME_OUTPUT_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
			LIBRARY_OUTPUT_DIRECTORY ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}
			ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_ARCHIVE_OUTPUT_DIRECTORY}
		)
		# Link FTXUI targets PUBLICLY so include paths propagate to anyone using NovaCore (like Extensions)
		target_link_libraries(NovaCore PUBLIC ftxui::screen ftxui::dom ftxui::component)
		
		# Link Cpptrace for shared stack trace support
		target_link_libraries(NovaCore PRIVATE cpptrace::cpptrace)

		# On Windows, avoid placing outputs under a per-config directory; instead
		# append the build configuration to the filename, e.g. NovaCore-Development.dll
		set_target_properties(NovaCore PROPERTIES 
			OUTPUT_NAME "NovaCore-${BUILD_CONFIGURATION}"
			FOLDER "CelestiaNova"
		)

		if(MSVC AND BUILD_CONFIGURATION STREQUAL "Production")
			target_compile_options(NovaCore PRIVATE /O2 /GL)
			target_link_options(NovaCore PRIVATE /LTCG)
		endif()

		# Also set per-configuration output directories on the NovaCore target so
		# Visual Studio does not create a separate '<Config>' subfolder.
		if(CMAKE_CONFIGURATION_TYPES)
			foreach(_cfg IN LISTS CMAKE_CONFIGURATION_TYPES)
				string(TOUPPER "${_cfg}" _cfg_up)
				set_target_properties(NovaCore PROPERTIES
					RUNTIME_OUTPUT_DIRECTORY_${_cfg_up} ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
					LIBRARY_OUTPUT_DIRECTORY_${_cfg_up} ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}
					ARCHIVE_OUTPUT_DIRECTORY_${_cfg_up} ${CMAKE_ARCHIVE_OUTPUT_DIRECTORY}
				)
			endforeach()
		endif()
	endif()
endif()

# Build tools
add_subdirectory(Tools/PluginTester)
add_subdirectory(Tools/CelestiaNova)

# Map tools to engine folder
set_target_properties(PluginTester PROPERTIES FOLDER "CelestiaNova")
set_target_properties(CelestiaNova PROPERTIES FOLDER "CelestiaNova")

# Default install prefix if not specified by the user
if(NOT DEFINED CMAKE_INSTALL_PREFIX)
	set(CMAKE_INSTALL_PREFIX "${CMAKE_BINARY_DIR}/../Install" CACHE PATH "Installation prefix")
endif()

# Install host executables and libraries
install(TARGETS PluginTester CelestiaNova
	RUNTIME DESTINATION bin
	LIBRARY DESTINATION lib
	ARCHIVE DESTINATION lib
)

# The host resolves extension libraries relative to the runtime working
# directory (`Binaries/`).  Keep NovaCore and every built plugin in that
# portable runtime location instead of relying on a development build tree.
if(TARGET NovaCore)
	install(TARGETS NovaCore
		RUNTIME DESTINATION Binaries
		LIBRARY DESTINATION Binaries
		ARCHIVE DESTINATION lib
	)
endif()

install(DIRECTORY "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/" DESTINATION Binaries
	FILES_MATCHING
		PATTERN "*.so"
		PATTERN "*.dylib"
		PATTERN "*.dll"
)

# Discover extension plugin folders recursively. A plugin folder is any
# Extensions/<...> directory that contains Source/CMakeLists.txt.
set(PLUGIN_FOLDERS)
file(GLOB_RECURSE _extension_cmake_files
	RELATIVE ${CMAKE_SOURCE_DIR}/Extensions
	${CMAKE_SOURCE_DIR}/Extensions/*CMakeLists.txt
)
foreach(_plugin_cmake_rel IN LISTS _extension_cmake_files)
	if(NOT _plugin_cmake_rel MATCHES "(^|/)Source/CMakeLists\\.txt$")
		continue()
	endif()
	if(_plugin_cmake_rel MATCHES "(^|/)Intermediate(/|$)" OR _plugin_cmake_rel MATCHES "(^|/)Binaries(/|$)" OR _plugin_cmake_rel MATCHES "(^|/)Install(/|$)")
		continue()
	endif()
	get_filename_component(_plugin_source_rel_dir "${_plugin_cmake_rel}" DIRECTORY)
	get_filename_component(_plugin_folder_rel "${_plugin_source_rel_dir}" DIRECTORY)
	if(NOT _plugin_folder_rel STREQUAL "")
		list(APPEND PLUGIN_FOLDERS "${_plugin_folder_rel}")
	endif()
endforeach()
list(REMOVE_DUPLICATES PLUGIN_FOLDERS)
message(STATUS "PLUGIN_FOLDERS = ${PLUGIN_FOLDERS}")

# Install plugin binaries found under Extensions/*/Binaries into the install tree
foreach(p ${PLUGIN_FOLDERS})
	if(EXISTS "${CMAKE_SOURCE_DIR}/Extensions/${p}/Binaries")
		install(DIRECTORY "${CMAKE_SOURCE_DIR}/Extensions/${p}/Binaries/" DESTINATION "Extensions/${p}/Binaries")
	endif()
endforeach()

# Runtime discovery depends on extension descriptors and their declarative
# Content/MenuDefinitions directories.  Exclude build and source trees so a
# Linux package remains a runtime artifact rather than a source checkout.
if(EXISTS "${CMAKE_SOURCE_DIR}/Extensions")
	install(DIRECTORY "${CMAKE_SOURCE_DIR}/Extensions/" DESTINATION Extensions
		PATTERN "Source" EXCLUDE
		PATTERN "Intermediate" EXCLUDE
		PATTERN "Binaries" EXCLUDE
		PATTERN "CMakeLists.txt" EXCLUDE
	)
endif()

# Install Content/ (static runtime data) when present
if(EXISTS "${CMAKE_SOURCE_DIR}/Content")
	install(DIRECTORY "${CMAKE_SOURCE_DIR}/Content/" DESTINATION Content)
endif()

# Linux service-mode assets are installed with the application package.  The
# target host administrator explicitly enables the unit after reviewing it.
if(EXISTS "${CMAKE_SOURCE_DIR}/Utilities/linux/celestianova.service")
	install(FILES "${CMAKE_SOURCE_DIR}/Utilities/linux/celestianova.service"
		DESTINATION "share/celestianova/systemd")
endif()
if(EXISTS "${CMAKE_SOURCE_DIR}/Utilities/linux/bootstrap-docker.sh")
	install(PROGRAMS "${CMAKE_SOURCE_DIR}/Utilities/linux/bootstrap-docker.sh"
		DESTINATION "share/celestianova/bootstrap")
endif()
if(EXISTS "${CMAKE_SOURCE_DIR}/Utilities/linux/initialize_keyforge_credential.sh")
	install(PROGRAMS "${CMAKE_SOURCE_DIR}/Utilities/linux/initialize_keyforge_credential.sh"
		DESTINATION "share/celestianova/bootstrap")
endif()
if(EXISTS "${CMAKE_SOURCE_DIR}/Docs/LINUX_SERVICE_MODE.md")
	install(FILES "${CMAKE_SOURCE_DIR}/Docs/LINUX_SERVICE_MODE.md"
		DESTINATION "share/doc/celestianova")
endif()

option(NOVA_ENABLE_EXTENSION_DEPENDENCY_AUTOWIRE "Automatically resolve extension dependency order and exported include paths from extension JSON descriptors." ON)
option(NOVA_GENERATE_EXTENSION_CMAKELISTS_AT_CONFIGURE "Generate extension Source/CMakeLists.txt files from extension JSON descriptors during configure." ON)

if(NOVA_GENERATE_EXTENSION_CMAKELISTS_AT_CONFIGURE)
	set(_extension_cmake_generator_script "${CMAKE_SOURCE_DIR}/Utilities/generate_extension_cmakelists.cmake")
	if(EXISTS "${_extension_cmake_generator_script}")
		execute_process(
			COMMAND ${CMAKE_COMMAND} -DREPO_ROOT=${CMAKE_SOURCE_DIR} -P ${_extension_cmake_generator_script}
			RESULT_VARIABLE _extension_cmake_generation_result
		)
		if(NOT _extension_cmake_generation_result EQUAL 0)
			message(FATAL_ERROR "Failed to auto-generate extension CMakeLists files (result=${_extension_cmake_generation_result}).")
		endif()
	else()
		message(WARNING "Extension CMake generator script not found at ${_extension_cmake_generator_script}")
	endif()
endif()

# Discover plugin folders under Extensions/ and configure them as separate
# CMake projects so their intermediate outputs and binaries live under the
# plugin folder (Extensions/<Name>/Intermediate and Extensions/<Name>/Binaries).
function(_nova_json_get_string _json_text _key _out_var)
	set(_normalized "${_json_text}")
	string(REPLACE "\r" " " _normalized "${_normalized}")
	string(REPLACE "\n" " " _normalized "${_normalized}")
	string(REPLACE "\t" " " _normalized "${_normalized}")
	string(REGEX MATCH "\"${_key}\"[ ]*:[ ]*\"([^\"]*)\"" _match "${_normalized}")
	if(_match)
		set(${_out_var} "${CMAKE_MATCH_1}" PARENT_SCOPE)
	else()
		set(${_out_var} "" PARENT_SCOPE)
	endif()
endfunction()

function(_nova_json_get_string_array _json_text _key _out_var)
	set(_result)
	set(_normalized "${_json_text}")
	string(REPLACE "\r" " " _normalized "${_normalized}")
	string(REPLACE "\n" " " _normalized "${_normalized}")
	string(REPLACE "\t" " " _normalized "${_normalized}")
	string(REGEX MATCH "\"${_key}\"[ ]*:[ ]*\\[([^]]*)\\]" _array_match "${_normalized}")
	if(_array_match)
		set(_array_payload "${CMAKE_MATCH_1}")
		string(REGEX MATCHALL "\"([^\"]+)\"" _items "${_array_payload}")
		foreach(_item IN LISTS _items)
			string(REGEX REPLACE "^\"|\"$" "" _value "${_item}")
			if(NOT _value STREQUAL "")
				list(APPEND _result "${_value}")
			endif()
		endforeach()
	endif()
	set(${_out_var} "${_result}" PARENT_SCOPE)
endfunction()

function(_nova_find_descriptor_for_folder _folder _out_descriptor _out_plugin_id _out_dep_ids)
	set(_descriptor "")
	set(_plugin_id "")
	set(_dep_ids)

	file(GLOB _descriptor_candidates "${CMAKE_SOURCE_DIR}/Extensions/${_folder}/*.json")
	foreach(_candidate IN LISTS _descriptor_candidates)
		file(READ "${_candidate}" _candidate_json)
		string(STRIP "${_candidate_json}" _trimmed_json)
		if(_trimmed_json STREQUAL "")
			continue()
		endif()

		_nova_json_get_string("${_candidate_json}" "id" _candidate_id)
		if(NOT _candidate_id STREQUAL "")
			set(_descriptor "${_candidate}")
			set(_plugin_id "${_candidate_id}")
			_nova_json_get_string_array("${_candidate_json}" "dependencies" _dep_ids)
			break()
		endif()
	endforeach()

	if(_descriptor STREQUAL "" AND _descriptor_candidates)
		list(GET _descriptor_candidates 0 _descriptor)
		file(READ "${_descriptor}" _fallback_json)
		_nova_json_get_string("${_fallback_json}" "id" _plugin_id)
		_nova_json_get_string_array("${_fallback_json}" "dependencies" _dep_ids)
	endif()

	set(${_out_descriptor} "${_descriptor}" PARENT_SCOPE)
	set(${_out_plugin_id} "${_plugin_id}" PARENT_SCOPE)
	set(${_out_dep_ids} "${_dep_ids}" PARENT_SCOPE)
endfunction()


set(HOST_EXTENSIONS_ROOT "${CMAKE_SOURCE_DIR}/Extensions")
set(HOST_BUILD_OUTPUT_DIR "${BUILD_OUTPUT_DIR}")

foreach(p ${PLUGIN_FOLDERS})
	set(plugin_source "${CMAKE_SOURCE_DIR}/Extensions/${p}/Source")
	if(EXISTS "${plugin_source}/CMakeLists.txt")
		set(NOVA_CURRENT_PLUGIN_PATH "${p}")
		if(WIN32)
			add_subdirectory("${plugin_source}" "${CMAKE_BINARY_DIR}/Extensions/${p}")
		else()
			add_subdirectory("${plugin_source}" "${CMAKE_BINARY_DIR}/Extensions/${p}")
		endif()
	endif()
endforeach()
