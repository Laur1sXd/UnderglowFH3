#pragma once

// FMOD Studio's public entry points in the checked Xbox One FH3 image.
constexpr size_t eventLookup = 0x28a9f50, eventCreate = 0x28a4980, eventGroup = 0x28a99c0;
constexpr size_t eventRelease = 0x28ac570, eventStart = 0x28ad230, eventStop = 0x28ad3a0;
constexpr size_t eventPosition = 0x28ac830, eventStation = 0x28acc50, eventProperty = 0x28ace90;
constexpr size_t eventVolume = 0x28ad170, eventPause = 0x28acd40, removeDsp = 0x27c26f0;

struct Attributes {
    XMFLOAT3 position{}, velocity{}, forward{0, 0, 1}, up{0, 1, 0};
};
struct NativeVoice {
    uint64_t identity{}, epoch{};
    void* instance{};
    radio::Tap meter{};
    ULONGLONG retryAt{};
};
std::array<NativeVoice, voiceCount> nativeVoices{};
void* nativeStudio{};
void* description{};
ULONGLONG descriptionRetry{};
bool reportedEventFailure{}, reportedNativeSignal{};

bool validateNative() {
    struct Entry { size_t rva; const char* bytes; size_t size; };
    constexpr Entry entries[]{
        {eventLookup, "\x40\x53\x55\x56\x41\x56\x48\x81\xec\x48\x01\x00\x00", 13},
        {eventCreate, "\x48\x89\x5c\x24\x18\x48\x89\x74\x24\x20\x57\x48\x81\xec", 14},
        {eventGroup, "\x48\x89\x5c\x24\x18\x48\x89\x74\x24\x20\x57\x48\x81\xec", 14},
        {eventRelease, "\x48\x89\x5c\x24\x10\x57\x48\x81\xec\x40\x01\x00\x00", 13},
        {eventStart, "\x48\x89\x5c\x24\x10\x57\x48\x81\xec\x40\x01\x00\x00", 13},
        {eventStop, "\x48\x89\x5c\x24\x18\x48\x89\x74\x24\x20\x57\x48\x81\xec", 14},
        {eventPosition, "\x48\x89\x5c\x24\x18\x48\x89\x74\x24\x20\x57\x48\x81\xec", 14},
        {eventStation, "\x40\x53\x55\x56\x48\x81\xec\x50\x01\x00\x00", 11},
        {eventProperty, "\x40\x53\x55\x56\x48\x81\xec\x50\x01\x00\x00", 11},
        {eventVolume, "\x48\x89\x5c\x24\x18\x57\x48\x81\xec\x50\x01\x00\x00", 13},
        {eventPause, "\x48\x89\x5c\x24\x18", 5},
        {removeDsp, "\x48\x8b\xc4\x57\x48\x81\xec\x60\x01\x00\x00", 11},
        {0x197b4f0, "\x48\x83\xec\x48", 4},
    };
    for (const auto& entry : entries) {
        std::array<Byte, 16> bytes{};
        if (!read(game + entry.rva, bytes) || std::memcmp(bytes.data(), entry.bytes, entry.size)) return false;
    }
    return true;
}

void disconnect(NativeVoice& voice) {
    if (voice.meter.fft) {
        if (voice.meter.group)
            native<int (*)(void*, void*)>(removeDsp)(voice.meter.group, voice.meter.fft);
        native<radio::Release>(0x27c5710)(voice.meter.fft);
    }
    if (voice.instance) {
        native<int (*)(void*, int)>(eventStop)(voice.instance, 1);
        native<radio::Release>(eventRelease)(voice.instance);
    }
    voice = {};
}

bool attachMeter(NativeVoice& voice, void* group) {
    if (voice.meter.group == group && voice.meter.fft) return true;
    if (voice.meter.fft) {
        native<int (*)(void*, void*)>(removeDsp)(voice.meter.group, voice.meter.fft);
        native<radio::Release>(0x27c5710)(voice.meter.fft);
    }
    voice.meter = {};
    void* core{};
    void* fft{};
    if (native<radio::Pointer>(0x28aa280)(nativeStudio, &core) || !core ||
        native<radio::DspAt>(0x27be190)(core, radio::fftType, &fft) || !fft) return false;
    void* connection{};
    if (native<int (*)(void*, int, void*, void**)>(0x27c1910)(group, -1, fft, &connection)) {
        native<radio::Release>(0x27c5710)(fft);
        return false;
    }
    native<int (*)(void*, bool)>(0x27c58b0)(fft, true);
    int window{}, rate{}, mode{}, speakers{};
    native<int (*)(void*, int, int*, char*, int)>(0x27c53d0)(fft, radio::windowParameter, &window, nullptr, 0);
    native<int (*)(void*, int*, int*, int*)>(0x27bf4f0)(core, &rate, &mode, &speakers);
    voice.meter.window = window >= 64 && window <= 16384 ? window : 2048;
    voice.meter.rate = rate >= 8000 && rate <= 192000 ? rate : 48000;
    voice.meter.group = group;
    voice.meter.fft = fft;
    return true;
}

Attributes attributesFor(const Request& request) {
    const auto& m = request.matrix;
    return {{m._41, m._42, m._43}, {}, {m._31, m._32, m._33}, {m._21, m._22, m._23}};
}

bool createNative(NativeVoice& voice, const Request& request) {
    void* instance{};
    const auto check = [](int result, const char* step) {
        if (result && !reportedEventFailure) {
            reportedEventFailure = true;
            char message[160]{};
            std::snprintf(message, sizeof(message), "Drivatar native radio %s failed (FMOD %d); waiting to retry.", step, result);
            log(message);
        }
        return result == 0;
    };
    if (!check(native<radio::Pointer>(eventCreate)(description, &instance), "create") || !instance) return false;
    voice.instance = instance;
    audioCleanupDone = false;
    const auto attributes = attributesFor(request);
    const bool ok = check(native<int (*)(void*, const char*, float)>(eventStation)(instance, "PGG_RADIO_Station", static_cast<float>(request.station)), "station") &&
        check(native<int (*)(void*, const Attributes*)>(eventPosition)(instance, &attributes), "position") &&
        check(native<int (*)(void*, int, float)>(eventProperty)(instance, 3, 2.0f), "minimum distance") &&
        check(native<int (*)(void*, int, float)>(eventProperty)(instance, 4, audibleDistance), "maximum distance") &&
        check(native<int (*)(void*, float)>(eventVolume)(instance, playbackVolume), "volume") &&
        check(native<radio::Release>(eventStart)(instance), "start");
    if (!ok) disconnect(voice);
    else log("Independent native Drivatar station started.");
    return ok;
}

void updateNative(const std::array<Request, voiceCount>& current, ULONGLONG now) {
    const auto* engine = read<Byte*>(game + radio::engineGlobal);
    void* studio = engine ? read<void*>(engine + 0xa9fe8) : nullptr;
    if (nativeStudio != studio) {
        // The previous engine owns the retired handles. Never release through
        // a different engine after a session or title has been unloaded.
        nativeVoices = {};
        nativeStudio = studio;
        description = nullptr;
        descriptionRetry = 0;
    }
    if (!studio) return;
    for (auto& voice : nativeVoices) {
        if (!voice.identity) continue;
        const auto request = std::find_if(current.begin(), current.end(), [&](const Request& r) {
            return r.identity == voice.identity && r.epoch == voice.epoch && !r.network;
        });
        if (request == current.end()) disconnect(voice);
    }
    const bool needed = std::any_of(current.begin(), current.end(), [](const Request& r) { return r.identity && !r.network; });
    if (!needed) return;
    if (!description && now >= descriptionRetry) {
        descriptionRetry = now + 2000;
        for (const auto* path : {"event:/Master/Radio_3D/RADIO_3D_Music", "event:/Master/Radio_3D/EMT_3D_Music_Sml"}) {
            if (!native<int (*)(void*, const char*, void**)>(eventLookup)(studio, path, &description) && description) break;
            description = nullptr;
        }
    }
    if (!description) return;
    for (const auto& request : current) {
        if (!request.identity || request.network) continue;
        auto voice = std::find_if(nativeVoices.begin(), nativeVoices.end(), [&](const NativeVoice& v) { return v.identity == request.identity; });
        if (voice == nativeVoices.end()) voice = std::find_if(nativeVoices.begin(), nativeVoices.end(), [](const NativeVoice& v) { return !v.identity; });
        if (voice == nativeVoices.end()) continue;
        const auto slot = static_cast<size_t>(voice - nativeVoices.begin());
        voice->identity = request.identity;
        voice->epoch = request.epoch;
        if (!voice->instance && now >= voice->retryAt && !createNative(*voice, request)) {
            voice->identity = request.identity;
            voice->epoch = request.epoch;
            voice->retryAt = now + 2000;
        }
        if (!voice->instance) continue;
        const auto attributes = attributesFor(request);
        native<int (*)(void*, const Attributes*)>(eventPosition)(voice->instance, &attributes);
        native<int (*)(void*, bool)>(eventPause)(voice->instance, request.paused);
        if (request.paused) continue;
        void* group{};
        if (native<radio::Pointer>(eventGroup)(voice->instance, &group) || !group || !attachMeter(*voice, group) ||
            !radio::measure(now, voice->meter) || request.mix <= 1e-5f) voice->meter.levels = {};
        publish(nativeMeters, slot, request, voice->meter, now);
        if (!reportedNativeSignal && *std::max_element(voice->meter.levels.begin(), voice->meter.levels.end()) > .05f) {
            reportedNativeSignal = true;
            log("Independent native Drivatar radio received its own music signal.");
        }
    }
}
