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

################################################################
# as long as FEPSDK-FEPSDK-2196 we must do that hack
include_directories(CONAN_FEP_CONTROLLER_ROOT)
################################################################

find_package(fep3_system REQUIRED)
find_package(fep3_controller REQUIRED)

set(FEP_CONTROL_TOOL fep_control)

find_package(a_util REQUIRED)
find_package(Clara REQUIRED)

add_executable(${FEP_CONTROL_TOOL}
    linenoise/linenoise.h
    linenoise/linenoise.c
    linenoise/utf8.h
    linenoise/utf8.c
    linenoise_wrapper.h
    linenoise_wrapper.cpp
    control_tool_common_helper.h
    fep_control_tool.cpp
)

target_link_libraries(${FEP_CONTROL_TOOL}
    PUBLIC fep3_controller
    PUBLIC fep3_system
    PUBLIC a_util
    PRIVATE Clara)

install(TARGETS ${FEP_CONTROL_TOOL} 
        RUNTIME DESTINATION bin)

install(FILES linenoise/linenoise.h
        DESTINATION doc/license/used/linenoise
        CONFIGURATIONS Release Debug RelWithDebInfo
        RENAME linenoise-license.txt
    )

fep3_controller_install(${FEP_CONTROL_TOOL} bin)
fep3_controller_deploy(${FEP_CONTROL_TOOL})

set_target_properties(${FEP_CONTROL_TOOL} PROPERTIES INSTALL_RPATH "$ORIGIN")
set_target_properties(${FEP_CONTROL_TOOL} PROPERTIES FOLDER tools)
