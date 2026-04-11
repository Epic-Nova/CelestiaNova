## ExternalProject template for ExamplePlugin
## This file contains a ready-made but disabled ExternalProject_Add example.
## To enable, copy or include this file from the top-level CMakeLists.txt
## and remove or change the surrounding `if(FALSE)` guard.

if(FALSE)
  include(ExternalProject)
  ExternalProject_Add(EP_ExamplePlugin
    # Remote repository to fetch the plugin from
    GIT_REPOSITORY https://github.com/owner/ExamplePlugin.git
    GIT_TAG        main

    # Where the plugin's build files will be generated (keeps them inside the repo)
    BINARY_DIR ${CMAKE_SOURCE_DIR}/Extensions/ExamplePlugin/Intermediate

    # Configure the plugin project the same way the in-tree plugin expects
    CMAKE_ARGS -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE} -DBUILD_OUTPUT_DIR=${BUILD_OUTPUT_DIR} -DHOST_PROJECT_SOURCE_DIR=${CMAKE_SOURCE_DIR}

    # Optional: update step (e.g. do a `git pull` instead of re-cloning)
    # UPDATE_COMMAND git -C ${CMAKE_SOURCE_DIR}/Extensions/ExamplePlugin/Source pull --ff-only

    # Optional: apply a local patch before configure
    # PATCH_COMMAND ${CMAKE_COMMAND} -E echo "Applying patch" && git -C ${CMAKE_SOURCE_DIR}/Extensions/ExamplePlugin/Source apply ${CMAKE_SOURCE_DIR}/patches/ExamplePlugin/fix-foo.patch

    # Optional: you can override how the download happens
    # DOWNLOAD_COMMAND git clone --depth 1 https://github.com/owner/ExamplePlugin.git ${CMAKE_SOURCE_DIR}/Extensions/ExamplePlugin/Source

    # Don't run install for ExternalProject-managed plugin (we treat runtime artifacts in-place)
    INSTALL_COMMAND ""
  )
endif()
