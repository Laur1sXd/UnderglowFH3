#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <DirectXMath.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <climits>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cwchar>
#include <cwctype>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "sdk/fh3mod.h"
#include "Edition.h"
#include "Effects.h"
#include "platform_menu.hpp"

namespace {
using namespace DirectX;
namespace fx = underglow_effects;
using Byte = unsigned char;
using Id = uint32_t;
constexpr Id invalid = UINT32_MAX;
constexpr char modName[] = "Underglow";
constexpr char modVersion[] = "1.0.0";
constexpr char modDescription[] = "Customize per-car Underglow in Designs and Paints.";

// Xbox One Opus 1.0.125.0, as mapped by Azimuth.
constexpr DWORD timestamp = 0x5b16ecca;
constexpr DWORD imageSize = 0x5495000;
constexpr size_t playerTickCall = 0x18b8e16;
constexpr size_t carLightsCall = 0x1e7d0f1;
constexpr size_t sceneLightsCall = 0x1d9c7de;
constexpr size_t uploadLightsCall = 0x1d9c804;
constexpr size_t copyLightPacketCall = 0x1b6a617;
constexpr size_t viewLightFactorCall = 0x1dd7583;
constexpr size_t daytimeTrackShaderCall = 0x1e530e0;
constexpr size_t twilightTrackShader = 0x468d9a0;
constexpr size_t clusterLightCheck = 0x1b65bd0;
constexpr size_t playerManager = 0x4e5bb48;
constexpr size_t getSimulation = 0x1724f70;
constexpr size_t simulationAccessor = 0x1ee7230;
constexpr size_t carBoundsAccessor = 0x1e400c7;
constexpr size_t carBoundsFallback = 0x1e4010e;
constexpr size_t carPartList = 0x1c03fd2;
constexpr size_t currentUpgrade = 0x1bf2ff0;
constexpr size_t upgradeBounds = 0x1c049ed;
constexpr size_t standardPartBounds = 0x1c03617;
constexpr size_t carSceneType = 0x4676340;
constexpr size_t standardPartType = 0x4675dd0;
constexpr size_t upgradablePartType = 0x4675ee8;
constexpr size_t upgradeType = 0x4675ed8;

constexpr size_t createPreset = 0x1b64f20;
constexpr size_t destroyPreset = 0x1b69270;
constexpr size_t createLight = 0x1b64df0;
constexpr size_t destroyLight = 0x1b69010;
constexpr size_t assignPreset = 0x1b69780;
constexpr size_t setPosition = 0x1b69720;
constexpr size_t setRotation = 0x1b69a00;
constexpr size_t setIntensity = 0x1b69560;

constexpr size_t lightStride = 0x40;
constexpr size_t presetStride = 0xc0;

constexpr size_t zoneCount = 4;
constexpr std::array<size_t, zoneCount> zoneStarts{0, 6, 16, 22};
constexpr std::array<size_t, zoneCount> zoneSizes{6, 10, 6, 10};
constexpr size_t lightCount = zoneStarts.back() + zoneSizes.back();
// FH2 uses these extents when the car's locator bounds are unavailable.
constexpr float defaultHalfWidth = 0.8f;
constexpr float defaultHalfLength = 2.0f;
constexpr float defaultHeight = 0.01f;
constexpr float defaultRange = 1.0f;
constexpr float defaultPower = 4.0f;
constexpr float defaultFrontInset = 0.5f;
constexpr float defaultRearInset = 0.7f;
constexpr float defaultSideOffset = -0.2f;
constexpr float defaultSideTrim = 0.8f;
constexpr wchar_t section[] = L"Underglow";

struct Color {
    float r, g, b;
};

std::array<const char*, fx::kColorTotal> colorNames{};
Color colorAt(int i);
std::array<std::string, fx::kCustomCount> customNames;
constexpr const char* effectTargets[] = {"All", "Front", "Right Side", "Rear", "Left Side"};
constexpr const wchar_t* zoneEffectKeys[] = {L"FrontEffect", L"RightEffect", L"RearEffect", L"LeftEffect"};

enum Option { Primary, Secondary, Front, Right, Rear, Left };
enum Slider { Brightness, Speed };

struct OptionRow {
    const char* key;
    const wchar_t* iniKey;
    const char* label;
    const char* description;
    int initial;
    int max{1};
};
constexpr OptionRow optionRows[] = {
    {nullptr, L"PrimaryColor", nullptr, nullptr, 0, fx::kColorTotal - 1},
    {nullptr, L"SecondaryColor", nullptr, nullptr, 0, fx::kColorTotal - 1},
    {"Front", L"Front", "Underglow Front", "Enable the lights at the front of the car.", 1},
    {"Right", L"Right", "Underglow Right", "Enable the lights along the right side of the car.", 1},
    {"Rear", L"Rear", "Underglow Rear", "Enable the lights at the rear of the car.", 1},
    {"Left", L"Left", "Underglow Left", "Enable the lights along the left side of the car.", 1},
};

struct SliderRow {
    const wchar_t* iniKey;
    float initial, min, max, scale;
};
constexpr SliderRow sliderRows[] = {
    {L"Brightness", 0.5f, 0, 3, 100},
    {L"Speed", 0.65f, 0.1f, 3, 1},
};

struct Settings {
    std::array<int, std::size(optionRows)> options{};
    std::array<float, std::size(sliderRows)> sliders{};
    std::array<int, zoneCount> effects{};
    unsigned garageRgb{};
    bool lightsEnabled{};
};
static_assert(Front + zoneCount == std::size(optionRows));

const FH3ModApi* api;
FH3Mod* mod;
Byte* game;
wchar_t ini[MAX_PATH];
SRWLOCK iniWriteLock = SRWLOCK_INIT;
SRWLOCK settingsLock = SRWLOCK_INIT;
Settings savedSettings, draftSettings;
struct RowBinding {
    void* descriptor{};
    void* model{};
};
struct RowPreview {
    void* page{};
    const void* value{};
    int selectedRow{};
    RowBinding binding{};
};
RowPreview draftRow;
thread_local RowBinding changingRow;
thread_local bool refreshingRow;
bool legacyPalette;
std::array<std::array<int, fx::kEffectCount>, fx::kGroupCount> effectIds{};
std::array<uint32_t, fx::kGroupCount> effectChoiceCounts{};

const core::platform_menu::PlatformMenu* platformMenu;
constexpr size_t entrySize = 0xb0;
constexpr size_t entryMod = 0xa0;
constexpr int acceptButton = 0xf4240;
constexpr int backButton = 0xf423f;
const Byte* platform;
std::atomic<bool> menuReady{};
using MenuButtons = bool (*)(void*, int, bool);
using SetProperty = void (*)(void*, void*, void*);
using NotifyProperty = void (*)(void*, void*);
MenuButtons originalMenuButtons;
SetProperty originalRowSet, originalSelectionSet;
NotifyProperty notifyProperty;
const Byte* selectedRowAddress;
std::array<Color, fx::kCustomCount> customColors;
ULONGLONG animationStart;
double effectSeconds(ULONGLONG tick) {
    return static_cast<double>(tick >= animationStart ? tick - animationStart : 0) * 0.001;
}

bool autoFit;
float halfWidth, halfLength, height, range, power;
float frontInset, rearInset, sideOffset, sideTrim;

// Layout consumed by FH3's ForwardPlus preset constructor.
struct SmallString {
    char text[16]{};
    size_t length{};
    size_t capacity{15};
};
struct alignas(16) Preset {
    uint32_t type{3};
    float padding[3]{};
    XMFLOAT4 color{1, 1, 1, 0};
    float range{defaultRange};
    float power{defaultPower};
    float innerAngle{60.0f};
    float outerAngle{170.0f};
    float extra{};
    float reserved{};
    SmallString projectedTexture{};
    uint32_t flags{6};
    // FH2 neon presets have no time-of-day threshold or fade band.
    float extras[4]{};
};
static_assert(sizeof(Preset) == 0x70);
static_assert(offsetof(Preset, projectedTexture) == 0x38);
static_assert(offsetof(Preset, extras) == 0x5c);

using PlayerUpdate = void (*)(void*, float);
using Update = void (*)(void*);
PlayerUpdate originalPlayer;
Update originalCarLights;
Update originalSceneLights;

struct CarBounds {
    XMFLOAT4 min;
    XMFLOAT4 max;
};

struct BodyParts {
    CarBounds shell{};
    CarBounds frontBumper{};
    CarBounds rearBumper{};
    CarBounds sideSkirts{};
};

struct EndStrip {
    float halfWidth{}, z{};
};

struct CarExtents {
    float halfWidth{}, front{}, rear{};
    EndStrip frontBumper{}, rearBumper{};
    CarBounds sideSkirts{};
};

struct Snapshot {
    Byte* car{};
    Byte* presentation{};
    ULONGLONG playerTick{};
    Byte* manager{};
    ULONGLONG matrixTick{};
    XMFLOAT4X4 matrix{};
    CarExtents extents{};
};
SRWLOCK snapshotLock = SRWLOCK_INIT;
Snapshot snapshot;

struct World {
    Byte* manager{};
    Byte* lightPool{};
    Byte* presetPool{};
    std::array<Id, lightCount> presets{};
    std::array<Id, lightCount> lights{};
    std::array<Id, lightCount> generations{};
    ULONGLONG retryAfter{};
    bool reportedActive{};
    bool reportedCapacity{};

    World() { lights.fill(invalid); presets.fill(invalid); }
};
// The scene callback is the only code that changes the light pools.
World world;
SRWLOCK worldLock = SRWLOCK_INIT;

template<class T> bool read(const void* address, T& value) {
    __try {
        std::memcpy(&value, address, sizeof(T));
        return true;
    } __except (GetExceptionCode() == EXCEPTION_ACCESS_VIOLATION
                    ? EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH) {
        return false;
    }
}

template<class T> T read(const void* address) {
    T value{};
    read(address, value);
    return value;
}

template<class T> T native(size_t offset) {
    return reinterpret_cast<T>(game + offset);
}

void log(const char* line) { api->log(mod, line); }

template<class Edit>
bool updateIni(Edit edit) {
    wchar_t folder[MAX_PATH], temp[MAX_PATH];
    wcscpy_s(folder, ini);
    auto* separator = std::wcsrchr(folder, L'\\');
    if (!separator) return false;
    *separator = L'\0';
    if (!GetTempFileNameW(folder, L"UGL", 0, temp)) return false;

    AcquireSRWLockExclusive(&iniWriteLock);
    bool ok = (CopyFileW(ini, temp, FALSE) || GetLastError() == ERROR_FILE_NOT_FOUND) && edit(temp);
    if (ok) {
        WritePrivateProfileStringW(nullptr, nullptr, nullptr, temp);
        ok = MoveFileExW(temp, ini, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
    }
    ReleaseSRWLockExclusive(&iniWriteLock);
    if (!ok) DeleteFileW(temp);
    return ok;
}

bool initializeIni(const wchar_t* azimuthDir) {
    if (_snwprintf_s(ini, std::size(ini), _TRUNCATE, L"%ls\\fh3-mods\\Underglow.ini", azimuthDir) < 0)
        return false;
    wchar_t legacy[MAX_PATH];
    if (GetFileAttributesW(ini) == INVALID_FILE_ATTRIBUTES &&
        _snwprintf_s(legacy, std::size(legacy), _TRUNCATE,
            L"%ls\\fh3-mods\\UnderglowGarageSettings.ini", azimuthDir) >= 0 &&
        !CopyFileW(legacy, ini, TRUE) && GetLastError() != ERROR_FILE_NOT_FOUND)
        return false;

    wchar_t existingGarage[4];
    if (GetPrivateProfileSectionW(L"Garage", existingGarage, std::size(existingGarage), ini) ||
        _snwprintf_s(legacy, std::size(legacy), _TRUNCATE,
            L"%ls\\fh3-mods\\UnderglowGarage.ini", azimuthDir) < 0 ||
        GetFileAttributesW(legacy) == INVALID_FILE_ATTRIBUTES)
        return true;

    std::vector<wchar_t> records(4096);
    while (true) {
        const auto length = GetPrivateProfileSectionW(L"Garage", records.data(), static_cast<DWORD>(records.size()), legacy);
        if (!length) return true;
        if (length < records.size() - 2) break;
        if (records.size() >= (1u << 20)) return false;
        records.resize(records.size() * 2);
    }
    return updateIni([&](const wchar_t* temp) {
        return WritePrivateProfileSectionW(L"Garage", records.data(), temp) != 0;
    });
}

bool isAudioEffect(int effect) {
    return std::any_of(std::begin(fx::kEffectOrder), std::end(fx::kEffectOrder), [effect](const auto& item) {
        return item.group == fx::kGroupAudio && item.effect == effect;
    });
}

#include "Garage.h"

int getInt(const wchar_t* key, int fallback, int low, int high) {
    return std::clamp(static_cast<int>(GetPrivateProfileIntW(section, key, fallback, ini)), low, high);
}

float getFloat(const wchar_t* key, float fallback, float low, float high) {
    wchar_t value[48]{};
    GetPrivateProfileStringW(section, key, L"", value, static_cast<DWORD>(std::size(value)), ini);
    wchar_t* end;
    const float parsed = std::wcstof(value, &end);
    return end != value && *end == L'\0' && std::isfinite(parsed)
        ? std::clamp(parsed, low, high) : fallback;
}

Color getColor(const wchar_t* key, Color fallback) {
    wchar_t value[96]{};
    GetPrivateProfileStringW(L"CustomColors", key, L"", value, static_cast<DWORD>(std::size(value)), ini);
    auto* start = value;
    while (std::iswspace(*start)) ++start;
    auto* end = start + std::wcslen(start);
    while (end != start && std::iswspace(end[-1])) *--end = L'\0';
    const auto* hex = start + (*start == L'#');
    if (std::wcslen(hex) == 6 && std::wcsspn(hex, L"0123456789abcdefABCDEF") == 6) {
        const unsigned long rgb = std::wcstoul(hex, nullptr, 16);
        constexpr float scale = 1.0f / 255.0f;
        return {static_cast<float>((rgb >> 16) & 255) * scale,
                static_cast<float>((rgb >> 8) & 255) * scale, static_cast<float>(rgb & 255) * scale};
    }
    for (auto& ch : value) if (ch == L',') ch = L' ';
    float rgb[3];
    wchar_t extra;
    if (swscanf_s(start, L"%f %f %f %lc", &rgb[0], &rgb[1], &rgb[2], &extra, 1u) != 3) return fallback;
    for (float channel : rgb) if (!std::isfinite(channel) || channel < 0 || channel > 255) return fallback;
    const float scale = std::max({rgb[0], rgb[1], rgb[2]}) > 1 ? 1.0f / 255.0f : 1.0f;
    return {rgb[0] * scale, rgb[1] * scale, rgb[2] * scale};
}

void loadPalette() {
    for (size_t i = 0; i < std::size(fx::kColors); ++i) colorNames[i] = fx::kColors[i].name;
    for (int i = 0; i < fx::kCustomCount; ++i) {
        wchar_t key[16], name[32];
        swprintf_s(key, L"Color%d", i + 1);
        const auto& rgb = fx::kCustomDefaults[i];
        customColors[i] = getColor(key, {rgb[0], rgb[1], rgb[2]});
        swprintf_s(key, L"Name%d", i + 1);
        GetPrivateProfileStringW(L"CustomColors", key, L"", name, static_cast<DWORD>(std::size(name)), ini);
        char utf8[128]{};
        if (*name && WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, name, -1, utf8,
            static_cast<int>(std::size(utf8)), nullptr, nullptr)) customNames[i] = utf8;
        else customNames[i] = "Custom " + std::to_string(i + 1);
        colorNames[fx::kColorCount + i] = customNames[i].c_str();
    }
}

void* optionsPage() {
    if (!menuReady.load() || !read<Byte>(platform + platformMenu->in_hub)) return nullptr;
    const auto page = read<void*>(platform + platformMenu->installed_page);
    const int view = read<int>(platform + platformMenu->view);
    if (!page || view < 1 || view > 64) return nullptr;
    const auto begin = read<uintptr_t>(platform + platformMenu->entries);
    const auto end = read<uintptr_t>(platform + platformMenu->entries + sizeof(uintptr_t));
    if (!begin || end < begin || (end - begin) % entrySize ||
        static_cast<size_t>(view) > (end - begin) / entrySize) return nullptr;
    const auto* entry = reinterpret_cast<const Byte*>(begin) + (view - 1) * entrySize;
    return read<FH3Mod*>(entry + entryMod) == mod ? page : nullptr;
}

bool saveDraft() {
    if (savedSettings.options == draftSettings.options && savedSettings.effects == draftSettings.effects) return true;

    const bool ok = updateIni([](const wchar_t* temp) {
        wchar_t value[32];
        for (size_t i = 0; i < std::size(optionRows); ++i) {
            if (draftSettings.options[i] == savedSettings.options[i] && !(legacyPalette && (i == Primary || i == Secondary))) continue;
            swprintf_s(value, L"%d", draftSettings.options[i]);
            if (!WritePrivateProfileStringW(section, optionRows[i].iniKey, value, temp)) return false;
        }
        for (size_t i = 0; i < zoneCount; ++i) {
            if (draftSettings.effects[i] == savedSettings.effects[i]) continue;
            swprintf_s(value, L"%d", draftSettings.effects[i]);
            if (!WritePrivateProfileStringW(section, zoneEffectKeys[i], value, temp)) return false;
        }
        if (legacyPalette && !WritePrivateProfileStringW(section, L"PaletteVersion", L"2", temp)) return false;
        return true;
    });
    if (ok) legacyPalette = false;
    return ok;
}

// Called with settingsLock held. SDK setters do not call our change callbacks.
void syncRows(const Settings& before, const Settings& after) {
    for (size_t i = 0; i < std::size(optionRows); ++i) {
        if (optionRows[i].key && before.options[i] != after.options[i])
            api->set_setting(mod, optionRows[i].key, after.options[i]);
    }
}

RowBinding discardDraft() {
    const auto binding = draftRow.binding;
    syncRows(draftSettings, savedSettings);
    draftSettings = savedSettings;
    draftRow = {};
    return binding;
}

int selectedRow() {
    return read<int>(selectedRowAddress);
}

void refreshRow(RowBinding row) {
    if (!row.model || !row.descriptor) return;
    const bool wasRefreshing = refreshingRow;
    refreshingRow = true;
    notifyProperty(row.model, row.descriptor);
    refreshingRow = wasRefreshing;
}

void menuRowSet(void* descriptor, void* model, void* value) {
    const auto previous = changingRow;
    changingRow = {descriptor, model};
    originalRowSet(descriptor, model, value);
    changingRow = previous;
}

void menuSelectionSet(void* descriptor, void* model, void* value) {
    originalSelectionSet(descriptor, model, value);
    RowBinding refresh;
    AcquireSRWLockExclusive(&settingsLock);
    if (draftRow.page && draftRow.page == optionsPage() && draftRow.selectedRow != selectedRow())
        refresh = discardDraft();
    ReleaseSRWLockExclusive(&settingsLock);
    refreshRow(refresh);
}

void changeOption(void* user, const char*, int value) {
    if (refreshingRow) return;
    auto* field = static_cast<int*>(user);
    value = std::clamp(value, 0, 1);
    RowBinding refresh;
    AcquireSRWLockExclusive(&settingsLock);
    if (auto* page = optionsPage()) {
        const int selected = selectedRow();
        if (draftRow.page && (draftRow.page != page || draftRow.value != field || draftRow.selectedRow != selected)) {
            const bool samePage = draftRow.page == page;
            const auto previous = discardDraft();
            if (samePage) refresh = previous;
        }
        draftRow = {page, field, selected, changingRow};
        *field = value;
    } else if (!platform) *field = value;
    ReleaseSRWLockExclusive(&settingsLock);
    refreshRow(refresh);
}

bool menuButtons(void* page, int button, bool pressed) {
    bool saveFailed = false;
    bool accepted = false;
    RowBinding refresh;
    AcquireSRWLockExclusive(&settingsLock);
    const auto activePage = optionsPage();
    if (draftRow.page && (draftRow.page != activePage || (pressed && page == draftRow.page && button == backButton))) {
        discardDraft();
    } else if (draftRow.page && draftRow.selectedRow != selectedRow()) {
        refresh = discardDraft();
    } else if (draftRow.page && pressed && page == draftRow.page && button == acceptButton) {
        saveFailed = !saveDraft();
        if (!saveFailed) {
            savedSettings = draftSettings;
            discardDraft();
            accepted = true;
        }
    }
    ReleaseSRWLockExclusive(&settingsLock);
    refreshRow(refresh);
    if (saveFailed) log("Could not save Underglow.ini. Press Accept to retry or Back to discard.");
    if (accepted) log("Saved Underglow part settings.");
    return originalMenuButtons(page, button, pressed);
}

Settings settingsForFrame(bool& animateInMenu) {
    AcquireSRWLockExclusive(&settingsLock);
    const auto page = optionsPage();
    if (draftRow.page && draftRow.page != page) discardDraft();
    auto settings = draftRow.page && draftRow.selectedRow == selectedRow() ? draftSettings : savedSettings;
    const bool garageOptionsOpen = garage::optionsPreviewActive;
    if (garageOptionsOpen) settings.sliders = garage::optionsPreviewSliders;
    ReleaseSRWLockExclusive(&settingsLock);
    const auto record = garage::current();
    const auto preview = garage::currentPreview();
    if (preview.active && !garageOptionsOpen && preview.sliders != garage::unknownSliders) {
        for (size_t i = 0; i < preview.sliders.size(); ++i) settings.sliders[i] = preview.sliders[i] / 1000.0f;
    }
    settings.lightsEnabled = preview.active ? preview.enabled :
        record.state == garage::State::installed && !record.lightsOff;
    animateInMenu = page != nullptr || preview.active || garageOptionsOpen;
    const auto appearance = preview.active ? preview.appearance : garage::look(record);
    settings.options[Primary] = appearance.primary == garage::unsetPrimary ?
        (appearance.secondary == garage::unsetSecondary ? 0 : appearance.secondary) : appearance.primary;
    settings.options[Secondary] = appearance.secondary == garage::unsetSecondary ? 0 : appearance.secondary;
    settings.garageRgb = appearance.rgb;
    if (preview.active) {
        for (size_t zone = 0; zone < zoneCount; ++zone) settings.options[Front + zone] = 1;
    }
    for (size_t zone = 0; zone < zoneCount; ++zone) {
        if (!garage::hasUnderglowOn(appearance.effects, zone)) {
            settings.options[Front + zone] = 0;
            continue;
        }
        const int audio = preview.active ? -1 : garage::zoneEffect(record.audio, zone);
        const int visual = garage::zoneEffect(appearance.effects, zone);
        if (audio >= 0) settings.effects[zone] = audio;
        else if (visual >= 0) settings.effects[zone] = visual;
    }
    return settings;
}

void addOption(size_t index) {
    const auto& row = optionRows[index];
    FH3Setting setting{};
    setting.size = sizeof(setting);
    setting.key = row.key;
    setting.label = row.label;
    const std::string description = std::string(row.description) + (platform ?
        " Press A to keep the preview before leaving this row." : " Confirm in Apply Underglow Settings.");
    setting.description = description.c_str();
    setting.value = savedSettings.options[index];
    setting.kind = FH3_SETTING_ON_OFF;
    setting.user = &draftSettings.options[index];
    setting.on_change = changeOption;
    api->add_setting(mod, &setting);
}

constexpr int discardPortable = 0, pendingPortable = 1, acceptPortable = 2;

void applyPortableSettings(void*, const char* key, int action) {
    if (action != acceptPortable && action != discardPortable) return;
    const bool accept = action == acceptPortable;
    AcquireSRWLockExclusive(&settingsLock);
    const bool saved = !accept || saveDraft();
    if (saved) {
        if (accept) savedSettings = draftSettings;
        discardDraft();
    }
    ReleaseSRWLockExclusive(&settingsLock);
    api->set_setting(mod, key, pendingPortable);
    if (!saved) log("Could not save Underglow.ini. Select Apply to retry.");
    else if (accept) log("Saved Underglow settings through the Mod Platform API.");
}

bool addPortableSettingsAction() {
    constexpr const char* actions[]{"Discard", "Pending", "Apply"};
    FH3Setting setting{};
    setting.size = sizeof(setting);
    setting.key = "ApplySettings";
    setting.label = "Apply Underglow Settings";
    setting.description = "Press Right to apply the part switches, or Left to discard them.";
    setting.value = pendingPortable;
    setting.on_change = applyPortableSettings;
    setting.kind = FH3_SETTING_CHOICE;
    setting.choices = actions;
    setting.choice_count = static_cast<uint32_t>(std::size(actions));
    return api->add_setting(mod, &setting) != 0;
}

bool isLocalDriver(Byte* car) {
    if (!car || read<uintptr_t>(car) != reinterpret_cast<uintptr_t>(game + 0x42e0650) ||
        read<int>(car + 0x7370) != 0 || read<int>(car + 0x7398) != 1 ||
        !read<Byte*>(car + 0x7378)) return false;
    // The native accessor follows these two pointers. Use guarded reads because
    // the paused scene may still render while a simulation is being unloaded.
    const auto* manager = read<Byte*>(game + playerManager);
    const auto* session = manager ? read<Byte*>(manager + 0x50) : nullptr;
    const auto* simulation = session ? read<Byte*>(session + 0x98) : nullptr;
    const auto* list = simulation ? read<Byte*>(simulation + 0x298) : nullptr;
    if (!list) return false;
    const auto begin = read<uintptr_t>(list);
    const auto end = read<uintptr_t>(list + sizeof(uintptr_t));
    if (!begin || end < begin || end - begin > 64 * sizeof(void*) ||
        (end - begin) % sizeof(void*)) return false;
    for (auto entry = begin; entry < end; entry += sizeof(void*)) {
        if (read<Byte*>(reinterpret_cast<Byte*>(entry)) == car) return true;
    }
    return false;
}

bool isPresentation(Byte* presentation) {
    if (!presentation) return false;
    const auto type = read<Byte*>(presentation);
    return type == game + 0x468e848 || type == game + 0x46902e0;
}

Byte* presentationFor(const Byte* car) {
    auto* presentation = read<Byte*>(car + 0x73a0);
    if (presentation) {
        const auto type = read<Byte*>(presentation);
        if (type == game + 0x468b058 || type == game + 0x468b9b8) {
            presentation = read<Byte*>(presentation + 8);
        }
    }
    return isPresentation(presentation) ? presentation : nullptr;
}

Byte* managerFor(const Byte* presentation) {
    const auto* scene = read<Byte*>(presentation + 0x10);
    return scene ? read<Byte*>(scene + 0x440) : nullptr;
}

void playerUpdate(void* object, float dt) {
    originalPlayer(object, dt);
    auto* car = static_cast<Byte*>(object);
    if (!isLocalDriver(car)) return;
    garage::updateCar(car, GetTickCount64());
    auto* presentation = presentationFor(car);

    AcquireSRWLockExclusive(&snapshotLock);
    if (snapshot.car != car || snapshot.presentation != presentation) {
        snapshot.matrixTick = 0;
        snapshot.manager = nullptr;
    }
    snapshot.car = car;
    snapshot.presentation = presentation;
    snapshot.playerTick = GetTickCount64();
    ReleaseSRWLockExclusive(&snapshotLock);
}

bool validMatrix(const XMFLOAT4X4& matrix) {
    for (int row = 0; row < 4; ++row) {
        for (int col = 0; col < 4; ++col) {
            if (!std::isfinite(matrix.m[row][col])) return false;
        }
    }
    const auto dot = matrix._21 * matrix._21 + matrix._22 * matrix._22 + matrix._23 * matrix._23;
    // During loading the presentation can sit at a placeholder below the map.
    return dot > 0.8f && dot < 1.2f && std::abs(matrix._44 - 1.0f) < 0.01f && matrix._42 > -500;
}

bool validBounds(const CarBounds& bounds) {
    const float width = bounds.max.x - bounds.min.x;
    const float length = bounds.max.z - bounds.min.z;
    return width >= 0.6f && width <= 4.5f && length >= 1.2f && length <= 14.0f &&
        std::abs(bounds.max.x + bounds.min.x) <= 2.0f && std::abs(bounds.max.z + bounds.min.z) <= 8.0f;
}

bool validPartBounds(const CarBounds& part) {
    return part.max.x - part.min.x >= 0.1f && part.min.x > -3.0f && part.max.x < 3.0f &&
        part.max.z - part.min.z >= 0.02f && part.min.z > -10.0f && part.max.z < 10.0f;
}

BodyParts fittedParts(const Byte* scene) {
    BodyParts parts;
    if (!scene || read<Byte*>(scene) != game + carSceneType) return parts;

    // Scene::mParts stores the installed parts. Preview and installed upgrades
    // use the same active pointer and the base pointer supplies the stock variant.
    const auto begin = read<uintptr_t>(scene + 0x3a0);
    const auto end = read<uintptr_t>(scene + 0x3a8);
    if (!begin || end < begin || (end - begin) % sizeof(void*) ||
        end - begin > 128 * sizeof(void*)) return parts;
    for (auto entry = begin; entry < end; entry += sizeof(void*)) {
        const auto* part = read<Byte*>(reinterpret_cast<const void*>(entry));
        if (!part) continue;
        const auto* type = read<Byte*>(part);
        if (type != game + standardPartType && type != game + upgradablePartType) continue;

        CarBounds* target;
        switch (read<int>(part + 0x10)) {
        case 2: target = &parts.shell; break;
        case 34: target = &parts.frontBumper; break;
        case 35: target = &parts.rearBumper; break;
        case 37: target = &parts.sideSkirts; break;
        default: continue;
        }
        CarBounds box;
        if (type == game + standardPartType) {
            if (!read(part + 0xd0, box)) continue;
        } else {
            const auto* upgrade = read<Byte*>(part + 0xc8);
            if (!upgrade) upgrade = read<Byte*>(part + 0xc0);
            if (!upgrade || read<Byte*>(upgrade) != game + upgradeType || !read(upgrade + 0x30, box)) continue;
        }
        if (validPartBounds(box)) *target = box;
    }
    return parts;
}

CarBounds boundsFor(const Byte* presentation) {
    // Read the same car-local box as FH3's presentation bounds accessor.
    const auto* model = read<Byte*>(presentation + 0x7a8);
    CarBounds bounds;
    if (model && read(model + 0x1e0, bounds) && validBounds(bounds)) return bounds;

    const auto* definition = read<Byte*>(presentation + 0x5e8);
    CarBounds halfSizeAndCenter;
    if (definition && read(definition + 0x890, halfSizeAndCenter)) {
        const auto half = XMLoadFloat4(&halfSizeAndCenter.min);
        const auto center = XMLoadFloat4(&halfSizeAndCenter.max);
        XMStoreFloat4(&bounds.min, XMVectorSubtract(center, half));
        XMStoreFloat4(&bounds.max, XMVectorAdd(center, half));
        if (validBounds(bounds)) return bounds;
    }
    return {};
}

CarExtents lightExtents(const Byte* model) {
    if (!model || read<Byte*>(model) != game + carSceneType) return {};

    // FH3's car light update walks these registered lights. The model slot
    // selects the lamp mounted on the currently fitted part, even in daylight.
    // One lamp assembly can supply dozens of entries, so sample the whole list.
    constexpr size_t slotSize = 0x90;
    const auto begin = read<uintptr_t>(model + 0x6320);
    const auto end = read<uintptr_t>(model + 0x6328);
    const auto slots = read<uintptr_t>(model + 0x62d8);
    const auto slotsEnd = read<uintptr_t>(model + 0x62e0);
    if (!begin || end < begin || (end - begin) % sizeof(void*) || end - begin > 512 * sizeof(void*) ||
        !slots || slotsEnd < slots || (slotsEnd - slots) % slotSize) return {};

    CarExtents extents{};
    bool found = false;
    for (auto entry = begin; entry < end; entry += sizeof(void*)) {
        const auto* lamp = read<Byte*>(reinterpret_cast<const void*>(entry));
        if (!lamp) continue;
        const auto index = read<uint32_t>(lamp + 8);
        if (index >= (slotsEnd - slots) / slotSize ||
            !read<Byte>(reinterpret_cast<const void*>(slots + index * slotSize + 0x25))) continue;
        XMFLOAT4 pos;
        if (!read(lamp + 0x10, pos) || !(std::abs(pos.x) < 3.0f && std::abs(pos.y) < 4.0f &&
            std::abs(pos.z) < 10.0f)) continue;
        extents.halfWidth = std::max(extents.halfWidth, std::abs(pos.x));
        extents.front = found ? std::max(extents.front, pos.z) : pos.z;
        extents.rear = found ? std::min(extents.rear, pos.z) : pos.z;
        found = true;
    }
    return extents.halfWidth >= 0.3f && extents.front >= 0.6f && extents.rear <= -0.6f ? extents : CarExtents{};
}

CarExtents bodyExtents(const CarBounds& body, const BodyParts& parts) {
    if (!validBounds(body)) return {};

    const float length = body.max.z - body.min.z;
    const float middleZ = (body.max.z + body.min.z) * 0.5f;
    const auto& shell = validBounds(parts.shell) ? parts.shell : body;
    const auto& front = validPartBounds(parts.frontBumper) && parts.frontBumper.min.x <= -0.225f &&
        parts.frontBumper.max.x >= 0.225f &&
        parts.frontBumper.max.z > middleZ + length * 0.25f &&
        std::abs(parts.frontBumper.max.z - body.max.z) < 1.5f ? parts.frontBumper : body;
    const auto& rear = validPartBounds(parts.rearBumper) && parts.rearBumper.min.x <= -0.225f &&
        parts.rearBumper.max.x >= 0.225f &&
        parts.rearBumper.min.z < middleZ - length * 0.25f &&
        std::abs(parts.rearBumper.min.z - body.min.z) < 1.5f ? parts.rearBumper : body;
    const auto& skirts = parts.sideSkirts;
    const bool hasSkirts = validPartBounds(skirts) && std::max(-skirts.min.x, skirts.max.x) >= 0.3f &&
        skirts.min.z < middleZ - length * 0.12f && skirts.max.z > middleZ + length * 0.12f &&
        skirts.min.z >= rear.min.z - 0.2f && skirts.max.z <= front.max.z + 0.2f;
    return {std::max(-shell.min.x, shell.max.x), body.max.z, body.min.z,
        {std::min(-front.min.x, front.max.x), front.max.z},
        {std::min(-rear.min.x, rear.max.x), rear.min.z}, hasSkirts ? skirts : CarBounds{}};
}

CarExtents extentsFor(const Byte* presentation) {
    const auto* model = read<Byte*>(presentation + 0x7a8);
    const auto parts = fittedParts(model);
    auto body = boundsFor(presentation);
    if (!validBounds(body)) body = parts.shell;
    const auto fitted = bodyExtents(body, parts);
    if (fitted.halfWidth > 0) return fitted;

    auto lights = lightExtents(model);
    lights.frontBumper = {lights.halfWidth, lights.front};
    lights.rearBumper = {lights.halfWidth, lights.rear};
    return lights;
}

void carLights(void* object) {
    originalCarLights(object);
    auto* presentation = static_cast<Byte*>(object);
    AcquireSRWLockExclusive(&snapshotLock);
    const auto now = GetTickCount64();
    if (snapshot.presentation == presentation && now - snapshot.playerTick <= 750) {
        auto* manager = managerFor(presentation);
        XMFLOAT4X4 matrix;
        if (manager && read(presentation + 0x1a0, matrix) && validMatrix(matrix)) {
            snapshot.manager = manager;
            snapshot.matrix = matrix;
            if (autoFit) snapshot.extents = extentsFor(presentation);
            snapshot.matrixTick = now;
        }
    }
    ReleaseSRWLockExclusive(&snapshotLock);
}

struct Pool {
    Byte* begin;
    Byte* end;
};

Pool readPool(const Byte* manager, size_t offset, size_t stride) {
    const auto pool = read<Pool>(manager + offset);
    const auto size = reinterpret_cast<uintptr_t>(pool.end) - reinterpret_cast<uintptr_t>(pool.begin);
    if (!pool.begin || !size || size % stride || size / stride > 65536) return {};
    return pool;
}

Byte* allocated(const Pool& pool, size_t stride, Id id) {
    if (id == invalid || !pool.begin) return nullptr;
    const auto bytes = reinterpret_cast<uintptr_t>(pool.end) - reinterpret_cast<uintptr_t>(pool.begin);
    if (id >= bytes / stride) return nullptr;
    auto* node = pool.begin + id * stride;
    // Both links point inside the pool for a live node. A free-list tail can have a null link.
    for (size_t offset : {size_t{0}, sizeof(void*)}) {
        const auto link = reinterpret_cast<uintptr_t>(read<Byte*>(node + offset)) -
                          reinterpret_cast<uintptr_t>(pool.begin);
        if (link >= bytes || link % stride) return nullptr;
    }
    return node;
}

bool ownsLight(const Pool& lights, size_t i) {
    const auto* node = allocated(lights, lightStride, world.lights[i]);
    return node && read<Id>(node + 0x38) == world.generations[i] &&
           read<Id>(node + 0x30) == world.presets[i];
}

void removeLights(Byte* manager, const Pool& lights, const Pool& presets, bool partial = false) {
    if (world.lightPool != lights.begin || world.presetPool != presets.begin) return;
    using Destroy = void (*)(void*, Id*);
    bool ownsPoolState = partial;
    for (size_t i = 0; i < lightCount; ++i) {
        if (!ownsLight(lights, i)) continue;
        native<Destroy>(destroyLight)(manager, &world.lights[i]);
        ownsPoolState = true;
    }
    if (ownsPoolState) {
        for (auto& id : world.presets) {
            const auto* node = allocated(presets, presetStride, id);
            if (node && read<uint32_t>(node + 0x30) == 0) {
                native<Destroy>(destroyPreset)(manager, &id);
            }
        }
    }
    world.lights.fill(invalid);
    world.presets.fill(invalid);
}

bool makeLights(Byte* manager, const Pool& lights, const Pool& presets) {
    using CreatePreset = Id* (*)(void*, Id*, const SmallString*, const Preset*);
    using CreateLight = Id* (*)(void*, Id*);
    using Assign = void (*)(void*, Id, Id);
    using Intensity = void (*)(void*, Id, float, bool);
    if (!read<Byte*>(manager + 0x50)) return false;
    const SmallString unnamed;
    Preset preset;
    preset.range = range;
    preset.power = power;
    for (size_t i = 0; i < lightCount; ++i) {
        auto& presetId = world.presets[i];
        native<CreatePreset>(createPreset)(manager, &presetId, &unnamed, &preset);
        if (!allocated(presets, presetStride, presetId)) return false;
        auto& id = world.lights[i];
        native<CreateLight>(createLight)(manager, &id);
        const auto* node = allocated(lights, lightStride, id);
        if (!node) return false;
        world.generations[i] = read<Id>(node + 0x38);
        native<Assign>(assignPreset)(manager, id, presetId);
        native<Intensity>(setIntensity)(manager, id, 0, false);
    }
    return true;
}

Color colorAt(int i) {
    i = std::clamp(i, 0, fx::kColorTotal - 1);
    if (i >= fx::kColorCount) return customColors[i - fx::kColorCount];
    const auto& rgb = fx::kColors[i].rgb;
    return {rgb[0], rgb[1], rgb[2]};
}

struct Footprint {
    float right, left, front, rear;
    EndStrip frontStrip, rearStrip;
};

Footprint footprintFor(CarExtents extents) {
    if (extents.halfWidth <= 0) extents = {halfWidth, halfLength, -halfLength};
    const float side = std::max(0.1f, extents.halfWidth + sideOffset);
    const float front = extents.front - frontInset;
    const float rear = extents.rear + rearInset;
    const float middle = (front + rear) * 0.5f;
    const float halfSideLength = front - rear >= 0.2f ? (front - rear) * 0.5f : 0;
    const auto frontBumper = extents.frontBumper;
    const auto rearBumper = extents.rearBumper;
    // Use the FH2 inset from each fitted bumper. Scale the maximum inset with
    // body length so the front and rear remain separate on very short cars.
    const float maxBumperInset = (extents.front - extents.rear) * 0.25f;
    const EndStrip frontStrip = frontBumper.halfWidth > 0 ?
        EndStrip{std::max(0.1f, frontBumper.halfWidth + sideOffset), frontBumper.z - std::min(frontInset, maxBumperInset)} :
        EndStrip{side, middle + halfSideLength};
    const EndStrip rearStrip = rearBumper.halfWidth > 0 ?
        EndStrip{std::max(0.1f, rearBumper.halfWidth + sideOffset), rearBumper.z + std::min(rearInset, maxBumperInset)} :
        EndStrip{side, middle - halfSideLength};
    const auto& skirts = extents.sideSkirts;
    const bool fittedBody = frontBumper.halfWidth > 0 || rearBumper.halfWidth > 0;
    if (validPartBounds(skirts)) {
        const float width = std::max(-skirts.min.x, skirts.max.x);
        // Some cars store one skirt and mirror it across the centerline.
        const float right = skirts.max.x >= 0.3f ? skirts.max.x : width;
        const float left = skirts.min.x <= -0.3f ? -skirts.min.x : width;
        const float trim = std::min((skirts.max.z - skirts.min.z) * 0.45f, sideTrim * 0.15f);
        return {std::max(0.1f, right + sideOffset), -std::max(0.1f, left + sideOffset),
            skirts.max.z - trim, skirts.min.z + trim, frontStrip, rearStrip};
    }
    const float sideTrimLimit = halfSideLength * (fittedBody ? 0.6f : 0.9f);
    const float sideSpan = halfSideLength - std::min(sideTrimLimit, fittedBody ? sideTrim * 0.85f : sideTrim);
    return {side, -side, middle + sideSpan, middle - sideSpan, frontStrip, rearStrip};
}

XMFLOAT4 localPosition(const Footprint& footprint, size_t zone, size_t segment) {
    const auto t = (static_cast<float>(segment) + 0.5f) / static_cast<float>(zoneSizes[zone]);
    const auto& [right, left, front, rear, frontStrip, rearStrip] = footprint;
    const float across = 2.0f * t - 1.0f;
    const float sideLength = front - rear;
    const XMFLOAT4 points[] = {
        {frontStrip.halfWidth * across, height, frontStrip.z, 1},
        {right, height, front - sideLength * t, 1},
        {-rearStrip.halfWidth * across, height, rearStrip.z, 1},
        {left, height, rear + sideLength * t, 1},
    };
    return points[zone];
}

#include "Radio.h"

void renderLights(Byte* manager, const Pool& presets, const Snapshot& frame, const Settings& settings,
                  ULONGLONG animationTick) {
    using Transform = void (*)(void*, Id, const XMFLOAT4*);
    using Intensity = void (*)(void*, Id, float, bool);
    const auto matrix = XMLoadFloat4x4(&frame.matrix);
    const auto footprint = footprintFor(frame.extents);
    const auto carRotation = XMQuaternionRotationMatrix(matrix);
    // Rotate the light's +Z axis to the car's local -Y axis.
    const auto down = XMVectorSet(0.70710678f, 0, 0, 0.70710678f);
    XMFLOAT4 rotation;
    XMStoreFloat4(&rotation, XMQuaternionMultiply(down, carRotation));
    const Color first = garage::resolvedColor(settings.options[Primary], settings.garageRgb);
    const Color second = garage::effectSecondary(first, garage::resolvedColor(settings.options[Secondary], settings.garageRgb));
    const auto audio = radio::frame(GetTickCount64());
    const bool radioPlaying = std::any_of(std::begin(audio.lvl), std::end(audio.lvl), [](float level) { return level > 0; });
    const float brightness = settings.sliders[Brightness];
    const fx::EffectFrame effects{settings.sliders[Speed], {first.r, first.g, first.b}, {second.r, second.g, second.b}, audio};
    const double seconds = effectSeconds(animationTick);
    std::array<int, fx::kEffectCount> counts{}, cursor{};
    for (size_t zone = 0; zone < zoneCount; ++zone)
        if (settings.options[Front + zone]) counts[settings.effects[zone]] += static_cast<int>(zoneSizes[zone]);
    bool dirty = false;
    for (size_t zone = 0; zone < zoneCount; ++zone) {
        const int mode = settings.effects[zone];
        const bool lit = settings.options[Front + zone] && (radioPlaying || !isAudioEffect(mode));
        for (size_t segment = 0; segment < zoneSizes[zone]; ++segment) {
            const size_t index = zoneStarts[zone] + segment;
            const Id light = world.lights[index];
            const auto local = localPosition(footprint, zone, segment);
            XMFLOAT4 position;
            XMStoreFloat4(&position, XMVector3TransformCoord(XMLoadFloat4(&local), matrix));
            float level = 0;
            if (lit) {
                auto p = fx::pixel(static_cast<int>(zone), static_cast<int>(segment), static_cast<int>(zoneSizes[zone]));
                p.id = static_cast<int>(index);
                p.gi = cursor[mode]++;
                p.gn = counts[mode];
                p.ring = (static_cast<float>(p.gi) + 0.5f) / static_cast<float>(p.gn);
                float rgb[3];
                fx::effect_now(effects, mode, p, seconds, rgb, &level);
                if (level > 0.01f) {
                    auto* preset = allocated(presets, presetStride, world.presets[index]);
                    if (preset && std::memcmp(preset + 0x50, rgb, sizeof(rgb))) {
                        std::memcpy(preset + 0x50, rgb, sizeof(rgb));
                        dirty = true;
                    }
                }
            }
            native<Transform>(setPosition)(manager, light, &position);
            native<Transform>(setRotation)(manager, light, &rotation);
            native<Intensity>(setIntensity)(manager, light, level * brightness, false);
        }
    }
    if (dirty) manager[0x100] = 1;
}

std::atomic<bool> daylightReady{};
std::atomic<Byte*> daylightManager;
std::atomic<ULONGLONG> daylightUntil;

Byte* activeDaylightManager() {
    auto* manager = daylightReady.load() ? daylightManager.load() : nullptr;
    return manager && GetTickCount64() <= daylightUntil.load() ? manager : nullptr;
}

void updateSceneLights(Byte* manager, const Settings& settings, bool animateInMenu) {
    Snapshot frame;
    AcquireSRWLockShared(&snapshotLock);
    frame = snapshot;
    ReleaseSRWLockShared(&snapshotLock);
    const auto now = GetTickCount64();
    const bool hasPose = frame.manager == manager && frame.matrixTick;
    // The pause menu renders the same scene with the player simulation stopped.
    // Reuse its last pose only while the original car still owns the presentation.
    const bool hasOwner = hasPose && isLocalDriver(frame.car) &&
        presentationFor(frame.car) == frame.presentation && managerFor(frame.presentation) == manager;
    const bool active = hasOwner && settings.lightsEnabled;
    if (active || world.manager == manager) {
        const auto lights = readPool(manager, 0, lightStride);
        const auto presets = readPool(manager, 0x38, presetStride);
        if (world.manager != manager || world.lightPool != lights.begin || world.presetPool != presets.begin) {
            world = World{};
            world.manager = manager;
            world.lightPool = lights.begin;
            world.presetPool = presets.begin;
        }
        if (world.lights.front() != invalid) {
            bool valid = true;
            for (size_t i = 0; i < lightCount; ++i) valid &= ownsLight(lights, i);
            if (!hasOwner || !valid) removeLights(manager, lights, presets);
        }
        if (active && lights.begin && presets.begin && now >= world.retryAfter && world.lights.front() == invalid) {
            if (!makeLights(manager, lights, presets)) {
                removeLights(manager, lights, presets, true);
                world.retryAfter = now + 2000;
                if (!world.reportedCapacity) log("Waiting for free light and preset slots.");
                world.reportedCapacity = true;
            } else if (!world.reportedActive) {
                log("Player underglow active. Thirty-two lights attached.");
                world.reportedActive = true;
            }
        }
        if (world.lights.front() != invalid) {
            if (active) {
                renderLights(manager, presets, frame, settings, animateInMenu ? now : frame.matrixTick);
            } else {
                // Keep the current car's lights ready to resume without recycling its GPU presets.
                const auto setLevel = native<void (*)(void*, Id, float, bool)>(setIntensity);
                for (const Id id : world.lights) setLevel(manager, id, 0, false);
            }
        }
        const auto firstPart = std::begin(settings.options) + Front;
        const bool hasLitParts = std::any_of(firstPart, firstPart + zoneCount, [](int on) { return on != 0; });
        const bool keepDaylight = daylightReady.load() && active && hasLitParts && settings.sliders[Brightness] > 0 &&
            world.lights.front() != invalid;
        if (keepDaylight) daylightUntil = now + 500;
        daylightManager = keepDaylight ? manager : nullptr;
    }
}

void sceneLights(void* object) {
    bool animateInMenu;
    const auto settings = settingsForFrame(animateInMenu);
    AcquireSRWLockExclusive(&worldLock);
    updateSceneLights(static_cast<Byte*>(object), settings, animateInMenu);
    originalSceneLights(object);
    ReleaseSRWLockExclusive(&worldLock);
}

using AssignShader = void* (*)(void*, const char*, size_t);
using ViewLightFactor = float (*)(void*);
using UploadLights = void (*)(void*, void*, unsigned char);
using CopyPacket = void* (*)(void*, const void*, size_t);
AssignShader originalAssignDaytimeShader;
ViewLightFactor originalViewLightFactor;
UploadLights originalUploadLights;
CopyPacket originalCopyLightPacket;

void* assignDaytimeShader(void* target, const char* name, size_t length) {
    // The twilight track shader still renders the sun and also reads the local light buffers.
    if (activeDaylightManager()) {
        if (const auto* twilight = read<const char*>(game + twilightTrackShader))
            return originalAssignDaytimeShader(target, twilight, sizeof("TrackNightLightScenario") - 1);
    }
    return originalAssignDaytimeShader(target, name, length);
}

float viewLightFactor(void* manager) {
    const float factor = originalViewLightFactor(manager);
    return factor == 0 && manager == activeDaylightManager() ? 1.0f : factor;
}

void buildDaylightClusters(FH3Context* context, void*) {
    // The game also skips building the light grid when its environment scale is zero.
    // Keep the normal cluster dispatch active while the local neon needs the grid.
    const auto* environment = reinterpret_cast<const Byte*>(context->r9);
    if (environment && activeDaylightManager() && read<float>(environment + 0x28c) == 0)
        context->xmm[0].f[0] = -1.0f;
}

constexpr size_t lightPacketSize = 0x30;
constexpr float daylightIntensityScale = 16.0f;

struct DaylightUpload {
    Pool presets;
    std::array<Id, lightCount> slots{};
    bool prepared{};
    Byte* firstPacket{};
    Id packetCount{};
};
thread_local DaylightUpload* daylightUpload;

void* copyLightPacket(void* destination, const void* source, size_t size) {
    auto* upload = daylightUpload;
    if (!upload || size != lightPacketSize) return originalCopyLightPacket(destination, source, size);
    if (!upload->prepared) {
        // The native upload assigns the GPU preset slots before packing the lights.
        for (size_t i = 0; i < lightCount; ++i) {
            const auto* preset = allocated(upload->presets, presetStride, world.presets[i]);
            upload->slots[i] = preset ? read<Id>(preset + 0x34) : invalid;
        }
        std::sort(upload->slots.begin(), upload->slots.end());
        upload->prepared = true;
    }
    // Compact the player neon into the front of the mapped GPU input buffer.
    // The other world lights stay suppressed during the game's daytime pass.
    if (!upload->firstPacket) upload->firstPacket = static_cast<Byte*>(destination);
    const auto* bytes = static_cast<const Byte*>(source);
    const Id slot = read<Id>(bytes + 0x20);
    if (slot != invalid && upload->packetCount < lightCount &&
        std::binary_search(upload->slots.begin(), upload->slots.end(), slot)) {
        std::array<Byte, lightPacketSize> packet;
        std::memcpy(packet.data(), source, size);
        // Apply exposure compensation to the same brightness and effect level used at night.
        const float level = read<float>(bytes) * daylightIntensityScale;
        std::memcpy(packet.data(), &level, sizeof(level));
        originalCopyLightPacket(upload->firstPacket + upload->packetCount * size, packet.data(), size);
        ++upload->packetCount;
    }
    return destination;
}

void uploadLights(void* object, void* renderer, unsigned char night) {
    if (night || object != activeDaylightManager()) {
        originalUploadLights(object, renderer, night);
        return;
    }
    auto* manager = static_cast<Byte*>(object);
    AcquireSRWLockExclusive(&worldLock);
    const auto presets = readPool(manager, 0x38, presetStride);
    const bool active = world.manager == manager && world.lights.front() != invalid &&
        world.presetPool == presets.begin && manager == activeDaylightManager();
    DaylightUpload upload{presets};
    auto* previous = daylightUpload;
    if (active) daylightUpload = &upload;
    originalUploadLights(object, renderer, active ? 1 : night);
    // The native upload publishes its count after unmapping the GPU buffer.
    if (active && upload.firstPacket) {
        std::memcpy(manager + 0x390, &upload.packetCount, sizeof(upload.packetCount));
    }
    daylightUpload = previous;
    ReleaseSRWLockExclusive(&worldLock);
}

struct CodeCheck { size_t offset; const char* bytes; size_t count; };
#define CHECK(at, bytes) {at, bytes, sizeof(bytes) - 1}
constexpr CodeCheck checks[] = {
    CHECK(playerTickCall, "\xe8\x95\x3c\x00\x00"),
    CHECK(carLightsCall, "\xe8\xea\xf0\xff\xff"),
    CHECK(sceneLightsCall, "\xe8\x1d\xd8\xdc\xff"),
    CHECK(getSimulation, "\x48\x8b\x49\x50\x48\x85\xc9\x0f\x85\xb3\x22\x7c\x00"),
    CHECK(simulationAccessor, "\x48\x8b\x81\x98\x00\x00\x00\xc3"),
    CHECK(carBoundsAccessor, "\x48\x8b\xbb\xa8\x07\x00\x00\x48\x81\xc7\xe0\x01\x00\x00"),
    CHECK(carBoundsFallback, "\x48\x8b\x83\xe8\x05\x00\x00\xc5\xf8\x10\x80\xa0\x08\x00\x00"),
    CHECK(carPartList, "\x49\x8b\x9d\xa0\x03\x00\x00\x49\x3b\x9d\xa8\x03\x00\x00"),
    CHECK(currentUpgrade, "\x48\x8b\x81\xc8\x00\x00\x00\x48\x85\xc0\x75\x10\x48\x8b\x81\xc0"),
    CHECK(upgradeBounds, "\x48\x8d\x55\x30"),
    CHECK(standardPartBounds, "\x48\x8d\x57\x28"),
    CHECK(createPreset, "\x40\x55\x56\x57\x41\x54\x41\x55"),
    CHECK(destroyPreset, "\x40\x57\x41\x56\x41\x57\x48\x83\xec\x20"),
    CHECK(createLight, "\x40\x57\x48\x83\xec\x20\x4c\x8b\x41\x18"),
    CHECK(destroyLight, "\x48\x89\x5c\x24\x08\x57\x48\x83\xec\x20"),
    CHECK(assignPreset, "\x48\x83\xec\x28\x44\x8b\xca\x4c\x8b\xd1"),
    CHECK(setPosition, "\x83\xfa\xff\x74\x3b\x4c\x8b\x09"),
    CHECK(setRotation, "\x48\x83\xec\x18\xc4\xc1\x78\x10\x18"),
    CHECK(setIntensity, "\x4c\x8b\xd1\x83\xfa\xff\x74\x3b"),
};
#undef CHECK

bool matchesImage(const Byte* base, DWORD stamp, DWORD size) {
    IMAGE_DOS_HEADER dos{};
    IMAGE_NT_HEADERS64 nt{};
    return base && read(base, dos) && dos.e_magic == IMAGE_DOS_SIGNATURE &&
        dos.e_lfanew >= 0 && dos.e_lfanew <= 0x1000 && read(base + dos.e_lfanew, nt) &&
        nt.Signature == IMAGE_NT_SIGNATURE && nt.FileHeader.Machine == IMAGE_FILE_MACHINE_AMD64 &&
        nt.FileHeader.TimeDateStamp == stamp && nt.OptionalHeader.SizeOfImage == size;
}

const Byte* menuPlatform() {
    if (!api->hook_function) return nullptr;
    // Look for the module that owns the settings API. Registration can be
    // forwarded through a different module, so check the API table as well.
    const void* anchors[]{
        reinterpret_cast<const void*>(api->add_setting),
        api,
        reinterpret_cast<const void*>(api->register_mod),
    };
    for (const auto* anchor : anchors) {
        HMODULE module{};
        if (!anchor || !GetModuleHandleExW(
            GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            static_cast<LPCWSTR>(anchor), &module)) continue;
        const auto* base = reinterpret_cast<const Byte*>(module);
        if ((platformMenu = core::platform_menu::find_platform_menu(base))) return base;
    }
    return nullptr;
}

bool supported() {
    if (!matchesImage(game, timestamp, imageSize)) return false;
    for (const auto& check : checks) {
        Byte actual[16]{};
        if (!read(game + check.offset, actual) || std::memcmp(actual, check.bytes, check.count)) return false;
    }
    return true;
}

bool installMenuHook() {
    // Attach to the loader's handler. It can wire the HUD slot after a slow database rebuild.
    struct SelectionLayout {
        DWORD stamp;
        size_t setter, store, selected;
    };
    constexpr SelectionLayout layouts[] = {
        {0x6aee7575, 0xca690, 0x69, 0x21f138},
        {0x47a22c2d, 0xbe0e0, 0x67, 0x2155f8},
        {0x6ae83e9d, 0xa0220, 0x69, 0x1e5d10},
        {0x6aef5530, 0xdab50, 0x69, 0x233268},
        {0x6aef54cf, 0xd1470, 0x69, 0x21c218},
    };
    const auto selection = std::find_if(std::begin(layouts), std::end(layouts),
        [](const SelectionLayout& layout) { return layout.stamp == platformMenu->stamp; });
    if (selection == std::end(layouts)) return false;
    const auto* store = platform + selection->setter + selection->store;
    const auto* prologue = selection->store == 0x67 ? "\x55\x56\x57\x48\x83\xec\x50\x48\x8d\x6c"
                                                   : "\x56\x57\x48\x83\xec\x58\x4d\x85\xc0\x74";
    using core::platform_menu::matches;
    using core::platform_menu::matches_button;
    if (!matches_button(platform + selection->setter, prologue) ||
        !matches(store, "\x87\x05", 2) ||
        store + 6 + read<int32_t>(store + 2) != platform + selection->selected ||
        !matches_button(platform + platformMenu->row_set, platformMenu->row_set_prologue) ||
        !matches(platform + platformMenu->notify, platformMenu->notify_prologue, 11)) return false;
    selectedRowAddress = platform + selection->selected;
    notifyProperty = reinterpret_cast<NotifyProperty>(const_cast<Byte*>(platform + platformMenu->notify));
    const std::array<FH3Hook*, 3> hooks{
        api->hook_function(mod, "Underglow settings Accept and Back", const_cast<Byte*>(platform + platformMenu->button),
            reinterpret_cast<void*>(menuButtons), reinterpret_cast<void**>(&originalMenuButtons)),
        api->hook_function(mod, "Underglow slider binding", const_cast<Byte*>(platform + platformMenu->row_set),
            reinterpret_cast<void*>(menuRowSet), reinterpret_cast<void**>(&originalRowSet)),
        api->hook_function(mod, "Underglow settings row selection", const_cast<Byte*>(platform + selection->setter),
            reinterpret_cast<void*>(menuSelectionSet), reinterpret_cast<void**>(&originalSelectionSet)),
    };
    for (size_t i = 0; i < hooks.size(); ++i) {
        if (!hooks[i] || !api->enable_hooks(mod, hooks[i])) {
            while (i) api->disable_hooks(mod, hooks[--i]);
            return false;
        }
    }
    menuReady.store(true);
    return true;
}

bool installDaylightHooks() {
    if (!api->hook_instruction ||
        std::memcmp(game + clusterLightCheck, "\xc4\xc1\x78\x2f\x81\x8c\x02\x00\x00", 9)) return false;
    struct DaylightHook {
        size_t address;
        const char* bytes;
        const char* name;
        void* detour;
        void** original;
    };
    const DaylightHook sites[] = {
        {copyLightPacketCall, "\xe8\xa4\xae\xec\xff", "Underglow daylight light packets",
            reinterpret_cast<void*>(copyLightPacket), reinterpret_cast<void**>(&originalCopyLightPacket)},
        {uploadLightsCall, "\xe8\xe7\xda\xdc\xff", "Underglow daylight upload",
            reinterpret_cast<void*>(uploadLights), reinterpret_cast<void**>(&originalUploadLights)},
        {viewLightFactorCall, "\xe8\xe8\x06\xd9\xff", "Underglow daylight local lights",
            reinterpret_cast<void*>(viewLightFactor), reinterpret_cast<void**>(&originalViewLightFactor)},
        {daytimeTrackShaderCall, "\xe8\xbb\x20\x3f\xfe", "Underglow daylight track shader",
            reinterpret_cast<void*>(assignDaytimeShader), reinterpret_cast<void**>(&originalAssignDaytimeShader)},
    };
    const auto* shader = read<const char*>(game + twilightTrackShader);
    if (!shader || std::strcmp(shader, "TrackNightLightScenario") != 0) return false;
    for (const auto& site : sites) {
        if (std::memcmp(game + site.address, site.bytes, 5)) return false;
    }
    FH3Hook* hooks[std::size(sites) + 1]{};
    for (size_t i = 0; i < std::size(sites); ++i) {
        const auto& site = sites[i];
        hooks[i] = api->hook_call(mod, site.name, game + site.address, site.detour, site.original);
        if (!hooks[i]) return false;
    }
    hooks[std::size(sites)] = api->hook_instruction(mod, "Underglow daylight light grid",
        game + clusterLightCheck, buildDaylightClusters, nullptr);
    if (!hooks[std::size(sites)]) return false;
    for (size_t i = 0; i < std::size(hooks); ++i) {
        if (!api->enable_hooks(mod, hooks[i])) {
            while (i > 0) api->disable_hooks(mod, hooks[--i]);
            return false;
        }
    }
    daylightReady.store(true);
    return true;
}

#include "Anna.h"

using TrackDraw = void (*)(void*, const Byte*, void*);
TrackDraw originalTrackDraw;

bool hasTrackDrawBinding(const Byte* mesh) {
    const auto* material = mesh ? read<const Byte*>(mesh) : nullptr;
    return material && read<const Byte*>(material + 0x10);
}

void recordTrackDraw(void* recorder, const Byte* mesh, void* context) {
    // WreckingBall records a second draw list for its loose canopy. A cloned
    // mesh can reach the native recorder without its render binding populated.
    if (hasTrackDrawBinding(mesh)) {
        originalTrackDraw(recorder, mesh, context);
    } else {
        static std::atomic<bool> reported{};
        if (!reported.exchange(true)) log("Skipped an unbound track mesh during WreckingBall draw recording.");
    }
}

void installWreckingBallGuard() {
    if (!api->mod_version || !api->mod_version("WreckingBall")) return;
    constexpr size_t call = 0x1e59cc3;
    if (!core::platform_menu::matches(game + call, "\xe8\x38\xf3\xff\xff", 5)) return;
    auto* hook = api->hook_call(mod, "WreckingBall track draw binding guard", game + call,
        reinterpret_cast<void*>(recordTrackDraw), reinterpret_cast<void**>(&originalTrackDraw));
    if (hook && api->enable_hooks(mod, hook))
        log("WreckingBall track draw compatibility guard is ready.");
}

void waitForExtendedSettings() {
    if (!api->mod_version || !api->mod_version("Horizon 3 Extended")) return;
    const auto config = std::wstring(api->azimuth_dir) + L"\\Horizon3Extended\\Horizon3Extended.ini";
    if (!GetPrivateProfileIntW(L"Global", L"Enabled", 1, config.c_str())) return;
    // Extended validates the original row setter before enabling its hooks.
    // Once its setter is in place, Mod Platform can chain our preview handler.
    const auto* setter = platform + platformMenu->row_set;
    for (unsigned attempt = 0; attempt < 400 &&
         core::platform_menu::matches(setter, platformMenu->row_set_prologue, 12); ++attempt) {
        Sleep(25);
    }
}

DWORD WINAPI installHooks(void*) {
    if (api->wait_for_mods) api->wait_for_mods(INFINITE);
    game = static_cast<Byte*>(api->wait_for_code(INFINITE));
    if (!supported()) {
        log("Underglow stayed off. The mapped FH3 build or a required call site differs.");
        return 0;
    }
    if (!api->hook_call(mod, "Local player presentation", game + playerTickCall,
                        reinterpret_cast<void*>(playerUpdate), reinterpret_cast<void**>(&originalPlayer)) ||
        !api->hook_call(mod, "Car light transform", game + carLightsCall,
                        reinterpret_cast<void*>(carLights), reinterpret_cast<void**>(&originalCarLights)) ||
        !api->hook_call(mod, "ForwardPlus scene lights", game + sceneLightsCall,
                        reinterpret_cast<void*>(sceneLights), reinterpret_cast<void**>(&originalSceneLights)) ||
        !api->enable_hooks(mod, nullptr)) {
        api->disable_hooks(mod, nullptr);
        log("Underglow stayed off. A required hook could not be enabled.");
        return 0;
    }
    log("FH3 lighting hooks enabled. Waiting for the local player car.");
    if (supportHorizon3Extended) installWreckingBallGuard();
    log(installDaylightHooks() ? "Daylight underglow enabled for the local car." :
        "Daylight underglow is unavailable on this game build.");
    if (garage::install()) {
        log("Garage Underglow is ready in Designs and Paints.");
        if (!radio::install()) log("FH3 radio visualizer is unavailable. Audio effects will stay dark.");
        if (anna::install()) log("ANNA audio effects are ready under Car > Underglow.");
        else log("ANNA audio effects are unavailable on this game build.");
    } else log("Garage Underglow stayed off because the garage menu hooks could not be installed.");
    if (platform) {
        if (supportHorizon3Extended) waitForExtendedSettings();
        if (installMenuHook()) log("Settings preview is ready. Press Accept to save changes.");
        else log("Could not enable settings confirmation. Underglow will use its saved settings.");
    }
    return 0;
}
} // namespace

FH3MOD_EXPORT int fh3mod_init(const FH3ModApi* host) {
    if (!host || host->api_version < 15 || host->size < offsetof(FH3ModApi, set_slider) + sizeof(host->set_slider) ||
        !host->register_mod || !host->log || !host->azimuth_dir || !host->wait_for_code ||
        !host->hook_call || !host->enable_hooks || !host->disable_hooks || !host->add_setting ||
        !host->set_setting) return 0;
    if (host->mod_version && host->mod_version(modName)) return 0;
    const FH3ModInfo info{sizeof(FH3ModInfo), modName, modVersion, nullptr, "1.0.0", 0, modDescription, 1};
    mod = host->register_mod(&info);
    if (!mod) return 0;
    api = host;
    animationStart = GetTickCount64();
    if (!initializeIni(host->azimuth_dir)) {
        log("Underglow stayed off. It could not prepare Underglow.ini.");
        return 0;
    }
    for (size_t i = 0; i < std::size(optionRows); ++i) {
        const auto& row = optionRows[i];
        savedSettings.options[i] = getInt(row.iniKey, row.initial, 0, row.max);
    }
    for (size_t i = 0; i < std::size(sliderRows); ++i) {
        const auto& row = sliderRows[i];
        savedSettings.sliders[i] = getFloat(row.iniKey, row.initial, row.min, row.max);
    }
    legacyPalette = getInt(L"PaletteVersion", 1, 1, 2) == 1;
    if (legacyPalette) {
        constexpr int oldPalette[] = {0, 1, 2, 13, 4, 5, 15, 7, 16, 11, 8, 18, 19};
        for (const auto slot : {Primary, Secondary}) {
            const int old = getInt(optionRows[slot].iniKey, -1, -1, static_cast<int>(std::size(oldPalette)) - 1);
            if (old >= 0) savedSettings.options[slot] = oldPalette[old];
        }
    }
    constexpr int oldEffects[] = {fx::kEffectStatic, fx::kEffectBreathe, fx::kEffectComet,
        fx::kEffectGradientWave, fx::kEffectWave};
    const int fallbackEffect = oldEffects[getInt(L"Effect", 0, 0, static_cast<int>(std::size(oldEffects)) - 1)];
    for (size_t i = 0; i < zoneCount; ++i) {
        savedSettings.effects[i] = getInt(zoneEffectKeys[i], fallbackEffect, 0, fx::kEffectCount - 1);
        if (isAudioEffect(savedSettings.effects[i])) savedSettings.effects[i] = fx::kEffectStatic;
    }
    draftSettings = savedSettings;
    loadPalette();
    for (const auto& effect : fx::kEffectOrder) {
        if (effect.group == fx::kGroupAudio)
            effectIds[effect.group][effectChoiceCounts[effect.group]++] = effect.effect;
    }
    autoFit = getInt(L"AutoFit", 1, 0, 1) != 0;
    const bool legacyGeometry = getInt(L"GeometryVersion", 1, 1, 2) == 1;
    halfWidth = getFloat(L"HalfWidth", defaultHalfWidth, 0.35f, 1.5f);
    frontInset = getFloat(L"FrontInset", defaultFrontInset, 0, 3);
    rearInset = getFloat(L"RearInset", defaultRearInset, 0, 3);
    sideOffset = getFloat(L"SideOffset", defaultSideOffset, -1, 1);
    sideTrim = getFloat(L"SideTrim", defaultSideTrim, 0, 3);
    const float oldHalfLength = getFloat(L"HalfLength", 1.75f, 0.7f, 3.2f);
    const float bodyLengthFallback = legacyGeometry && oldHalfLength != 1.75f
        ? oldHalfLength + (frontInset + rearInset) * 0.5f : defaultHalfLength;
    halfLength = getFloat(L"BodyHalfLength", bodyLengthFallback, 1.05f, 4.2f);
    height = getFloat(L"Height", defaultHeight, 0.01f, 1.25f);
    range = getFloat(L"Range", defaultRange, 0.5f, 6.0f);
    power = getFloat(L"Power", defaultPower, 0, 16);
    if (legacyGeometry && halfWidth == 0.9f) halfWidth = defaultHalfWidth;
    if (legacyGeometry && height == 0.46f) height = defaultHeight;
    if (legacyGeometry && range == 3.3f) range = defaultRange;

    platform = menuPlatform();
    if (platform || addPortableSettingsAction()) {
        for (size_t i = Front; i < std::size(optionRows); ++i) addOption(i);
        if (!platform) log("Using the Mod Platform API for Underglow settings. Use Apply Underglow Settings to confirm.");
    } else {
        log("This Mod Platform build does not support Underglow's options menu. Using Underglow.ini.");
    }

    if (HANDLE thread = CreateThread(nullptr, 0, installHooks, nullptr, 0, nullptr)) {
        CloseHandle(thread);
    } else {
        log("Underglow stayed off. The setup thread could not be created.");
    }
    return 0;
}
