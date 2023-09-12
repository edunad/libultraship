#pragma once

#include "Mod.h"
#include "Plugin.h"
#include <sol/sol.hpp>

namespace LUS {
template <typename T>
concept IsLuaType = requires(sol::state lua) {
    { T::registerLua(lua) };
};

class Scripting {
  protected:
    static std::unordered_map<std::string, std::unique_ptr<Mod>> mods;
    static std::unique_ptr<sol::state> lua;

    static std::vector<std::unique_ptr<Plugin>> plugins;

  public:
    // EVENTS ----
    static std::function<void()> onRegisterTypes;
    static std::function<void(Mod*)> onRegisterGlobals;
    static std::function<void(Mod*)> onLoadExtensions;
    static std::function<void(Mod*)> onModHotReload;
    // -------

    static bool initialized;

    static void Init(int hotReloadMs = 0);
    static void Shutdown();

    // LOAD -----
    static void LoadLibraries();
    static void LoadTypes();
    static void LoadLuaExtensions(Mod* mod);
    static void LoadGlobals(Mod* mod);

    static void Load();
    static bool LoadLuaFile(const std::string& path, const sol::environment& env);
    //------

    // UTILS -----
    [[nodiscard]] static sol::state& GetLua();
    [[nodiscard]] static const std::unordered_map<std::string, std::unique_ptr<Mod>>& GetMods();
    [[nodiscard]] static const std::vector<std::string> GetModsIds();
    //------

    // PLUGINS ---
    template <typename T = Plugin, typename... CallbackArgs> static void RegisterPlugin(CallbackArgs&&... args) {
        auto plugin = std::make_unique<T>(std::forward<CallbackArgs>(args)...);

        SPDLOG_INFO("Registered lua plugin '{}'", typeid(T).name());
        plugins.push_back(std::move(plugin));
    }
    // -----

    template <typename T> static void RegisterType() {
        if (lua == nullptr) {
            throw std::runtime_error("LUA is not set! Reference got destroyed?");
        }

        if constexpr (IsLuaType<T>) {
            T::registerLua(*lua);
        } else {
            throw std::runtime_error("Type missing 'registerLua'");
        }
    }

    template <typename... CallbackArgs> static void Call(const std::string& hookName, CallbackArgs&&... args) {
        for (auto& mod : mods) {
            mod.second->Call(hookName, args...);
        }
    }
};
} // namespace LUS
