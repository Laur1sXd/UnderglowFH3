// The official ANNA plugin owns the Car/Lights hierarchy. This adapter uses
// only its public API 1; it never hooks or falls back to ExtraOptions' ANNA.
namespace pluginAnna {
const FH3AnnaApi* provider{};
FH3AnnaGetApiFn queryApi = fh3anna_get;
std::atomic<bool> registered{}, ready{};
std::atomic<bool> pluginRequested{supportExtraOptions};
HANDLE worker{};

bool usesNativeMenu() { return !pluginRequested.load(); }
constexpr bool choosePluginRoute(bool extraOptions, bool liveApi, bool loadedModule, bool installedPlugin) {
    return extraOptions || liveApi || loadedModule || installedPlugin;
}
bool expectedOnDisk(const std::filesystem::path& azimuth) {
    const auto config = azimuth / L"FH3_ExtraOptions.ini";
    if (!GetPrivateProfileIntW(L"Global", L"Enabled", 1, config.c_str())) return false;
    if (!GetPrivateProfileIntW(L"Mods", L"Enabled", 1, config.c_str())) return false;
    const auto mods = azimuth / L"mods/Forza Horizon 3";
    const auto enabled = [&](const std::filesystem::path& folder) {
        const auto dll = folder / L"plugins/ANNA.dll";
        const auto attributes = GetFileAttributesW(dll.c_str());
        const auto name = folder.filename().wstring();
        return attributes != INVALID_FILE_ATTRIBUTES && !(attributes & FILE_ATTRIBUTE_DIRECTORY) &&
            GetPrivateProfileIntW(L"Mods", name.c_str(), 1, config.c_str()) != 0;
    };
    if (enabled(mods / L"ANNA")) return true;
    // Extra Options permits a different mod-folder name. Search only its
    // immediate mod directories, never a mod's potentially large media tree.
    std::error_code error;
    std::filesystem::directory_iterator it(mods, std::filesystem::directory_options::skip_permission_denied, error);
    const std::filesystem::directory_iterator end;
    for (unsigned count = 0; !error && it != end && count < 256; it.increment(error), ++count) {
        if (it->is_directory(error) && enabled(it->path())) return true;
    }
    return false;
}
void selectRoute() {
    if (supportExtraOptions || registered.load()) { pluginRequested = true; return; }
    const bool live = queryApi(1) != nullptr;
    const bool module = GetModuleHandleW(FH3ANNA_DLL) != nullptr;
    const bool installed = api && api->azimuth_dir && expectedOnDisk(api->azimuth_dir);
    pluginRequested = choosePluginRoute(supportExtraOptions, live, module, installed);
}
enum class Kind { root, audio, target, effect, lights };
struct Entry {
    Kind kind{};
    int target{}, effect{-1};
    std::string key, parent, label;
};
std::vector<Entry> entries;
SRWLOCK pendingLock = SRWLOCK_INIT;
struct Change { Id id{}; std::wstring save; int target{}, effect{-1}; bool lights{}; };
std::vector<Change> pending;

bool available() { return ready.load() && modEnabled.load() && !stopping.load(); }
int enabled(void* user, const char*) {
    if (!available() || !user) return 0;
    const auto& entry = *static_cast<Entry*>(user);
    const auto record = garage::current();
    if (entry.kind == Kind::lights) return record.state == garage::State::installed;
    if (entry.kind == Kind::target || entry.kind == Kind::effect)
        return garage::audioTargetAvailable(record, entry.target - 1);
    return record.state == garage::State::installed && !record.lightsOff;
}
// API 1 supports visibility, not disabled rows. Hide unavailable choices and
// recheck every action so stale callbacks cannot apply to an unequipped part.
int shown(void* user, const char* key) { return enabled(user, key); }

bool selected(const Entry& entry, const garage::CarSetup& record) {
    if (entry.target) return garage::zoneEffect(record.audio, entry.target - 1) == entry.effect;
    for (size_t zone = 0; zone < zoneCount; ++zone)
        if (garage::zoneEffect(record.audio, zone) != entry.effect) return false;
    return true;
}

const char* label(void* user, const char* key) {
    auto& entry = *static_cast<Entry*>(user);
    static thread_local std::string text;
    text = entry.label;
    const auto record = garage::current();
    if (entry.kind == Kind::lights && record.state == garage::State::installed)
        text = record.lightsOff ? "Underglow On" : "Underglow Off";
    else if (!enabled(user, key)) text += " (Off)";
    else if (entry.kind == Kind::effect && selected(entry, record)) text.insert(0, "\xc2\xbb ");
    return text.c_str();
}

void choose(void* user, const char* key) {
    if (!enabled(user, key)) return;
    const auto& entry = *static_cast<Entry*>(user);
    if (entry.kind != Kind::effect && entry.kind != Kind::lights) return;
    Change change;
    AcquireSRWLockShared(&garage::garageLock);
    change.id = garage::activeId;
    change.save = garage::activeSave;
    ReleaseSRWLockShared(&garage::garageLock);
    if (!change.id || change.save.empty()) return;
    change.target = entry.target; change.effect = entry.effect;
    change.lights = entry.kind == Kind::lights;
    // ANNA callbacks run on the HUD thread. Disk I/O stays on our worker.
    AcquireSRWLockExclusive(&pendingLock);
    if (pending.size() < 64) pending.push_back(std::move(change));
    ReleaseSRWLockExclusive(&pendingLock);
}

bool apply(const Change& change) {
    if (!available()) return false;
    AcquireSRWLockExclusive(&garage::garageLock);
    bool ok = false;
    if (change.id == garage::activeId && change.save == garage::activeSave &&
        change.save == saveScope::current().key) {
        auto record = garage::load(change.id);
        if (change.lights && record.state == garage::State::installed) {
            record.lightsOff = !record.lightsOff;
            ok = garage::store(change.id, record);
        } else if (!change.lights && (change.effect == -1 || isAudioEffect(change.effect)) &&
                   garage::audioTargetAvailable(record, change.target - 1)) {
            record.audio = garage::changeEffect(record.audio, change.effect, change.target - 1);
            ok = garage::store(change.id, record);
        }
    }
    ReleaseSRWLockExclusive(&garage::garageLock);
    return ok;
}

void discardPending() {
    AcquireSRWLockExclusive(&pendingLock); pending.clear(); ReleaseSRWLockExclusive(&pendingLock);
}
bool connect();
DWORD WINAPI run(void*) {
    try {
        if (!registered.load() && !connect()) return 0;
    } catch (...) {
        log("ANNA API registration failed; Garage Underglow remains available.");
        return 0;
    }
    while (!stopping.load() && WaitForSingleObject(stopEvent, 20) == WAIT_TIMEOUT) {
        std::vector<Change> changes;
        AcquireSRWLockExclusive(&pendingLock); changes.swap(pending); ReleaseSRWLockExclusive(&pendingLock);
        for (const auto& change : changes) {
            try { apply(change); }
            catch (...) { log("ANNA Underglow change could not be saved."); }
        }
    }
    discardPending(); return 0;
}
bool startWorker() {
    if (usesNativeMenu() || (registered.load() && !ready.load()) || worker) return true;
    worker = CreateThread(nullptr, 0, run, nullptr, 0, nullptr);
    return worker != nullptr;
}
void stopWorker() {
    if (worker) { WaitForSingleObject(worker, INFINITE); CloseHandle(worker); worker = nullptr; }
    discardPending();
}

bool addEntries(const FH3AnnaApi* host) {
    if (registered.load()) return provider == host && ready.load();
    if (!host || host->version < 1 || host->size < sizeof(FH3AnnaApi) ||
        !host->add_entry || !host->is_open) return false;
    pluginRequested = true;
    // ANNA has no unregister API. Pin our callback code/data before publication
    // so an unload racing the first registration cannot leave dangling pointers.
    HMODULE resident{};
    if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_PIN,
        reinterpret_cast<LPCWSTR>(&addEntries), &resident)) return false;
    provider = host;
    entries.reserve(100); // stable callback user pointers after publishing
    const auto add = [](Kind kind, int target, int effect, std::string key, std::string parent, std::string text) {
        entries.push_back({kind, target, effect, std::move(key), std::move(parent), std::move(text)});
    };
    add(Kind::root, 0, -1, "underglow", FH3ANNA_LIST_CAR, "Underglow");
    add(Kind::audio, 0, -1, "underglow.audio", "underglow", "Audio Visualizer");
    constexpr int targetOrder[] = {0, fx::kZoneFront + 1, fx::kZoneRear + 1, fx::kZoneLeft + 1, fx::kZoneRight + 1};
    for (int target : targetOrder) {
        const auto parent = "underglow.audio." + std::to_string(target);
        add(Kind::target, target, -1, parent, "underglow.audio", effectTargets[target]);
        add(Kind::effect, target, -1, parent + ".off", parent, "Off");
        for (const auto entry : fx::kEffectOrder)
            if (entry.group == fx::kGroupAudio)
                add(Kind::effect, target, entry.effect, parent + ".fx" + std::to_string(entry.effect), parent,
                    fx::kEffects[entry.effect]);
    }
    if (supportExtraOptions)
        add(Kind::lights, 0, -1, "underglow.lights", FH3ANNA_LIST_CAR_LIGHTS, "Underglow");
    registered = true; // never republish keys, even after partial registration
    bool ok = true;
    for (auto& entry : entries) {
        FH3AnnaEntry item{};
        item.size = sizeof(item); item.key = entry.key.c_str(); item.parent = entry.parent.c_str();
        item.label = entry.label.c_str(); item.label_now = label; item.shown = shown;
        item.user = &entry; item.stays_open = 1;
        if (entry.kind == Kind::effect || entry.kind == Kind::lights) item.on_choose = choose;
        if (!host->add_entry(&item)) { ok = false; break; }
    }
    ready = ok;
    return ready.load();
}

bool connect() {
    const FH3AnnaApi* host = queryApi(1);
    // Platform mods start before ExtraOptions plugins. Wait cancellably on our
    // dedicated worker, never in fh3mod_init or on the game's HUD thread.
    for (unsigned i = 0; !host && i < 2400 && !stopping.load(); ++i) {
        if ((host = queryApi(1))) break;
        if (stopWait(50)) return false;
    }
    if (!host) {
        log("Official ANNA API 1 is unavailable. Garage Underglow remains available; native ANNA hooks were suppressed to avoid competing with the plugin.");
        return false;
    }
    if (!addEntries(host)) {
        log("Could not register Underglow with the official ANNA API 1.");
        return false;
    }
    else log("Underglow audio controls registered with the ANNA plugin; its Car and Lights menus remain owned by ANNA.");
    return true;
}
bool install() {
    if (usesNativeMenu()) return false;
    if (!startWorker()) {
        log("ANNA API worker could not start; Garage Underglow remains available.");
        return false;
    }
    log("Waiting for the official ANNA API 1 on a separate worker; built-in ExtraOptions ANNA integration is disabled.");
    return true;
}
} // namespace pluginAnna
