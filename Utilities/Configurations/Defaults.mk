# --------------------------------------------------------------------------------------------------------------------------------------------------------------------#

# --------------------------------------------------------------------------------------------------------------------------------------------------------------------#
#				         				IF YOU WANT TO EDIT VARIABLES IN THIS FILE DO THIS BY ENV VARIABLES OR THE CUSTOM FILE				     					  #
#---------------------------------------------------------------------------------------------------------------------------------------------------------------------#
# ------------ Definitions ------------ # # --------------------------- Defualt Values --------------------------- # # ------------------ Comments -------------------#
# ------------------------------------------------------------------------- General ----------------------------------------------------------------------------------#
BUILD_NAME	  								?=		  SetupDocumentation#		      # Build Name											  	 		  			  #
BUILD_SYSTEM_VERSION						?=		  0.0.1#						  # Build System Version (Major.Minor.Patch)					  				  #
SOURCE_DIRECTORY							?= 		  ${shell pwd}/Source#		  	  # Build Directory												  				  #
BUILD_DIRECTORY								?= 		  ${shell pwd}/Binaries#		  # Source Directory												  		  	  #
BUILD_OBJECT_DIRECTORY						?= 		  ${shell pwd}/Intermediate#	  # Build Object Directory												  		  #
THIRD_PARTY_DIRECTORY						?= 		  ${shell pwd}/ThirdParty#		  # Third Party Directory												  		  #
# ------------------------------------------------------------------- Build Configuration ----------------------------------------------------------------------------#
BUILD_CONFIGURATION 						?=		  Development#  				  # Build configuration (Production, Staging, Development, Testing)				  #
BUILD_TOOLCHAIN								?=		  g++# 	  					      # Build toolchain								  				  				  #
BUILD_FLAGS									?=		  -std=c++17 -Wall -g#			  # Build flags											  						  #
BUILD_OPTIMSATIONS							?=		  -O3#							  # Build optimsations											  				  #
# --------------------------------------------------------------------------------------------------------------------------------------------------------------------#

#This adds the Build Configuration as macro for the compiler
BUILD_FLAGS += -DBUILD_CONFIGURATION=\"${BUILD_CONFIGURATION}\"

#This adds the source public directory to the include paths
PUBLIC_DIRECTORY = ${SOURCE_DIRECTORY}/Public
BUILD_FLAGS += -I ${PUBLIC_DIRECTORY}
BUILD_FLAGS += -I ${PUBLIC_DIRECTORY}/Core
BUILD_FLAGS += -I ${PUBLIC_DIRECTORY}/Menus
BUILD_FLAGS += -I ${PUBLIC_DIRECTORY}/Helpers
BUILD_FLAGS += -I ${PUBLIC_DIRECTORY}/Utils

#This adds the ThirdParty include directory to the include paths
UNAME_S := $(shell uname -s)

# Platform-specific configuration
ifeq ($(UNAME_S), Darwin)
    THIRD_PARTY_DIRECTORY := $(THIRD_PARTY_DIRECTORY)/Darwin
    # Add macOS-specific frameworks
    PLATFORM_FRAMEWORKS += -framework Security
    PLATFORM_FRAMEWORKS += -framework Foundation
else ifeq ($(UNAME_S), Linux)
    THIRD_PARTY_DIRECTORY := $(THIRD_PARTY_DIRECTORY)/Linux
    # Add Linux-specific libraries if needed
    PLATFORM_LIBRARIES +=
else
    # Windows or other platforms
    $(warning Unsupported platform: $(UNAME_S))
endif

THIRD_PARTY_INCLUDE_DIRECTORY = ${SOURCE_DIRECTORY}/ThirdParty
BUILD_FLAGS += -I ${THIRD_PARTY_INCLUDE_DIRECTORY}
BUILD_FLAGS += -I ${THIRD_PARTY_INCLUDE_DIRECTORY}/ftxui
BUILD_FLAGS += -I ${THIRD_PARTY_INCLUDE_DIRECTORY}/restclient-cpp
BUILD_FLAGS += -I ${THIRD_PARTY_INCLUDE_DIRECTORY}/json

#This adds the third party libraries to the link paths
FTXUI_LIB_DIR := ${THIRD_PARTY_DIRECTORY}/ftxui
THIRD_PARTY_LIBRARIES += ${FTXUI_LIB_DIR}/libftxui-component.a
THIRD_PARTY_LIBRARIES += ${FTXUI_LIB_DIR}/libftxui-dom.a
THIRD_PARTY_LIBRARIES += ${FTXUI_LIB_DIR}/libftxui-screen.a
THIRD_PARTY_LIBRARIES += -lpthread

# Add platform-specific libraries/frameworks
THIRD_PARTY_LIBRARIES += $(PLATFORM_FRAMEWORKS)
THIRD_PARTY_LIBRARIES += $(PLATFORM_LIBRARIES)