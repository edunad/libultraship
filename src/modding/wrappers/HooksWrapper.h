#pragma once

#include <sol/sol.hpp>

namespace LUS {
class Hooks;

class HooksWrapper {
  protected:
    Hooks* mHooks = nullptr;

  public:
    HooksWrapper(Hooks* hooks);
    HooksWrapper(const HooksWrapper&) = default;
    HooksWrapper(HooksWrapper&&) = default;
    HooksWrapper& operator=(const HooksWrapper&) = default;
    HooksWrapper& operator=(HooksWrapper&&) = default;
    virtual ~HooksWrapper() = default;

    virtual void Listen(const std::string& hook, const std::string& name, sol::function callback);
    virtual void Call(const std::string& hook, sol::variadic_args args);
    virtual void Remove(const std::string& hook, const std::string& id);

    static void RegisterLua(sol::state& lua);
};
} // namespace LUS
