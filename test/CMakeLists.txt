 #######################################################################
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
 #######################################################################
if(NOT FEP_BASE_UTILITIES_LIB_BUILD)
    cmake_minimum_required(VERSION 3.5 FATAL_ERROR)

    # Disable extensions here and require the chosen CMAKE_CXX_STANDARD (coming from e.g. Conan)
    set(CMAKE_CXX_EXTENSIONS OFF)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)

    if(NOT CMAKE_CXX_STANDARD)
        message(WARNING "CMAKE_CXX_STANDARD not found, setting it to 14")
        set(CMAKE_CXX_STANDARD "14")
    elseif(CMAKE_CXX_STANDARD LESS 14)
        message(FATAL_ERROR "CMAKE_CXX_STANDARD >= 14 required")
    endif()

    cmake_policy(SET CMP0002 NEW)
    cmake_policy(SET CMP0003 NEW)
    cmake_policy(SET CMP0011 NEW)

    project(fep-base-utililities-tests)

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

    set(FEP_CONTROLLER_LIB_DIR ${CONAN_FEP_SDK_CONTROLLER_ROOT})

    # Enable project folder structure for Visual Studio IDE
    set_property(GLOBAL PROPERTY USE_FOLDERS true)

    #include(../scripts/cmake/enable_multicore_compilation.cmake)
    #include(../scripts/cmake/use_integrated_debug_symbols.cmake)

    include_directories(${CMAKE_CURRENT_BINARY_DIR}/include)
    set (FEP_POST_BUILD_COPY true)
    
    find_package(fep3_controller REQUIRED)
    find_package(fep3_controller REQUIRED)
    find_package(fep3_system REQUIRED)
endif()
find_package(fep3_participant_core REQUIRED)
find_package(fep3_participant_cpp REQUIRED)

# for test_setup.h
include_directories(${CMAKE_CURRENT_LIST_DIR})

# tests have access to internal header
include_directories(../src)

enable_testing()
add_subdirectory(function)