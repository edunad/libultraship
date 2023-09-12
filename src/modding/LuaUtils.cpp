
#include "LuaUtils.h"

#include <filesystem>

namespace LUS {
sol::object LuaUtils::JsonToLuaObject(const nlohmann::json j, sol::state_view& lua) {
    if (j.is_null()) {
        return sol::nil;
    } else if (j.is_boolean()) {
        return sol::make_object<bool>(lua, j.get<bool>());
    } else if (j.is_number_integer()) {
        return sol::make_object<int>(lua, j.get<int>());
    } else if (j.is_number_unsigned()) {
        return sol::make_object<unsigned int>(lua, j.get<unsigned int>());
    } else if (j.is_number_float()) {
        return sol::make_object<double>(lua, j.get<double>());
    } else if (j.is_string()) {
        return sol::make_object<char*>(lua, j.get<std::string>().c_str());
    } else if (j.is_object()) {
        auto obj = lua.create_table();
        for (nlohmann::json::const_iterator it = j.begin(); it != j.end(); ++it) {
            obj[it.key().c_str()] = JsonToLuaObject(*it, lua);
        }

        return obj.as<sol::object>();
    } else if (j.is_array()) {
        auto obj = lua.create_table();
        unsigned long i = 1;

        for (const auto& it : j) {
            obj[i++] = JsonToLuaObject(it, lua);
        }

        return obj;
    }

    return sol::nil;
}

sol::object LuaUtils::JsonToLuaObject(const nlohmann::json j, sol::state& lua) {
    // NOLINTBEGIN(cppcoreguidelines-slicing)
    sol::state_view view = lua;
    // NOLINTEND(cppcoreguidelines-slicing)
    return JsonToLuaObject(j, view);
}

nlohmann::json LuaUtils::LuaToJsonObject(const sol::object& l, bool filterNull) {
    switch (l.get_type()) {
        case sol::type::nil:
            return {};
        case sol::type::boolean:
            return l.as<bool>();
        case sol::type::number:
            if (l.is<int>()) {
                return l.as<int>();
            }

            if (l.is<double>()) {
                return l.as<double>();
            }

            if (l.is<float>()) {
                return l.as<float>();
            }
        case sol::type::string:
            return l.as<std::string>();
        case sol::type::table: {
            bool isArray = true;
            int index = 0;

            for (auto& entry : l.as<sol::table>()) {
                if (entry.first.get_type() != sol::type::number || entry.first.as<int>() != ++index) {
                    isArray = false;
                    break;
                }
            }

            if (isArray) {
                auto arr = nlohmann::json::array();
                for (auto& entry : l.as<sol::table>()) {
                    auto val = LuaToJsonObject(entry.second);
                    if (filterNull && val.empty()) {
                        continue;
                    }

                    arr.push_back(val);
                }

                return arr;
            } else {
                auto obj = nlohmann::json::object();
                for (auto& entry : l.as<sol::table>()) {
                    auto type = entry.first.get_type();
                    if (type != sol::type::number && type != sol::type::string) {
                        continue;
                    }

                    auto val = LuaToJsonObject(entry.second);
                    if (filterNull && val.empty()) {
                        continue;
                    }

                    if (type == sol::type::number) {
                        obj[std::to_string(entry.first.as<uint64_t>())] = val;
                    } else if (type == sol::type::string) {
                        obj[entry.first.as<std::string>()] = val;
                    }
                }

                return obj;
            }
        }
        default:
            return {};
    }
}

std::string Replace(std::string str, const std::string& find, const std::string& replace) {
    if (str.empty()) {
        return str;
    }

    auto fnd = str.find(find);
    if (fnd == std::string::npos) {
        return str;
    }

    str.replace(fnd, find.size(), replace);
    return str;
}

// @/ == Root content
// @cats/ == `cats` mod
// normal_path == current mod
std::string LuaUtils::GetContent(const std::filesystem::path& path, const std::filesystem::path& modPath) {
    if (path.empty()) {
        return modPath.generic_string(); // Invalid path
    }

    auto pth = path.generic_string();
    if (pth.starts_with("mods/")) {
        return modPath.generic_string(); // Already has the mod
    }

    pth = Replace(pth, "\\", "/");
    pth = Replace(pth, "./", "");
    pth = Replace(pth, "../", "");

    // content/blabalba.png = my current mod
    if (!modPath.empty() && pth.front() != '@') {
        return std::filesystem::path(modPath.generic_string() + "/" + pth)
            .string();                 // Becomes mods/mymod/content/blabalba.png
    } else if (pth.front() == '@') {
        auto slashPos = pth.find("/"); // Find the first /
        std::string cleanPath = pth.substr(slashPos + 1);

        // @/textures/blabalba.png = c++ content
        if (pth.rfind("@/", 0) == 0) { // C++
            return std::filesystem::path("content/" + cleanPath).string();
        } else {                       // @otherMod/textures/blabalba.png = @othermod content
            return std::filesystem::path(pth.substr(1, slashPos - 1) + "/" + cleanPath).string();
        }
    }

    return pth;
}

} // namespace LUS
