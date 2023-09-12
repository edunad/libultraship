#include "Scripting.h"

namespace LUS {
// PROTECTED -----
std::unordered_map<std::string, std::unique_ptr<Mod>> Scripting::mods = {};
std::unique_ptr<sol::state> Scripting::lua = std::make_unique<sol::state>();
std::vector<std::unique_ptr<Plugin>> Scripting::plugins = {};
// ----------------

// PUBLIC ----
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

    // AABBWrapper::registerLua(*_lua);

    // Register plugins types ---
    for (auto& p : plugins) {
        p->RegisterTypes(*lua);
    }
    //  -----

    // Custom ----
    onRegisterTypes();
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
    onLoadExtensions(mod);
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

        // fmt::print("{}\n", fmt::join(prtData, " "));
    };

    env["printTable"] = [](sol::table table) {
        auto json = LuaUtils::LuaToJsonObject(table);
        // fmt::print("{}\n", json.dump(1, ' ', false));
    };

    env["include"] = [&env, mod](const std::string& path) {
        auto fixedPath = LuaUtils::GetContent(path, mod->GetFolder());

        bool loaded = LoadLuaFile(fixedPath, env);
        if (!loaded) {
            // fmt::print("[RawrBox-Scripting] Failed to load '{}'\n", fixedPath);
        }

        // Register file for hot-reloading
        // RegisterLoadedFile(mod->GetId(), fixedPath);
        // ----

        return loaded ? 1 : 0;
    };

    // Security ------------------------------------
    env["debug"]["setlocal"] = []() {
        // fmt::print("[RawrBox-Scripting] 'debug.setlocal' removed due to security reasons\n");
        return;
    };

    env["debug"]["setupvalue"] = []() {
        // fmt::print("[RawrBox-Scripting] 'debug.setupvalue' removed due to security reasons\n");
        return;
    };

    env["debug"]["upvalueid"] = []() {
        // fmt::print("[RawrBox-Scripting] 'debug.upvalueid' removed due to security reasons\n");
        return;
    };

    env["debug"]["upvaluejoin"] = []() {
        // fmt::print("[RawrBox-Scripting] 'debug.upvaluejoin' removed due to security reasons\n");
        return;
    };
    // --------------

    // ID ------------------
    env["__mod_folder"] = mod->GetFolder().generic_string();
    env["__mod_id"] = mod->GetId();
    // ---------------------

    // Register plugins env types ---
    for (auto& p : plugins) {
        p->RegisterGlobal(mod);
    }
    //  -----

    // Custom global env types ---
    onRegisterGlobals(mod);
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
            // fmt::print("[RawrBox-Scripting] Failed to load mod '{}'\n", mod.first); // TODO: Replace with
            // internal logger logger
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
        // fmt::print("[RawrBox-Scripting] Failed to load lua : {}\n", path);  // TODO: Replace with internal logger
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

    // fmt::print("[RawrBox-Scripting] Failed to load '{}'\n  └── Lua error : {}\n", path, errStr); // TODO: Replace
    // with internal logger
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
