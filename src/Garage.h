namespace anna {
bool patchMarkup(std::string_view source, std::string& output);
}

// Per-car Garage setups and the native Designs & Paints entry point.
namespace garage {
constexpr int installColor = -1;
constexpr int unsetPrimary = -1;
constexpr int unsetSecondary = -1;
constexpr int explicitSecondaryFormat = -2;
constexpr unsigned allColors = (1u << fx::kColorTotal) - 1;
constexpr int tunedColor = fx::kColorTotal;
constexpr int colorCount = tunedColor + 1;
constexpr unsigned colorMask = (1u << colorCount) - 1;
constexpr unsigned rgbMask = 0xffffff;
constexpr int effectChoice = 32, noUnderglowChoice = 111, historyChoice = 128;
constexpr size_t designsVtable = 0x4572050;
constexpr size_t paintVtable = 0x45729e8;
constexpr size_t paintItemVtable = 0x4572948;
constexpr size_t careerCar = 0x379640;
constexpr size_t enumItemVtable = 0x44a7b80;
constexpr size_t colorVtable = 0x456d9e8;
constexpr size_t collectionVtable = 0x4525bd0;
constexpr size_t swatchVtable = 0x44f5a68;
constexpr size_t swatchPanelVtable = 0x46ba0b8;
constexpr size_t swatchScrollVtable = 0x46ba848;
constexpr int swatchMarker = 0x55470000;
constexpr size_t maxHistoryEntries = static_cast<size_t>(INT_MAX - swatchMarker - historyChoice) + 1;
constexpr size_t historyPageSize = 64;
constexpr size_t historyPageBuffer = 4096;
constexpr wchar_t historySection[] = L"GarageHistory";
constexpr char underglowButton[] = "PART_Button_UnderglowGarage";
constexpr char swatchPanelName[] = "UG_SwatchPanel";

enum class State : unsigned { missing = 0, installed = 2, disabled = 3, invalid };
struct Look {
    int primary{unsetPrimary}, secondary{unsetSecondary};
    unsigned rgb{}, effects{};
    bool operator==(const Look& other) const {
        return primary == other.primary && secondary == other.secondary && rgb == other.rgb && effects == other.effects;
    }
};

using SliderValues = std::array<int, std::size(sliderRows)>;
constexpr SliderValues unknownSliders{-1, -1};

struct SavedLook {
    Look appearance;
    SliderValues sliders{unknownSliders};
    bool operator==(const SavedLook& other) const {
        return appearance == other.appearance && sliders == other.sliders;
    }
};

struct CarSetup {
    State state{};
    int primary{unsetPrimary}, secondary{unsetSecondary};
    unsigned rgb{}, effects{};
    unsigned audio{};
    bool lightsOff{};
};

struct Record : CarSetup {
    unsigned colors{};
    std::vector<SavedLook> history;
    SliderValues completedSliders{unknownSliders};
    bool legacyFormat{};
    bool pendingHistory{};
};

bool owns(const Record& record, int color) {
    return (record.state == State::installed || record.state == State::disabled) && color >= 0 && color < colorCount &&
        (record.colors & (1u << color));
}

std::wstring wideName(const char* name) {
    wchar_t text[64]{};
    if (!name || !MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, name, -1, text, static_cast<int>(std::size(text))))
        return L"Underglow";
    std::wstring result(text);
    for (auto& c : result)
        if (c < L' ' || c == 0xfffe || c == 0xffff) c = L' ';
    return result;
}

Look look(const CarSetup& record) {
    return {record.primary, record.secondary, record.rgb, record.effects};
}

SliderValues readSavedSliders() {
    SliderValues values;
    for (size_t i = 0; i < values.size(); ++i)
        values[i] = static_cast<int>(std::lround(std::clamp(savedSettings.sliders[i], sliderRows[i].min, sliderRows[i].max) * 1000));
    return values;
}

SliderValues sliderSnapshot() {
    AcquireSRWLockShared(&settingsLock);
    const auto values = readSavedSliders();
    ReleaseSRWLockShared(&settingsLock);
    return values;
}

bool sameSavedLook(const SavedLook& left, const SavedLook& right) {
    return left.appearance == right.appearance &&
        (left.sliders == unknownSliders || right.sliders == unknownSliders || left.sliders == right.sliders);
}

void removeMatchingHistory(Record& record, const SavedLook& lookToRemove) {
    const auto kept = std::remove_if(record.history.begin(), record.history.end(), [&](const SavedLook& entry) {
        return sameSavedLook(entry, lookToRemove);
    });
    record.history.erase(kept, record.history.end());
}

void remember(Record& record, const SavedLook& applied) {
    removeMatchingHistory(record, applied);
    record.history.insert(record.history.begin(), applied);
}

unsigned allEffects(int effect) {
    unsigned value = 0;
    for (size_t zone = 0; zone < zoneCount; ++zone) value |= static_cast<unsigned>(effect + 1) << (zone * 6);
    return value;
}

int zoneEffect(unsigned effects, size_t zone) {
    return static_cast<int>((effects >> (zone * 6)) & 63u) - 1;
}

bool hasUnderglowOn(unsigned effects, size_t zone) {
    // A color-only setup uses the default glow on all four parts.
    return !effects || zoneEffect(effects, zone) >= 0;
}

bool audioTargetAvailable(const CarSetup& record, int zone) {
    if (record.state != State::installed || record.lightsOff || zone < -1 || zone >= static_cast<int>(zoneCount)) return false;
    if (!record.effects) return true;
    if (record.effects == allEffects(zoneEffect(record.effects, fx::kZoneFront))) return zone == -1;
    return zone >= 0 && hasUnderglowOn(record.effects, zone);
}

int groupForEffect(int effect) {
    const auto* end = std::end(fx::kEffectOrder);
    const auto* match = std::find_if(std::begin(fx::kEffectOrder), end, [effect](const auto& entry) {
        return entry.effect == effect;
    });
    return match == end ? -1 : match->group;
}

bool hasTwoColorEffect(unsigned effects) {
    for (size_t zone = 0; zone < zoneCount; ++zone)
        if (groupForEffect(zoneEffect(effects, zone)) == fx::kGroupTwoColors) return true;
    return false;
}

std::wstring_view unlockMessage(int group, const Look& appearance) {
    const bool needsPrimary = (group == fx::kGroupOneColor || group == fx::kGroupTwoColors) &&
        appearance.primary == unsetPrimary;
    const bool needsSecondary = group == fx::kGroupTwoColors && appearance.secondary == unsetSecondary;
    if (needsPrimary && needsSecondary)
        return L"Apply Primary Underglow to Unlock\nApply Secondary Underglow to Unlock";
    if (needsPrimary) return L"Apply Primary Underglow to Unlock";
    if (needsSecondary) return L"Apply Secondary Underglow to Unlock";
    return {};
}

unsigned changeEffect(unsigned effects, int effect, int zone) {
    if (zone < 0) return allEffects(effect);
    const auto shift = zone * 6;
    return (effects & ~(63u << shift)) | (static_cast<unsigned>(effect + 1) << shift);
}

void clearEquippedLook(Record& record) {
    record.primary = unsetPrimary;
    record.secondary = unsetSecondary;
    record.rgb = record.effects = record.audio = 0;
    record.completedSliders = unknownSliders;
}

bool validEffects(unsigned effects) {
    if (effects > rgbMask) return false;
    for (size_t zone = 0; zone < zoneCount; ++zone) {
        const int effect = zoneEffect(effects, zone);
        if (effect >= fx::kEffectCount || isAudioEffect(effect)) return false;
    }
    return true;
}

bool validAudio(unsigned audio) {
    if (audio > rgbMask) return false;
    for (size_t zone = 0; zone < zoneCount; ++zone)
        if (const int effect = zoneEffect(audio, zone); effect >= 0 && !isAudioEffect(effect)) return false;
    return true;
}

enum class Category { standard, special, custom, owned, count };
constexpr const wchar_t* categories[]{L"Standard Colors", L"Special Colors", L"Custom Colors", L"Owned Colors"};
// Stable palette IDs, displayed in hue order like the Paint Body palette.
constexpr int standardColors[]{8, 4, 5, 6, 7, 0, 1, 2, 3};
constexpr int specialColors[]{14, 15, 9, 16, 10, 11, 12, 17, 13};

std::wstring colorName(int color) {
    if (color == tunedColor) return L"Fine-Tuned";
    return colorNames[color] ? wideName(colorNames[color]) : L"Color " + std::to_wstring(color + 1);
}

struct Palette {
    std::array<int, colorCount> ids{};
    size_t count{};
};

Palette palette(Category category, const Record& record) {
    Palette result;
    const auto add = [&](int color) {
        if (category != Category::owned || owns(record, color)) result.ids[result.count++] = color;
    };
    if (category == Category::standard || category == Category::owned)
        for (int color : standardColors) add(color);
    if (category == Category::special || category == Category::owned)
        for (int color : specialColors) add(color);
    if (category == Category::custom || category == Category::owned)
        for (int color = fx::kColorCount; color < fx::kColorTotal; ++color) add(color);
    if (category == Category::owned && owns(record, tunedColor)) add(tunedColor);
    return result;
}

SRWLOCK garageLock = SRWLOCK_INIT;
Id activeId{};
CarSetup activeRecord;
std::atomic<bool> installed{};
using PageFn = void (*)(void*);
PageFn originalDesignsLoaded, originalPaintOpen, originalPaintResume;
PageFn originalColorItems, originalColorAccept, originalColorTab, originalResume, originalLeave;
PageFn originalMarkup;
void (*originalTabScroll)(void*, void*);
void (*originalFineInput)(void*, int);
void (*originalColorHelp)(void*, Byte);
void (*originalSetHelpPrompt)(void*, int, Byte**);
Byte (*originalCustomFocus)(void*, Byte);
using ColorFocus = void (*)(void*, void*);
ColorFocus originalColorFocus;
struct Preview { Id id{}; Look appearance{}; bool active{}; bool enabled{true}; SliderValues sliders{unknownSliders}; };
Preview preview;
Id openAtUnderglows{};
Id enterFromDesigns{}, returnToDesigns{};
bool selectFirstOnEntry{};
bool garageTuningChanged{};
void* underglowPage{};
thread_local void* underglowHelpPanel{};
bool helpForNoUnderglow{};
Record displayed;
bool refreshPalette{};
bool returnFromRecall{};
bool returnFromHistoryEdit{};
int lastColorTab = -1;
int nextLockedTab = -1;
int lockedTabStep{};
bool restoringColorTab{};
int appliedEffectSwatch = installColor;
int recalledColor = installColor;

bool specialChoice(int choice) {
    return choice >= effectChoice && choice < effectChoice + fx::kEffectCount &&
        validEffects(allEffects(choice - effectChoice));
}

bool previousChoice(int choice) {
    return choice >= historyChoice && static_cast<size_t>(choice - historyChoice) < displayed.history.size();
}

Color resolvedColor(int color, unsigned rgb) {
    if (color != tunedColor) return colorAt(color);
    return {static_cast<float>((rgb >> 16) & 255u) / 255.0f,
        static_cast<float>((rgb >> 8) & 255u) / 255.0f, static_cast<float>(rgb & 255u) / 255.0f};
}

unsigned encodeColor(const Color& color) {
    const auto channel = [](float value) { return static_cast<unsigned>(std::lround(std::clamp(value, 0.0f, 1.0f) * 255.0f)); };
    return (channel(color.r) << 16) | (channel(color.g) << 8) | channel(color.b);
}

Color effectSecondary(const Color& first, const Color& second) {
    const unsigned rgb = encodeColor(first);
    if (rgb != encodeColor(second)) return second;
    return colorAt(rgb == 0xff00ff ? 0 : 13);
}

struct SwatchColors {
    unsigned first, second;
    bool rainbow{};
};

SwatchColors effectSwatch(const fx::GroupedEffect& entry, const Look& appearance) {
    const auto primary = resolvedColor(appearance.primary, appearance.rgb);
    const unsigned first = encodeColor(primary);
    if (entry.group == fx::kGroupOneColor) return {first, first};
    if (entry.group == fx::kGroupTwoColors) {
        return {first, encodeColor(effectSecondary(primary, resolvedColor(appearance.secondary, appearance.rgb)))};
    }
    if (entry.effect == fx::kEffectPolice) return {0xff0000, 0x0026ff};
    if (entry.effect == fx::kEffectSunrise) return {0xff1000, 0xffdd80};
    return {0xff0000, 0xa000ff, true};
}

struct HistorySwatch {
    SwatchColors colors;
    std::wstring name;
};

HistorySwatch historySwatch(const Look& appearance) {
    const int firstColor = appearance.primary == unsetPrimary ? appearance.secondary : appearance.primary;
    const unsigned first = encodeColor(resolvedColor(firstColor, appearance.rgb));
    const unsigned second = appearance.secondary == unsetSecondary ? first :
        encodeColor(resolvedColor(appearance.secondary, appearance.rgb));
    SwatchColors colors{first, second};

    std::array<int, zoneCount> effects;
    if (appearance.effects) {
        for (size_t zone = 0; zone < zoneCount; ++zone)
            effects[zone] = zoneEffect(appearance.effects, zone);
    } else {
        AcquireSRWLockShared(&settingsLock);
        effects = savedSettings.effects;
        ReleaseSRWLockShared(&settingsLock);
    }

    const auto primary = appearance.primary == unsetPrimary ? L"None" : colorName(appearance.primary);
    const auto secondary = appearance.secondary == unsetSecondary ? L"None" : colorName(appearance.secondary);
    const bool allParts = std::all_of(effects.begin(), effects.end(), [](int effect) { return effect >= 0; });
    std::wstring effectNames, partNames = allParts ? wideName(effectTargets[0]) : L"";
    std::array<bool, fx::kEffectCount> seen{};
    bool hasMulticolorSwatch = false;
    for (size_t zone = 0; zone < effects.size(); ++zone) {
        const int effect = effects[zone];
        if (effect < 0) continue;
        if (!allParts) {
            if (!partNames.empty()) partNames += L" + ";
            partNames += wideName(effectTargets[zone + 1]);
        }
        if (seen[effect]) continue;
        seen[effect] = true;
        if (!effectNames.empty()) effectNames += L" + ";
        effectNames += wideName(fx::kEffects[effect]);
        if (appearance.effects && !hasMulticolorSwatch && groupForEffect(effect) == fx::kGroupNoColor) {
            colors = effectSwatch({fx::kGroupNoColor, effect}, appearance);
            hasMulticolorSwatch = true;
        }
    }
    return {colors, primary + L" / " + secondary + L" / " + effectNames + L" / " + partNames};
}

void recordKey(wchar_t (&key)[24], Id id) {
    swprintf_s(key, L"Car_%u", id);
}

bool validSliderValues(const SliderValues& values) {
    if (values == unknownSliders) return true;
    for (size_t i = 0; i < values.size(); ++i)
        if (values[i] < std::lround(sliderRows[i].min * 1000) || values[i] > std::lround(sliderRows[i].max * 1000)) return false;
    return true;
}

bool appendHistoryEntries(const wchar_t* tail, Record& record) {
    while (*tail) {
        int end{};
        SavedLook saved;
        auto& entry = saved.appearance;
        if (swscanf_s(tail, L";%d,%d,%u,%u%n", &entry.primary, &entry.secondary, &entry.rgb, &entry.effects, &end) != 4 ||
            (entry.primary != unsetPrimary && !owns(record, entry.primary)) ||
            (entry.secondary != unsetSecondary && !owns(record, entry.secondary)) || entry.rgb > rgbMask ||
            !validEffects(entry.effects)) return false;
        tail += end;
        if (*tail == L',') {
            if (swscanf_s(tail, L",%d,%d%n", &saved.sliders[Brightness], &saved.sliders[Speed], &end) != 2 ||
                !validSliderValues(saved.sliders)) return false;
            tail += end;
        }
        record.history.push_back(saved);
    }
    return true;
}

void historyPageKey(wchar_t (&key)[48], Id id, size_t page) {
    swprintf_s(key, L"Car_%u_Page_%zu", id, page);
}

bool loadHistory(Id id, const wchar_t* key, Record& record) {
    const size_t total = GetPrivateProfileIntW(historySection, key, 0, ini);
    if (total > maxHistoryEntries) return false;
    const auto inlineCount = record.history.size();
    for (size_t start = 0; start < total; start += historyPageSize) {
        wchar_t pageKey[48], page[historyPageBuffer];
        historyPageKey(pageKey, id, start / historyPageSize);
        const auto length = GetPrivateProfileStringW(historySection, pageKey, L"", page,
            static_cast<DWORD>(std::size(page)), ini);
        const auto before = record.history.size();
        if (!length || length >= std::size(page) - 1 || !appendHistoryEntries(page, record) ||
            record.history.size() - before != std::min(historyPageSize, total - start)) return false;
    }
    if (inlineCount && total) {
        // An older build may have added inline entries after pages were saved.
        const auto firstPaged = record.history.begin() + inlineCount;
        const auto unique = std::remove_if(firstPaged, record.history.end(), [&](const SavedLook& entry) {
            return std::find(record.history.begin(), firstPaged, entry) != firstPaged;
        });
        record.history.erase(unique, record.history.end());
    }
    return record.history.size() <= maxHistoryEntries;
}

bool storeHistory(const wchar_t* path, Id id, const wchar_t* key, const std::vector<SavedLook>& history) {
    const size_t previous = GetPrivateProfileIntW(historySection, key, 0, path);
    if (history.size() > maxHistoryEntries || previous > maxHistoryEntries) return false;
    for (size_t start = 0; start < history.size(); start += historyPageSize) {
        wchar_t pageKey[48], page[historyPageBuffer]{};
        historyPageKey(pageKey, id, start / historyPageSize);
        size_t length = 0;
        for (size_t i = start, end = std::min(start + historyPageSize, history.size()); i < end; ++i) {
            const auto& saved = history[i];
            const auto& entry = saved.appearance;
            const int written = swprintf_s(page + length, std::size(page) - length, L";%d,%d,%u,%u,%d,%d",
                entry.primary, entry.secondary, entry.rgb, entry.effects, saved.sliders[Brightness], saved.sliders[Speed]);
            if (written < 0) return false;
            length += static_cast<size_t>(written);
        }
        if (!WritePrivateProfileStringW(historySection, pageKey, page, path)) return false;
    }
    for (size_t page = (history.size() + historyPageSize - 1) / historyPageSize;
        page < (previous + historyPageSize - 1) / historyPageSize; ++page) {
        wchar_t pageKey[48];
        historyPageKey(pageKey, id, page);
        if (!WritePrivateProfileStringW(historySection, pageKey, nullptr, path)) return false;
    }
    wchar_t count[24];
    swprintf_s(count, L"%zu", history.size());
    return WritePrivateProfileStringW(historySection, key, history.empty() ? nullptr : count, path) != 0;
}

Record load(Id id) {
    if (!id) return {};
    wchar_t key[24], data[512];
    recordKey(key, id);
    const auto length = GetPrivateProfileStringW(L"Garage", key, L"", data,
        static_cast<DWORD>(std::size(data)), ini);
    if (!length) return {};
    if (length >= std::size(data) - 1) return {{State::invalid}};
    unsigned state{}, legacyBalance{};
    int end{}, legacyColor{};
    Record record;
    if (swscanf_s(data, L"%u,%u,%d,%d,%u,%d%n", &state, &legacyBalance, &record.primary,
            &record.secondary, &record.colors, &legacyColor, &end) != 6 ||
        (state != 1 && state != 2 && state != 3) || record.colors > colorMask)
        return {{State::invalid}};
    // Older builds staged purchases as state 1. Applying a free setup starts a fresh record.
    if (state == 1) return {};
    record.state = static_cast<State>(state);
    record.legacyFormat = legacyColor != explicitSecondaryFormat;
    // Free setups use the old balance field for history and light-switch flags.
    if (record.state == State::installed && !record.legacyFormat && legacyBalance <= 3) {
        record.pendingHistory = (legacyBalance & 1u) != 0;
        record.lightsOff = supportExtraOptions && (legacyBalance & 2u) != 0;
    }
    const auto* tail = data + end;
    bool hasSnapshotFormat = false;
    if (*tail) {
        if (swscanf_s(tail, L",%u,%u%n", &record.rgb, &record.effects, &end) != 2 ||
            record.rgb > rgbMask || !validEffects(record.effects)) return {{State::invalid}};
        tail += end;
        if (*tail == L',') {
            if (swscanf_s(tail, L",%u%n", &record.audio, &end) != 1 || !validAudio(record.audio))
                return {{State::invalid}};
            tail += end;
        }
        if (*tail == L'|') {
            hasSnapshotFormat = true;
            if (swscanf_s(tail, L"|%d,%d%n", &record.completedSliders[Brightness], &record.completedSliders[Speed], &end) != 2 ||
                !validSliderValues(record.completedSliders)) return {{State::invalid}};
            tail += end;
        }
        if (!appendHistoryEntries(tail, record)) return {{State::invalid}};
    }
    if (!loadHistory(id, key, record)) return {{State::invalid}};
    if ((record.primary != unsetPrimary && !owns(record, record.primary)) ||
        (record.secondary != unsetSecondary && !owns(record, record.secondary)))
        return {{State::invalid}};
    if (record.legacyFormat && record.secondary == 0) {
        // Older records also used Cyan when no secondary was chosen. Keep it if
        // the current effect or a prior secondary selection establishes it.
        const bool usedSecondary = hasTwoColorEffect(record.effects) ||
            std::any_of(record.history.begin(), record.history.end(), [](const SavedLook& entry) {
                return entry.appearance.secondary > 0 || hasTwoColorEffect(entry.appearance.effects);
            });
        if (!usedSecondary) {
            record.secondary = unsetSecondary;
            for (auto& entry : record.history) entry.appearance.secondary = unsetSecondary;
        }
    }
    if (record.state == State::disabled) clearEquippedLook(record);
    if (!hasSnapshotFormat && record.state == State::installed) {
        // Older builds included the equipped look in Previous Underglows.
        removeMatchingHistory(record, {look(record)});
    }
    return record;
}

// Update the car record without replacing the settings or other cars.
bool store(Id id, const Record& record, const SliderValues& restoreSliders = unknownSliders) {
    wchar_t key[24], data[512];
    recordKey(key, id);
    int length = swprintf_s(data, L"%u,%u,%d,%d,%u,%d,%u,%u", static_cast<unsigned>(record.state),
        static_cast<unsigned>(record.pendingHistory) | (record.lightsOff ? 2u : 0u),
        record.primary, record.secondary, record.colors,
        explicitSecondaryFormat, record.rgb, record.effects);
    if (record.audio)
        length += swprintf_s(data + length, std::size(data) - length, L",%u", record.audio);
    length += swprintf_s(data + length, std::size(data) - length, L"|%d,%d",
        record.completedSliders[Brightness], record.completedSliders[Speed]);
    const bool ok = updateIni([&](const wchar_t* temp) {
        if (!WritePrivateProfileStringW(L"Garage", key, record.state == State::missing ? nullptr : data, temp)) return false;
        if (!storeHistory(temp, id, key, record.history)) return false;
        if (restoreSliders != unknownSliders) {
            for (size_t i = 0; i < restoreSliders.size(); ++i) {
                wchar_t value[32];
                swprintf_s(value, L"%.3f", restoreSliders[i] / 1000.0);
                if (!WritePrivateProfileStringW(section, sliderRows[i].iniKey, value, temp)) return false;
            }
        }
        return true;
    });
    if (ok && id == activeId) activeRecord = record;
    return ok;
}

struct Profile {
    void* user{};
    void* value{};

    Profile() {
        if (!read<void*>(game + 0x4d6fc00)) return;
        native<void** (*)(void**)>(0x14988e0)(&user);
        if (!user) return;
        const auto* vtable = read<const Byte*>(user);
        if (vtable) {
            auto get = read<void* (*)(void*)>(vtable + 0xf8);
            if (get) value = get(user);
        }
    }

    ~Profile() {
        const auto* table = user ? read<const Byte*>(user) : nullptr;
        if (table) {
            const auto release = read<PageFn>(table + 0x18);
            if (release) release(user);
        }
    }

    Profile(const Profile&) = delete;
    Profile& operator=(const Profile&) = delete;
};

void updateCar(Byte* car, ULONGLONG now) {
    if (!installed.load()) return;
    static Byte* previous;
    static ULONGLONG next;
    if (car == previous && now < next) return;
    previous = car;
    next = now + 250;
    const Profile profile;
    const Id id = profile.value ? native<Id (*)(void*)>(careerCar)(profile.value) : 0;
    AcquireSRWLockExclusive(&garageLock);
    if (id != activeId) {
        activeId = id;
        activeRecord = load(id);
    }
    ReleaseSRWLockExclusive(&garageLock);
}

CarSetup current() {
    AcquireSRWLockShared(&garageLock);
    const auto record = activeRecord;
    ReleaseSRWLockShared(&garageLock);
    return record;
}

bool selectableColor(const Record& record, int color) {
    return (color >= 0 && color < fx::kColorTotal) || (color == tunedColor && owns(record, color));
}

bool storeLook(Id id, Record& record, const Look& appearance, const SliderValues& currentSliders,
               const SliderValues& restoreSliders = unknownSliders, bool archiveEquipped = true) {
    const bool hasPrimary = appearance.primary != unsetPrimary;
    const bool hasSecondary = appearance.secondary != unsetSecondary;
    if (record.state == State::invalid || (hasPrimary && !selectableColor(record, appearance.primary)) ||
        (hasSecondary && !selectableColor(record, appearance.secondary)) || appearance.rgb > rgbMask ||
        !validEffects(appearance.effects))
        return false;
    for (size_t zone = 0; zone < zoneCount; ++zone) {
        const int effect = zoneEffect(appearance.effects, zone);
        if (effect == fx::kEffectStatic && hasSecondary) continue;
        if (!unlockMessage(groupForEffect(effect), appearance).empty()) return false;
    }
    const bool equipped = record.state == State::installed;
    const bool changed = !equipped || !(look(record) == appearance) ||
        (restoreSliders != unknownSliders && restoreSliders != currentSliders);
    if (archiveEquipped && equipped && changed && !record.pendingHistory) {
        const auto& previousSliders = record.completedSliders == unknownSliders ? currentSliders : record.completedSliders;
        remember(record, {look(record), previousSliders});
    }
    record.state = State::installed;
    record.primary = appearance.primary;
    record.secondary = appearance.secondary;
    record.rgb = appearance.rgb;
    record.effects = appearance.effects;
    record.lightsOff = false;
    if (hasPrimary) record.colors |= 1u << appearance.primary;
    if (hasSecondary) record.colors |= 1u << appearance.secondary;
    record.pendingHistory |= changed;
    return store(id, record, restoreSliders);
}

bool setColor(Id id, bool secondary, int color) {
    if (!id || color < 0) return false;
    const auto sliders = sliderSnapshot();
    AcquireSRWLockExclusive(&garageLock);
    auto record = load(id);
    auto appearance = look(record);
    (secondary ? appearance.secondary : appearance.primary) = color;
    const bool ok = storeLook(id, record, appearance, sliders);
    ReleaseSRWLockExclusive(&garageLock);
    return ok;
}

enum class ClearColor { primary, secondary, discardSetup };

Look withoutColor(Look appearance, bool secondary) {
    (secondary ? appearance.secondary : appearance.primary) = secondary ? unsetSecondary : unsetPrimary;
    for (size_t zone = 0; zone < zoneCount; ++zone) {
        if (!unlockMessage(groupForEffect(zoneEffect(appearance.effects, zone)), appearance).empty())
            appearance.effects = changeEffect(appearance.effects, fx::kEffectStatic, static_cast<int>(zone));
    }
    return appearance;
}

bool setNoUnderglow(Id id, ClearColor action = ClearColor::primary) {
    if (!id) return false;
    const auto sliders = sliderSnapshot();
    AcquireSRWLockExclusive(&garageLock);
    auto record = load(id);
    bool ok = record.state != State::invalid;
    if (record.state == State::installed) {
        const auto equipped = look(record);
        const auto remaining = withoutColor(equipped, action == ClearColor::secondary);
        if (action != ClearColor::discardSetup &&
            (remaining.primary != unsetPrimary || remaining.secondary != unsetSecondary)) {
            if (!(equipped == remaining)) ok = storeLook(id, record, remaining, sliders);
        } else {
            // Clearing both colors in one visit keeps the original setup that
            // the first clear archived, along with any other saved looks.
            const bool intermediate = record.pendingHistory && !record.history.empty() && !garageTuningChanged &&
                (withoutColor(record.history[0].appearance, false) == equipped ||
                 withoutColor(record.history[0].appearance, true) == equipped);
            if (action == ClearColor::discardSetup) removeMatchingHistory(record, {equipped, sliders});
            else if (!intermediate) {
                const auto& equippedSliders = !record.pendingHistory && !garageTuningChanged &&
                    record.completedSliders != unknownSliders ? record.completedSliders : sliders;
                remember(record, {equipped, equippedSliders});
            }
            clearEquippedLook(record);
            record.state = State::disabled;
            record.pendingHistory = false;
            record.lightsOff = false;
            ok = store(id, record);
            if (ok && action != ClearColor::discardSetup) log("Garage Underglow saved the equipped setup to history.");
        }
    }
    ReleaseSRWLockExclusive(&garageLock);
    return ok;
}

bool savePreviousUnderglow(Id id, bool tuningChanged = false) {
    const auto sliders = sliderSnapshot();
    AcquireSRWLockExclusive(&garageLock);
    auto record = load(id);
    bool ok = true;
    if (record.state == State::installed && (record.pendingHistory || tuningChanged || record.completedSliders == unknownSliders)) {
        if (!record.pendingHistory && tuningChanged && record.completedSliders != unknownSliders && record.completedSliders != sliders)
            remember(record, {look(record), record.completedSliders});
        removeMatchingHistory(record, {look(record), sliders});
        record.completedSliders = sliders;
        record.pendingHistory = false;
        ok = store(id, record);
    }
    ReleaseSRWLockExclusive(&garageLock);
    return ok;
}

bool deleteHistory(Id id, size_t index, const SavedLook& expected) {
    if (!id) return false;
    const auto sliders = sliderSnapshot();
    AcquireSRWLockExclusive(&garageLock);
    auto record = load(id);
    bool ok = false;
    auto visible = record;
    if (visible.state == State::installed) removeMatchingHistory(visible, {look(visible), sliders});
    if (index < visible.history.size() && visible.history[index] == expected) {
        const auto entry = std::find(record.history.begin(), record.history.end(), expected);
        record.history.erase(entry);
        ok = store(id, record);
    }
    ReleaseSRWLockExclusive(&garageLock);
    return ok;
}

bool setLook(Id id, const Look& appearance, const SliderValues& restoreSliders = unknownSliders, bool fromHistory = false) {
    if (!id) return false;
    AcquireSRWLockExclusive(&settingsLock);
    AcquireSRWLockExclusive(&garageLock);
    auto record = load(id);
    if (fromHistory) {
        const auto end = record.history.end();
        const auto entry = std::find(record.history.begin(), end, SavedLook{appearance, restoreSliders});
        if (entry == end) {
            ReleaseSRWLockExclusive(&garageLock);
            ReleaseSRWLockExclusive(&settingsLock);
            return false;
        }
        record.history.erase(entry);
    }
    record.audio = 0;
    const bool ok = storeLook(id, record, appearance, readSavedSliders(), restoreSliders, !fromHistory);
    if (ok && restoreSliders != unknownSliders) {
        for (size_t i = 0; i < restoreSliders.size(); ++i)
            draftSettings.sliders[i] = savedSettings.sliders[i] = restoreSliders[i] / 1000.0f;
    }
    ReleaseSRWLockExclusive(&garageLock);
    ReleaseSRWLockExclusive(&settingsLock);
    return ok;
}

Look withEffect(Look appearance, int effect, int zone = -1) {
    appearance.effects = changeEffect(0, effect, zone);
    return appearance;
}

bool setVisualEffect(Id id, int effect, int zone) {
    if (!id || effect < 0 || effect >= fx::kEffectCount || zone < -1 || zone >= static_cast<int>(zoneCount))
        return false;
    const auto sliders = sliderSnapshot();
    AcquireSRWLockExclusive(&garageLock);
    auto record = load(id);
    record.audio = 0;
    const bool ok = storeLook(id, record, withEffect(look(record), effect, zone), sliders);
    ReleaseSRWLockExclusive(&garageLock);
    return ok;
}

bool setAudio(int effect, int zone) {
    if (effect != -1 && !isAudioEffect(effect)) return false;
    AcquireSRWLockExclusive(&garageLock);
    auto record = load(activeId);
    bool ok = false;
    if (activeId && audioTargetAvailable(record, zone)) {
        const unsigned audio = changeEffect(record.audio, effect, zone);
        if (audio == record.audio) ok = true;
        else {
            record.audio = audio;
            ok = store(activeId, record);
        }
    }
    ReleaseSRWLockExclusive(&garageLock);
    return ok;
}

bool toggleLights() {
    if (!supportExtraOptions) return false;
    AcquireSRWLockExclusive(&garageLock);
    auto record = load(activeId);
    bool ok = false;
    if (record.state == State::installed) {
        record.lightsOff = !record.lightsOff;
        ok = store(activeId, record);
    }
    ReleaseSRWLockExclusive(&garageLock);
    return ok;
}

Id designCar(void* page) {
    const auto* bytes = static_cast<const Byte*>(page);
    const auto* table = read<const Byte*>(bytes);
    if ((table != game + designsVtable && table != game + paintVtable && table != game + colorVtable) ||
        !read<void*>(bytes + 0x400)) return 0;
    auto* flow = read<void*>(game + 0x4dbf670);
    if (!flow) return 0;
    const int id = native<int (*)(void*, int)>(0x597130)(flow, 0x33);
    return id > 0 ? static_cast<Id>(id) : 0;
}

void setPreview(Id id, const Look& appearance, bool enabled = true, const SliderValues& sliders = unknownSliders) {
    AcquireSRWLockExclusive(&garageLock);
    preview = {id, appearance, id != 0, enabled, sliders};
    ReleaseSRWLockExclusive(&garageLock);
}

void setPreview(Id id, int color, unsigned rgb = 0) {
    setPreview(color >= 0 ? id : 0, {color, color, rgb, allEffects(fx::kEffectStatic)});
}

Preview currentPreview() {
    AcquireSRWLockShared(&garageLock);
    const auto current = preview.id == activeId ? preview : Preview{};
    ReleaseSRWLockShared(&garageLock);
    return current;
}

bool patchDesigns(std::string_view source, std::string& result) {
    if (!installed.load() || source.find("x:Class=\"ForzaUI.A_LiveryModeSelect\"") == source.npos ||
        source.find(underglowButton) != source.npos) return false;
    const auto findTile = source.find("<local:FZButton x:Name=\"PART_Button_FindDesign\"");
    const auto end = findTile == source.npos ? source.npos : source.find("/>", findTile);
    if (end == source.npos) return false;
    const auto column = source.find("GridX=\"2\"", findTile);
    const auto width = source.find("A_Style_Button_Tile_6x2", findTile);
    if (column >= end || width >= end || column >= width) return false;
    constexpr std::string_view tile = R"XAML(<local:FZButton x:Name="PART_Button_UnderglowGarage"
                                        local:TileMenuNavigationManager.TileState="Enabled"
                                        local:TileMenuNavigationManager.GridX="2"
                                        local:TileMenuNavigationManager.GridY="0"
                                        Style="{StaticResource A_Style_Button_Tile_2x2}"
                                        Template="{StaticResource A_ControlTemplate_IconTile}"
                                        local:UIPageTransitionManager.TransitionIndex="3"
                                        local:BindingProxy.C="DisableMotionBlur">
                                    <local:FZButton.DataContext>
                                        <local:TileMenuDataItem Heading="UNDERGLOW"
                                            Icon="/ForzaUI;component/Textures/Anthem/Icons/Livery/Paint_Car_Icon.png" />
                                    </local:FZButton.DataContext>
                                </local:FZButton>
                                )XAML";
    result.assign(source);
    result.replace(width, sizeof("A_Style_Button_Tile_6x2") - 1, "A_Style_Button_Tile_4x2");
    result.replace(column, sizeof("GridX=\"2\"") - 1, "GridX=\"4\"");
    result.insert(findTile, tile);
    return true;
}

bool patchColorSelect(std::string_view source, std::string& result) {
    if (!installed.load() || source.find("x:Class=\"ForzaUI.A_ColorSelect\"") == source.npos ||
        source.find("UG_NativeCustomColor") != source.npos) return false;
    const auto begin = source.find("<Grid x:Name=\"CustomColorContainer\"");
    if (begin == source.npos) return false;
    const auto end = source.find("</Grid>", begin);
    if (end == source.npos || source.find("PART_Control_CustomColorItem", begin) > end) return false;
    constexpr std::string_view wrapper = R"XAML(<Grid x:Name="UG_NativeCustomColor">
        <Grid.Style>
            <Style TargetType="Grid">
                <Style.Triggers>
                    <DataTrigger Binding="{Binding Tag, RelativeSource={RelativeSource AncestorType=Page}}" Value="UNDERGLOW SELECT">
                        <Setter Property="Visibility" Value="Collapsed" />
                    </DataTrigger>
                </Style.Triggers>
            </Style>
        </Grid.Style>
    )XAML";
    constexpr std::string_view stockTitle = R"XAML(<ContentControl Tag="{Binding PreviousScreenTitle}" Content="{local:Loc ScreenTitles.IDS_Color_Select}")XAML";
    constexpr std::string_view title = R"XAML(<ContentControl Tag="{Binding PreviousScreenTitle}" Focusable="False" Margin="0,40,0,0"
                                    local:BindingProxy.A="/ForzaUI;component/Textures/Anthem/Icons/Titles/Painting.png">
                            <ContentControl.Style>
                                <Style TargetType="ContentControl" BasedOn="{StaticResource A_Style_ContentControl_ScreenTitleBlock}">
                                    <Setter Property="Content" Value="{local:Loc ScreenTitles.IDS_Color_Select}" />
                                    <Style.Triggers>
                                        <DataTrigger Binding="{Binding Tag, RelativeSource={RelativeSource AncestorType=Page}}" Value="UNDERGLOW SELECT">
                                            <Setter Property="Content" Value="UNDERGLOW SELECT" />
                                        </DataTrigger>
                                    </Style.Triggers>
                                </Style>
                            </ContentControl.Style>
                        </ContentControl>)XAML";
    const auto titleStart = source.find(stockTitle);
    const auto titleEnd = titleStart == source.npos ? source.npos : source.find("/>", titleStart + stockTitle.size());
    if (titleEnd == source.npos || titleEnd >= begin) return false;
    const auto largeSwatch = source.find("x:Key=\"A_ControlTemplate_LargeColorItem\"");
    const auto stripe = largeSwatch == source.npos ? source.npos : source.find("Color=\"{Binding StripeColor}\"", largeSwatch);
    const auto stripeEnd = stripe == source.npos ? source.npos : source.find("/>", stripe);
    if (stripeEnd == source.npos || stripeEnd >= titleStart) return false;
    constexpr std::string_view panelTag = "<VirtualizingStackPanel";
    const auto panelStart = source.find("<VirtualizingStackPanel Margin=\"2,0,0,0\"");
    if (panelStart == source.npos || panelStart >= stripeEnd) return false;
    constexpr std::string_view rainbow = R"XAML(
                        <Rectangle IsHitTestVisible="False">
                            <Rectangle.Fill>
                                <LinearGradientBrush StartPoint="0.5,0" EndPoint="0.5,1">
                                    <GradientStop Color="#FFFF3344" Offset="0" />
                                    <GradientStop Color="#FFFFB000" Offset="0.18" />
                                    <GradientStop Color="#FF32EA66" Offset="0.37" />
                                    <GradientStop Color="#FF00C8FF" Offset="0.55" />
                                    <GradientStop Color="#FF324CFF" Offset="0.72" />
                                    <GradientStop Color="#FFC032FF" Offset="0.89" />
                                    <GradientStop Color="#FFFF3388" Offset="1" />
                                </LinearGradientBrush>
                            </Rectangle.Fill>
                            <Rectangle.Style>
                                <Style TargetType="Rectangle">
                                    <Setter Property="Visibility" Value="Collapsed" />
                                    <Style.Triggers>
                                        <MultiDataTrigger>
                                            <MultiDataTrigger.Conditions>
                                                <Condition Binding="{Binding HasStripe}" Value="True" />
                                                <Condition Binding="{Binding Tag, RelativeSource={RelativeSource AncestorType=Page}}" Value="UNDERGLOW SELECT" />
                                            </MultiDataTrigger.Conditions>
                                            <Setter Property="Visibility" Value="Visible" />
                                        </MultiDataTrigger>
                                    </Style.Triggers>
                                </Style>
                            </Rectangle.Style>
                        </Rectangle>)XAML";
    result.assign(source);
    result.insert(end + sizeof("</Grid>") - 1, "</Grid>");
    result.insert(begin, wrapper);
    result.replace(titleStart, titleEnd + 2 - titleStart, title);
    result.insert(stripeEnd + 2, rainbow);
    result.insert(panelStart + panelTag.size(), " x:Name=\"" + std::string(swatchPanelName) + "\"");
    return true;
}

void release(void* pointer) {
    auto* bytes = static_cast<Byte*>(pointer);
    if (bytes && --*reinterpret_cast<int*>(bytes + 0x10) == 0)
        native<PageFn>(0x31eb20)(pointer);
}

bool patchOptionsMarkup(std::string_view source, std::string& result);

void loadMarkup(void* loader) {
    originalMarkup(loader);
    auto& original = *reinterpret_cast<Byte**>(static_cast<Byte*>(loader) + 0x38);
    if (!original) return;
    const int length = read<int>(original + 0x30);
    const auto* text = read<const char*>(original + 0x28);
    if (length <= 0 || length > 256 * 1024 || !core::pe::readable(text, static_cast<size_t>(length))) return;
    std::string replacement;
    const std::string_view source{text, static_cast<size_t>(length)};
    if (!patchDesigns(source, replacement) && !patchColorSelect(source, replacement) &&
        !patchOptionsMarkup(source, replacement) &&
        !anna::patchMarkup(source, replacement)) return;
    Byte* buffer = nullptr;
    native<Byte** (*)(Byte**, unsigned)>(0x208f320)(&buffer, static_cast<unsigned>(replacement.size() + 1));
    auto* data = buffer ? read<char*>(buffer + 0x28) : nullptr;
    if (!data) { release(buffer); return; }
    std::memcpy(data, replacement.c_str(), replacement.size() + 1);
    *reinterpret_cast<int*>(buffer + 0x30) = static_cast<int>(replacement.size());
    release(original);
    original = buffer;
}

Byte* uiText(std::wstring_view text) {
    Byte* box = nullptr;
    const auto* chars = text.data();
    const auto length = static_cast<int>(text.size());
    native<Byte** (*)(Byte**, const wchar_t* const*, const int*)>(0x1ed12b0)(&box, &chars, &length);
    return box;
}

void setPopupText(Byte* popup, size_t field, size_t property, Byte* text) {
    auto& stored = *reinterpret_cast<Byte**>(popup + field);
    release(stored);
    stored = text;
    if (native<bool (*)(void*)>(0x2117810)(popup))
        native<void (*)(void*, void*)>(0x56d4f0)(popup, native<void* (*)()>(property)());
}

bool addChoice(Byte* list, int row, int action, std::wstring_view text) {
    auto* label = uiText(text);
    if (!label) return false;
    auto* type = native<void* (*)()>(0x127ba60)();
    auto* item = native<Byte* (*)(size_t, void*)>(0x1f72ce0)(0x60, type);
    if (!item) { release(label); return false; }
    native<PageFn>(0x2120730)(item);
    *reinterpret_cast<const Byte**>(item) = game + enumItemVtable;
    *reinterpret_cast<int*>(item + 0x50) = action;
    *reinterpret_cast<Byte**>(item + 0x58) = label;
    const auto* vtable = read<const Byte*>(list);
    read<void (*)(void*, int, void*)>(vtable + 0x40)(list, row, item);
    release(item);
    return true;
}

enum class View { swatch, historyActions, unavailable };
enum Choice { back, applyLook, effectAll, effectPart,
    removeEquipped = effectPart + static_cast<int>(zoneCount), deletePrevious };
struct Request {
    void* page{};
    Id id{};
    View view{};
    int color{installColor};
};
Request nextView;

Record recordFor(Id id);

// Inline function object consumed by the native MessageBox action list.
struct PopupAction {
    void** table;
    Request request;
    void* popup;
    unsigned char padding[0x10]{};
    PopupAction* implementation{};
};
static_assert(offsetof(PopupAction, padding) == 0x28);
static_assert(sizeof(PopupAction) == 0x40);

void* copyPopup(PopupAction* from, void* to) {
    if (to) std::memcpy(to, from, offsetof(PopupAction, padding));
    return to;
}

void choose(PopupAction* action) {
    auto request = action->request;
    if (request.page != underglowPage || designCar(request.page) != request.id) return;
    auto* selected = native<Byte* (*)(void*)>(0x1607210)(action->popup);
    if (!selected || read<const Byte*>(selected) != game + enumItemVtable) return;
    const int value = read<int>(selected + 0x50);
    if (value == back) return;
    bool saved = false;
    if (request.view == View::historyActions && previousChoice(request.color)) {
        const size_t index = request.color - historyChoice;
        if (value == removeEquipped) {
            saved = setNoUnderglow(request.id, ClearColor::discardSetup);
            if (saved) log("Garage Underglow removed the equipped setup and kept its history.");
        } else if (value == deletePrevious) {
            saved = deleteHistory(request.id, index, displayed.history[index]);
            if (saved) log("Garage Underglow deleted the selected previous setup.");
        } else return;
        if (saved) {
            refreshPalette = returnFromHistoryEdit = true;
            return;
        }
    } else if (value == applyLook && previousChoice(request.color)) {
        const auto& previous = displayed.history[request.color - historyChoice];
        saved = setLook(request.id, previous.appearance, previous.sliders, true);
        if (saved) returnFromRecall = true;
    } else if (value >= effectAll && value < effectPart + static_cast<int>(zoneCount) && specialChoice(request.color)) {
        saved = setVisualEffect(request.id, request.color - effectChoice, value - effectPart);
    } else return;
    if (saved) {
        if (specialChoice(request.color)) {
            appliedEffectSwatch = request.color;
            setPreview(request.id, look(recordFor(request.id)));
        }
        refreshPalette = true;
        log("Garage Underglow selection applied.");
    } else {
        request.view = View::unavailable;
        nextView = request;
    }
}

void* popupType(PopupAction*) { return nullptr; }
void deletePopup(PopupAction*, unsigned char) {}
void* popupData(PopupAction* action) { return &action->request.page; }
void* popupTable[]{reinterpret_cast<void*>(copyPopup), reinterpret_cast<void*>(copyPopup),
    reinterpret_cast<void*>(choose), reinterpret_cast<void*>(popupType),
    reinterpret_cast<void*>(deletePopup), reinterpret_cast<void*>(popupData)};

void show(Request request) {
    const bool color = request.color >= 0 && request.color < colorCount;
    const bool effect = specialChoice(request.color);
    const bool previous = previousChoice(request.color);
    if (request.page != underglowPage || !request.id || designCar(request.page) != request.id ||
        (!color && !effect && !previous))
        return;
    Byte* list = nullptr;
    native<Byte** (*)(Byte**)>(0x110d5c0)(&list);
    if (!list) return;
    std::wstring heading = color ? colorName(request.color) : effect ? wideName(fx::kEffects[request.color - effectChoice]) :
        L"Previous Underglow " + std::to_wstring(request.color - historyChoice + 1);
    std::wstring_view body;
    int row = 0;
    bool ok = true;
    const auto add = [&](int action, std::wstring_view text) {
        if (ok) ok = addChoice(list, row++, action, text);
    };
    if (request.view == View::unavailable) {
        body = L"Underglow could not save this change.";
    } else if (request.view == View::historyActions) {
        heading = L"Delete Underglow";
        body = L"Choose which setup to remove.";
        if (displayed.state == State::installed) add(removeEquipped, L"Delete Equipped Underglow");
        add(deletePrevious, L"Delete This Previous Underglow");
    } else if (effect) {
        body = L"Choose where to apply this effect.";
        add(effectAll, wideName(effectTargets[0]));
        for (int zone : {fx::kZoneFront, fx::kZoneRear, fx::kZoneLeft, fx::kZoneRight})
            add(effectPart + zone, wideName(effectTargets[zone + 1]));
    } else if (previous) {
        body = L"Restore the colors and effects from this setup.";
        add(applyLook, L"Apply Underglow");
    }
    add(back, L"Back");
    if (!ok) { release(list); return; }
    auto* title = uiText(heading);
    auto* description = body.empty() ? nullptr : uiText(body);
    auto* popup = native<Byte* (*)(void*)>(0x13fdee0)(request.page);
    if (popup) {
        // The stock localized setters take a different string type. These fields own AVUI strings.
        setPopupText(popup, 0x470, 0x161f750, title);
        setPopupText(popup, 0x478, 0x161e9e0, description);
        native<void (*)(void*, void*)>(0x1616a20)(popup, list + 0x38);
    } else {
        release(title);
        release(description);
    }
    release(list);
    if (!popup) return;
    native<void (*)(void*, int)>(0x1598950)(popup, 6);
    PopupAction action{popupTable, request, popup};
    action.implementation = &action;
    native<void (*)(void*, int, PopupAction*, unsigned char)>(0x1598860)(popup, 2, &action, 1);
}

void openStockPaint(void* page) {
    enterFromDesigns = returnToDesigns = openAtUnderglows = 0;
    originalPaintOpen(page);
}

void openUnderglowTile(void* page) {
    if (read<const Byte*>(page) != game + designsVtable) return;
    enterFromDesigns = designCar(page);
    if (!enterFromDesigns) return;
    returnToDesigns = openAtUnderglows = 0;
    log("Designs and Paints requested Underglow Select.");
    originalPaintOpen(page);
}

void designsLoaded(void* page) {
    originalDesignsLoaded(page);
    if (read<const Byte*>(page) != game + designsVtable) return;
    // The game's binding helper takes a 16-byte member target and copies it.
    struct ButtonHandler { PageFn invoke; int adjustment; int padding; };
    const ButtonHandler handler{openUnderglowTile, 0, 0};
    native<void (*)(void*, const char*, const ButtonHandler*)>(0x14b6610)(page, underglowButton, &handler);
}

Byte* selectedAt(void* page, size_t offset) {
    auto* selector = read<Byte*>(static_cast<Byte*>(page) + offset);
    Byte* item = nullptr;
    if (selector) native<Byte** (*)(void*, Byte**)>(0x11e0ec0)(selector, &item);
    return item;
}

int collectionCount(const Byte* list) {
    const auto* data = read<const Byte*>(list + 0x50);
    return data ? read<int>(data + 0x30) : 0;
}

void append(Byte* list, Byte* item) {
    const auto* table = read<const Byte*>(list);
    read<void (*)(void*, int, void*)>(table + 0x40)(list, collectionCount(list), item);
}

void setValue(void* object, void* property, void* value) {
    native<void (*)(void*, void*, void*)>(0x2145760)(object, property, value);
}

void setSwatchName(Byte* swatch, std::wstring_view name) {
    auto* label = uiText(name);
    if (!label) return;
    const auto* holder = native<const Byte* (*)()>(0x1249bb0)();
    setValue(swatch, read<void*>(holder + 0x20), label);
    release(label);
}

bool setSwatchColor(Byte* item, size_t property, unsigned rgb) {
    XMFLOAT4 rgba;
    native<void (*)(XMFLOAT4*, unsigned)>(0x211fcc0)(&rgba, 0xff000000u | rgb);
    Byte* boxed = nullptr;
    native<Byte** (*)(Byte**, const XMFLOAT4*)>(0x1249670)(&boxed, &rgba);
    if (!boxed) return false;
    const auto* holder = native<const Byte* (*)()>(property)();
    setValue(item, read<void*>(holder + 0x20), boxed);
    release(boxed);
    return true;
}

void setSwatchFlag(Byte* item, size_t property, bool enabled) {
    Byte* boxed = nullptr;
    native<Byte** (*)(Byte**, const bool*)>(0x8a2460)(&boxed, &enabled);
    const auto* holder = native<const Byte* (*)()>(property)();
    if (boxed) setValue(item, read<void*>(holder + 0x20), boxed);
    release(boxed);
}

bool updateSwatch(Byte* item, int choice, SwatchColors colors, std::wstring_view name) {
    *reinterpret_cast<int*>(item + 0x58) = swatchMarker + choice;
    if (!setSwatchColor(item, 0x1249f20, colors.first) || !setSwatchColor(item, 0x1249fd0, colors.second)) return false;
    setSwatchFlag(item, 0x1249d10, colors.rainbow); // HasStripe
    if (choice == noUnderglowChoice) setSwatchFlag(item, 0x1249dc0, true); // IsClearColor
    setSwatchName(item, name);
    return true;
}

enum Tab { primaryColors, secondaryColors, oneColorEffects, twoColorEffects, multicolorEffects, previous, tabCount };
constexpr const wchar_t* tabTitles[]{L"PRIMARY UNDERGLOW", L"SECONDARY UNDERGLOW", L"EFFECTS WITH 1 COLOR",
    L"EFFECTS WITH 2 COLORS", L"MULTICOLOR EFFECTS", L"PREVIOUS UNDERGLOWS"};
static_assert(std::size(tabTitles) == tabCount);
constexpr std::wstring_view emptyHistoryMessage = L"There are no previous underglows for this car.";

bool isColorTab(int tab) {
    return tab == primaryColors || tab == secondaryColors;
}

int effectGroupForTab(int tab) {
    return tab >= oneColorEffects && tab <= multicolorEffects ? fx::kGroupOneColor + tab - oneColorEffects : -1;
}

bool effectInTab(int tab, int choice) {
    const int group = effectGroupForTab(tab);
    return group >= 0 && groupForEffect(choice - effectChoice) == group;
}

int focusedTab(void* page) {
    auto* group = selectedAt(page, 0x448);
    const int marker = group && read<const Byte*>(group) == game + collectionVtable ? read<int>(group + 0xa8) : 0;
    release(group);
    return marker >= swatchMarker && marker < swatchMarker + tabCount ? marker - swatchMarker : -1;
}

bool scrollSelectedTab(void* page) {
    auto* selector = read<void*>(static_cast<Byte*>(page) + 0x448);
    auto* group = selectedAt(page, 0x448);
    const int marker = group && read<const Byte*>(group) == game + collectionVtable ? read<int>(group + 0xa8) : 0;
    const bool isUnderglowTab = marker >= swatchMarker && marker < swatchMarker + tabCount;
    if (isUnderglowTab) originalTabScroll(selector, group);
    release(group);
    return isUnderglowTab;
}

void scrollTabItem(void* selector, void* item) {
    // The stock bumper command scrolls to its original destination after the selection callback.
    if (!underglowPage || selector != read<void*>(static_cast<Byte*>(underglowPage) + 0x448) ||
        !scrollSelectedTab(underglowPage))
        originalTabScroll(selector, item);
}

void selectIndex(void* page, size_t offset, int index);
void focusTabSwatch(void* page, int tab);
void colorHelp(void* page, Byte advancedCamera);

void restoreTab(void* page, int tab) {
    restoringColorTab = true;
    selectIndex(page, 0x448, tab);
    scrollSelectedTab(page);
    focusTabSwatch(page, tab);
    restoringColorTab = false;
}

bool showTabNotice(void* page, bool emptyHistory, std::wstring_view message) {
    // Use the stock acknowledgement dialog and its A / OK controller action.
    auto* popup = native<Byte* (*)(void*)>(0x13c5750)(page);
    if (!popup) return false;
    setPopupText(popup, 0x470, 0x161f750, uiText(emptyHistory ? L"PREVIOUS UNDERGLOWS" : L"UNDERGLOW LOCKED"));
    setPopupText(popup, 0x478, 0x161e9e0, uiText(message));
    native<void (*)(void*, int)>(0x1598950)(popup, 0);
    log(emptyHistory ? "Garage Underglow opened the empty history notice." :
        "Garage Underglow opened the locked effects tab notice.");
    return true;
}

void visitTab(void* page, int tab, int step) {
    const bool emptyHistory = tab == previous && displayed.history.empty();
    const auto message = emptyHistory ? emptyHistoryMessage :
        unlockMessage(effectGroupForTab(tab), look(displayed));
    if (message.empty()) {
        lastColorTab = tab;
        appliedEffectSwatch = installColor;
        recalledColor = installColor;
        if (tab != focusedTab(page)) restoreTab(page, tab);
        else focusTabSwatch(page, tab);
        return;
    }

    setPreview(0, installColor);
    restoreTab(page, lastColorTab >= 0 ? lastColorTab : primaryColors);
    if (!emptyHistory) {
        nextLockedTab = tab + step;
        lockedTabStep = step;
    }
    if (!showTabNotice(page, emptyHistory, message)) nextLockedTab = -1;
}

void colorTabChanged(void* page) {
    originalColorTab(page);
    if (page != underglowPage || selectFirstOnEntry || restoringColorTab || nextLockedTab >= 0 || !designCar(page))
        return;
    const int selected = focusedTab(page);
    if (selected >= 0 && selected != lastColorTab)
        visitTab(page, selected, selected > lastColorTab ? 1 : -1);
}

int focusedChoice(void* page) {
    auto* item = selectedAt(page, 0x450);
    const int marker = item && read<const Byte*>(item) == game + swatchVtable ? read<int>(item + 0x58) : 0;
    release(item);
    if (marker < swatchMarker) return installColor;
    const int choice = marker - swatchMarker;
    return choice <= noUnderglowChoice || previousChoice(choice) ? choice : installColor;
}

Byte** collectionEntries(Byte* list) {
    const auto* storage = list ? read<const Byte*>(list + 0x50) : nullptr;
    const auto* array = storage ? read<const Byte*>(storage + 0x28) : nullptr;
    return array ? read<Byte**>(array + 0x28) : nullptr;
}

void selectIndex(void* page, size_t offset, int index) {
    auto* selector = read<void*>(static_cast<Byte*>(page) + offset);
    if (!selector) return;
    Byte* boxed = nullptr;
    native<Byte** (*)(Byte**, const int*)>(0x53cba0)(&boxed, &index);
    setValue(selector, read<void*>(game + 0x4fdfeb0), boxed);
    release(boxed);
}

void colorFocus(void* page, void* source);

void jumpSwatchStrip(void* swatches, int index) {
    const auto* table = read<const Byte*>(swatches);
    if (!table) return;
    const auto templateChild = read<Byte* (*)(void*, Byte*)>(table + 0x388);
    if (!templateChild) return;
    Byte* name = nullptr;
    native<Byte** (*)(Byte**, const char*)>(0x20c6e50)(&name, swatchPanelName);
    if (!name) return;
    auto* panel = templateChild(swatches, name);
    release(name);
    if (!panel || read<const Byte*>(panel) != game + swatchPanelVtable) return;

    // The virtualizing panel scrolls horizontally in whole swatch units.
    auto* scroll = panel + 0x168;
    const auto* scrollTable = read<const Byte*>(scroll);
    if (scrollTable == game + swatchScrollVtable)
        read<void (*)(void*, float)>(scrollTable + 0xb8)(scroll, static_cast<float>(index));
}

void focusTabSwatch(void* page, int tab) {
    auto* swatches = read<void*>(static_cast<Byte*>(page) + 0x450);
    if (!swatches) return;

    auto* group = selectedAt(page, 0x448);
    const int count = group && read<const Byte*>(group) == game + collectionVtable ? collectionCount(group) : 0;
    auto** items = count ? collectionEntries(group) : nullptr;
    const int color = isColorTab(tab) ? (tab == primaryColors ? displayed.primary : displayed.secondary) : installColor;
    int index = 0;
    if (items && color >= 0 && color < colorCount) {
        for (int i = 1; i < count; ++i) {
            auto* item = items[i];
            if (item && read<const Byte*>(item) == game + swatchVtable && read<int>(item + 0x58) == swatchMarker + color) {
                index = i;
                break;
            }
        }
    }
    release(group);
    if (!count) return;

    selectIndex(page, 0x450, index);
    jumpSwatchStrip(swatches, index);
    native<void (*)(void*, int)>(0x1ee56a0)(swatches, index);
    colorFocus(page, nullptr);
}

void resumePaint(void* page) {
    originalPaintResume(page);
    auto* bytes = static_cast<Byte*>(page);
    const Id id = designCar(page);
    if (!id || read<void*>(bytes + 0x218)) return;
    if (returnToDesigns == id) {
        returnToDesigns = 0;
        // Match Paint Car's own Back action for a clean paint session.
        native<PageFn>(0x370790)(read<void*>(bytes + 0x400));
        bytes[0x410] = bytes[0x414] = 1;
        auto* transition = game + 0x4e25c58;
        if (read<void*>(bytes + 0x280)) {
            native<void (*)(void*, void*)>(0x1f046d0)(page, transition + 0x10);
        } else {
            const auto* table = read<const Byte*>(transition);
            read<void (*)(void*, void*)>(table + 0x10)(transition, page);
        }
        log("Underglow returned through Paint Car's Back transition.");
    } else if (enterFromDesigns == id) {
        enterFromDesigns = 0;
        selectIndex(page, 0x430, 0);
        auto* item = selectedAt(page, 0x430);
        const bool body = item && read<const Byte*>(item) == game + paintItemVtable && read<int>(item + 0x60) == 8;
        release(item);
        if (body) {
            openAtUnderglows = id;
            native<PageFn>(0x14d65a0)(page);
        }
    }
}

void tagPage(void* page, const wchar_t* title) {
    auto* label = title ? uiText(title) : nullptr;
    if (auto* tag = read<void*>(game + 0x4fe8b38)) setValue(page, tag, label);
    release(label);
}

#include "GarageOptions.h"

bool fillSwatches(Byte* group, Tab tab, const Record& record) {
    int index = 0;
    const int count = collectionCount(group);
    const int effectGroup = effectGroupForTab(tab);
    if (!unlockMessage(effectGroup, look(record)).empty()) {
        if (count) native<PageFn>(0x12481a0)(group);
        return true;
    }
    auto** items = count ? collectionEntries(group) : nullptr;
    if (count && !items) return false;
    const auto add = [&](int choice, SwatchColors colors, std::wstring_view name) {
        if (index < count) {
            auto* item = items[index++];
            return item && read<const Byte*>(item) == game + swatchVtable && updateSwatch(item, choice, colors, name);
        }
        Byte* item = nullptr;
        native<Byte** (*)(Byte**)>(0x123ecb0)(&item);
        const bool ok = item && updateSwatch(item, choice, colors, name);
        if (ok) append(group, item);
        release(item);
        return ok;
    };
    bool ok = true;
    if (isColorTab(tab)) {
        ok = add(noUnderglowChoice, {}, L"No Underglow");
        for (auto category : {Category::standard, Category::special, Category::custom}) {
            if (ok && category == Category::custom && owns(record, tunedColor))
                ok = add(tunedColor, {record.rgb, record.rgb}, colorName(tunedColor));
            const auto colors = palette(category, record);
            for (size_t i = 0; i < colors.count && ok; ++i) {
                const int color = colors.ids[i];
                const unsigned rgb = encodeColor(colorAt(color));
                ok = add(color, {rgb, rgb}, colorName(color));
            }
        }
    } else if (effectGroup >= 0) {
        for (const auto& entry : fx::kEffectOrder) {
            if (entry.group != effectGroup) continue;
            const auto name = wideName(fx::kEffects[entry.effect]);
            if (!add(effectChoice + entry.effect, effectSwatch(entry, look(record)), name)) {
                ok = false;
                break;
            }
        }
    } else {
        for (size_t i = 0; i < record.history.size() && ok; ++i) {
            const auto swatch = historySwatch(record.history[i].appearance);
            ok = add(historyChoice + static_cast<int>(i), swatch.colors, swatch.name);
        }
        if (ok && index < count) {
            const auto* table = read<const Byte*>(group);
            const auto removeAt = read<void (*)(void*, int)>(table + 0x60);
            for (int tail = count; tail > index;) removeAt(group, --tail);
        }
    }
    return ok;
}

Byte* makeTab(Tab tab, const Record& record) {
    Byte* group = nullptr;
    native<Byte** (*)(Byte**)>(0x14b7bf0)(&group);
    if (!group) return nullptr;
    auto* title = uiText(tabTitles[tab]);
    if (!title) { release(group); return nullptr; }
    auto& stored = *reinterpret_cast<Byte**>(group + 0xa0);
    release(stored);
    stored = title;
    *reinterpret_cast<int*>(group + 0x68) = 3; // Native swatch strip without the paint-only custom card.
    *reinterpret_cast<int*>(group + 0xa8) = swatchMarker + tab;
    if (!fillSwatches(group, tab, record)) { release(group); return nullptr; }
    return group;
}

Record recordFor(Id id) {
    const auto sliders = sliderSnapshot();
    AcquireSRWLockShared(&garageLock);
    auto record = load(id);
    ReleaseSRWLockShared(&garageLock);
    if (record.state == State::installed) removeMatchingHistory(record, {look(record), sliders});
    return record;
}

bool fillTabs(void* page, Id id) {
    const auto record = recordFor(id);
    Byte* list = nullptr;
    native<Byte** (*)(Byte**)>(0x13c78b0)(&list);
    if (!list) return false;
    for (int tab = 0; tab < tabCount; ++tab) {
        auto* group = makeTab(static_cast<Tab>(tab), record);
        if (!group) { release(list); return false; }
        append(list, group);
        release(group);
    }
    displayed = record;
    native<void (*)(void*, void*)>(0x15045f0)(page, list);
    release(list);
    return true;
}

void updateTabs(void* page, Id id) {
    displayed = recordFor(id);
    auto* list = read<Byte*>(static_cast<Byte*>(page) + 0x438);
    auto** groups = collectionEntries(list);
    if (!groups || collectionCount(list) != tabCount) return;
    for (int i = 0; i < tabCount; ++i) {
        auto* group = groups[i];
        if (group && read<const Byte*>(group) == game + collectionVtable && read<int>(group + 0xa8) == swatchMarker + i)
            fillSwatches(group, static_cast<Tab>(i), displayed);
    }
}

void refreshEffectSwatches(void* page, Id id) {
    const auto oldLook = look(displayed);
    displayed = recordFor(id);
    nextLockedTab = -1;
    if (oldLook.primary == displayed.primary && oldLook.secondary == displayed.secondary) return;
    auto* list = read<Byte*>(static_cast<Byte*>(page) + 0x438);
    auto** groups = collectionEntries(list);
    if (!groups || collectionCount(list) != tabCount) return;
    for (const auto tab : {oneColorEffects, twoColorEffects}) {
        auto* group = groups[tab];
        if (group && read<const Byte*>(group) == game + collectionVtable &&
            read<int>(group + 0xa8) == swatchMarker + tab)
            fillSwatches(group, tab, displayed);
    }
}

void colorItems(void* page) {
    originalColorItems(page);
    const Id id = designCar(page);
    if (!installed.load() || !id || (openAtUnderglows != id && underglowPage != page)) return;
    const bool opening = openAtUnderglows == id;
    openAtUnderglows = 0;
    underglowPage = page;
    if (opening) {
        lastColorTab = primaryColors;
        helpForNoUnderglow = false;
        nextLockedTab = -1;
        appliedEffectSwatch = installColor;
        recalledColor = installColor;
        returnFromRecall = false;
        returnFromHistoryEdit = false;
        garageTuningChanged = false;
    }
    if (fillTabs(page, id)) {
        if (opening) selectFirstOnEntry = true;
        tagPage(page, L"UNDERGLOW SELECT");
        log("Native Underglow Select loaded color, effect-group and previous setup tabs.");
    }
}

void colorFocus(void* page, void* source) {
    if (page != underglowPage) {
        originalColorFocus(page, source);
        return;
    }
    const Id id = designCar(page);
    const int choice = focusedChoice(page);
    const int tab = focusedTab(page);
    const bool noUnderglow = isColorTab(tab) && choice == noUnderglowChoice;
    if (choice != installColor && noUnderglow != helpForNoUnderglow &&
        !read<void*>(static_cast<Byte*>(page) + 0x218)) colorHelp(page, 0);
    if (returnFromRecall || (tab == primaryColors && choice == recalledColor && recalledColor != installColor)) return;
    recalledColor = installColor;
    if (choice != installColor && choice != appliedEffectSwatch) appliedEffectSwatch = installColor;
    if (noUnderglow) {
        const auto appearance = withoutColor(look(displayed), tab == secondaryColors);
        setPreview(id, appearance, appearance.primary != unsetPrimary || appearance.secondary != unsetSecondary);
    }
    else if (isColorTab(tab) && choice >= 0 && choice < colorCount) setPreview(id, choice, displayed.rgb);
    else if (effectInTab(tab, choice)) {
        if (!unlockMessage(effectGroupForTab(tab), look(displayed)).empty()) setPreview(0, installColor);
        else if (choice != appliedEffectSwatch) setPreview(id, withEffect(look(displayed), choice - effectChoice));
    } else if (tab == previous && previousChoice(choice)) {
        const auto& previous = displayed.history[choice - historyChoice];
        setPreview(id, previous.appearance, true, previous.sliders);
    }
    // The native list can temporarily have no selected item while moving between swatches.
    // Keep the current preview until the next swatch receives focus.
}

void colorAccept(void* page) {
    if (page != underglowPage) {
        originalColorAccept(page);
        return;
    }
    const int choice = focusedChoice(page);
    const int tab = focusedTab(page);
    const Id id = designCar(page);
    if (!id) return;
    if (isColorTab(tab) && choice == noUnderglowChoice) {
        if (setNoUnderglow(id, tab == secondaryColors ? ClearColor::secondary : ClearColor::primary)) {
            refreshEffectSwatches(page, id);
            refreshPalette = true;
            log("Garage Underglow cleared the selected color.");
        } else log("Could not save No Underglow.");
    } else if (isColorTab(tab) && choice >= 0 && choice < colorCount) {
        if (setColor(id, tab == secondaryColors, choice)) {
            refreshEffectSwatches(page, id);
            refreshPalette = true;
            log("Garage Underglow color applied.");
        } else show({page, id, View::unavailable, choice});
    } else if ((effectInTab(tab, choice) && unlockMessage(effectGroupForTab(tab), look(displayed)).empty()) ||
        (tab == previous && previousChoice(choice))) {
        show({page, id, View::swatch, choice});
    }
}

void resumeColor(void* page) {
    originalResume(page);
    if (page != underglowPage || read<void*>(static_cast<Byte*>(page) + 0x218)) return;
    closeOptions(page);
    const Id id = designCar(page);
    if (!id) return;
    if (nextLockedTab >= 0) {
        const int next = std::exchange(nextLockedTab, -1);
        visitTab(page, next, lockedTabStep);
        return;
    }
    if (selectFirstOnEntry) {
        auto* bytes = static_cast<Byte*>(page);
        if (!read<void*>(bytes + 0x450) || !read<void*>(bytes + 0x448)) return;
        selectFirstOnEntry = false;
        restoreTab(page, primaryColors);
    } else if (refreshPalette) {
        refreshPalette = false;
        updateTabs(page, id);
        if (returnFromHistoryEdit) {
            returnFromHistoryEdit = false;
            lastColorTab = primaryColors;
            nextLockedTab = -1;
            appliedEffectSwatch = recalledColor = installColor;
            restoreTab(page, primaryColors);
            setPreview(0, installColor);
            colorHelp(page, 0);
            if (displayed.history.empty()) showTabNotice(page, true, emptyHistoryMessage);
            return;
        }
        if (returnFromRecall) {
            lastColorTab = primaryColors;
            nextLockedTab = -1;
            restoreTab(page, primaryColors);
            recalledColor = displayed.primary == unsetPrimary ? noUnderglowChoice : displayed.primary;
            setPreview(id, look(displayed));
            returnFromRecall = false;
            colorHelp(page, 0);
            return;
        }
    } else if (focusedTab(page) < 0) selectIndex(page, 0x448, 0);
    colorFocus(page, nullptr);
    if (nextView.page == page) {
        const auto request = nextView;
        nextView = {};
        show(request);
    }
}

void leaveColor(void* page) {
    if (page == underglowPage) {
        closeOptions(page);
        returnToDesigns = designCar(page);
        if (returnToDesigns && !savePreviousUnderglow(returnToDesigns, garageTuningChanged))
            log("Could not finish saving the Garage Underglow setup.");
        setPreview(0, installColor);
        underglowPage = nullptr;
        nextView = {};
        refreshPalette = false;
        returnFromRecall = false;
        returnFromHistoryEdit = false;
        selectFirstOnEntry = false;
        garageTuningChanged = false;
        lastColorTab = -1;
        nextLockedTab = -1;
        appliedEffectSwatch = installColor;
        recalledColor = installColor;
    }
    originalLeave(page);
}

void setHelpPrompt(void* panel, int button, Byte** label) {
    if (underglowHelpPanel && panel == underglowHelpPanel) {
        // The native setter consumes the caller's reference.
        if (button == 2 || button == 5) {
            release(*label);
            return;
        }
        if (button == 0) {
            if (auto* apply = uiText(L"Apply")) {
                release(*label);
                originalSetHelpPrompt(panel, button, &apply);
                return;
            }
        }
    }
    originalSetHelpPrompt(panel, button, label);
}

void colorHelp(void* page, Byte advancedCamera) {
    if (page != underglowPage) {
        originalColorHelp(page, advancedCamera);
        return;
    }
    auto* panel = native<void* (*)(void*)>(0x1498a10)(page);
    auto* previousPanel = underglowHelpPanel;
    underglowHelpPanel = panel;
    originalColorHelp(page, advancedCamera);
    underglowHelpPanel = previousPanel;
    if (!panel) return;
    const int tab = focusedTab(page);
    const int choice = focusedChoice(page);
    helpForNoUnderglow = isColorTab(tab) && choice == noUnderglowChoice;
    const auto* prompt = !advancedCamera && hasGarageOptions(tab, choice) ? L"Options" :
        (tab == previous && previousChoice(choice) ? L"Delete" : nullptr);
    if (!prompt) return;
    auto* label = uiText(prompt);
    if (label) native<void (*)(void*, int, Byte**)>(0x112b4e0)(panel, 2, &label); // Native X prompt.
}

void paintFineInput(void* page, int channel) {
    if (page != underglowPage) {
        if (!optionsParent) optionsOverlay = nullptr;
        originalFineInput(page, channel);
        return;
    }
    // Color Select maps X to fine tune channel zero and Y to channel one.
    if (channel != 0 || read<void*>(static_cast<Byte*>(page) + 0x218)) return;
    const int tab = focusedTab(page);
    const int choice = focusedChoice(page);
    const Id id = designCar(page);
    if (!id) return;
    if (hasGarageOptions(tab, choice)) {
        const int effect = effectInTab(tab, choice) ? choice - effectChoice : fx::kEffectStatic;
        openOptions(page, id, static_cast<Tab>(tab), effect, look(displayed));
        return;
    }
    if (tab == previous && previousChoice(choice)) show({page, id, View::historyActions, choice});
}

Byte paintCustomFocus(void* page, Byte force) {
    // Paint's left-edge handler clears the swatch and focuses its custom paint card.
    // The card is hidden in Underglow Select, so let the swatch list handle left.
    return page == underglowPage ? 0 : originalCustomFocus(page, force);
}

bool install() {
    if (!api->hook_function || read<const Byte*>(game + designsVtable + 0x578) != game + 0x14ddf60 ||
        read<const Byte*>(game + paintVtable + 0x580) != game + 0x14e3d00 ||
        read<const Byte*>(game + colorVtable + 0x498) != game + 0x14d90b0 ||
        read<const Byte*>(game + colorVtable + 0x580) != game + 0x14e34c0 ||
        read<const Byte*>(game + colorVtable + 0x588) != game + 0x14e2ec0 ||
        read<const Byte*>(game + optionsVtable + 0x580) != game + 0x15e3c30 ||
        !core::platform_menu::matches(game + 0x14b8910, "\x40\x57\x48\x83\xec\x30", 6) ||
        !core::platform_menu::matches(game + careerCar, "\x40\x53\x48\x83\xec\x40", 6))
        return false;
    struct Hook { size_t rva; const char* name; const char* prefix; void* callback; void** original; };
    const Hook hooks[]{
        {0x14ddf60, "Designs and Paints Underglow tile", "\x48\x8b\xc4\x55\x41\x56", reinterpret_cast<void*>(designsLoaded), reinterpret_cast<void**>(&originalDesignsLoaded)},
        {0x14d1f70, "Designs and Paints stock Paint Car", "\x40\x55\x53\x56\x57\x41", reinterpret_cast<void*>(openStockPaint), reinterpret_cast<void**>(&originalPaintOpen)},
        {0x14e3d00, "Underglow Paint Body transition", "\x40\x57\x48\x83\xec\x40", reinterpret_cast<void*>(resumePaint), reinterpret_cast<void**>(&originalPaintResume)},
        {0x14d90b0, "Underglow Select categories", "\x40\x55\x53\x56\x57\x41", reinterpret_cast<void*>(colorItems), reinterpret_cast<void**>(&originalColorItems)},
        {0x14d8ae0, "Underglow Select tab selection", "\x40\x55\x56\x57\x48\x83", reinterpret_cast<void*>(colorTabChanged), reinterpret_cast<void**>(&originalColorTab)},
        {0x1f04a50, "Underglow Select tab strip scrolling", "\x40\x57\x48\x83\xec\x40", reinterpret_cast<void*>(scrollTabItem), reinterpret_cast<void**>(&originalTabScroll)},
        {0x14d8230, "Color Select Underglow preview", "\x40\x55\x56\x57\x41\x56", reinterpret_cast<void*>(colorFocus), reinterpret_cast<void**>(&originalColorFocus)},
        {0x14d8570, "Color Select Underglow selection", "\x40\x55\x53\x56\x57\x41", reinterpret_cast<void*>(colorAccept), reinterpret_cast<void**>(&originalColorAccept)},
        {0x14e34c0, "Color Select Underglow resume", "\x40\x55\x56\x57\x48\x83", reinterpret_cast<void*>(resumeColor), reinterpret_cast<void**>(&originalResume)},
        {0x14e2ec0, "Color Select Underglow close", "\x40\x57\x48\x83\xec\x40", reinterpret_cast<void*>(leaveColor), reinterpret_cast<void**>(&originalLeave)},
        {0x14f3410, "Color Select Underglow Options and history deletion", "\x48\x8b\xc4\x55\x56\x57", reinterpret_cast<void*>(paintFineInput), reinterpret_cast<void**>(&originalFineInput)},
        {0x14f7870, "Color Select Underglow Options and Delete prompts", "\x40\x55\x56\x57\x41\x56", reinterpret_cast<void*>(colorHelp), reinterpret_cast<void**>(&originalColorHelp)},
        {0x15e3c30, "Garage Underglow Options live preview", "\x48\x8b\xc4\x55\x57\x41", reinterpret_cast<void*>(optionsUpdate), reinterpret_cast<void**>(&originalOptionsUpdate)},
        {0x156d260, "Garage Underglow Options Apply and Back prompts", "\x48\x8b\xc4\x4c\x89\x48", reinterpret_cast<void*>(optionsPrompt), reinterpret_cast<void**>(&originalOptionsPrompt)},
        {0x112b4e0, "Underglow Select Apply and Save prompts", "\x4c\x89\x44\x24\x18\x89", reinterpret_cast<void*>(setHelpPrompt), reinterpret_cast<void**>(&originalSetHelpPrompt)},
        {0x14f70a0, "Color Select paint-only left navigation", "\x40\x57\x48\x83\xec\x30", reinterpret_cast<void*>(paintCustomFocus), reinterpret_cast<void**>(&originalCustomFocus)},
        {0x21dd6c0, "Underglow Select markup", "\x40\x55\x53\x56\x57\x41", reinterpret_cast<void*>(loadMarkup), reinterpret_cast<void**>(&originalMarkup)},
    };
    std::array<FH3Hook*, std::size(hooks)> enabled{};
    for (size_t i = 0; i < std::size(hooks); ++i) {
        const auto& hook = hooks[i];
        if (core::platform_menu::matches(game + hook.rva, hook.prefix, 6))
            enabled[i] = api->hook_function(mod, hook.name, game + hook.rva, hook.callback, hook.original);
        if (!enabled[i] || !api->enable_hooks(mod, enabled[i])) {
            while (i) api->disable_hooks(mod, enabled[--i]);
            return false;
        }
    }
    installed.store(true);
    return true;
}
} // namespace garage
