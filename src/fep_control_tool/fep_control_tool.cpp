/**

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
 */

#include <stdlib.h>
#include <iostream>
#include <cctype>
#include <cstring>

#include <a_util/filesystem.h>

#include <fep_system/fep_system.h>
#include <fep_controller/fep_controller.h>
#include "linenoise_wrapper.h"
#include "control_tool_common_helper.h"

static void skipWhitespace(const char*& p, const char* pAdditionalWhitechars = nullptr)
{
    if (nullptr == p)
    {
        return;
    }
    if (pAdditionalWhitechars != nullptr)
    {
        while (std::isspace(*p) || (*p != '\0' && strchr(pAdditionalWhitechars, *p) != nullptr))
        {
            p++;
        }
    }
    else
    {
        while (std::isspace(*p))
        {
            p++;
        }
    }
}

static bool getNextWord(const char*& pSrc, std::string& strDest, const char* pAdditionalSeparator = nullptr, bool bUseEscape = true)
{
    if (nullptr == pSrc)
    {
        return false;
    }
    strDest.clear();

    skipWhitespace(pSrc);

    if (*pSrc == '\0')
    {
        return false;
    }

    char bEscapeActive = false;
    char cLastChar = '\0';
    char cQuote = '\0';

    if (*pSrc == '\"' || *pSrc == '\'')
    {
        cQuote = *(pSrc++);
        const char* pSrcStart = pSrc;

        while (*pSrc != '\0' && (bEscapeActive || *pSrc != cQuote))
        {
            bEscapeActive = bUseEscape && (*pSrc == '\\' && cLastChar != '\\');  // escape next char?
            cLastChar = *pSrc;
            pSrc++;
        }

        strDest = std::string(pSrcStart, pSrc);

        if (*pSrc == cQuote)
        {
            pSrc++;
        }
    }
    else
    {
        const char* pSrcStart = pSrc;

        if (pAdditionalSeparator == nullptr)
        {
            while (*pSrc != '\0' && !std::isspace(*pSrc))
            {
                pSrc++;
                if (*pSrc == '\"' || *pSrc == '\'')
                {
                    cQuote = *(pSrc);

                    do
                    {
                        bEscapeActive = bUseEscape && (*pSrc == '\\' && cLastChar != '\\');
                        cLastChar = *pSrc;
                        pSrc++;
                    } while (*pSrc != '\0' && (bEscapeActive || *pSrc != cQuote));
                }
            }

            strDest = std::string(pSrcStart, pSrc);
        }
        else
        {
            while (*pSrc != '\0' && (!std::isspace(*pSrc) && strchr(pAdditionalSeparator, *pSrc) == nullptr))
            {
                pSrc++;
            }

            strDest = std::string(pSrcStart, pSrc);
        }
    }

    return true;
}

static std::vector<std::string> parseLine(const std::string& line)
{
    const char *p = line.c_str();
    std::vector<std::string> words(1);
    while (getNextWord(p, words.back()))
    {
        words.emplace_back();
    }
    words.pop_back();
    return words;
}

namespace
{
    typedef std::vector<std::string>::const_iterator TokenIterator;
    typedef std::function<bool(TokenIterator first, TokenIterator last)> ActionFunction;
    typedef std::function<std::vector<std::string>(const std::string& input)> ArgumentCompletionFunction;

    std::map<std::string, fep3::System> connected_or_discovered_systems;
    bool auto_discovery_of_systems = false;
    std::string last_system_name_used = "";
    const std::string empty_system_name = "-";

    static void discoverSystemByName(const std::string& name)
    {
        auto system_name = name;
        //this updates for completion
        last_system_name_used = system_name;
        if (system_name == empty_system_name)
        {
            system_name = "";
        }
        auto system = fep3::discoverSystem(system_name);
        connected_or_discovered_systems[system.getSystemName()] = std::move(system);
    }

    decltype(connected_or_discovered_systems)::iterator getConnectedOrDiscoveredSystem(const std::string& name, 
        bool auto_discovery)
    {
        auto it = connected_or_discovered_systems.find(name);
        if (it != connected_or_discovered_systems.end())
        {
            last_system_name_used = name;
            return it;
        }
        else
        {
            if (auto_discovery)
            {
                discoverSystemByName(name);
                return getConnectedOrDiscoveredSystem(name, false);
            }
        }
        std::cout << "system \"" << name << "\" is not connected" << std::endl;
        return connected_or_discovered_systems.end();
    }

    struct ArgumentHandler
    {
        std::string _description;
        ArgumentCompletionFunction _completion;
    };


    std::vector<std::string> noCompletion(const std::string&)
    {
        return std::vector<std::string>();
    }


    std::vector<std::string> commandNameCompletion(const std::string& word_prefix);

    std::vector<std::string> localFilesCompletion(const std::string& word_prefix)
    {
        std::vector<a_util::filesystem::Path> file_list;
        a_util::filesystem::enumDirectory(".", file_list, a_util::filesystem::ED_FILES);

        std::vector<std::string> completions;
        for (const auto& file_path : file_list)
        {
            std::string file_name = file_path.getLastElement().toString();
            if (file_name.compare(0u, word_prefix.size(), word_prefix) == 0)
            {
                completions.push_back(quoteFilenameIfNecessary(file_name));
            }
        }
        return completions;
    }

    std::vector<std::string> connectedSystemsCompletion(const std::string& word_prefix)
    {
        std::vector<std::string> completions;
        for (const auto& system : connected_or_discovered_systems)
        {
            if (system.first.compare(0u, word_prefix.size(), word_prefix) == 0)
            {
                completions.push_back(system.first);
            }
        }
        return completions;
    }

    std::vector<std::string> connectedParticipantsCompletion(const std::string& word_prefix)
    {
        std::vector<std::string> completions;
        const auto& found_system = connected_or_discovered_systems.find(last_system_name_used);
        if (found_system != connected_or_discovered_systems.cend())
        {
            auto parts = found_system->second.getParticipants();
            for (const auto& part : parts)
            {
                if (part.getName().compare(0u, word_prefix.size(), word_prefix) == 0)
                {
                    completions.push_back(part.getName());
                }
            }
        }
        return completions;
    }

    fep3::SystemAggregatedState getStateFromString(const std::string& state_string)
    {
        if (state_string == "shutdowned")
        {
            return fep3::SystemAggregatedState::unreachable;
        }
        else if (state_string == "unloaded")
        {
            return fep3::SystemAggregatedState::unloaded;
        }
        else if (state_string == "loaded")
        {
            return fep3::SystemAggregatedState::loaded;
        }
        else if (state_string == "initialized")
        {
            return fep3::SystemAggregatedState::initialized;
        }
        else if (state_string == "paused")
        {
            return fep3::SystemAggregatedState::paused;
        }
        else if (state_string == "running")
        {
            return fep3::SystemAggregatedState::running;
        }
        return fep3::SystemAggregatedState::undefined;
    }

    std::vector<std::string> possibleSystemsStateCompletion(const std::string& word_prefix)
    {
        std::vector<std::string> completions;
        std::vector<std::string> possible_states = { "shutdowned", "unloaded", "loaded", "initialized", "paused", "running" };
        for (const auto& state_val : possible_states)
        {
            if (state_val.compare(0u, word_prefix.size(), word_prefix) == 0)
            {
                completions.push_back(state_val);
            }
        }
        return completions;
    }

    struct ControlCommand
    {
        std::string _name, _description;
        ActionFunction _action;
        std::vector<ArgumentHandler> _arguments;
        size_t _last_optional_parameters;
    };

    static std::string resolveFilesystemErrorCode(a_util::filesystem::Error error_code)
    {
        switch (error_code)
        {
            case a_util::filesystem::OK: return "OK";
            case a_util::filesystem::OPEN_FAILED: return "OPEN_FAILED";
            case a_util::filesystem::GENERAL_FAILURE: return "GENERAL_FAILURE";
            case a_util::filesystem::IO_ERROR: return "IO_ERROR";
            case a_util::filesystem::INVALID_PATH: return "INVALID_PATH";
            case a_util::filesystem::ACCESS_DENIED: return "ACCESS_DENIED";
            default: assert(false);
        }
        return "UNKNOWN_ERROR";
    }

    static std::string resolveSystemState(fep3::System::AggregatedState st)
    {
        switch (st)
        {
            case fep3::System::AggregatedState::undefined: return "undefined";
            case fep3::System::AggregatedState::unreachable: return "unreachable";
            case fep3::System::AggregatedState::unloaded: return "unloaded";
            case fep3::System::AggregatedState::loaded: return "loaded";
            case fep3::System::AggregatedState::initialized: return "initialized";
            case fep3::System::AggregatedState::paused: return "paused";
            case fep3::System::AggregatedState::running: return "running";
            default: break;
        }
        return "NOT RESOLVABLE";
    }

    static void dumpSystemParticipants(const fep3::System& system)
    {
        auto system_name = system.getSystemName();
        if (system_name.empty())
        {
            system_name = empty_system_name;
        }
        std::cout << system_name << " : ";
        auto participants = system.getParticipants();
        bool first_participant = true;
        for (const auto& participant : participants)
        {
            if (first_participant)
            {
                first_participant = false;
            }
            else
            {
                std::cout << ", ";
            }
            std::cout << participant.getName();
        }
        std::cout << std::endl;
    }

    class Monitor : public fep3::legacy::EventMonitor
    {
    public:
        void onStateChanged(const std::string& participant, fep3::rpc::ParticipantState state) override
        {
            std::cout << std::endl;
            std::cout << "####### state changed! #######" << std::endl;
            std::cout << "        participant: " << participant << std::endl;
            std::cout << "        state: " << state << std::endl;
        }
        void onNameChanged(const std::string& new_name, const std::string& old_name) override
        {
            std::cout << std::endl;
            std::cout << "####### name changed! #######" << std::endl;
            std::cout << "        old name: " << old_name << std::endl;
            std::cout << "        new name: " << new_name << std::endl;
        }

        std::string sevToString(fep3::logging::Severity severity_level)
        {
            if (fep3::logging::Severity::debug == severity_level)
            {
                return "[DEBUG]";
            }
            else if (fep3::logging::Severity::error == severity_level)
            {
                return "[ERROR]";
            }
            else if (fep3::logging::Severity::fatal == severity_level)
            {
                return "[FATAL]";
            }
            else if (fep3::logging::Severity::info == severity_level)
            {
                return "[INFO ]";
            }
            else if (fep3::logging::Severity::warning == severity_level)
            {
                return "[WARN ]";
            }
            return "[NONE ]";
        }

        void onLog(std::chrono::milliseconds,
            fep3::logging::Category category,
            fep3::logging::Severity severity_level,
            const std::string& participant_name,
            const std::string& logger_name, //depends on the Category ... 
            const std::string& message) override
        {
            std::cout << "    LOG " << sevToString(severity_level) << " " << logger_name 
                << "@" << participant_name  << " :" << message << std::endl << "fep> ";
        }
    };

    Monitor monitor;

    static bool discoverAllSystems(TokenIterator, TokenIterator)
    {
        auto systems = fep3::discoverAllSystems();
        for (auto& system : systems)
        {
            dumpSystemParticipants(system);
            auto system_name = system.getSystemName();
            if (system_name.empty())
            {
                //special system name -
                system_name = empty_system_name;
            }
            connected_or_discovered_systems[system_name] = std::move(system);
            //this updates for completion
            last_system_name_used = system_name;
        }
        return true;
    }

    static bool discoverSystem(TokenIterator first, TokenIterator)
    {
        auto system_name = *first;
        //this updates for completion
        last_system_name_used = system_name;
        if (system_name == empty_system_name)
        {
            system_name = "";
        }
        auto system = fep3::discoverSystem(system_name);
        dumpSystemParticipants(system);
        connected_or_discovered_systems[system.getSystemName()] = std::move(system);
        return true;
    }

    static bool setCurrentWorkingDirectory(TokenIterator first, TokenIterator)
    {
        auto path = a_util::filesystem::Path(*first);
        path.makeCanonical();
        auto result = a_util::filesystem::setWorkingDirectory(path);
        if (result == a_util::filesystem::OK)
        {
            std::cout << "working directory : " << path.toString() << std::endl;
            return true;
        }
        std::cout << "cannot set working directory to \"" << path.toString() << "\", error code = " << resolveFilesystemErrorCode(result) << std::endl;
        return false;
    }
    static bool getCurrentWorkingDirectory(TokenIterator, TokenIterator)
    {
        auto current_path = a_util::filesystem::getWorkingDirectory();
        std::cout << "working directory : " << current_path.toString() << std::endl;
        return true;
    }
    static bool connectSystem(TokenIterator first, TokenIterator)
    {
        const std::string fep_sdk_system_file = *first;
        try
        {
            fep3::System new_system = fep3::controller::connectSystem(fep_sdk_system_file);
            std::string new_system_name = new_system.getSystemName();
            //this updates for completion
            last_system_name_used = new_system_name;
            dumpSystemParticipants(new_system);
            auto success = connected_or_discovered_systems.emplace(new_system_name, std::move(new_system));
            
            if (!success.second)
            {
                std::cout << "connect system returned an already existing system name \"" << new_system_name << "\" for \""
                    << fep_sdk_system_file << "\"" << std::endl;
                return false;
            }
        }
        catch (const std::exception& e)
        {
            std::cout << "cannot connect system from file \"" << fep_sdk_system_file << "\", error: " << e.what() << std::endl;
            return false;
        }
        return true;
    }
    static bool help(TokenIterator first, TokenIterator last);

    static bool changeStateMethod(
        TokenIterator first,
        std::function<void(fep3::System& system)> call,
        const std::string& success_message,
        const std::string& failed_message)
    {
        auto it = getConnectedOrDiscoveredSystem(*first, auto_discovery_of_systems);
        if (it == connected_or_discovered_systems.end())
        {
            return false;
        }
        try
        {
            call(it->second);
        }
        catch (const std::exception& e)
        {
            std::cout << "cannot " << failed_message << " system \"" << *first << "\", error: " << e.what() << std::endl;
            return false;
        }
        std::cout << *first << " " << success_message << std::endl;
        return true;
    }

    static bool startSystem(TokenIterator first, TokenIterator)
    {
        return changeStateMethod(first,
            [](fep3::System& sys)
            { 
                sys.start(); 
            },
            "started",
            "start");
    }
    static bool stopSystem(TokenIterator first, TokenIterator)
    {
        return changeStateMethod(first,
            [](fep3::System& sys)
            {
                sys.stop();
            },
            "stopped",
            "stop");
    }
    static bool loadSystem(TokenIterator first, TokenIterator)
    {
        return changeStateMethod(first,
            [](fep3::System& sys)
            {
                sys.load();
            },
            "loaded",
            "load");
    }
    static bool unloadSystem(TokenIterator first, TokenIterator)
    {
        return changeStateMethod(first,
            [](fep3::System& sys)
            {
                sys.unload();
            },
            "unloaded",
            "unload");
    }

    static bool initializeSystem(TokenIterator first, TokenIterator)
    {
        return changeStateMethod(first,
            [](fep3::System& sys)
            {
                sys.initialize();
            },
            "initialized",
            "initialize");
    }
    static bool deinitializeSystem(TokenIterator first, TokenIterator)
    {
        return changeStateMethod(first,
            [](fep3::System& sys)
            {
                sys.deinitialize();
            },
            "deinitialized",
            "deinitialize");
    }
    static bool pauseSystem(TokenIterator first, TokenIterator)
    {
        return changeStateMethod(first,
            [](fep3::System& sys)
            {
                sys.pause();
            },
            "paused",
            "pause");
    }
    static bool shutdownSystem(TokenIterator first, TokenIterator)
    {
        return changeStateMethod(first,
            [](fep3::System& sys)
            {
                std::string name = sys.getSystemName();
                sys.shutdown();
                connected_or_discovered_systems.erase(name);
            },
            "shutdowned",
            "shutdown");
    }

    static bool startMonitoringSystem(TokenIterator first, TokenIterator)
    {
        auto it = getConnectedOrDiscoveredSystem(*first, auto_discovery_of_systems);
        if (it == connected_or_discovered_systems.end())
        {
            return false;
        }
        else
        {
            try
            {
                it->second.unregisterMonitoring(monitor);
            }
            catch (const std::exception&)
            {
                //...
            }
            it->second.registerMonitoring(monitor);
            return true;
        }
    }

    static bool stopMonitoringSystem(TokenIterator first, TokenIterator)
    {
        auto it = getConnectedOrDiscoveredSystem(*first, auto_discovery_of_systems);
        if (it == connected_or_discovered_systems.end())
        {
            return false;
        }
        else
        {
            try
            {
                it->second.unregisterMonitoring(monitor);
            }
            catch (const std::exception&)
            {
                //...
            }
            return true;
        }
    }

    static bool doParticipantStateChange(TokenIterator& first,
                                         std::function<void(fep3::RPCComponent<fep3::rpc::IRPCParticipantStateMachine>&)> change_state,
                                         const std::string& message_1,
                                         const std::string& message_2)
    {
        auto it = getConnectedOrDiscoveredSystem(*first, auto_discovery_of_systems);
        if (it == connected_or_discovered_systems.end())
        {
            return false;
        }
        std::string partname = "";
        try
        {
            partname = *std::next(first);
            auto part = it->second.getParticipant(partname);
            if (part)
            {
                auto state_machine = part.getRPCComponentProxy<fep3::rpc::arya::IRPCParticipantStateMachine>();
                if (state_machine)
                {
                    change_state(state_machine);
                }
                else
                {
                    std::cout << "participant \"" << partname << "@" << *first << "\" has no state machine" << std::endl;
                    return false;
                }
            }
            else
            {
                std::cout << "participant \"" << partname << "\" is not in system \"" << *first << "\"" << std::endl;
                return false;
            }
            //this updates for completion
            last_system_name_used = it->first;
        }
        catch (const std::exception& e)
        {
            std::cout << "cannot " << message_1 << " participant \"" << partname << "@" << *first << "\", error: " << e.what() << std::endl;
            return false;
        }
        std::cout << partname << "@" << *first << " " << message_2 << std::endl;
        return true;
    }

    static bool loadParticipant(TokenIterator first, TokenIterator)
    {
        return doParticipantStateChange(first,
            [](fep3::RPCComponent<fep3::rpc::IRPCParticipantStateMachine>& part)
            { 
            part->load();
            } , "load", "loaded" );
    }
    static bool unloadParticipant(TokenIterator first, TokenIterator)
    {
        return doParticipantStateChange(first,
            [](fep3::RPCComponent<fep3::rpc::IRPCParticipantStateMachine>& part)
        {
            part->unload();
        }, "unload", "unloaded");
    }
    static bool initializeParticipant(TokenIterator first, TokenIterator)
    {
        return doParticipantStateChange(first,
            [](fep3::RPCComponent<fep3::rpc::IRPCParticipantStateMachine>& part)
        {
            part->initialize();
        }, "initialize", "initialized");
    }
    static bool deinitializeParticipant(TokenIterator first, TokenIterator)
    {
        return doParticipantStateChange(first,
            [](fep3::RPCComponent<fep3::rpc::IRPCParticipantStateMachine>& part)
        {
            part->deinitialize();
        }, "deinitialize", "deinitialized");
    }
    static bool startParticipant(TokenIterator first, TokenIterator)
    {
        return doParticipantStateChange(first,
            [](fep3::RPCComponent<fep3::rpc::IRPCParticipantStateMachine>& part)
        {
            part->start();
        }, "start", "started");
    }
    static bool stopParticipant(TokenIterator first, TokenIterator)
    {
        return doParticipantStateChange(first,
            [](fep3::RPCComponent<fep3::rpc::IRPCParticipantStateMachine>& part)
        {
            part->stop();
        }, "stop", "stopped");
    }
    static bool pauseParticipant(TokenIterator first, TokenIterator)
    {
        return doParticipantStateChange(first,
            [](fep3::RPCComponent<fep3::rpc::IRPCParticipantStateMachine>& part) 
        {
            part->pause();
        }, "pause", "paused");
    }
    static bool shutdownParticipant(TokenIterator first, TokenIterator)
    {
        return doParticipantStateChange(first,
            [](fep3::RPCComponent<fep3::rpc::IRPCParticipantStateMachine>& part)
        {
            part->shutdown();
        }, "shutdown", "shutdowned");
    }

    static bool getSystemState(TokenIterator first, TokenIterator)
    {
        auto it = getConnectedOrDiscoveredSystem(*first, auto_discovery_of_systems);
        if (it == connected_or_discovered_systems.end())
        {
            return false;
        }
        try
        {
            auto state = it->second.getSystemState();
            std::cout << int(state._state) << " - " << resolveSystemState(state._state) 
                << " - homogeneous : " << state._homogeneous << std::endl;
        }
        catch (const std::exception& e)
        {
            std::cout << "cannot get system state for \"" << *first << "\", error: " << e.what() << std::endl;
            return false;
        }
        return true;
    }
    static bool setSystemState(TokenIterator first, TokenIterator last)
    {
        auto it = getConnectedOrDiscoveredSystem(*first, auto_discovery_of_systems);
        std::string state_string = *std::next(first);
        if (it == connected_or_discovered_systems.end())
        {
            return false;
        }
        try
        {
            auto state_to_set = getStateFromString(state_string);
            if (state_to_set == fep3::SystemAggregatedState::unreachable)
            {
                it->second.setSystemState(fep3::SystemAggregatedState::unloaded);
                return shutdownSystem(first, last);
            }
            else
            {
                it->second.setSystemState(state_to_set);
                getSystemState(first, last);
            }
            
        }
        catch (const std::exception& e)
        {
            std::cout << "cannot set system state \"" + state_string + "\" for \"" << *first << "\", error: " << e.what() << std::endl;
            return false;
        }
        return true;
    }
    static bool getParticipants(TokenIterator first, TokenIterator)
    {
        auto it = getConnectedOrDiscoveredSystem(*first, auto_discovery_of_systems);
        if (it == connected_or_discovered_systems.end())
        {
            return false;
        }
        dumpSystemParticipants(it->second);
        return true;
    }
    static bool configureSystem(TokenIterator first, TokenIterator)
    {
        auto it = getConnectedOrDiscoveredSystem(*first, auto_discovery_of_systems);
        if (it == connected_or_discovered_systems.end())
        {
            return false;
        }
        try
        {
            fep3::controller::configureSystemProperties(it->second, *std::next(first));
        }
        catch (const std::exception& e)
        {
            std::cout << "cannot set properties for \"" << *first << "\" from file \"" << *std::next(first) << "\", error: " << e.what() << std::endl;
            return false;
        }
        return true;
    }

    static bool quit(TokenIterator, TokenIterator)
    { 
        std::cout << "bye bye" << std::endl;
        exit(0);
    }

    static bool configureSystemTimingSystemTime(TokenIterator first, TokenIterator)
    {
        std::string system_name = *first;
        std::string master_name = *std::next(first);
        auto it = getConnectedOrDiscoveredSystem(*first, auto_discovery_of_systems);
        if (it == connected_or_discovered_systems.end())
        {
            return false;
        }
        try
        {
            it->second.configureTiming3ClockSyncOnlyInterpolation(master_name, "100");
        }
        catch (const std::exception& e)
        {
            std::cout << "cannot set timing for \"" << *first << "\" , error: " << e.what() << std::endl;
            return false;
        }
        return true;
    }

    static bool configureSystemTimingDiscrete(TokenIterator first, TokenIterator)
    {
        std::string system_name = *first;
        std::string master_name = *(++first);
        std::string factor = *(++first);
        std::string step_size = *(++first);
        auto it = getConnectedOrDiscoveredSystem(system_name, auto_discovery_of_systems);
        if (it == connected_or_discovered_systems.end())
        {
            return false;
        }
        try
        {
            it->second.configureTiming3DiscreteSteps(master_name, step_size, factor);
        }
        catch (const std::exception& e)
        {
            std::cout << "cannot set timing for \"" << *first << "\" , error: " << e.what() << std::endl;
            return false;
        }
        return true;
    }

    static bool configureSystemTimeNoSync(TokenIterator first, TokenIterator)
    {
        std::string system_name = *first;
        auto it = getConnectedOrDiscoveredSystem(*first, auto_discovery_of_systems);
        if (it == connected_or_discovered_systems.end())
        {
            return false;
        }
        try
        {
            //this updates for completion
            last_system_name_used = system_name;
            it->second.configureTiming3NoMaster();
        }
        catch (const std::exception& e)
        {
            std::cout << "cannot set timing for \"" << *first << "\" , error: " << e.what() << std::endl;
            return false;
        }
        return true;
    }
    
    static bool getCurrentTimingMaster(TokenIterator first, TokenIterator)
    {
        std::string system_name = *first;
        auto it = getConnectedOrDiscoveredSystem(*first, auto_discovery_of_systems);
        if (it == connected_or_discovered_systems.end())
        {
            return false;
        }
        try
        {
            auto masters = it->second.getCurrentTimingMasters();
            auto masters_string = a_util::strings::join(masters, ",");
            std::cout << "timing masters: " << masters_string << std::endl;
        }
        catch (const std::exception& e)
        {
            std::cout << "cannot get timing masters for \"" << *first << "\" , error: " << e.what() << std::endl;
            return false;
        }
        return true;
    }

    static bool enableAutoDiscovery(TokenIterator, TokenIterator)
    {
        auto_discovery_of_systems = true;
        std::cout << "auto_discovery: enabled" << std::endl;
        return true;
    }

    static bool disableAutoDiscovery(TokenIterator, TokenIterator)
    {
        auto_discovery_of_systems = false;
        std::cout << "auto_discovery: disabled" << std::endl;
        return true;
    }


    static bool getParticipantState(TokenIterator first, TokenIterator)
    {
        auto it = getConnectedOrDiscoveredSystem(*first, auto_discovery_of_systems);
        std::string participant_name = *(++first);
        if (it == connected_or_discovered_systems.end())
        {
            return false;
        }
        try
        {
            auto part = it->second.getParticipant(participant_name);
            if (part)
            {
                auto state_machine = part.getRPCComponentProxy<fep3::rpc::arya::IRPCParticipantStateMachine>();
                if (state_machine)
                {
                    auto value = state_machine->getState();
                    std::cout << int(value) << " - " << resolveSystemState(value) << std::endl;
                }
                else
                {
                    std::cout << "participant \"" << participant_name << "@" << *first << "\" has no state machine" << std::endl;
                    return false;
                }
            }
            else
            {
                std::cout << "participant \"" << participant_name << "\" is not in system \"" << *first << "\"" << std::endl;
                return false;
            }
        }
        catch (const std::exception& e)
        {
            std::cout << "cannot get participant state for participant \"" + participant_name  << "@" << *first << "\", error: " << e.what() << std::endl;
            return false;
        }
        return true;
    }

    static bool setParticipantState(TokenIterator first, TokenIterator last)
    {
        auto it = getConnectedOrDiscoveredSystem(*first, auto_discovery_of_systems);
        std::string system_name = *first;
        std::string participant_name = *(++first);
        std::string state_string = *(++first);
        if (it == connected_or_discovered_systems.end())
        {
            return false;
        }
        try
        {
            auto part = it->second.getParticipant(participant_name);
            if (part)
            {
                //create a second temporary system with only one participant
                fep3::System system_temp(system_name);
                system_temp.add(participant_name);

                auto state_to_set = getStateFromString(state_string);
                if (state_to_set == fep3::SystemAggregatedState::unreachable)
                {
                    system_temp.setSystemState(fep3::SystemAggregatedState::unloaded);
                    system_temp.shutdown();
                    std::cout << int(state_to_set) << " - " << resolveSystemState(state_to_set) << std::endl;
                }
                else
                {
                    system_temp.setSystemState(state_to_set);
                    std::cout << int(state_to_set) << " - " << resolveSystemState(state_to_set) << std::endl;
                }
            }
            else
            {
                std::cout << "participant \"" << participant_name << "\" is not in system \"" << system_name << "\"" << std::endl;
                return false;
            }
        }
        catch (const std::exception& e)
        {
            std::cout << "cannot set participant state" + state_string + "for participant \"" + participant_name << "@" << system_name << "\", error: " << e.what() << std::endl;
            return false;
        }
        return true;
    }

    bool getRPCObjectsParticipant(TokenIterator first, TokenIterator last)
    {
        std::string system_name = *(first);
        auto it = getConnectedOrDiscoveredSystem(system_name, auto_discovery_of_systems);
        std::string participant_name = *std::next(first);
        if (it == connected_or_discovered_systems.end())
        {
            return false;
        }
        try
        {
            auto part = it->second.getParticipant(participant_name);
            if (part)
            {
                auto info = part.getRPCComponentProxy<fep3::rpc::arya::IRPCParticipantInfo>();
                if (info)
                {
                    auto value = info->getRPCComponents();
                    std::cout << a_util::strings::join(value, ",") << std::endl;
                }
                else
                {
                    std::cout << "participant \"" << participant_name << "@" << system_name << "\" has no RPC Info" << std::endl;
                    return false;
                }
            }
            else
            {
                std::cout << "participant \"" << participant_name << "\" is not in system \"" << system_name << "\"" << std::endl;
                return false;
            }
        }
        catch (const std::exception& e)
        {
            std::cout << "cannot get participant state for participant \"" + participant_name << "@" << system_name << "\", error: " << e.what() << std::endl;
            return false;
        }
        return true;
    }

    bool getRPCObjectIIDSParticipant(TokenIterator first, TokenIterator last)
    {
        auto it = getConnectedOrDiscoveredSystem(*first, auto_discovery_of_systems);
        std::string system_name = *(first);
        std::string participant_name = *(++first);
        std::string object_name = *(++first);
        if (it == connected_or_discovered_systems.end())
        {
            return false;
        }
        try
        {
            auto part = it->second.getParticipant(participant_name);
            if (part)
            {
                auto info = part.getRPCComponentProxy<fep3::rpc::arya::IRPCParticipantInfo>();
                if (info)
                {
                    try
                    {
                        auto value = info->getRPCComponentIIDs(object_name);
                        std::cout << a_util::strings::join(value, ",") << std::endl;
                    }
                    catch (const std::exception&)
                    {
                        std::cout << "participant \"" << participant_name << "@" << system_name << "\" IID info can not be retrieved " << std::endl;
                        return false;
                    }

                }
                else
                {
                    std::cout << "participant \"" << participant_name << "@" << system_name << "\" has no RPC Info" << std::endl;
                    return false;
                }
            }
            else
            {
                std::cout << "participant \"" << participant_name << "\" is not in system \"" << system_name << "\"" << std::endl;
                return false;
            }
        }
        catch (const std::exception& e)
        {
            std::cout << "cannot get participant state for participant \"" + participant_name << "@" << system_name << "\", error: " << e.what() << std::endl;
            return false;
        }
        return true;
    }

    bool getRPCObjectDefinitionParticipant(TokenIterator first, TokenIterator last)
    {
        auto it = getConnectedOrDiscoveredSystem(*first, auto_discovery_of_systems);
        std::string system_name = *(first);
        std::string participant_name = *(++first);
        std::string object_name = *(++first);
        std::string intf_name = *(++first);
        if (it == connected_or_discovered_systems.end())
        {
            return false;
        }
        try
        {
            auto part = it->second.getParticipant(participant_name);
            if (part)
            {
                auto info = part.getRPCComponentProxy<fep3::rpc::arya::IRPCParticipantInfo>();
                if (info)
                {
                    try
                    {
                        auto value = info->getRPCComponentInterfaceDefinition(object_name, intf_name);
                        std::cout << value << std::endl;
                    }
                    catch (const std::exception& e)
                    {
                        std::cout << "participant \"" << participant_name << "@" << system_name << "\" IID info can not be retrieved " << std::endl;
                        return false;
                    }

                }
                else
                {
                    std::cout << "participant \"" << participant_name << "@" << system_name << "\" has no RPC Info" << std::endl;
                    return false;
                }
            }
            else
            {
                std::cout << "participant \"" << participant_name << "\" is not in system \"" << system_name << "\"" << std::endl;
                return false;
            }
        }
        catch (const std::exception& e)
        {
            std::cout << "cannot get participant state for participant \"" + participant_name << "@" << system_name << "\", error: " << e.what() << std::endl;
            return false;
        }
        return true;
    }


    std::vector<ControlCommand> Commands = {
    { "exit", "quits this program", quit, {}, 0u },
    { "quit", "quits this program", quit, {}, 0u },
    { "discoverAllSystems", "discovers all systems and registers logging monitor for them", discoverAllSystems, {}, 0u },
    { "discoverSystem", "discover one system with the given name and register the logging monitor for them", discoverSystem, { {"system name", noCompletion} }, 0u },
    { "setCurrentWorkingDirectory", "changes the current working dir of this fep_control instance", setCurrentWorkingDirectory, { {"directory name", noCompletion} }, 0u },
    { "getCurrentWorkingDirectory", "prints the current working dir of this fep_control instance", getCurrentWorkingDirectory, {}, 0u },
    { "connectSystem", "connects the given system", connectSystem, { {"FEP SDK system descriptor (xml) file name", localFilesCompletion} }, 0u },
    { "help", "prints out the description of the commands", help, { {"command name", commandNameCompletion } }, 1u },
    { "loadSystem", "loads the given system", loadSystem, { {"system name", connectedSystemsCompletion} }, 0u },
    { "unloadSystem", "unloads the given system", unloadSystem, { {"system name", connectedSystemsCompletion} }, 0u },
    { "initializeSystem", "initializes the given system", initializeSystem, { {"system name", connectedSystemsCompletion} }, 0u },
    { "deinitializeSystem", "deinitializes the given system", deinitializeSystem, { {"system name", connectedSystemsCompletion} }, 0u },
    { "startSystem", "starts the given system", startSystem, { {"system name", connectedSystemsCompletion} }, 0u },
    { "stopSystem", "stops the given system", stopSystem, { {"system name", connectedSystemsCompletion} }, 0u },
    { "pauseSystem", "pauses the given system", pauseSystem, { {"system name", connectedSystemsCompletion} }, 0u },
    { "shutdownSystem", "shutdown the given system", shutdownSystem, { {"system name", connectedSystemsCompletion} }, 0u },
    { "startMonitoringSystem", "monitor logging messages of the given system", startMonitoringSystem, { {"system name", connectedSystemsCompletion} }, 0u },
    { "stopMonitoringSystem", "stop monitoring logging messages of the given system", stopMonitoringSystem, { {"system name", connectedSystemsCompletion} }, 0u },
    { "loadParticipant", "loads the given participant", loadParticipant, { {"system name", connectedSystemsCompletion}, {"participant name", connectedParticipantsCompletion}}, 0u },
    { "unloadParticipant", "unloads the given participant", unloadParticipant, { {"system name", connectedSystemsCompletion}, {"participant name", connectedParticipantsCompletion} }, 0u },
    { "initializeParticipant", "initializes the given participant", initializeParticipant, { {"system name", connectedSystemsCompletion}, {"participant name", connectedParticipantsCompletion} }, 0u },
    { "deinitializeParticipant", "deinitializes the given participant", deinitializeParticipant, { {"system name", connectedSystemsCompletion}, {"participant name", connectedParticipantsCompletion} }, 0u },
    { "startParticipant", "starts the given participant", startParticipant, { {"system name", connectedSystemsCompletion}, {"participant name", connectedParticipantsCompletion} }, 0u },
    { "stopParticipant", "stops the given participant", stopParticipant, { {"system name", connectedSystemsCompletion}, {"participant name", connectedParticipantsCompletion} }, 0u },
    { "pauseParticipant", "pauses the given participant", pauseParticipant, { {"system name", connectedSystemsCompletion}, {"participant name", connectedParticipantsCompletion} }, 0u },
    { "getParticipantRPCObjects", "retrieve the RPC Objects of the given participant", getRPCObjectsParticipant, { {"system name", connectedSystemsCompletion}, {"participant name", connectedParticipantsCompletion} }, 0u },
    { "getParticipantRPCObjectIIDs", "retrieve the RPC IIDs of a concrete RPC Objects of the given participant", getRPCObjectIIDSParticipant, { {"system name", connectedSystemsCompletion}, {"participant name", connectedParticipantsCompletion}, {"object name", noCompletion}  }, 0u },
    { "getParticipantRPCObjectIIDDefinition", "retrieve the RPC Definition of an IID of a concrete RPC Objects of the given participant", getRPCObjectDefinitionParticipant, { {"system name", connectedSystemsCompletion}, {"participant name", connectedParticipantsCompletion}, {"object name", noCompletion}, {"interface id", noCompletion}  }, 0u },
    { "shutdownParticipant", "shutdown the given participant", shutdownParticipant, { {"system name", connectedSystemsCompletion}, {"participant name", connectedParticipantsCompletion} }, 0u },
    { "getSystemState", "retrieves the given system", getSystemState, { {"system name", connectedSystemsCompletion} }, 0u },
    { "setSystemState", "sets the given system state", setSystemState, { {"system name", connectedSystemsCompletion}, {"system state", possibleSystemsStateCompletion} }, 0u },
    { "getParticipantState", "retrieves the given participants state", getParticipantState, { {"system name", connectedSystemsCompletion}, {"participant name", connectedParticipantsCompletion} }, 0u },
    { "setParticipantState", "sets the given participants system state", setParticipantState, { {"system name", connectedSystemsCompletion}, {"participant name", connectedParticipantsCompletion} , {"particiapnt state", possibleSystemsStateCompletion} }, 0u },
    { "getParticipants", "lists the participants of the given system", getParticipants, { {"system name", connectedSystemsCompletion} }, 0u},
    { "configureSystem", "configures the given system", configureSystem, { {"system name", connectedSystemsCompletion}, {"FEP system properties file", localFilesCompletion} }, 0u },
    { "configureTiming3SystemTime", "configures the given system for timing System Time (Sync only to the master)", configureSystemTimingSystemTime, { {"system name", connectedSystemsCompletion}, {"master participant name", connectedParticipantsCompletion} }, 0u },
    { "configureTiming3DiscreteTime", "configures the given system for timing Discrete Time (for AFAP use 0.0 as factor)", configureSystemTimingDiscrete, { {"system name", connectedSystemsCompletion}, {"master participant name", connectedParticipantsCompletion}, {"factor", noCompletion} , {"step size (in ms)", noCompletion} }, 0u },
    { "configureTiming3NoSync", "resets the timing configuration", configureSystemTimeNoSync, { {"system name", connectedSystemsCompletion} } , 0u },
    { "getCurrentTimingMaster", "retrieves the timing master from the systems participants", getCurrentTimingMaster, { {"system name", connectedSystemsCompletion} } , 0u },
    { "enableAutoDiscovery", "enable the auto discovery for commands on systems", enableAutoDiscovery, {}, 0u },
    { "disableAutoDiscovery", "disable the auto discovery for commands on systems", disableAutoDiscovery, {}, 0u }
    };

    static inline std::vector<ControlCommand>::const_iterator findCommand(const std::string& command_candidate)
    {
        return std::find_if(Commands.begin(), Commands.end(),
            [&command_candidate](const ControlCommand& cmd) { return cmd._name == command_candidate; });
    }

    std::vector<std::string> commandNameCompletion(const std::string& word_prefix)
    {
        std::vector<std::string> completions;
        for (const auto& cmd : Commands)
        {
            const std::string& cmd_name = cmd._name;
            if (cmd_name.compare(0u, word_prefix.size(), word_prefix) == 0)
            {
                completions.push_back(cmd_name);
            }
        }
        return completions;
    }

    static bool help(TokenIterator first, TokenIterator last)
    {
        if (first == last)
        {
            for (const auto& cmd : Commands)
            {
                std::cout << cmd._name << " : " << cmd._description << std::endl;
            }
        }
        else
        {
            auto it = findCommand(*first);
            if (it == Commands.end())
            {
                std::cout << "no such command as \"" << *first << "\"" << std::endl;
                return false;
            }
            std::cout << *first;
            for (const auto& argument : it->_arguments)
            {
                std::cout << " <" << argument._description << '>';
            }
            std::cout << " : " << it->_description << std::endl;
        }
        return true;
    }
}

static int processCommandline(const std::vector<std::string>& command_line)
{
    assert(!command_line.empty());
    auto it = findCommand(command_line[0]);
    if (it == Commands.end())
    {
        std::cout << "Invalid command \"" << command_line[0] << "\", use \"help\" for valid commands" << std::endl;
        return -2;
    }
    if (command_line.size() > (*it)._arguments.size() + 1u || command_line.size() < (*it)._arguments.size() + 1u - (*it)._last_optional_parameters)
    {
        std::cout << "Invalid number of arguments for \"" << command_line[0] << "\" (" << command_line.size() - 1u << " instead of ";
        if ((*it)._last_optional_parameters == 0u)
        {
            std::cout << (*it)._arguments.size();
        }
        else
        {
            std::cout << (*it)._arguments.size() - (*it)._last_optional_parameters << ".." << (*it)._arguments.size();
        }
        std::cout << "), use \"help\" for more information" << std::endl;
        return -3;
    }
    return (*it)._action(command_line.begin() + 1, command_line.end()) ? 0 : 1;
}

static std::vector<std::string> commandCompletion(const std::string& input)
{
    std::vector<std::string> input_tokens = parseLine(input);
    std::vector<std::string> completions;

    if (input_tokens.empty() || std::isspace(input.back()))
    {
        input_tokens.emplace_back();
    }

    if (input_tokens.size() == 1u)
    {
        return commandNameCompletion(input_tokens[0]);
    }
    else
    {
        auto it = findCommand(input_tokens[0]);
        if (it != Commands.end())
        {
            size_t index_in_args = input_tokens.size() - 2u;
            if (index_in_args < (*it)._arguments.size())
            {
                auto completion_list = (*it)._arguments[index_in_args]._completion(input_tokens.back());
                if (!completion_list.empty())
                {
                    input_tokens.pop_back();
                    std::string command_prefix = a_util::strings::join(input_tokens, " ") + " ";
                    for (const std::string& word_completion : completion_list)
                    {
                        completions.push_back(command_prefix + word_completion);
                    }
                    return completions;
                }
            }
        }
    }
    
    return completions;
}

static void interactiveLoop()
{
    line_noise::setCallback(commandCompletion);

    std::string line;
    while (line_noise::readLine(line))
    {
        auto lineTokens = parseLine(line);
        if (lineTokens.empty())
        {
            continue;
        }
        line_noise::addToHistory(line);
        processCommandline(lineTokens);
    }
}

static void printWelcomeMessage()
{
    std::cout << "******************************************************************\n";
    std::cout << "* Welcome to FEP Control(c) 2020 AUDI AG                         *\n";
    std::cout << "*  use help to print help                                        *\n";
    std::cout << "******************************************************************" << std::endl;
}

int parseAndExecuteCommandline(int argc, char *argv[])
{
    static const std::vector<std::string> executeOption = { "-e", "--execute" };
    static const std::vector<std::string> autoDiscoveryOption = { "-ad", "--auto_discovery" };
    assert(argc >= 1);
    if (std::find(autoDiscoveryOption.begin(), autoDiscoveryOption.end(), argv[0]) != autoDiscoveryOption.end())
    {
        auto_discovery_of_systems = true;
        assert(argc >= 2);
        if (std::find(executeOption.begin(), executeOption.end(), argv[1]) != executeOption.end())
        {
            return processCommandline(std::vector<std::string>(argv + 2, argv + argc));
        }
    }
    else if (std::find(executeOption.begin(), executeOption.end(), argv[0]) != executeOption.end())
    {
        return processCommandline(std::vector<std::string>(argv + 1, argv + argc));
    }
    std::cerr << "invalid commandline, use: fep_control --auto_discovery --execute <execute_command>" << std::endl;
    std::cerr << "                     or:  fep_control --execute <execute_command>" << std::endl;
    std::cerr << "                     or:  fep_control -ad -e <execute_command>" << std::endl;
    return -1;
}

int main(int argc, char *argv[])
{
    if (argc > 1)
    {
        return parseAndExecuteCommandline(argc - 1, argv + 1); // shift by one
    }
	printWelcomeMessage();
    interactiveLoop();

    //we clear that here before any static variable ist closed 
    connected_or_discovered_systems.clear();

    return 0;
}
