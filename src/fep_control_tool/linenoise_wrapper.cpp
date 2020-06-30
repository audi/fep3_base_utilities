/**
* @file
*
* @copyright
* @verbatim
* Copyright @ 2020 AUDI AG. All rights reserved.
*
* This Source Code Form is subject to the terms of the Mozilla
* Public License, v. 2.0. If a copy of the MPL was not distributed
* with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
*
* If it is not possible or desirable to put the notice in a particular file, then
* You may include the notice in a location (such as a LICENSE file in a
* relevant directory) where a recipient would be likely to look for such a notice.
*
* You may add additional accurate notices of copyright ownership.
* @endverbatim
*/

#include "linenoise_wrapper.h"
extern "C"
{
#include "linenoise/linenoise.h"
}

#include <cstring>

bool line_noise::readLine(std::string& line)
{
    char *strLine = linenoise("fep> ");
    if (strLine == nullptr)
    {
        return false;
    }
    line = strLine;
    free(strLine);
    return true;
}

namespace
{
    line_noise::CallBackFunction cpp_callback_function;
}

static void callback(const char * input, linenoiseCompletions * output)
{
    std::vector<std::string> completionList = cpp_callback_function(input);
    output->len = completionList.size();
    output->cvec = output->len == 0u ? nullptr : reinterpret_cast<char**>(malloc(sizeof(char*) * output->len));
    for (size_t i = 0u; i < output->len; ++i)
    {
        output->cvec[i] = strdup(completionList[i].c_str());
    }
}

void line_noise::setCallback(CallBackFunction callback_function)
{
    static bool callback_initialized = false;

    cpp_callback_function = callback_function;
    if (!callback_initialized)
    {
        linenoiseSetCompletionCallback(callback);
    }
    callback_initialized = true;
}

void line_noise::addToHistory(const std::string& line)
{
    linenoiseHistoryAdd(line.c_str());
}
