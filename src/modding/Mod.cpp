
#include "Mod.h"
#include "Scripting.h"

#include <filesystem>
#include <utility>

namespace LUS {
Mod::Mod(std::string id, std::filesystem::path folderPath) : mFolder(std::move(folderPath)), mId(std::move(id)) {
}
Mod::~Mod() {
    this->mEnvironment.reset();
}

void Mod::Init() {
    auto& lua = Scripting::GetLua();
    this->mEnvironment = { lua, sol::create, lua.globals() };
}

bool Mod::Load() {
    auto& lua = Scripting::GetLua();

    this->mModTable = lua.create_table();
    this->mEnvironment["MOD"] = this->mModTable;

    // Load init script
    auto pth = this->GetEntryFilePath();
    if (std::filesystem::exists(pth)) {
        if (!Scripting::LoadLuaFile(pth, this->mEnvironment)) {
            return false;
        }
    } else {
        return false;
    }
    // -----

    return true;
}

// UTILS ----
const std::string& Mod::GetId() const {
    return this->mId;
}
const std::string Mod::GetEntryFilePath() const {
    return this->mFolder.generic_string() + "/init.lua";
}
const std::filesystem::path& Mod::GetFolder() const {
    return this->mFolder;
}

sol::environment& Mod::GetEnvironment() {
    return this->mEnvironment;
}
// -----
} // namespace LUS
