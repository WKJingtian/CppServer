#include "pch.h"
#include "Config/LuaConfigLoader.h"

#include <algorithm>
#include <cmath>
#include <limits>

extern "C" {
#include "lua/include/lua.h"
#include "lua/include/lauxlib.h"
#include "lua/include/lualib.h"
}

namespace
{
    void OpenSafeLibs(lua_State* L)
    {
        luaL_openlibs(L);

        lua_pushnil(L);
        lua_setglobal(L, "dofile");
        lua_pushnil(L);
        lua_setglobal(L, "loadfile");
        lua_pushnil(L);
        lua_setglobal(L, "load");
#if LUA_VERSION_NUM <= 501
        lua_pushnil(L);
        lua_setglobal(L, "loadstring");
#endif
        lua_pushnil(L);
        lua_setglobal(L, "require");
        lua_pushnil(L);
        lua_setglobal(L, "package");
        lua_pushnil(L);
        lua_setglobal(L, "io");
        lua_pushnil(L);
        lua_setglobal(L, "os");
        lua_pushnil(L);
        lua_setglobal(L, "debug");
        lua_pushnil(L);
        lua_setglobal(L, "coroutine");
    }

    std::string WideToUtf8(const std::wstring& input)
    {
        if (input.empty())
            return std::string();

        int size = WideCharToMultiByte(CP_UTF8, 0, input.c_str(),
            static_cast<int>(input.size()), nullptr, 0, nullptr, nullptr);
        if (size <= 0)
            return std::string();

        std::string result(size, '\0');
        WideCharToMultiByte(CP_UTF8, 0, input.c_str(),
            static_cast<int>(input.size()), result.data(), size, nullptr, nullptr);
        return result;
    }

    std::string JoinPath(const std::string& base, const std::string& relative)
    {
        std::string normalized = relative;
        std::replace(normalized.begin(), normalized.end(), '/', '\\');

        if (base.empty())
            return normalized;

        char last = base.back();
        if (last == '\\' || last == '/')
            return base + normalized;

        return base + "\\" + normalized;
    }

    std::string GetLuaString(lua_State* L, int index)
    {
        size_t length = 0;
        const char* value = lua_tolstring(L, index, &length);
        if (!value)
            return std::string();

        return std::string(value, length);
    }

    int ToAbsoluteIndex(lua_State* L, int index)
    {
#if LUA_VERSION_NUM >= 502
        return lua_absindex(L, index);
#else
        if (index > 0 || index <= LUA_REGISTRYINDEX)
            return index;
        return lua_gettop(L) + index + 1;
#endif
    }

    bool TryGetInt(lua_State* L, int index, int* outValue, std::string& error, const std::string& key)
    {
        if (!lua_isnumber(L, index))
            return false;

#if LUA_VERSION_NUM >= 503
        if (lua_isinteger(L, index))
        {
            lua_Integer value = lua_tointeger(L, index);
            if (value < std::numeric_limits<int>::min() || value > std::numeric_limits<int>::max())
            {
                error = "config value out of int range: " + key;
                return false;
            }
            *outValue = static_cast<int>(value);
            return true;
        }
#endif

        lua_Number rawValue = lua_tonumber(L, index);
        double value = static_cast<double>(rawValue);
        if (!std::isfinite(value))
        {
            error = "config value is not finite: " + key;
            return false;
        }

        double intPart = 0.0;
        double fracPart = std::modf(value, &intPart);
        if (std::fabs(fracPart) > 0.0)
        {
            error = "config value must be int: " + key;
            return false;
        }

        if (intPart < std::numeric_limits<int>::min() || intPart > std::numeric_limits<int>::max())
        {
            error = "config value out of int range: " + key;
            return false;
        }

        *outValue = static_cast<int>(intPart);
        return true;
    }

    bool FlattenTable(lua_State* L, int index, const std::string& prefix,
        LuaConfig::ConfigMap& out, std::string& error)
    {
        index = ToAbsoluteIndex(L, index);
        lua_pushnil(L);
        while (lua_next(L, index) != 0)
        {
            if (lua_type(L, -2) != LUA_TSTRING)
            {
                error = "config table contains non-string key";
                lua_pop(L, 1);
                return false;
            }

            std::string key = GetLuaString(L, -2);
            if (key.empty())
            {
                error = "config table contains empty key";
                lua_pop(L, 1);
                return false;
            }

            std::string fullKey = prefix.empty() ? key : prefix + "." + key;

            if (lua_istable(L, -1))
            {
                if (!FlattenTable(L, -1, fullKey, out, error))
                {
                    lua_pop(L, 1);
                    return false;
                }
            }
            else if (lua_type(L, -1) == LUA_TSTRING)
            {
                std::string value = GetLuaString(L, -1);
                if (!out.emplace(fullKey, value).second)
                {
                    error = "duplicate config key: " + fullKey;
                    lua_pop(L, 1);
                    return false;
                }
            }
            else if (lua_isnumber(L, -1))
            {
                int value = 0;
                if (!TryGetInt(L, -1, &value, error, fullKey))
                {
                    lua_pop(L, 1);
                    return false;
                }
                if (!out.emplace(fullKey, value).second)
                {
                    error = "duplicate config key: " + fullKey;
                    lua_pop(L, 1);
                    return false;
                }
            }
            else
            {
                const char* typeName = lua_typename(L, lua_type(L, -1));
                error = "unsupported value type for key " + fullKey +
                    ": " + (typeName ? typeName : "unknown");
                lua_pop(L, 1);
                return false;
            }

            lua_pop(L, 1);
        }

        return true;
    }
}

namespace LuaConfig
{
    LoadResult LoadFromFile(const std::string& path)
    {
        LoadResult result;

        lua_State* L = luaL_newstate();
        if (!L)
        {
            result.error = "luaL_newstate failed";
            return result;
        }

        OpenSafeLibs(L);

        int loadStatus = luaL_loadfile(L, path.c_str());
        if (loadStatus != LUA_OK)
        {
            const char* message = lua_tostring(L, -1);
            result.error = message ? message : "luaL_loadfile failed";
            lua_close(L);
            return result;
        }

        int callStatus = lua_pcall(L, 0, 1, 0);
        if (callStatus != LUA_OK)
        {
            const char* message = lua_tostring(L, -1);
            result.error = message ? message : "lua_pcall failed";
            lua_close(L);
            return result;
        }

        if (!lua_istable(L, -1))
        {
            result.error = "config script must return a table";
            lua_close(L);
            return result;
        }

        if (!FlattenTable(L, -1, std::string(), result.values, result.error))
        {
            lua_close(L);
            return result;
        }

        lua_close(L);
        return result;
    }

    LoadResult LoadFromExeDir(const std::string& relativePath)
    {
        std::string exeDir = GetExecutableDir();
        if (exeDir.empty())
        {
            LoadResult result;
            result.error = "failed to resolve executable directory";
            return result;
        }

        return LoadFromFile(JoinPath(exeDir, relativePath));
    }

    std::string GetExecutableDir()
    {
        std::wstring path(MAX_PATH, L'\0');
        DWORD size = GetModuleFileNameW(nullptr, path.data(), static_cast<DWORD>(path.size()));
        if (size == 0)
            return std::string();

        while (size == path.size())
        {
            path.resize(path.size() * 2);
            size = GetModuleFileNameW(nullptr, path.data(), static_cast<DWORD>(path.size()));
            if (size == 0)
                return std::string();
        }

        path.resize(size);
        size_t pos = path.find_last_of(L"\\/");
        if (pos == std::wstring::npos)
            return WideToUtf8(path);

        path.resize(pos);
        return WideToUtf8(path);
    }
}
