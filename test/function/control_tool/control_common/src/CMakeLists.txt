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

find_package(a_util REQUIRED)
find_package(fep3_system REQUIRED)
find_package(fep3_participant REQUIRED)

########################################################
# as long as FEPSDK-FEPSDK-2196 we must do that hack
include_directories(CONAN_FEP_CONTROLLER_ROOT)

find_package(GTest REQUIRED ${gtest_search_mode})
add_executable(test_control_tool basic_test.cpp)
add_test(NAME test_control_tool
    COMMAND test_control_tool
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/../"
)
fep3_participant_deploy(test_control_tool)
fep3_participant_install(test_control_tool bin)
fep3_system_deploy(test_control_tool)
fep3_system_install(test_control_tool bin)
set_target_properties(test_control_tool PROPERTIES FOLDER tests)
set_target_properties(test_control_tool PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/../")
target_link_libraries(test_control_tool PRIVATE fep3_system fep3_participant_core a_util GTest::Main)

if (FEP_BASE_UTILITIES_LIB_BUILD)
    target_link_libraries(test_control_tool INTERFACE fep_control)
    target_compile_definitions(test_control_tool
        PRIVATE TESTFILES_DIR="${CMAKE_CURRENT_SOURCE_DIR}/../"
        PRIVATE BINARY_TOOL_PATH=$<TARGET_FILE:fep_control>
    )
else()
	if (WIN32)
		target_compile_definitions(test_control_tool
			PRIVATE TESTFILES_DIR="${CMAKE_CURRENT_SOURCE_DIR}/../"
			PRIVATE BINARY_TOOL_PATH="${CONAN_FEP_SDK_BASE_UTILITIES_ROOT}/bin/fep_control.exe"
		)
	else()
		target_compile_definitions(test_control_tool
			PRIVATE TESTFILES_DIR="${CMAKE_CURRENT_SOURCE_DIR}/../"
			PRIVATE BINARY_TOOL_PATH="${CONAN_FEP_SDK_BASE_UTILITIES_ROOT}/bin/fep_control"
		)
	endif()
endif()
# depends on binary
# "SYSTEM" includes (compiler warnings will be ignored)
target_include_directories(test_control_tool SYSTEM
    PRIVATE
    ${CONAN_INCLUDE_DIRS}
)
set(Boost_USE_STATIC_LIBS ON)
target_link_libraries(test_control_tool PRIVATE CONAN_PKG::boost_filesystem)
