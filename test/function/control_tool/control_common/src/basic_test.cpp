/**
 * @file

   @copyright
   @verbatim
   Copyright @ 2019 Audi AG. All rights reserved.
   
       This Source Code Form is subject to the terms of the Mozilla
       Public License, v. 2.0. If a copy of the MPL was not distributed
       with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
   
   If it is not possible or desirable to put the notice in a particular file, then
   You may include the notice in a location (such as a LICENSE file in a
   relevant directory) where a recipient would be likely to look for such a notice.
   
   You may add additional accurate notices of copyright ownership.
   @endverbatim
 *
 *
 * @remarks
 *
 */

#include <unordered_set>
#include <chrono>
#include <thread>
#include "gtest/gtest.h"
#include <fep_system/fep_system.h>
#include <boost/process.hpp>
#include <a_util/strings.h>
#include <a_util/filesystem.h>
#include "../../../../../src/fep_control_tool/control_tool_common_helper.h"
#include <fep3/core.h>
#include <fep3/core/participant_executor.hpp>

#define STR(x) #x
#define STRINGIZE(x) STR(x)
namespace
{
    const std::string binary_tool_path = STRINGIZE(BINARY_TOOL_PATH);
}
#undef STR
#undef STRINGIZE

namespace bp = boost::process;

inline void skipUntilPrompt(bp::child& c, bp::ipstream& reader_stream)
{
    std::string str;
    for (;;)
    {
        ASSERT_TRUE(c.running());
        reader_stream >> str;
        if (str == "fep>")
        {
            break;
        }
    }
}

const std::unordered_set<std::string> skippables = { "sendto","127.0.0.1:9990", "for", "multicast", "address", "230.230.230.1:9990",
"failed,", "errno", "=", "A", "socket", "operation", "was", "attempted", "to", "an", "unreachable", "network.", "(10051)" };

inline void checkUntilPrompt(bp::child& c, bp::ipstream& reader_stream, const std::vector<std::string>& expected_answer)
{
    std::string str;
    for (size_t i = 0u; ; ++i)
    {
        for (;;)
        {
            ASSERT_TRUE(c.running());
            reader_stream >> str;
            if (str == "fep>")
            {
                EXPECT_EQ(i, expected_answer.size());
                return;
            }
            if ((i == expected_answer.size() || str != expected_answer[i]) && skippables.count(str))
            {
                continue;
            }
            break;
        }
        ASSERT_TRUE(i < expected_answer.size());
        EXPECT_EQ(str, expected_answer[i]);
    }
}

inline void closeSession(bp::child& c, bp::opstream& writer_stream)
{
    ASSERT_TRUE(c.running());
    writer_stream << std::endl << "quit" << std::endl;
    c.wait();
}

/**
* @brief Test welcome message
*/
TEST(ControlTool, testWelcomeMessage)
{
    bp::opstream writer_stream;
    bp::ipstream reader_stream;

    bp::child c(binary_tool_path, bp::std_out > reader_stream, bp::std_in < writer_stream);
    const std::vector<std::string> expected_answer = {
        "******************************************************************",
        "*", "Welcome", "to", "FEP", "Control(c)", "2020", "AUDI", "AG", "*",
        "*", "use", "help", "to", "print", "help", "*",
        "******************************************************************"
    };
    checkUntilPrompt(c, reader_stream, expected_answer);
    closeSession(c, writer_stream);
}

/**
* @brief Test help command
*/
TEST(ControlTool, testHelp)
{
    bp::opstream writer_stream;
    bp::ipstream reader_stream;
    bp::child c(binary_tool_path, bp::std_out > reader_stream, bp::std_in < writer_stream);
    skipUntilPrompt(c, reader_stream);

    writer_stream << "help" << std::endl;

    std::vector<std::string> commands = {
        "exit",
        "quit",
        "discoverAllSystems",
        "discoverSystem",
        "setCurrentWorkingDirectory",
        "getCurrentWorkingDirectory",
        "connectSystem",
        "help",
        "loadSystem",
        "unloadSystem",
        "initializeSystem",
        "deinitializeSystem",
        "startSystem",
        "stopSystem",
        "pauseSystem",
        "shutdownSystem",
        "startMonitoringSystem",
        "stopMonitoringSystem",
        "loadParticipant",
        "unloadParticipant",
        "initializeParticipant",
        "deinitializeParticipant",
        "startParticipant",
        "stopParticipant",
        "pauseParticipant",
        "getParticipantRPCObjects",
        "getParticipantRPCObjectIIDs",
        "getParticipantRPCObjectIIDDefinition",
        "shutdownParticipant",
        "getSystemState",
        "setSystemState",
        "getParticipantState",
        "setParticipantState",
        "getParticipants",
        "configureSystem",
        "configureTiming3SystemTime",
        "configureTiming3DiscreteTime",
        "configureTiming3NoSync",
        "getCurrentTimingMaster",
        "enableAutoDiscovery",
        "disableAutoDiscovery",
	};
    std::vector<std::string> listed_commands;

    std::string str, laststr;
    for (;;)
    {
        reader_stream >> str;
        if (str == "fep>")
        {
            break;
        }
        if (str == ":")
        {
            listed_commands.emplace_back(std::move(laststr));
        }
        laststr = std::move(str);
    }
    ASSERT_EQ(commands.size(), listed_commands.size());

    std::sort(commands.begin(), commands.end());
    std::sort(listed_commands.begin(), listed_commands.end());

    EXPECT_EQ(commands, listed_commands);

    writer_stream << "help help" << std::endl;
    const std::vector<std::string> expected_answer = { "help", "<command", "name>", ":", "prints", "out", "the", "description", "of", "the", "commands" };
    checkUntilPrompt(c, reader_stream, expected_answer);
    closeSession(c, writer_stream);
}

/**
* @brief Test getCurrentWorkingDirectory (if it returns an absolute path)
*/
TEST(ControlTool, testGetCurrentWorkingDirectory)
{
    bp::opstream writer_stream;
    bp::ipstream reader_stream;
    bp::child c(binary_tool_path, bp::std_out > reader_stream, bp::std_in < writer_stream);
    skipUntilPrompt(c, reader_stream);

    std::string line;
    writer_stream << "getCurrentWorkingDirectory" << std::endl;
    ASSERT_TRUE(c.running());
    ASSERT_TRUE(std::getline(reader_stream, line));
    a_util::strings::trim(line);
    std::string expected_prefix = "working directory : ";
    ASSERT_EQ(line.compare(0u, expected_prefix.size(), expected_prefix), 0);
    a_util::filesystem::Path current_dir(line.substr(expected_prefix.size()));
    EXPECT_TRUE(current_dir.isAbsolute());
    closeSession(c, writer_stream);
}

/**
* @brief Test setCurrentWorkingDirectory (if it returns an absolute path)
*/
TEST(ControlTool, testSetCurrentWorkingDirectory)
{
    const auto current_path = a_util::filesystem::getWorkingDirectory();

    bp::opstream writer_stream;
    bp::ipstream reader_stream;
    bp::child c(binary_tool_path, bp::std_out > reader_stream, bp::std_in < writer_stream);
    skipUntilPrompt(c, reader_stream);

    std::string line;
    const auto new_path = current_path + "files";
    writer_stream << "setCurrentWorkingDirectory " << quoteFilenameIfNecessary(new_path.toString()) << std::endl;
    ASSERT_TRUE(c.running());
    ASSERT_TRUE(std::getline(reader_stream, line));
    a_util::strings::trim(line);
    std::string expected_prefix = "working directory : ";
    ASSERT_EQ(line.compare(0u, expected_prefix.size(), expected_prefix), 0);
    a_util::filesystem::Path returned_new_path(line.substr(expected_prefix.size()));
    ASSERT_TRUE(returned_new_path.isAbsolute());
    EXPECT_TRUE(returned_new_path == new_path);
    closeSession(c, writer_stream);
    a_util::filesystem::setWorkingDirectory(current_path);
}

struct TestElement : public fep3::core::ElementBase
{
    TestElement() : fep3::core::ElementBase("Testelement", "3.0")
    {
    }
};
struct PartStruct
{
    PartStruct(PartStruct&&) = default;
    ~PartStruct() = default;
    PartStruct(fep3::core::Participant&& part) : _part(std::move(part)), _part_executor(_part)
    {
    }
    fep3::core::Participant _part;
    fep3::core::ParticipantExecutor _part_executor;
};
using TestParticipants = std::map<std::string, std::unique_ptr<PartStruct>>;
inline TestParticipants createTestParticipants(const std::vector<std::string>& participant_names, const std::string& system_name)
{
    using namespace fep3::core;
    TestParticipants test_parts;
    std::for_each(participant_names.begin(), participant_names.end(), [&](const std::string& name)
    {
        auto part = createParticipant<ElementFactory<TestElement>>(name, "1.0", system_name);
        auto part_exec = std::make_unique<PartStruct>(std::move(part));
        part_exec->_part_executor.exec();
        test_parts[name].reset(part_exec.release());
    }
    );
    return std::move(test_parts);
}

inline std::unique_ptr<fep3::System> createSystem(TestParticipants& test_parts, bool start_system = true)
{
    const std::string sys_name = "FEP_SYSTEM";
    const std::string part_name_1 = "test_part_0";
    const std::string part_name_2 = "test_part_1";
    const auto participant_names = std::vector<std::string>{ part_name_1, part_name_2 };
    test_parts = createTestParticipants(participant_names, sys_name);
    std::unique_ptr<fep3::System> my_sys(new fep3::System(sys_name));
    my_sys->add(part_name_1);
    my_sys->add(part_name_2);

    auto part_1 = my_sys->getParticipant(part_name_1);
    auto logging_service_1 = part_1.getRPCComponentProxyByIID<fep3::rpc::IRPCLoggingService>();
    logging_service_1->setLoggerFilter("participant", { fep3::logging::Severity::off, {"console"} });

    auto part_2 = my_sys->getParticipant(part_name_2);
    auto logging_service_2 = part_2.getRPCComponentProxyByIID<fep3::rpc::IRPCLoggingService>();
    logging_service_2->setLoggerFilter("participant", { fep3::logging::Severity::off, {"console"} });

    my_sys->load();
    my_sys->initialize();
    if (start_system)
    {
        my_sys->start();
    }
    return my_sys;
}

/**
* @brief Test discoverAllSystems
*/
TEST(ControlTool, testDiscoverAllSystems)
{
    TestParticipants test_parts;
    auto fep_system = createSystem(test_parts);

    bp::opstream writer_stream;
    bp::ipstream reader_stream;
    bp::child c(binary_tool_path, bp::std_out > reader_stream, bp::std_in < writer_stream);
    skipUntilPrompt(c, reader_stream);

    writer_stream << "discoverAllSystems" << std::endl;

    const std::vector<std::string> expected_answer = { "FEP_SYSTEM", ":", "test_part_0,", "test_part_1" };
    checkUntilPrompt(c, reader_stream, expected_answer);
    closeSession(c, writer_stream);
}

/**
* @brief Test connectSystem, getParticipants, getSystemState, startSystem, stopSystem
*/
TEST(ControlTool, testSystemHandling)
{
    const auto current_path = a_util::filesystem::getWorkingDirectory();
    TestParticipants test_parts;
    auto fep_system = createSystem(test_parts, false);

    bp::opstream writer_stream;
    bp::ipstream reader_stream;
    bp::child c(binary_tool_path, bp::std_out > reader_stream, bp::std_in < writer_stream);
    skipUntilPrompt(c, reader_stream);

    const auto test_files_path = current_path + "files";
    writer_stream << "setCurrentWorkingDirectory " << quoteFilenameIfNecessary(test_files_path.toString()) << std::endl;
    skipUntilPrompt(c, reader_stream);

    writer_stream << "connectSystem \"DEMO fep_sdk.system\"" << std::endl;
    const std::vector<std::string> expected_answer_participants = { "FEP_SYSTEM", ":", "test_part_0,", "test_part_1" };
    checkUntilPrompt(c, reader_stream, expected_answer_participants);

    writer_stream << "getParticipants FEP_SYSTEM" << std::endl;
    checkUntilPrompt(c, reader_stream, expected_answer_participants);

    writer_stream << "getSystemState FEP_SYSTEM" << std::endl;
    const std::vector<std::string> expected_answer_state_initialized = { "4", "-", "initialized", "-", "homogeneous", ":", "1"};
    checkUntilPrompt(c, reader_stream, expected_answer_state_initialized);

    writer_stream << "startSystem FEP_SYSTEM" << std::endl;
    const std::vector<std::string> expected_answer_started = { "FEP_SYSTEM", "started" };
    checkUntilPrompt(c, reader_stream, expected_answer_started);

    writer_stream << "getSystemState FEP_SYSTEM" << std::endl;
    const std::vector<std::string> expected_answer_state_running = { "6", "-", "running", "-", "homogeneous", ":", "1" };
    checkUntilPrompt(c, reader_stream, expected_answer_state_running);

	writer_stream << "pauseSystem FEP_SYSTEM" << std::endl;
	const std::vector<std::string> expected_answer_paused = { "FEP_SYSTEM", "paused" };
	checkUntilPrompt(c, reader_stream, expected_answer_paused);

	writer_stream << "getSystemState FEP_SYSTEM" << std::endl;
	const std::vector<std::string> expected_answer_state_paused = { "5", "-", "paused", "-", "homogeneous", ":", "1" };
	checkUntilPrompt(c, reader_stream, expected_answer_state_paused);

    writer_stream << "stopSystem FEP_SYSTEM" << std::endl;
    const std::vector<std::string> expected_answer_stopped = { "FEP_SYSTEM", "stopped" };
    checkUntilPrompt(c, reader_stream, expected_answer_stopped);

    writer_stream << "getSystemState FEP_SYSTEM" << std::endl;
    checkUntilPrompt(c, reader_stream, expected_answer_state_initialized);

	writer_stream << "deinitializeSystem FEP_SYSTEM" << std::endl;
	const std::vector<std::string> expected_answer_deinitialized = { "FEP_SYSTEM", "deinitialized" };
	checkUntilPrompt(c, reader_stream, expected_answer_deinitialized);

	writer_stream << "getSystemState FEP_SYSTEM" << std::endl;
	const std::vector<std::string> expected_answer_state_loaded = { "3", "-", "loaded", "-", "homogeneous", ":", "1" };
	checkUntilPrompt(c, reader_stream, expected_answer_state_loaded);

	writer_stream << "unloadSystem FEP_SYSTEM" << std::endl;
	const std::vector<std::string> expected_answer_unloaded = { "FEP_SYSTEM", "unloaded" };
	checkUntilPrompt(c, reader_stream, expected_answer_unloaded);

	writer_stream << "getSystemState FEP_SYSTEM" << std::endl;
	const std::vector<std::string> expected_answer_state_unloaded = { "2", "-", "unloaded", "-", "homogeneous", ":", "1" };
	checkUntilPrompt(c, reader_stream, expected_answer_state_unloaded);

	writer_stream << "loadSystem FEP_SYSTEM" << std::endl;
	const std::vector<std::string> expected_answer_loaded = { "FEP_SYSTEM", "loaded" };
	checkUntilPrompt(c, reader_stream, expected_answer_loaded);

	writer_stream << "getSystemState FEP_SYSTEM" << std::endl;
	checkUntilPrompt(c, reader_stream, expected_answer_state_loaded);

	writer_stream << "initializeSystem FEP_SYSTEM" << std::endl;
	const std::vector<std::string> expected_answer_initialized = { "FEP_SYSTEM", "initialized" };
	checkUntilPrompt(c, reader_stream, expected_answer_initialized);

	writer_stream << "getSystemState FEP_SYSTEM" << std::endl;
	checkUntilPrompt(c, reader_stream, expected_answer_state_initialized);

    closeSession(c, writer_stream);
    a_util::filesystem::setWorkingDirectory(current_path);
}

/**
* @brief Test configureSystem
*/
TEST(ControlTool, testConfigureSystem)
{
    const auto current_path = a_util::filesystem::getWorkingDirectory();
    TestParticipants test_parts;
    auto fep_system = createSystem(test_parts, false);

    bp::opstream writer_stream;
    bp::ipstream reader_stream;
    bp::child c(binary_tool_path, bp::std_out > reader_stream, bp::std_in < writer_stream);
    skipUntilPrompt(c, reader_stream);

    const auto test_files_path = current_path + "files";
    writer_stream << "setCurrentWorkingDirectory " << quoteFilenameIfNecessary(test_files_path.toString()) << std::endl;
    skipUntilPrompt(c, reader_stream);

    writer_stream << "connectSystem \"DEMO fep_sdk.system\"" << std::endl;
    const std::vector<std::string> expected_answer_participants = { "FEP_SYSTEM", ":", "test_part_0,", "test_part_1" };
    checkUntilPrompt(c, reader_stream, expected_answer_participants);

    writer_stream << "configureSystem FEP_SYSTEM DEMO.properties" << std::endl;
    const std::vector<std::string> expected_answer_configure = { "properties", "set"};
    checkUntilPrompt(c, reader_stream, expected_answer_configure);

    closeSession(c, writer_stream);
    a_util::filesystem::setWorkingDirectory(current_path);
}

/**
* @brief Test discoverSystem
*/
TEST(ControlTool, testDiscoverSystem)
{
	TestParticipants test_parts;
	auto fep_system = createSystem(test_parts);

	bp::opstream writer_stream;
	bp::ipstream reader_stream;
	bp::child c(binary_tool_path, bp::std_out > reader_stream, bp::std_in < writer_stream);
	skipUntilPrompt(c, reader_stream);

	writer_stream << "discoverSystem FEP_SYSTEM" << std::endl;

	const std::vector<std::string> expected_answer = { "FEP_SYSTEM", ":", "test_part_0,", "test_part_1" };
	checkUntilPrompt(c, reader_stream, expected_answer);
	closeSession(c, writer_stream);
}

/**
* @brief Test wrong command
*/
TEST(ControlTool, testInvalidCommand)
{
    bp::opstream writer_stream;
    bp::ipstream reader_stream;
    bp::child c(binary_tool_path, bp::std_out > reader_stream, bp::std_in < writer_stream);
    skipUntilPrompt(c, reader_stream);

    writer_stream << "hlep" << std::endl;

    const std::vector<std::string> expected_answer = { "Invalid", "command", "\"hlep\",", "use", "\"help\"", "for", "valid", "commands" };
    checkUntilPrompt(c, reader_stream, expected_answer);
    closeSession(c, writer_stream);
}

/**
* @brief Test wrong number of arguments for a command
*/
TEST(ControlTool, testWrongNumberOfArguments)
{
    bp::opstream writer_stream;
    bp::ipstream reader_stream;
    bp::child c(binary_tool_path, bp::std_out > reader_stream, bp::std_in < writer_stream);
    skipUntilPrompt(c, reader_stream);

    writer_stream << "getCurrentWorkingDirectory c:" << std::endl;

    const std::vector<std::string> expected_answer = { "Invalid", "number", "of", "arguments", "for", "\"getCurrentWorkingDirectory\"",
        "(1", "instead", "of", "0),", "use", "\"help\"", "for", "more", "information" };
    checkUntilPrompt(c, reader_stream, expected_answer);
    closeSession(c, writer_stream);
}

/**
* @brief Test outer command line
*/
TEST(ControlTool, testCommandlineArgument)
{
    bp::opstream writer_stream;
    bp::ipstream reader_stream;
    bp::child c(binary_tool_path + " -e getCurrentWorkingDirectory", bp::std_out > reader_stream, bp::std_in < writer_stream);

    ASSERT_TRUE(c.running());

    std::string line;
    ASSERT_TRUE(std::getline(reader_stream, line));
    a_util::strings::trim(line);
    std::string expected_prefix = "working directory : ";
    ASSERT_EQ(line.compare(0u, expected_prefix.size(), expected_prefix), 0);
    a_util::filesystem::Path current_dir(line.substr(expected_prefix.size()));
    EXPECT_TRUE(current_dir.isAbsolute());
    closeSession(c, writer_stream);
}

/**
* @brief Test exit
*/
TEST(ControlTool, testExit)
{
    using namespace std::chrono_literals;

    bp::opstream writer_stream;
    bp::ipstream reader_stream;
    bp::child c(binary_tool_path, bp::std_out > reader_stream, bp::std_in < writer_stream);
    skipUntilPrompt(c, reader_stream);

    writer_stream << "exit" << std::endl;
    std::this_thread::sleep_for(1s);

    EXPECT_FALSE(c.running());
}

/**
* @brief Test quit
*/
TEST(ControlTool, testQuit)
{
    using namespace std::chrono_literals;

    bp::opstream writer_stream;
    bp::ipstream reader_stream;
    bp::child c(binary_tool_path, bp::std_out > reader_stream, bp::std_in < writer_stream);
    skipUntilPrompt(c, reader_stream);

    writer_stream << "quit" << std::endl;
    std::this_thread::sleep_for(1s);

    EXPECT_FALSE(c.running());
}

/**
* @brief Test participant state transitions, setSystemState, setParticipantState
*/
TEST(ControlTool, testParticipantStates)
{
    TestParticipants test_parts;
    auto fep_system = createSystem(test_parts, false);

    bp::opstream writer_stream;
    bp::ipstream reader_stream;
    bp::child c(binary_tool_path, bp::std_out > reader_stream, bp::std_in < writer_stream);
    skipUntilPrompt(c, reader_stream);

    writer_stream << "discoverAllSystems" << std::endl;

    const std::vector<std::string> expected_answer = { "FEP_SYSTEM", ":", "test_part_0,", "test_part_1" };
    checkUntilPrompt(c, reader_stream, expected_answer);

    using State = fep3::SystemAggregatedState;
    const std::vector<std::string> states_desc = {
        "undefined", //0
        "unreachable", //1
        "unloaded", //2
        "loaded", //3
        "initialized", //4
        "paused", //5
        "running"
    };

    auto check_states = [&](State system_state, int homogenous, State part0_state, State part1_state)
    {
        writer_stream << "getSystemState FEP_SYSTEM" << std::endl;
        const std::vector<std::string> expected_answer_system_state = { std::to_string(system_state), "-", states_desc[system_state],
            "-", "homogeneous", ":", std::to_string(homogenous) };
        
        checkUntilPrompt(c, reader_stream, expected_answer_system_state);

        writer_stream << "getParticipantState FEP_SYSTEM test_part_0" << std::endl;
        const std::vector<std::string> expected_answer_part0_state = { std::to_string(part0_state), "-", states_desc[part0_state] };
        checkUntilPrompt(c, reader_stream, expected_answer_part0_state);

        writer_stream << "getParticipantState FEP_SYSTEM test_part_1" << std::endl;
        const std::vector<std::string> expected_answer_part1_state = { std::to_string(part1_state), "-", states_desc[part1_state] };
        checkUntilPrompt(c, reader_stream, expected_answer_part1_state);
    };

    auto action_participant = [&](const std::string& participant_number, const std::string& action, const std::string& result)
    {
        writer_stream << action << "Participant FEP_SYSTEM test_part_" << participant_number << std::endl;
        const std::vector<std::string> expected_answer = { "test_part_" + participant_number + "@FEP_SYSTEM", result };
        checkUntilPrompt(c, reader_stream, expected_answer);
    };

    check_states(State::initialized, 1, State::initialized, State::initialized);
    action_participant("0", "start", "started");
    check_states(State::initialized, 0, State::running, State::initialized);
    action_participant("1", "start", "started");
    check_states(State::running, 1, State::running, State::running);
    action_participant("1", "pause", "paused");
    check_states(State::paused, 0, State::running, State::paused);
    action_participant("0", "stop", "stopped");
    check_states(State::initialized, 0, State::initialized, State::paused);
    action_participant("1", "stop", "stopped");
    check_states(State::initialized, 1, State::initialized, State::initialized);
    action_participant("0", "deinitialize", "deinitialized");
    check_states(State::loaded, 0, State::loaded, State::initialized);
    action_participant("0", "unload", "unloaded");
    check_states(State::unloaded, 0, State::unloaded, State::initialized);
    action_participant("0", "load", "loaded");
    check_states(State::loaded, 0, State::loaded, State::initialized);
    action_participant("0", "initialize", "initialized");
    check_states(State::initialized, 1, State::initialized, State::initialized);

    writer_stream << "setSystemState FEP_SYSTEM paused" << std::endl;
    const std::vector<std::string> expected_answer_system_state_paused = { "5", "-", "paused", "-", "homogeneous", ":", "1" };
    checkUntilPrompt(c, reader_stream, expected_answer_system_state_paused);

    writer_stream << "setParticipantState FEP_SYSTEM test_part_1 running" << std::endl;
    const std::vector<std::string> expected_answer_part_state_running = { "6", "-", "running"};
    checkUntilPrompt(c, reader_stream, expected_answer_part_state_running);

    check_states(State::paused, 0, State::paused, State::running);

    closeSession(c, writer_stream);
 }

/**
* @brief Test getCurrentTimingMaster, configureTiming3SystemTime, configureTiming3NoSync and configureTiming3DiscreteTime
*/
TEST(ControlTool, testTimingMaster)
{
    TestParticipants test_parts;
    auto fep_system = createSystem(test_parts, false);

    bp::opstream writer_stream;
    bp::ipstream reader_stream;
    bp::child c(binary_tool_path, bp::std_out > reader_stream, bp::std_in < writer_stream);
    skipUntilPrompt(c, reader_stream);

    writer_stream << "discoverAllSystems" << std::endl;

    const std::vector<std::string> expected_answer = { "FEP_SYSTEM", ":", "test_part_0,", "test_part_1" };
    checkUntilPrompt(c, reader_stream, expected_answer);

    writer_stream << "getCurrentTimingMaster FEP_SYSTEM" << std::endl;
    const std::vector<std::string> expected_answer_empty_timing_masters = { "timing", "masters:" };
    checkUntilPrompt(c, reader_stream, expected_answer_empty_timing_masters);

    writer_stream << "configureTiming3SystemTime FEP_SYSTEM test_part_0" << std::endl;
    checkUntilPrompt(c, reader_stream, {});

    writer_stream << "getCurrentTimingMaster FEP_SYSTEM" << std::endl;
    const std::vector<std::string> expected_answer_part0_timing_masters = { "timing", "masters:", "test_part_0" };
    checkUntilPrompt(c, reader_stream, expected_answer_part0_timing_masters);

    writer_stream << "configureTiming3NoSync FEP_SYSTEM" << std::endl;
    checkUntilPrompt(c, reader_stream, {});

    writer_stream << "getCurrentTimingMaster FEP_SYSTEM" << std::endl;
    checkUntilPrompt(c, reader_stream, expected_answer_empty_timing_masters);

    writer_stream << "configureTiming3DiscreteTime FEP_SYSTEM test_part_1 1.0 0.0" << std::endl;
    checkUntilPrompt(c, reader_stream, {});

    writer_stream << "getCurrentTimingMaster FEP_SYSTEM" << std::endl;
    const std::vector<std::string> expected_answer_part1_timing_masters = { "timing", "masters:", "test_part_1"};
    checkUntilPrompt(c, reader_stream, expected_answer_part1_timing_masters);

    closeSession(c, writer_stream);
}

/**
* @brief Test enableAutoDiscovery, disableAutoDiscovery // off by default
*/
TEST(ControlTool, testAutoDiscovery)
{
    TestParticipants test_parts;
    auto fep_system = createSystem(test_parts, false);

    bp::opstream writer_stream;
    bp::ipstream reader_stream;
    bp::child c(binary_tool_path, bp::std_out > reader_stream, bp::std_in < writer_stream);
    skipUntilPrompt(c, reader_stream);

    writer_stream << "getSystemState FEP_SYSTEM" << std::endl;
    const std::vector<std::string> expected_answer_not_connected = { "system", "\"FEP_SYSTEM\"",
        "is", "not", "connected" };
    checkUntilPrompt(c, reader_stream, expected_answer_not_connected);

    writer_stream << "disableAutoDiscovery" << std::endl;

    const std::vector<std::string> expected_answer_disabled = {  "auto_discovery:", "disabled" };
    checkUntilPrompt(c, reader_stream, expected_answer_disabled);

    writer_stream << "getSystemState FEP_SYSTEM" << std::endl;
    checkUntilPrompt(c, reader_stream, expected_answer_not_connected);

    writer_stream << "enableAutoDiscovery" << std::endl;

    const std::vector<std::string> expected_answer_enabled = { "auto_discovery:", "enabled" };
    checkUntilPrompt(c, reader_stream, expected_answer_enabled);

    writer_stream << "getSystemState FEP_SYSTEM" << std::endl;
    const std::vector<std::string> expected_answer_state_initialized = { "4", "-", "initialized", "-", "homogeneous", ":", "1" };
    checkUntilPrompt(c, reader_stream, expected_answer_state_initialized);

}
