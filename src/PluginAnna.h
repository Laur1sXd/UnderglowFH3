// The official ANNA plugin owns the Car/Lights hierarchy. This adapter uses
// only its public API 1; it never hooks or falls back to ExtraOptions' ANNA.
namespace pluginAnna {
const FH3AnnaApi* provider{};
FH3AnnaGetApiFn queryApi = fh3anna_get;
std::atomic<bool> registered{}, ready{}, extrasReady{};
std::atomic<bool> pluginRequested{false};
HANDLE worker{};

bool usesNativeMenu() { return !pluginRequested.load(); }
constexpr bool choosePluginRoute(bool liveApi, bool loadedModule, bool installedPlugin) {
    return liveApi || loadedModule || installedPlugin;
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
    if (registered.load()) { pluginRequested = true; return; }
    const bool live = queryApi(1) != nullptr;
    const bool module = GetModuleHandleW(FH3ANNA_DLL) != nullptr;
    const bool installed = api && api->azimuth_dir && expectedOnDisk(api->azimuth_dir);
    pluginRequested = choosePluginRoute(live, module, installed);
}
// The menu is laid out like FH2 Underglow: Car > Underglow > Primary Color / Secondary Color / Effect, and
// Effect > part (all, front, rear, left, right) > group > effect. A group's `effect` is its fx group; the Audio
// Visualizer group holds Off and the 15 radio-reactive effects (kind `effect`, kept in the audio overlay), the
// others the visual effects (kind `visual`, kept in the setup). colorList/color: target 0 or 1, effect the color.
// `extra` entries (the colors, the visual groups) are registered last: an ANNA too old to take them (entry cap
// 256) leaves the audio controls working.
enum class Kind { root, effect, lights, colorList, color, effectList, part, group, visual };
struct Entry {
    Kind kind{};
    int target{}, effect{-1};
    bool extra{};
    std::string key, parent, label;
};
constexpr size_t entryCapacity = 512;
std::vector<Entry> entries;
SRWLOCK pendingLock = SRWLOCK_INIT;
struct Change { Id id{}; std::wstring save; Kind kind{}; int target{}, effect{-1}; bool lights{}; };
std::vector<Change> pending;

bool available() { return ready.load() && modEnabled.load() && !stopping.load(); }
int enabled(void* user, const char*) {
    if (!available() || !user) return 0;
    const auto& entry = *static_cast<Entry*>(user);
    if (entry.extra && !extrasReady.load()) return 0;
    const auto record = garage::current();
    if (entry.kind == Kind::lights) return record.state == garage::State::installed;
    if (entry.kind == Kind::effect)
        return garage::audioTargetAvailable(record, entry.target - 1);
    if (entry.kind == Kind::group) {
        if (record.state != garage::State::installed || record.lightsOff) return 0;
        // The audio overlay can't take a part the setup has switched off (or one part when all four share an effect).
        if (entry.effect == fx::kGroupAudio) return garage::audioTargetAvailable(record, entry.target - 1);
        // A group the setup can't use yet (it has no primary or secondary color) hides, like its Garage tab locks.
        return garage::unlockMessage(entry.effect, garage::look(record)).empty();
    }
    return record.state == garage::State::installed && !record.lightsOff;
}
// API 1 supports visibility, not disabled rows. Hide unavailable choices and
// recheck every action so stale callbacks cannot apply to an unequipped part.
int shown(void* user, const char* key) { return enabled(user, key); }

// What a part plays: an audio effect picked in the Audio Visualizer wins over the setup's own, as when rendering.
int playing(const garage::CarSetup& record, size_t zone) {
    const int audio = garage::zoneEffect(record.audio, zone);
    return audio >= 0 ? audio : garage::zoneEffect(record.effects, zone);
}

bool selected(const Entry& entry, const garage::CarSetup& record) {
    if (entry.kind == Kind::color) return (entry.target ? record.secondary : record.primary) == entry.effect;
    if (entry.kind == Kind::visual) {
        if (entry.target) return playing(record, static_cast<size_t>(entry.target - 1)) == entry.effect;
        for (size_t zone = 0; zone < zoneCount; ++zone)
            if (playing(record, zone) != entry.effect) return false;
        return true;
    }
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
    else if ((entry.kind == Kind::effect || entry.kind == Kind::visual || entry.kind == Kind::color) &&
             selected(entry, record))
        text.insert(0, "\xc2\xbb ");
    return text.c_str();
}

void choose(void* user, const char* key) {
    if (!enabled(user, key)) return;
    const auto& entry = *static_cast<Entry*>(user);
    if (entry.kind != Kind::effect && entry.kind != Kind::lights && entry.kind != Kind::color &&
        entry.kind != Kind::visual) return;
    Change change;
    AcquireSRWLockShared(&garage::garageLock);
    change.id = garage::activeId;
    change.save = garage::activeSave;
    ReleaseSRWLockShared(&garage::garageLock);
    if (!change.id || change.save.empty()) return;
    change.kind = entry.kind; change.target = entry.target; change.effect = entry.effect;
    change.lights = entry.kind == Kind::lights;
    // ANNA callbacks run on the HUD thread. Disk I/O stays on our worker.
    AcquireSRWLockExclusive(&pendingLock);
    if (pending.size() < 64) pending.push_back(std::move(change));
    ReleaseSRWLockExclusive(&pendingLock);
}

bool apply(const Change& change) {
    if (!available()) return false;
    // Taken before the garage lock: setLook holds the settings lock and then the garage lock.
    const auto sliders = garage::sliderSnapshot();
    AcquireSRWLockExclusive(&garage::garageLock);
    bool ok = false;
    if (change.id == garage::activeId && change.save == garage::activeSave &&
        change.save == saveScope::current().key) {
        auto record = garage::load(change.id);
        if (change.lights && record.state == garage::State::installed) {
            record.lightsOff = !record.lightsOff;
            ok = garage::store(change.id, record);
        } else if (change.kind == Kind::color && record.state == garage::State::installed && change.effect >= 0) {
            auto appearance = garage::look(record);
            (change.target ? appearance.secondary : appearance.primary) = change.effect;
            ok = garage::storeLook(change.id, record, appearance, sliders);
        } else if (change.kind == Kind::visual && record.state == garage::State::installed &&
                   change.effect >= 0 && change.effect < fx::kEffectCount) {
            // One part (target 0 is all four) gets the effect; the others keep theirs. A color-only setup glows with
            // the default effect on every part. An audio pick on that part no longer overrides it.
            auto appearance = garage::look(record);
            appearance.effects = garage::changeEffect(
                appearance.effects ? appearance.effects : garage::allEffects(fx::kEffectStatic),
                change.effect, change.target - 1);
            record.audio = garage::changeEffect(record.audio, -1, change.target - 1);
            ok = garage::storeLook(change.id, record, appearance, sliders);
        } else if (change.kind == Kind::effect && (change.effect == -1 || isAudioEffect(change.effect)) &&
                   garage::audioTargetAvailable(record, change.target - 1)) {
            record.audio = garage::changeEffect(record.audio, change.effect, change.target - 1);
            ok = garage::store(change.id, record);
            // The radio may be silent: show the pick on a made-up beat for a few seconds. Real audio always wins.
            if (ok && change.effect >= 0) radio::demoUntil = GetTickCount64() + 8000;
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
    entries.reserve(entryCapacity); // stable callback user pointers after publishing: never grow past it
    const auto add = [](Kind kind, int target, int effect, std::string key, std::string parent, std::string text) {
        if (entries.size() >= entryCapacity) return;
        entries.push_back({kind, target, effect, false, std::move(key), std::move(parent), std::move(text)});
    };
    constexpr int targetOrder[] = {0, fx::kZoneFront + 1, fx::kZoneRear + 1, fx::kZoneLeft + 1, fx::kZoneRight + 1};
    add(Kind::root, 0, -1, "underglow", FH3ANNA_LIST_CAR, "Underglow");
    // ANNA lists children in the order they are added. The color lists come first (empty, hidden until their colors
    // are added below), then Effect with its parts and, in each, the Audio Visualizer group, so those register first.
    const std::string colorList[2] = {"underglow.color", "underglow.color2"};
    add(Kind::colorList, 0, -1, colorList[0], "underglow", "Primary Color");
    add(Kind::colorList, 1, -1, colorList[1], "underglow", "Secondary Color");
    add(Kind::effectList, 0, -1, "underglow.effect", "underglow", "Effect");
    for (int target : targetOrder) {
        const auto part = "underglow.effect." + std::to_string(target);
        add(Kind::part, target, -1, part, "underglow.effect", effectTargets[target]);
        const auto audio = part + "." + fx::kGroupKeys[fx::kGroupAudio];
        add(Kind::group, target, fx::kGroupAudio, audio, part, fx::kGroupNames[fx::kGroupAudio]);
        add(Kind::effect, target, -1, audio + ".off", audio, "Off");
        for (const auto entry : fx::kEffectOrder)
            if (entry.group == fx::kGroupAudio)
                add(Kind::effect, target, entry.effect, audio + ".fx" + std::to_string(entry.effect), audio,
                    fx::kEffects[entry.effect]);
    }
    // Car > Lights > Underglow On/Off: ANNA exists only with Extra Options, so every edition offers it through her API.
    add(Kind::lights, 0, -1, "underglow.lights", FH3ANNA_LIST_CAR_LIGHTS, "Underglow");
    const auto addExtra = [&](Kind kind, int target, int effect, std::string key, std::string parent, std::string text) {
        const auto before = entries.size();
        add(kind, target, effect, std::move(key), std::move(parent), std::move(text));
        if (entries.size() > before) entries.back().extra = true;
    };
    for (int which = 0; which < 2; ++which) {
        const auto addColor = [&](int color) {
            addExtra(Kind::color, which, color, colorList[which] + ".c" + std::to_string(color), colorList[which],
                     colorNames[color] ? std::string(colorNames[color]) : "Color " + std::to_string(color + 1));
        };
        for (const int color : garage::standardColors) addColor(color);
        for (const int color : garage::specialColors) addColor(color);
        for (int color = fx::kColorCount; color < fx::kColorTotal; ++color) addColor(color);
    }
    for (int target : targetOrder) {
        const auto part = "underglow.effect." + std::to_string(target);
        for (int group = fx::kGroupOneColor; group < fx::kGroupCount; ++group) {
            const auto groupKey = part + "." + fx::kGroupKeys[group];
            addExtra(Kind::group, target, group, groupKey, part, fx::kGroupNames[group]);
            for (const auto entry : fx::kEffectOrder)
                if (entry.group == group)
                    addExtra(Kind::visual, target, entry.effect, groupKey + ".fx" + std::to_string(entry.effect),
                             groupKey, fx::kEffects[entry.effect]);
        }
    }
    registered = true; // never republish keys, even after partial registration
    bool ok = true, extrasOk = true;
    for (auto& entry : entries) {
        FH3AnnaEntry item{};
        item.size = sizeof(item); item.key = entry.key.c_str(); item.parent = entry.parent.c_str();
        item.label = entry.label.c_str(); item.label_now = label; item.shown = shown;
        item.user = &entry; item.stays_open = 1;
        // An entry with no on_choose is a list to ANNA: every leaf needs one, or its lists stay empty and hidden.
        if (entry.kind == Kind::effect || entry.kind == Kind::lights || entry.kind == Kind::color ||
            entry.kind == Kind::visual) item.on_choose = choose;
        if (!host->add_entry(&item)) {
            // The audio controls come first: an ANNA that stops short (cap 256) keeps them and hides the rest.
            if (entry.extra) extrasOk = false; else ok = false;
            break;
        }
    }
    ready = ok;
    extrasReady = ok && extrasOk;
    if (ok && !extrasOk)
        log("This ANNA refused Underglow's color and effect lists (entry limit). Update to ANNA 1.0.2 or later; the audio controls still work.");
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
