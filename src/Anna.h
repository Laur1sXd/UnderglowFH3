// Underglow's entries in ANNA (the FH3-ANNA mod), laid out like FH2 Underglow:
//   Car > Underglow > Primary Color / Secondary Color / Effect
//   Effect > All / Front / Rear / Left Side / Right Side > a group > an effect
// and, in the ExtraOptions edition, Car > Lights > Underglow On/Off.
// ANNA owns the menu. This file only registers entries through her API (fh3anna.h), so the game keeps one Car list.
// The effect tree is about 330 entries, so it needs an ANNA with room for them (MAX_ENTRIES 32767).
// Every callback below runs on the game's thread.
namespace anna {
constexpr char underglowList[] = "underglow";
constexpr char effectList[] = "underglow.effect";
constexpr char colorLists[][24] = {"underglow.color", "underglow.color2"};
// ANNA's order for the parts, with the key each one's lists are named by.
constexpr struct { int target; const char* key; } targetOrder[] = {
    {0, "all"}, {fx::kZoneFront + 1, "front"}, {fx::kZoneRear + 1, "rear"},
    {fx::kZoneLeft + 1, "left"}, {fx::kZoneRight + 1, "right"}};

Byte* playerCar() {
    if (stopping.load()) return nullptr;
    AcquireSRWLockShared(&snapshotLock);
    auto* car = snapshot.car;
    const bool current = snapshot.matrixTick && GetTickCount64() - snapshot.playerTick < 1000;
    ReleaseSRWLockShared(&snapshotLock);
    return current && isLocalDriver(car) ? car : nullptr;
}

bool underglowInstalled() {
    return garage::current().state == garage::State::installed;
}

bool underglowAvailable() {
    const auto record = garage::current();
    return record.state == garage::State::installed && !record.lightsOff;
}

// A target and an effect (or a group) travel in an entry's user pointer. Target 0 is "All".
void* pack(int target, int value) {
    return reinterpret_cast<void*>(static_cast<uintptr_t>(target) | (static_cast<uintptr_t>(value) << 8));
}
int targetOf(void* user) { return static_cast<int>(reinterpret_cast<uintptr_t>(user) & 0xff); }
int valueOf(void* user) { return static_cast<int>(reinterpret_cast<uintptr_t>(user) >> 8); }

// What a part plays now: an audio effect an older version chose in ANNA wins, as it does when rendering.
// -1: none. A color-only setup (no effects saved) has no effect of its own.
int effectOf(const garage::CarSetup& record, size_t zone) {
    const int audio = garage::zoneEffect(record.audio, zone);
    return audio >= 0 ? audio : garage::zoneEffect(record.effects, zone);
}

bool effectSelected(int target, int effect) {
    const auto record = garage::current();
    if (target) return effectOf(record, static_cast<size_t>(target - 1)) == effect;
    for (size_t zone = 0; zone < zoneCount; ++zone)
        if (effectOf(record, zone) != effect) return false;
    return true;
}

// The menu is there for a car with a setup (the glow may be off: choosing anything switches it on) and for one
// with none yet, where choosing a color equips the default glow.
int showUnderglow(void*, const char*) {
    const auto state = garage::current().state;
    return playerCar() && (state == garage::State::installed || state == garage::State::missing);
}
// Effects need a setup to change.
int showEffects(void*, const char*) { return playerCar() && underglowInstalled(); }
int showLights(void*, const char*) { return playerCar() && underglowInstalled(); }

// A group the setup can't use yet (one or two colors it has none of) hides, as its Garage tab locks.
int showGroup(void* user, const char*) {
    return playerCar() && underglowInstalled() &&
        garage::unlockMessage(valueOf(user), garage::look(garage::current())).empty();
}

const char* effectLabel(void* user, const char*) {
    static char text[128]; // ANNA copies the label straight away.
    const int effect = valueOf(user);
    std::snprintf(text, sizeof(text), "%s%s", effectSelected(targetOf(user), effect) ? "\xC2\xBB " : "",
                  fx::kEffects[effect]);
    return text;
}

// The colors, as the Garage lists them: standard, special, then the custom slots. user: which (0 primary), color.
const char* colorName(int color, char (&text)[64]) {
    if (colorNames[color]) std::snprintf(text, sizeof(text), "%s", colorNames[color]);
    else std::snprintf(text, sizeof(text), "Color %d", color + 1);
    return text;
}

const char* colorLabel(void* user, const char*) {
    static char text[128]; // ANNA copies the label straight away.
    const int color = valueOf(user);
    const auto record = garage::current();
    const bool chosen = (targetOf(user) ? record.secondary : record.primary) == color;
    char name[64];
    std::snprintf(text, sizeof(text), "%s%s", chosen ? "\xC2\xBB " : "", colorName(color, name));
    return text;
}

void chooseColor(void* user, const char*) {
    if (!playerCar()) return;
    log(garage::setActiveColor(targetOf(user) != 0, valueOf(user)) ? "ANNA Underglow color applied." :
        "Could not save the ANNA Underglow color.");
}

const char* lightsLabel(void*, const char*) {
    return garage::current().lightsOff ? "Underglow On" : "Underglow Off";
}

void chooseEffect(void* user, const char*) {
    if (!playerCar() || !underglowAvailable()) return;
    const int effect = valueOf(user);
    // The radio may be off. Show an audio effect on a made-up beat for a few seconds; a real signal wins.
    if (isAudioEffect(effect)) radio::demoUntil = GetTickCount64() + 8000;
    log(garage::setEffect(effect, targetOf(user) - 1) ? "ANNA Underglow effect applied." :
        "Could not save the ANNA Underglow effect.");
}

void chooseLights(void*, const char*) {
    if (playerCar() && underglowInstalled()) garage::toggleLights();
}

bool add(const FH3AnnaApi& host, const char* key, const char* parent, const char* label,
         FH3AnnaChooseFn choose = nullptr, FH3AnnaShownFn shown = nullptr,
         FH3AnnaLabelFn labelNow = nullptr, void* user = nullptr, bool staysOpen = false) {
    FH3AnnaEntry entry{};
    entry.size = sizeof(entry);
    entry.key = key;
    entry.parent = parent;
    entry.label = label;
    entry.on_choose = choose;
    entry.shown = shown;
    entry.label_now = labelNow;
    entry.user = user;
    entry.stays_open = staysOpen;
    return host.add_entry(&entry) != 0;
}

bool addEntries(const FH3AnnaApi& host) {
    bool ok = add(host, underglowList, FH3ANNA_LIST_CAR, "Underglow", nullptr, showUnderglow);
    for (int which = 0; which < 2; ++which) {
        ok = add(host, colorLists[which], underglowList, which ? "Secondary Color" : "Primary Color") && ok;
        const auto addColor = [&](int color) {
            char key[48], name[64];
            std::snprintf(key, sizeof(key), "%s.c%d", colorLists[which], color);
            ok = add(host, key, colorLists[which], colorName(color, name), chooseColor, nullptr, colorLabel,
                     pack(which, color), true) && ok;
        };
        for (const int color : garage::standardColors) addColor(color);
        for (const int color : garage::specialColors) addColor(color);
        for (int color = fx::kColorCount; color < fx::kColorTotal; ++color) addColor(color);
    }
    ok = add(host, effectList, underglowList, "Effect", nullptr, showEffects) && ok;
    for (const auto& [target, name] : targetOrder) {
        char zone[48];
        std::snprintf(zone, sizeof(zone), "%s.%s", effectList, name);
        ok = add(host, zone, effectList, effectTargets[target]) && ok;
        for (int group = 0; group < fx::kGroupCount; ++group) {
            char groupKey[64];
            std::snprintf(groupKey, sizeof(groupKey), "%s.%s", zone, fx::kGroupKeys[group]);
            ok = add(host, groupKey, zone, fx::kGroupNames[group], nullptr, showGroup, nullptr,
                     pack(target, group)) && ok;
            for (const auto& entry : fx::kEffectOrder) {
                if (entry.group != group) continue;
                char key[80];
                std::snprintf(key, sizeof(key), "%s.e%d", groupKey, entry.effect);
                // Choosing an effect keeps the list open so the marker moves to the new choice.
                ok = add(host, key, groupKey, fx::kEffects[entry.effect], chooseEffect, nullptr, effectLabel,
                         pack(target, entry.effect), true) && ok;
            }
        }
    }
    if (supportExtraOptions)
        ok = add(host, "underglow.lights", FH3ANNA_LIST_CAR_LIGHTS, "Underglow Off", chooseLights, showLights,
                 lightsLabel, nullptr, true) && ok;
    return ok;
}

// The entries stay in ANNA for the session (she keeps their callbacks, and the platform keeps this DLL mapped), so
// a later start of Underglow in the same game (the runtime Load setting, a hot reload) must not add them again.
std::atomic<bool> registered{};
HANDLE waiter{};

// FH3 Mod Platform mods start before Extra Options plugins, so ANNA may not be loaded yet. Waits like
// fh3anna_wait, but ends when Underglow is shut down.
DWORD WINAPI waitForAnna(void*) {
    const ULONGLONG until = GetTickCount64() + 120000;
    const FH3AnnaApi* host = nullptr;
    while (!(host = fh3anna_get(1)) && GetTickCount64() < until)
        if (stopWait(250)) return 0;
    if (!host) {
        log("ANNA is not installed. The effect menu needs her: it appears under Car > Underglow.");
        return 0;
    }
    if (registered.exchange(true)) return 0;
    log(addEntries(*host) ? "ANNA effects are ready under Car > Underglow." :
        "ANNA refused some Underglow entries. Her entry limit may be too low (needs 32767).");
    return 0;
}

void install() {
    if (waiter && WaitForSingleObject(waiter, 0) == WAIT_OBJECT_0) {
        CloseHandle(waiter);
        waiter = nullptr;
    }
    if (registered.load() || waiter) return;
    waiter = CreateThread(nullptr, 0, waitForAnna, nullptr, 0, nullptr);
}

// From shutdown(), after the stop event is set: the waiting thread ends.
void stop() {
    if (!waiter) return;
    WaitForSingleObject(waiter, INFINITE);
    CloseHandle(waiter);
    waiter = nullptr;
}
} // namespace anna
