#pragma once

#include <string>
#include <unordered_map>
#include <variant>

namespace LuaConfig
{
    using ConfigValue = std::variant<int, std::string>;
    using ConfigMap = std::unordered_map<std::string, ConfigValue>;

    struct LoadResult
    {
        ConfigMap values;
        std::string error;

        bool Ok() const { return error.empty(); }
    };

    // Loads a Lua file that returns a table and flattens it into dot-separated keys.
    LoadResult LoadFromFile(const std::string& path);
    LoadResult LoadFromExeDir(const std::string& relativePath);
    std::string GetExecutableDir();
}
