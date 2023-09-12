#pragma once

#include "LuaUtils.h"
#include <sol/sol.hpp>

#include <string>
#include <unordered_map>
#include <vector>

namespace LUS {
struct Hook {
    std::string Name;
    sol::function Func;

    Hook(std::string name, sol::function func) : Name(std::move(name)), Func(std::move(func)) {
    }
};

class Hooks {
  protected:
    std::unordered_map<std::string, std::vector<Hook>> mHooks;

  public:
    template <typename... CallbackArgs> void Call(const std::string& name, CallbackArgs... args) {
        for (auto& hook : mHooks[name]) {
            LuaUtils::RunCallback(hook.Func, std::forward<CallbackArgs>(args)...);
        }
    }

    void Listen(const std::string& id, const std::string& name, sol::function func);
    void Remove(const std::string& id, const std::string& name);

    // Utils ---
    [[nodiscard]] size_t Count() const;
    [[nodiscard]] bool Empty() const;
    // ----
};
} // namespace LUS
