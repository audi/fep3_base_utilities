#pragma once
#include <string>
#include <cassert>
#include <algorithm>
#include <cctype>

static std::string quoteFilenameIfNecessary(const std::string& file_name)
{
    assert(!file_name.empty());
    if (file_name[0] == '"' || std::find_if(file_name.begin(), file_name.end(), isspace) == file_name.end())
    {
        return file_name;
    }
    return "\"" + file_name + "\"";
}
