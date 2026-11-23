// Included inside Underglow.cpp's game integration namespace.
namespace radio {
constexpr size_t tickRva = 0x19a2840;
constexpr size_t pauseMenuRva = 0x14b0df0;
constexpr size_t engineGlobal = 0x4e728e8;
constexpr char tracksBus[] = "bus:/MasterVolume/MUSIC/MX_Radio2D/MX_Tracks";
constexpr char grooveBus[] = "bus:/MasterVolume/MUSIC/MX_Radio2D/MX_Tracks/Track/Groove";
constexpr int fftType = 29, windowParameter = 0, spectrumParameter = 2;
enum class Route { off, tracks, groove };

#if UG_SUPPORT_HORIZON_RADIO_NETWORK
struct HorizonRadioBandsV1 {
    // 1 owns the radio route, including silence. 0 leaves FH3's native route active.
    int (*read)(float* levels);
};
constexpr char horizonRadioBandsName[] = "Horizon3Extended.RadioBands";
const HorizonRadioBandsV1* horizonRadio{};
bool networkActive{};
#endif

using Tick = void (*)(void*, float);
using Pointer = int (*)(void*, void**);
using DspAt = int (*)(void*, int, void**);
using DspName = int (*)(void*, char*, unsigned*, int*, int*, int*);
using Release = int (*)(void*);
Tick originalTick;
unsigned char (*inPauseMenu)();

struct Spectrum {
    int length, channels;
    const float* bins[32];
};

struct Tap {
    void* studio{};
    void* group{};
    void* fft{};
    Route route{};
    int rate{48000}, window{2048};
    ULONGLONG nextLookup{}, last{}, heard{};
    std::array<float, zoneCount> peaks{}, levels{};
    bool connected{}, faulted{};
} tap;
bool reportedTracks{}, reportedGroove{}, heardTracks{}, heardGroove{};

SRWLOCK sampleLock = SRWLOCK_INIT;
fx::AudioFrame sample;
bool samplePaused{};

void holdForPause(ULONGLONG now) {
    AcquireSRWLockExclusive(&sampleLock);
    if (!samplePaused && sample.last && now - sample.last > 500) sample = {};
    const bool held = !samplePaused &&
        std::any_of(std::begin(sample.lvl), std::end(sample.lvl), [](float value) { return value > 0; });
    samplePaused = true;
    ReleaseSRWLockExclusive(&sampleLock);
    if (held) log("Pause menu is holding the last radio underglow frame.");
}

void publish(ULONGLONG now, bool playing) {
    AcquireSRWLockExclusive(&sampleLock);
    if (samplePaused) {
        sample = {};
        samplePaused = false;
    }
    const float dt = sample.last ? static_cast<float>(now - sample.last) * 0.001f : 0;
    sample.last = now;
    for (size_t i = 0; i < zoneCount; ++i) {
        const float value = playing ? tap.levels[i] : 0;
        sample.lvl[i] = value;
        if (value >= sample.peak[i] || !playing) {
            sample.peak[i] = value;
            sample.peak_at[i] = now;
        } else if (now - sample.peak_at[i] > 400) {
            sample.peak[i] = std::max(value, sample.peak[i] - 0.8f * dt);
        }
    }
    ReleaseSRWLockExclusive(&sampleLock);
}

fx::AudioFrame frame(ULONGLONG now) {
    AcquireSRWLockShared(&sampleLock);
    const auto result = sample;
    const bool paused = samplePaused;
    ReleaseSRWLockShared(&sampleLock);
    return result.last && (paused || now - result.last <= 500) ? result : fx::AudioFrame{};
}

Route currentRoute(const Byte* manager) {
    std::array<Byte, 5> flags;
    if (!manager || !read(manager + 0x74, flags) || !flags[0] || flags[1] || flags[4] ||
        read<Byte>(manager + 0x99)) return Route::off;
    const auto* swapper = read<Byte*>(manager + 0x40);
    const auto* station = swapper ? read<Byte*>(swapper + 0x50) : nullptr;
    if (!station) return Route::off;
    if (!supportExtraOptions) return Route::tracks;
    // The native station name is an inline MSVC string. GrooveMusicRadio leaves
    // this internal name intact when it supplies a local file or a live stream.
    constexpr char grooveName[] = "Xbox Music";
    const auto* name = station + 0x200;
    std::array<char, sizeof(grooveName) - 1> label{};
    if (read<size_t>(name + 0x10) == label.size() && read<size_t>(name + 0x18) == 15 &&
        read(name, label) && !std::memcmp(label.data(), grooveName, label.size())) return Route::groove;
    return Route::tracks;
}

bool hasName(void* dsp, const char* expected) {
    char name[33]{};
    unsigned version;
    int channels, width, dialogHeight;
    if (native<DspName>(0x27c4af0)(dsp, name, &version, &channels, &width, &dialogHeight)) return false;
    for (int i = 31; i >= 0 && (name[i] == ' ' || !name[i]); --i) name[i] = '\0';
    return std::strcmp(name, expected) == 0;
}

bool connect(ULONGLONG now, Route route) {
    const auto* engine = read<Byte*>(game + engineGlobal);
    void* studio = engine ? read<void*>(engine + 0xa9fe8) : nullptr;
    if (tap.studio != studio) {
        tap.studio = studio;
        tap.group = tap.fft = nullptr;
        tap.nextLookup = 0;
        tap.connected = false;
    }
    if (!studio) return false;
    if (tap.route != route) {
        tap.route = route;
        tap.nextLookup = tap.heard = tap.last = 0;
        tap.levels = {};
        tap.peaks = {};
    }
    if (now < tap.nextLookup) return tap.connected;
    tap.nextLookup = now + 1000;
    tap.connected = false;
    void* bus = nullptr;
    void* group = nullptr;
    const bool groove = supportExtraOptions && route == Route::groove;
    const auto* path = groove ? grooveBus : tracksBus;
    if (native<int (*)(void*, const char*, void**)>(0x28a9840)(studio, path, &bus) || !bus ||
        native<Pointer>(0x28a9920)(bus, &group) || !group) return false;

    int position = 0;
    for (int i = 0; i < 32; ++i) {
        void* dsp = nullptr;
        if (native<DspAt>(0x27c1c30)(group, i, &dsp) || !dsp) break;
        if (tap.group == group && dsp == tap.fft) return tap.connected = true;
        if (!groove && hasName(dsp, "FMOD Fader")) position = i + 1;
    }
    if (tap.fft) native<Release>(0x27c5710)(tap.fft);
    tap.fft = tap.group = nullptr;

    void* core = nullptr;
    void* fft = nullptr;
    if (native<Pointer>(0x28aa280)(studio, &core) || !core ||
        native<DspAt>(0x27be190)(core, fftType, &fft) || !fft) return false;
    void* connection = nullptr;
    if (!hasName(fft, "FMOD FFT") ||
        native<int (*)(void*, int, void*, void**)>(0x27c1910)(group, position, fft, &connection)) {
        native<Release>(0x27c5710)(fft);
        return false;
    }
    native<int (*)(void*, bool)>(0x27c58b0)(fft, true);
    int window = 0, rate = 0, mode = 0, speakers = 0;
    native<int (*)(void*, int, int*, char*, int)>(0x27c53d0)(fft, windowParameter, &window, nullptr, 0);
    native<int (*)(void*, int*, int*, int*)>(0x27bf4f0)(core, &rate, &mode, &speakers);
    tap.window = window >= 64 && window <= 16384 ? window : 2048;
    tap.rate = rate >= 8000 && rate <= 192000 ? rate : 48000;
    tap.fft = fft;
    tap.group = group;
    tap.peaks = {};
    tap.levels = {};
    bool& reported = groove ? reportedGroove : reportedTracks;
    if (!reported) log(groove ? "Radio visualizer connected to the Groove Music bus." :
        "Radio visualizer is listening to the car's FMOD tracks bus.");
    reported = true;
    return tap.connected = true;
}

bool measureLevels(ULONGLONG now, const std::array<float, zoneCount>& raw) {
    const bool sound = *std::max_element(raw.begin(), raw.end()) > 1e-7f;
    if (sound) tap.heard = now;
    else if (tap.heard && now - tap.heard < 200) return true;
    else tap.levels = {};

    const float dt = tap.last ? static_cast<float>(now - tap.last) * 0.001f : 0.016f;
    tap.last = now;
    for (size_t i = 0; i < zoneCount; ++i) {
        tap.peaks[i] = std::max(raw[i], tap.peaks[i] * std::exp(-dt * 0.25f));
        const float target = tap.peaks[i] > 1e-8f ? std::clamp((raw[i] / tap.peaks[i] - 0.15f) / 0.85f, 0.0f, 1.0f) : 0;
        tap.levels[i] = target >= tap.levels[i] ? target : target + (tap.levels[i] - target) * std::exp(-dt * 7.0f);
    }
    return sound;
}

bool measure(ULONGLONG now) {
    void* data = nullptr;
    unsigned size = 0;
    if (native<int (*)(void*, int, void**, unsigned*, char*, int)>(0x27c5100)(
        tap.fft, spectrumParameter, &data, &size, nullptr, 0) || !data) return false;
    Spectrum fft;
    if (size < sizeof(fft) || !read(data, fft) || fft.length < 32 || fft.length > 16384 ||
        fft.channels < 1 || fft.channels > 32) return false;

    const int count = std::min(fft.length, tap.window / 2);
    const float hz = static_cast<float>(tap.rate) / static_cast<float>(tap.window);
    constexpr float bands[zoneCount][2]{{3000, 12000}, {800, 3000}, {30, 160}, {160, 800}};
    std::array<float, zoneCount> raw{};
    for (int channel = 0; channel < fft.channels; ++channel) {
        if (!fft.bins[channel]) return false;
        for (size_t zone = 0; zone < zoneCount; ++zone) {
            const int low = std::clamp(static_cast<int>(bands[zone][0] / hz), 1, count - 1);
            const int high = std::clamp(static_cast<int>(bands[zone][1] / hz), low, count - 1);
            float total = 0;
            for (int bin = low; bin <= high; ++bin) {
                float value;
                if (!read(fft.bins[channel] + bin, value) || !std::isfinite(value) || value < 0 || value >= 1000)
                    return false;
                total += value;
            }
            raw[zone] += total / static_cast<float>((high - low + 1) * fft.channels);
        }
    }
    if (*std::max_element(raw.begin(), raw.end()) > 1e-7f) {
        const bool groove = supportExtraOptions && tap.route == Route::groove;
        bool& heard = groove ? heardGroove : heardTracks;
        if (!heard) log(groove ? "Radio visualizer received Groove Music audio." :
            "Radio visualizer received car radio audio.");
        heard = true;
    }
    return measureLevels(now, raw);
}

bool audibleGroove() {
    float gain = 0;
    return !native<int (*)(void*, float*)>(0x27c1b50)(tap.group, &gain) && gain > 0.00001f;
}

#if UG_SUPPORT_HORIZON_RADIO_NETWORK
bool useNetwork(ULONGLONG now) {
    std::array<float, zoneCount> raw{};
    const bool active = horizonRadio && horizonRadio->read && horizonRadio->read(raw.data()) == 1;
    if (active != networkActive) {
        networkActive = active;
        tap.peaks = {};
        tap.levels = {};
        tap.heard = tap.last = 0;
        publish(now, false);
        if (active) log("Radio visualizer is listening to Horizon Radio Network.");
    }
    if (active) {
        for (auto& level : raw) level = std::isfinite(level) ? std::clamp(level, 0.0f, 1.0f) : 0.0f;
        // The provider keeps ownership while waiting for music. Publish silence
        // without listening to the stock bus under the replacement stream.
        if (std::none_of(raw.begin(), raw.end(), [](float level) { return level > 1e-7f; })) {
            tap.levels = {};
            tap.heard = 0;
        }
        publish(now, measureLevels(now, raw));
    }
    return active;
}
#endif

bool pauseMenuActive() {
    __try {
        return inPauseMenu && inPauseMenu();
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        inPauseMenu = nullptr;
        return false;
    }
}

void tick(void* manager, float dt) {
    originalTick(manager, dt);
    const auto now = GetTickCount64();
    __try {
        // FH3 can mute the radio while the pause menu is drawing over free roam.
        // Keep the last rendered audio levels until the native menu closes.
        if (pauseMenuActive()) {
            holdForPause(now);
            return;
        }
#if UG_SUPPORT_HORIZON_RADIO_NETWORK
        if (useNetwork(now)) return;
#endif
        if (tap.faulted) return;
        const auto route = currentRoute(static_cast<const Byte*>(manager));
        if (route == Route::tracks) {
            publish(now, connect(now, route) && measure(now));
        } else if (supportExtraOptions && connect(now, Route::groove) && audibleGroove() && measure(now)) {
            // Groove has its own streaming player. The stock station can be
            // inactive while Groove is audible, so check its post-fader bus.
            publish(now, true);
        } else {
            tap.levels = {};
            tap.heard = 0;
            publish(now, false);
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        tap.faulted = true;
        publish(now, false);
        log("Radio visualizer went dark after the FH3 audio tap failed.");
    }
}

bool install() {
#if UG_SUPPORT_HORIZON_RADIO_NETWORK
    if (api->get_interface) {
        horizonRadio = static_cast<const HorizonRadioBandsV1*>(api->get_interface(horizonRadioBandsName, 1));
        if (horizonRadio) log("Horizon Radio Network visualizer bridge is ready.");
    }
#endif
    constexpr char entry[] = "\x48\x8b\xc4\x55\x53\x56\x57\x41\x54";
    if (!api->hook_function || std::memcmp(game + tickRva, entry, sizeof(entry) - 1)) return false;
    // This native accessor reads the StateFlow InPauseMenu flag.
    constexpr char pauseQuery[] = "\x40\x57\x48\x83\xec\x40\x48\xc7\x44\x24\x20";
    if (core::platform_menu::matches(game + pauseMenuRva, pauseQuery, sizeof(pauseQuery) - 1))
        inPauseMenu = native<unsigned char (*)()>(pauseMenuRva);
    auto* hook = api->hook_function(mod, "FH3 car radio visualizer", game + tickRva,
        reinterpret_cast<void*>(tick), reinterpret_cast<void**>(&originalTick));
    return hook && api->enable_hooks(mod, hook);
}
} // namespace radio
