#include "../Hooks.h"
#include "HooksWrapper.h"

namespace LUS {
HooksWrapper::HooksWrapper(Hooks* hooks_) : mHooks(hooks_) {
}

void HooksWrapper::Listen(const std::string& hook, const std::string& name, sol::function callback) {
    this->mHooks->Listen(hook, name, callback);
}

void HooksWrapper::Call(const std::string& hook, sol::variadic_args args) {
    this->mHooks->Call(hook, args);
}

void HooksWrapper::Remove(const std::string& hook, const std::string& id) {
    this->mHooks->Remove(hook, id);
}

void HooksWrapper::RegisterLua(sol::state& lua) {
    lua.new_usertype<HooksWrapper>("hooks", sol::no_constructor, "add", &HooksWrapper::Listen, "call",
                                   &HooksWrapper::Call, "remove", &HooksWrapper::Remove);
}
} // namespace LUS
