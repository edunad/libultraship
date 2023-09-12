#pragma once
#include "LuaUtils.h"

#include <sol/sol.hpp>

#include <filesystem>
#include <string>

namespace LUS {
class Scripting;

class Mod {
  protected:
    sol::environment mEnvironment;
    sol::table mModTable;

    std::filesystem::path mFolder;
    std::string mId;

  public:
    // std::function<void(std::filesystem::path)> onLUAReload = nullptr;

    Mod(std::string id, std::filesystem::path folderName);
    Mod(const Mod&) = default;
    Mod(Mod&&) = delete;
    Mod& operator=(const Mod&) = default;
    Mod& operator=(Mod&&) = delete;
    virtual ~Mod();

    virtual void Init();
    virtual bool Load();

    // UTILS ----
    [[nodiscard]] virtual const std::string& GetId() const;
    [[nodiscard]] virtual const std::string GetEntryFilePath() const;
    [[nodiscard]] virtual const std::filesystem::path& GetFolder() const;

    virtual sol::environment& GetEnvironment();
    // -----

    template <typename... CallbackArgs> void Call(const std::string& name, CallbackArgs&&... args) {
        sol::function func = mModTable[name];
        if (func.get_type() != sol::type::function) {
            return;
        }

        LuaUtils::RunCallback(func, mModTable, std::forward<CallbackArgs>(args)...);
    }
};
} // namespace LUS
