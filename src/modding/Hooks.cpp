#include "Hooks.h"

namespace LUS {
void Hooks::Listen(const std::string& id, const std::string& name, sol::function func) {
    this->mHooks[id].emplace_back(name, func);
}

void Hooks::Remove(const std::string& id, const std::string& name) {
    auto nameMap = this->mHooks.find(id);
    if (nameMap == this->mHooks.end()) {
        return;
    }

    auto& arr = nameMap->second;
    auto hook = std::find_if(arr.begin(), arr.end(), [&](auto& elem) { return elem.Name == name; });

    if (hook == arr.end()) {
        return;
    }

    arr.erase(hook);

    if (arr.empty()) {
        this->mHooks.erase(id);
    }
}

// Utils ---
size_t Hooks::Count() const {
    return this->mHooks.size();
}
bool Hooks::Empty() const {
    return this->mHooks.empty();
}
// ----

} // namespace LUS
