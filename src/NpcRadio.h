#pragma once

namespace npcRadio {
constexpr size_t voiceCount = 12;
constexpr float audibleDistance = 45.0f;
constexpr float playbackVolume = 0.2f;
constexpr const wchar_t* stations[]{L"Horizon Pulse", L"Horizon Bass Arena", L"Horizon Block Party",
    L"Vagrant Records", L"Epitaph Records", L"Hospital Records", L"Future Classic", L"Timeless FM"};

struct Request {
    uint64_t identity{}, epoch{};
    int station{};
    XMFLOAT4X4 matrix{};
    float gain{}, distance{};
    bool network{}, paused{};
    float mix{1};
};
struct Meter {
    uint64_t identity{}, epoch{};
    fx::AudioFrame audio{};
};
SRWLOCK lock = SRWLOCK_INIT;
std::array<Request, voiceCount> requests{};
std::array<Meter, voiceCount> nativeMeters{}, networkMeters{};
ULONGLONG requestedAt{};
std::atomic<uint64_t> routeEpoch{1};
std::atomic<bool> ready{};
HANDLE networkThread{};

int stationFor(uint64_t identity) {
    const auto seed = static_cast<uint32_t>(identity ^ (identity >> 32)) ^ 0xa3c59ac3u;
    return 1 + static_cast<int>(fx::hash01(seed) * 16777216) % static_cast<int>(std::size(stations));
}

void publish(std::array<Meter, voiceCount>& bank, size_t slot, const Request& request,
             const radio::Tap& tap, ULONGLONG now) {
    AcquireSRWLockExclusive(&lock);
    auto& meter = bank[slot];
    if (meter.identity != request.identity || meter.epoch != request.epoch) meter = {};
    meter.identity = request.identity;
    meter.epoch = request.epoch;
    const float dt = meter.audio.last ? static_cast<float>(now - meter.audio.last) * .001f : 0;
    meter.audio.last = now;
    for (size_t zone = 0; zone < zoneCount; ++zone) {
        const float level = tap.levels[zone];
        meter.audio.lvl[zone] = level;
        if (level >= meter.audio.peak[zone]) {
            meter.audio.peak[zone] = level;
            meter.audio.peak_at[zone] = now;
        } else if (now - meter.audio.peak_at[zone] > 400) {
            meter.audio.peak[zone] = std::max(level, meter.audio.peak[zone] - dt * .8f);
        }
    }
    ReleaseSRWLockExclusive(&lock);
}

fx::AudioFrame frame(uint64_t identity, ULONGLONG now) {
    fx::AudioFrame result{};
    const auto epoch = routeEpoch.load();
    AcquireSRWLockShared(&lock);
    for (const auto* bank : {&nativeMeters, &networkMeters}) {
        for (const auto& meter : *bank) {
            if (meter.identity == identity && meter.epoch == epoch && meter.audio.last &&
                now <= meter.audio.last + 500) result = meter.audio;
        }
    }
    ReleaseSRWLockShared(&lock);
    return result;
}

std::array<Request, voiceCount> currentRequests(ULONGLONG now) {
    AcquireSRWLockShared(&lock);
    const auto result = now <= requestedAt + 750 ? requests : std::array<Request, voiceCount>{};
    ReleaseSRWLockShared(&lock);
    return result;
}

bool networkEnabled(ULONGLONG now) {
    static ULONGLONG refreshAt{};
    static bool enabled{};
    if (now < refreshAt) return enabled;
    refreshAt = now + 500;
    const auto path = std::filesystem::path(api->azimuth_dir) / L"Horizon3Extended/Horizon3Extended.ini";
    const bool next = api->mod_version && api->mod_version("Horizon 3 Extended") &&
        GetPrivateProfileIntW(L"Global", L"Enabled", 1, path.c_str()) &&
        GetPrivateProfileIntW(L"Audio", L"HorizonRadioNetwork", 0, path.c_str());
    if (next != enabled) {
        enabled = next;
        ++routeEpoch;
        log(next ? "Drivatar radios switched to Horizon Radio Network." : "Drivatar radios switched to native stations.");
    }
    return enabled;
}

float musicMix() {
    const auto* mixer = read<Byte*>(game + 0x4e72918);
    if (!mixer || read<Byte*>(mixer) != game + 0x4639028) return 0;
    SmallString name{};
    std::memcpy(name.text, "AOPT_Radio", 11);
    name.length = 10;
    const float slider = native<float (*)(const void*, const SmallString*)>(0x197b4f0)(mixer, &name);
    const auto* engine = read<Byte*>(game + radio::engineGlobal);
    void* studio = engine ? read<void*>(engine + 0xa9fe8) : nullptr;
    void* bus{};
    void* group{};
    float gain{};
    if (!std::isfinite(slider) || !studio ||
        native<int (*)(void*, const char*, void**)>(0x28a9840)(studio, "bus:/MasterVolume/MUSIC", &bus) || !bus ||
        native<radio::Pointer>(0x28a9920)(bus, &group) || !group ||
        native<int (*)(void*, float*)>(0x27c1b50)(group, &gain) || !std::isfinite(gain)) return 0;
    const float level = std::clamp(slider, 0.f, 1.f);
    return level * level * level * std::clamp(gain, 0.f, 1.f);
}

void updateRequests(ULONGLONG now) {
    std::array<DrivatarSnapshot, maxDrivatars> cars;
    Snapshot player;
    AcquireSRWLockShared(&snapshotLock);
    cars = drivatarSnapshots;
    player = snapshot;
    ReleaseSRWLockShared(&snapshotLock);
    now = std::max(now, GetTickCount64());
    std::array<Request, maxDrivatars> candidates{};
    size_t count = 0;
    const bool network = networkEnabled(now);
    const float mix = musicMix();
    if (modEnabled.load() && player.matrixTick) {
        for (const auto& car : cars) {
            if (!currentDrivatarPose(car, now) || car.pose.manager != player.manager) continue;
            const auto settings = drivatarSettings(car.identity);
            if (!settings.lightsEnabled || std::none_of(settings.effects.begin(), settings.effects.end(), isAudioEffect)) continue;
            const auto& matrix = car.pose.matrix;
            const float dx = matrix._41 - player.matrix._41, dy = matrix._42 - player.matrix._42;
            const float dz = matrix._43 - player.matrix._43;
            const float distance = std::sqrt(dx * dx + dy * dy + dz * dz);
            if (!std::isfinite(distance) || distance >= audibleDistance) continue;
            const float attenuation = 1 - distance / audibleDistance;
            candidates[count++] = {car.identity, routeEpoch.load(), stationFor(car.identity), matrix,
                playbackVolume * attenuation * attenuation * mix, distance, network, radio::pauseMenuActive(), mix};
        }
    }
    std::sort(candidates.begin(), candidates.begin() + count,
        [](const Request& a, const Request& b) { return a.distance < b.distance; });
    std::array<Request, voiceCount> next{};
    std::copy_n(candidates.begin(), std::min(count, next.size()), next.begin());
    AcquireSRWLockExclusive(&lock);
    requests = next;
    requestedAt = now;
    ReleaseSRWLockExclusive(&lock);
}

#include "NpcNativeRadio.h"
#include "NpcNetworkRadio.h"

void nativeTick(ULONGLONG now) {
    if (stopping.load()) return;
    if (!ready.load()) return;
    __try {
        updateRequests(now);
        updateNative(currentRequests(now), now);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        ready = false;
        AcquireSRWLockExclusive(&lock);
        requests = {};
        nativeMeters = {};
        networkMeters = {};
        ReleaseSRWLockExclusive(&lock);
        log("Drivatar radio stopped after its native audio route became unavailable.");
    }
}

DWORD WINAPI networkWorker(void*) {
    while (!stopping.load()) {
        try {
            updateNetwork(currentRequests(GetTickCount64()), GetTickCount64());
        } catch (...) {
            log("Drivatar network radio will retry after a playback error.");
        }
        if (stopWait(50)) break;
    }
    for (auto& voice : networkVoices) disconnect(voice);
    if (mpv.library) { FreeLibrary(mpv.library); mpv = {}; }
    return 0;
}

void cleanupNative() {
    const auto* engine = read<Byte*>(game + radio::engineGlobal);
    void* studio = engine ? read<void*>(engine + 0xa9fe8) : nullptr;
    if (studio && studio == nativeStudio)
        for (auto& voice : nativeVoices) disconnect(voice);
    else nativeVoices = {}; // the previous engine owns the retired handles
    nativeStudio = description = nullptr;
}

void stop() {
    ready = false;
    if (networkThread) {
        WaitForSingleObject(networkThread, INFINITE);
        CloseHandle(networkThread);
        networkThread = nullptr;
    }
    AcquireSRWLockExclusive(&lock);
    requests = {};
    nativeMeters = {};
    networkMeters = {};
    requestedAt = 0;
    ++routeEpoch;
    ReleaseSRWLockExclusive(&lock);
}

void start() {
    if (stopping.load()) return;
    ready = validateNative();
    if (!ready.load()) {
        log("Independent Drivatar radio is unavailable on this audio build.");
        return;
    }
    networkThread = CreateThread(nullptr, 0, networkWorker, nullptr, 0, nullptr);
    if (!networkThread) log("Drivatar Horizon Radio Network playback could not start.");
    log("Independent Drivatar station playback and audio meters are ready.");
}
} // namespace npcRadio
