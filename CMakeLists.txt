 #
 # Copyright @ 2019 Audi AG. All rights reserved.
 # 
 #     This Source Code Form is subject to the terms of the Mozilla
 #     Public License, v. 2.0. If a copy of the MPL was not distributed
 #     with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 # 
 # If it is not possible or desirable to put the notice in a particular file, then
 # You may include the notice in a location (such as a LICENSE file in a
 # relevant directory) where a recipient would be likely to look for such a notice.
 # 
 # You may add additional accurate notices of copyright ownership.
 #
cmake_minimum_required(VERSION 3.5 FATAL_ERROR)
cmake_policy(SET CMP0011 NEW)

# Disable extensions here and require the chosen CMAKE_CXX_STANDARD (coming from Conan)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_CXX_STANDARD)
    message(WARNING "CMAKE_CXX_STANDARD not found, setting it to 14")
    set(CMAKE_CXX_STANDARD "14")
elseif(CMAKE_CXX_STANDARD LESS 14)
    message(FATAL_ERROR "CMAKE_CXX_STANDARD >= 14 required")
endif()

option(fep_base_utilities_cmake_enable_tests "Enable tests - requires googletest (default: ON)" ON) 

project(fep_base_utilities-library VERSION 0.1.0)

set(FEP_BASE_UTILITIES_VERSION_MAJOR ${PROJECT_VERSION_MAJOR})
set(FEP_BASE_UTILITIES_VERSION_MINOR ${PROJECT_VERSION_MINOR})
set(FEP_BASE_UTILITIES_VERSION_PATCH ${PROJECT_VERSION_PATCH})
set(FEP_BASE_UTILITIES_LIB_BUILD ON)

# Enable strict compiler warnings
if(WIN32)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W4 /wd4100 /wd4251 /wd4505 /wd4290")
else()
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wno-unknown-pragmas -Wno-reorder")
endif()

if(CONAN_COMPILER)
    message(STATUS "Including Conan build info")
    if ( EXISTS ${CMAKE_CURRENT_BINARY_DIR}/conanbuildinfo.cmake)
        include(${CMAKE_CURRENT_BINARY_DIR}/conanbuildinfo.cmake)
    elseif ( EXISTS ${CMAKE_CURRENT_BINARY_DIR}/../conanbuildinfo.cmake)
        include(${CMAKE_CURRENT_BINARY_DIR}/../conanbuildinfo.cmake)
    elseif ( EXISTS ${CMAKE_CURRENT_BINARY_DIR}/conanbuildinfo_multi.cmake)
        include(${CMAKE_CURRENT_BINARY_DIR}/conanbuildinfo_multi.cmake)
    elseif ( EXISTS ${CMAKE_CURRENT_BINARY_DIR}/../conanbuildinfo_multi.cmake)
        include(${CMAKE_CURRENT_BINARY_DIR}/../conanbuildinfo_multi.cmake)
    else()
        message(FATAL_ERROR "Conan build info can't be found.")
    endif()

    if(CORTEX_WORKSPACE)
        conan_basic_setup(TARGETS)
    else()
        conan_basic_setup(TARGETS NO_OUTPUT_DIRS)
    endif()
    set(gtest_search_mode CONFIG)
endif()

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

add_subdirectory(src)

if (fep_base_utilities_cmake_enable_tests)
    enable_testing()
    add_subdirectory(test)
endif()

configure_file(fep_base_utilities-config.cmake.in ${CMAKE_BINARY_DIR}/fep_base_utilities-config.cmake @ONLY)
install(FILES ${CMAKE_BINARY_DIR}/fep_base_utilities-config.cmake DESTINATION .)
# Create version config
include(CMakePackageConfigHelpers)
write_basic_package_version_file(${CMAKE_CURRENT_BINARY_DIR}/fep_base_utilities-config-version.cmake
                                 COMPATIBILITY AnyNewerVersion)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/fep_base_utilities-config-version.cmake DESTINATION .)

# install content from include directory
install(
    DIRECTORY
        include
    DESTINATION
        ./
    FILES_MATCHING PATTERN
        "*.h"
)

install(FILES doc/extern/changelog.md
        DESTINATION doc)