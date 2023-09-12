#include "Scripting.h"
#include "wrappers/HooksWrapper.h"
#include "spdlog/spdlog.h"

namespace LUS {
// PROTECTED -----
std::unordered_map<std::string, std::unique_ptr<Mod>> Scripting::mods = {};
std::unique_ptr<sol::state> Scripting::lua = std::make_unique<sol::state>();
std::vector<std::unique_ptr<Plugin>> Scripting::plugins = {};
// ----------------

// PUBLIC ----
std::unique_ptr<Hooks> Scripting::hooks = std::make_unique<Hooks>();

std::function<void()> Scripting::onRegisterTypes = nullptr;
std::function<void(Mod*)> Scripting::onRegisterGlobals = nullptr;
std::function<void(Mod*)> Scripting::onLoadExtensions = nullptr;
std::function<void(Mod*)> Scripting::onModHotReload = nullptr;

bool Scripting::initialized = false;
// -------------

void Scripting::Init(int hotReloadMs) {
    mods.clear();

    // Loading initial libs ---
    LoadLibraries();
    LoadTypes();
    //  ----

    initialized = true;
}

void Scripting::Shutdown() {
    for (auto& mod : mods) {
        auto& env = mod.second->GetEnvironment();
        env.clear();
    }

    mods.clear();
    plugins.clear();

    lua->collect_garbage();
    lua.reset();
}

// LOAD ---
void Scripting::LoadLibraries() {
    if (lua == nullptr) {
        throw std::runtime_error("LUA is not set! Reference got destroyed?");
    }

    lua->open_libraries(sol::lib::base);
    lua->open_libraries(sol::lib::package);
    lua->open_libraries(sol::lib::math);
    lua->open_libraries(sol::lib::table);
    lua->open_libraries(sol::lib::debug);
    lua->open_libraries(sol::lib::string);
    lua->open_libraries(sol::lib::coroutine);
    lua->open_libraries(sol::lib::bit32);
}

void Scripting::LoadTypes() {
    if (lua == nullptr) {
        throw std::runtime_error("LUA is not set! Reference got destroyed?");
    }

    // REGISTER COMMON TYPES ---
    HooksWrapper::RegisterLua(*lua);
    // --------------

    // Register plugins types ---
    for (auto& p : plugins) {
        p->RegisterTypes(*lua);
    }
    //  -----

    // Custom ----
    if (onRegisterTypes != nullptr) {
        onRegisterTypes();
    }
    // ----
}

void Scripting::LoadLuaExtensions(Mod* mod) {
    if (lua == nullptr) {
        throw std::runtime_error("LUA is not set! Reference got destroyed?");
    }

    if (mod == nullptr) {
        throw std::runtime_error("Mod not set! Reference got destroyed?");
    }

    auto& env = mod->GetEnvironment();
    LoadLuaFile("./lua/table.lua", env);
    LoadLuaFile("./lua/string.lua", env);
    LoadLuaFile("./lua/math.lua", env);
    LoadLuaFile("./lua/json.lua", env);
    LoadLuaFile("./lua/sha2.lua", env);
    LoadLuaFile("./lua/util.lua", env);

    // Register plugins types ---
    for (auto& p : plugins) {
        p->LoadLuaExtensions(mod);
    }
    //  -----

    // Custom ----
    if (onLoadExtensions != nullptr) {
        onLoadExtensions(mod);
    }
    // ----
}

void Scripting::LoadGlobals(Mod* mod) {
    if (mod == nullptr) {
        throw std::runtime_error("Mod not set! Reference got destroyed?");
    }

    auto& env = mod->GetEnvironment();
    env["print"] = [](sol::variadic_args va) {
        auto vars = std::vector<sol::object>(va.begin(), va.end());

        std::vector<std::string> prtData;
        for (auto& var : vars) {
            prtData.push_back((*lua)["tostring"](var));
        }

        if (prtData.empty()) {
            return;
        }

        std::string data = "";
        for (auto& i : prtData) {
            data.append(i);
        }

        SPDLOG_INFO("{}", data);
    };

    env["printTable"] = [](sol::table table) {
        auto json = LuaUtils::LuaToJsonObject(table);
        SPDLOG_INFO("{}", json.dump(1, ' ', false));
    };

    env["include"] = [&env, mod](const std::string& path) {
        auto fixedPath = LuaUtils::GetContent(path, mod->GetFolder());

        bool loaded = LoadLuaFile(fixedPath, env);
        if (!loaded) {
            SPDLOG_WARN("Failed to load '{}'", fixedPath);
        }

        // Register file for hot-reloading
        // RegisterLoadedFile(mod->GetId(), fixedPath);
        // ----

        return loaded ? 1 : 0;
    };

    // Security ------------------------------------
    env["debug"]["setlocal"] = []() {
        SPDLOG_WARN("'debug.setlocal' removed due to security reasons");
        return;
    };

    env["debug"]["setupvalue"] = []() {
        SPDLOG_WARN("'debug.setupvalue' removed due to security reasons");
        return;
    };

    env["debug"]["upvalueid"] = []() {
        SPDLOG_WARN("'debug.upvalueid' removed due to security reasons");
        return;
    };

    env["debug"]["upvaluejoin"] = []() {
        SPDLOG_WARN("'debug.upvaluejoin' removed due to security reasons");
        return;
    };
    // --------------

    // ID ------------------
    env["__mod_folder"] = mod->GetFolder().generic_string();
    env["__mod_id"] = mod->GetId();
    // ---------------------

    // Global types ------------------------------------
    env["hooks"] = HooksWrapper(hooks.get());
    // -----------------------

    // Register plugins env types ---
    for (auto& p : plugins) {
        p->RegisterGlobal(mod);
    }
    //  -----

    // Custom global env types ---
    if (onRegisterGlobals != nullptr) {
        onRegisterGlobals(mod);
    }
    // ----
}

void Scripting::Load() {
    if (!std::filesystem::exists("./mods")) {
        throw std::runtime_error("Failed to locate folder './mods'");
    }

    // TODO: Do we need mod load ordering?
    for (auto& p : std::filesystem::directory_iterator("./mods")) {
        if (!p.is_directory()) {
            continue;
        }

        auto id = p.path().filename().string();
        auto folderPath = "mods/" + id;

        // Done
        auto mod = std::make_unique<Mod>(id, folderPath);
        mods.emplace(id, std::move(mod));
    }

    // Initialize
    for (auto& mod : mods) {
        mod.second->Init();

        LoadLuaExtensions(mod.second.get());
        LoadGlobals(mod.second.get());

        if (!mod.second->Load()) {
            SPDLOG_INFO("Failed to load mod '{}'", mod.first);
        } else {
            // Register file for hot-reloading
            // RegisterLoadedFile(mod.first, mod.second->GetEntryFilePath());
            // ----
        }
    }
    // -----
}

bool Scripting::LoadLuaFile(const std::string& path, const sol::environment& env) {
    if (!std::filesystem::exists(path)) {
        SPDLOG_WARN("Failed to load lua : {}", path);
        return false;
    }

    std::string errStr;
    // try to load the file while handling exceptions
    auto ret = lua->safe_script_file(
        path, env,
        [&errStr](lua_State*, sol::protected_function_result pfr) {
            sol::error err = pfr;
            errStr = err.what();
            return pfr;
        },
        sol::load_mode::text);

    // check if we loaded the file
    if (errStr.empty()) {
        return true;
    }

    SPDLOG_ERROR("Failed to load '{}'\n  └── Lua error : {}", path, errStr);
    return false;
}

// UTILS -----
sol::state& Scripting::GetLua() {
    return *lua;
}

const std::unordered_map<std::string, std::unique_ptr<Mod>>& Scripting::GetMods() {
    return mods;
}

const std::vector<std::string> Scripting::GetModsIds() {
    std::vector<std::string> modNames = {};
    for (auto& mod : mods) {
        modNames.push_back(mod.second->GetId());
    }

    return modNames;
}
} // namespace LUS
