#pragma once

#include "Mod.h"

#include <sol/sol.hpp>

namespace LUS {
class Mod;

class Plugin {
  public:
    Plugin() = default;
    Plugin(const Plugin&) = default;
    Plugin(Plugin&&) = default;
    Plugin& operator=(const Plugin&) = default;
    Plugin& operator=(Plugin&&) = default;
    virtual ~Plugin() = default;

    virtual void RegisterTypes(sol::state& /*_lua*/) {
    }
    virtual void RegisterGlobal(Mod* /*_mod*/) {
    }
    virtual void LoadLuaExtensions(Mod* /*_mod*/) {
    }
};
} // namespace LUS
