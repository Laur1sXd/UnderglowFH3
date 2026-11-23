#pragma once

// Dynamic use of libmpv's public C client ABI. Network operations and player
// destruction stay on this worker and never block a game/render callback.
struct MpvNode;
struct MpvList { int count; MpvNode* values; const char** keys; };
struct MpvNode {
    union { const char* text; int flag; int64_t integer; double real; MpvList* list; void* bytes; } value;
    int format;
};
struct MpvEvent { int id, error; uint64_t reply; void* data; };
struct MpvProperty { const char* name; int format; void* data; };
constexpr int mpvFlag = 3, mpvDouble = 5, mpvNode = 6, mpvMap = 8;

struct MpvApi {
    HMODULE library{};
    void* (*create)(){};
    int (*initialize)(void*){};
    void (*destroy)(void*){};
    int (*option)(void*, const char*, const char*){};
    int (*command)(void*, uint64_t, const char* const*){};
    int (*set)(void*, uint64_t, const char*, int, void*){};
    int (*get)(void*, uint64_t, const char*, int){};
    MpvEvent* (*event)(void*, double){};

    template<class Fn> bool bind(Fn& function, const char* name) {
        function = reinterpret_cast<Fn>(GetProcAddress(library, name));
        return function != nullptr;
    }
    bool load(const std::filesystem::path& root) {
        if (library) return true;
        library = LoadLibraryExW((root / L"libmpv-2.dll").c_str(), nullptr,
            LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_SYSTEM32);
        if (!library) return false;
        const bool ok = bind(create, "mpv_create") && bind(initialize, "mpv_initialize") &&
            bind(destroy, "mpv_terminate_destroy") && bind(option, "mpv_set_option_string") &&
            bind(command, "mpv_command_async") && bind(set, "mpv_set_property_async") &&
            bind(get, "mpv_get_property_async") && bind(event, "mpv_wait_event");
        if (!ok) { FreeLibrary(library); library = nullptr; }
        return ok;
    }
} mpv;

struct NetworkVoice {
    uint64_t identity{}, epoch{};
    void* player{};
    radio::Tap meter{};
    ULONGLONG retryAt{}, loadedAt{}, lastAudio{}, nextCommand{};
    bool pendingMeter{}, paused{}, loaded{};
    double gain{-1};
};
std::array<NetworkVoice, voiceCount> networkVoices{};

std::string utf8(std::wstring_view text) {
    if (text.empty() || text.size() > INT_MAX) return {};
    const int count = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, text.data(),
        static_cast<int>(text.size()), nullptr, 0, nullptr, nullptr);
    if (!count) return {};
    std::string result(count, '\0');
    WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, text.data(), static_cast<int>(text.size()),
        result.data(), count, nullptr, nullptr);
    return result;
}

std::vector<std::string> stationTracks(const std::filesystem::path& root, int station, uint64_t identity) {
    if (station < 1 || station > static_cast<int>(std::size(stations))) return {};
    constexpr const wchar_t* aliases[]{L"Pulse", L"BassArena", L"BlockParty", L"Vagrant",
        L"Epitaph", L"Hospital", L"FutureClassic", L"Timeless"};
    std::vector<std::string> tracks;
    for (const auto* directory : {stations[station - 1], aliases[station - 1]}) {
        std::error_code error;
        for (const auto& file : std::filesystem::directory_iterator(root / L"Radio" / directory, error)) {
            if (!file.is_regular_file(error)) continue;
            const auto extension = file.path().extension().wstring();
            for (const auto* allowed : {L".mp3", L".flac", L".ogg", L".wav", L".m4a", L".opus"}) {
                if (!_wcsicmp(extension.c_str(), allowed)) tracks.push_back(utf8(file.path().wstring()));
            }
            if (tracks.size() >= 256) break;
        }
        if (!tracks.empty()) break;
    }
    if (tracks.empty()) {
        wchar_t playlist[512]{};
        GetPrivateProfileStringW(stations[station - 1], L"Playlist", L"", playlist,
            static_cast<DWORD>(std::size(playlist)), (root / L"Radio.ini").c_str());
        std::string list = utf8(playlist);
        if (!list.empty()) {
            for (const std::string_view prefix : {"https://www.youtube.com/playlist?list=", "https://music.youtube.com/playlist?list="})
                if (list.starts_with(prefix)) list.erase(0, prefix.size());
            list.resize(std::min(list.size(), list.find('&')));
            if (list.size() >= 10 && list.size() <= 80 &&
                list.find_first_not_of("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_-") == list.npos)
                tracks.push_back("ytdl://https://music.youtube.com/playlist?list=" + list);
            return tracks;
        }
        for (const auto& folder : {root, root / L"StarterCatalogues"}) {
            const auto path = folder / (std::wstring(stations[station - 1]) + L".catalogue");
            std::ifstream file(path);
            std::string kind, id;
            size_t lines = 0;
            while (lines++ < 4096 && file >> kind >> id) {
                if (kind == "song" && id.size() == 11 &&
                    id.find_first_not_of("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_-") == id.npos)
                    tracks.push_back("ytdl://https://music.youtube.com/watch?v=" + id);
            }
            if (!tracks.empty()) break;
        }
    }
    std::sort(tracks.begin(), tracks.end());
    tracks.erase(std::unique(tracks.begin(), tracks.end()), tracks.end());
    if (!tracks.empty()) std::rotate(tracks.begin(), tracks.begin() + identity % tracks.size(), tracks.end());
    if (tracks.size() > 12) tracks.resize(12);
    return tracks;
}

std::array<float, zoneCount> metadataLevels(const MpvNode& node) {
    std::array<float, zoneCount> levels{};
    if (node.format != mpvMap || !node.value.list) return levels;
    constexpr const char* names[]{"lavfi.astats.6.RMS_level", "lavfi.astats.5.RMS_level",
        "lavfi.astats.3.RMS_level", "lavfi.astats.4.RMS_level"};
    const auto& list = *node.value.list;
    if (list.count < 0 || list.count > 1024 || !list.keys || !list.values) return levels;
    for (int i = 0; i < list.count; ++i) {
        const auto& value = list.values[i];
        if (value.format != 1 || !value.value.text || !list.keys[i]) continue;
        for (size_t zone = 0; zone < zoneCount; ++zone) {
            if (std::strcmp(names[zone], list.keys[i])) continue;
            char* end{};
            const float db = std::strtof(value.value.text, &end);
            if (end != value.value.text && !*end && std::isfinite(db) && db > -85)
                levels[zone] = std::pow(10.0f, std::min(0.0f, db) * .05f);
        }
    }
    // A filter's quiet stop-band leakage is not another instrument. Avoid
    // normalising that leakage into a full-scale beat on every car side.
    const float floor = std::max(1e-5f, *std::max_element(levels.begin(), levels.end()) * .03f);
    for (auto& level : levels) if (level < floor) level = 0;
    return levels;
}

bool createNetwork(NetworkVoice& voice, const Request& request, const std::filesystem::path& root) {
    const auto tracks = stationTracks(root, request.station, request.identity);
    if (tracks.empty()) return false;
    void* player = mpv.create();
    if (!player) return false;
    const auto option = [&](const char* key, const char* value) { return mpv.option(player, key, value) >= 0; };
    const auto resolver = "ytdl_hook-ytdl_path=" + utf8((root / L"yt-dlp.exe").wstring());
    constexpr const char* meter =
        "@ug_npc:lavfi=[aformat=channel_layouts=stereo,asplit=5[a][b][c][d][e];"
        "[b]pan=mono|c0=0.5*c0+0.5*c1,highpass=f=30,lowpass=f=160[bass];"
        "[c]pan=mono|c0=0.5*c0+0.5*c1,highpass=f=160,lowpass=f=800[low];"
        "[d]pan=mono|c0=0.5*c0+0.5*c1,highpass=f=800,lowpass=f=3000[mid];"
        "[e]pan=mono|c0=0.5*c0+0.5*c1,highpass=f=3000,lowpass=f=12000[high];"
        "[a][bass][low][mid][high]join=inputs=5:channel_layout=5.1:map=0.0-FL|0.1-FR|1.0-FC|2.0-LFE|3.0-BL|4.0-BR,"
        "asetnsamples=n=1536:p=0,astats=metadata=1:reset=1:measure_perchannel=RMS_level:measure_overall=none,pan=stereo|c0=c0|c1=c1]";
    const bool ok = option("config", "no") && option("terminal", "no") && option("msg-level", "all=no") && option("vid", "no") &&
        option("force-window", "no") && option("input-default-bindings", "no") && option("idle", "yes") &&
        option("volume", "0") && option("loop-playlist", "inf") && option("demuxer-max-bytes", "2MiB") &&
        option("network-timeout", "10") && option("ytdl-format", "bestaudio/best") &&
        option("ytdl-raw-options", "ignore-config=,no-cache-dir=,js-runtimes=node,socket-timeout=10,retries=1,extractor-retries=1,playlist-end=12") &&
        option("script-opts", resolver.c_str()) && option("af", meter) && mpv.initialize(player) >= 0;
    if (!ok) { mpv.destroy(player); return false; }
    voice.player = player;
    for (size_t i = 0; i < tracks.size(); ++i) {
        const char* command[]{"loadfile", tracks[i].c_str(), i ? "append" : "replace", nullptr};
        if (mpv.command(player, ++voice.nextCommand, command) < 0) {
            mpv.destroy(player);
            voice.player = nullptr;
            return false;
        }
    }
    voice.loadedAt = GetTickCount64();
    log("Independent Horizon Radio Network Drivatar station started.");
    return true;
}

void disconnect(NetworkVoice& voice) {
    if (voice.player) {
        double volume = 0;
        mpv.set(voice.player, 0, "volume", mpvDouble, &volume);
        mpv.destroy(voice.player);
    }
    voice = {};
}

void updateNetwork(const std::array<Request, voiceCount>& current, ULONGLONG now) {
    static ULONGLONG loadAfter{};
    const auto root = std::filesystem::path(api->azimuth_dir) / L"Horizon3Extended/HorizonRadioNetwork";
    // Silence every retired voice before any potentially blocking destruction.
    for (auto& voice : networkVoices) {
        if (!voice.player) continue;
        const bool wanted = std::any_of(current.begin(), current.end(), [&](const Request& r) {
            return r.identity == voice.identity && r.epoch == voice.epoch && r.network;
        });
        if (!wanted) { double zero = 0; mpv.set(voice.player, 0, "volume", mpvDouble, &zero); }
    }
    for (auto& voice : networkVoices) {
        const bool wanted = std::any_of(current.begin(), current.end(), [&](const Request& r) {
            return r.identity == voice.identity && r.epoch == voice.epoch && r.network;
        });
        if (voice.identity && !wanted) disconnect(voice);
    }
    if (std::none_of(current.begin(), current.end(), [](const Request& r) { return r.identity && r.network; })) return;
    if (!mpv.library) {
        if (now < loadAfter) return;
        loadAfter = now + 5000;
        if (!mpv.load(root)) return;
    }
    for (const auto& request : current) {
        if (!request.identity || !request.network) continue;
        auto voice = std::find_if(networkVoices.begin(), networkVoices.end(), [&](const NetworkVoice& v) { return v.identity == request.identity; });
        if (voice == networkVoices.end()) voice = std::find_if(networkVoices.begin(), networkVoices.end(), [](const NetworkVoice& v) { return !v.identity; });
        if (voice == networkVoices.end()) continue;
        const auto slot = static_cast<size_t>(voice - networkVoices.begin());
        voice->identity = request.identity;
        voice->epoch = request.epoch;
        if (!voice->player && now >= voice->retryAt && !createNetwork(*voice, request, root)) voice->retryAt = now + 10000;
        if (!voice->player) continue;
        if (voice->paused != request.paused) {
            int paused = request.paused;
            mpv.set(voice->player, 0, "pause", mpvFlag, &paused);
            voice->paused = request.paused;
        }
        const double gain = request.paused ? 0 : 100 * std::cbrt(static_cast<double>(request.gain));
        if (std::abs(voice->gain - gain) > .25) {
            double value = gain;
            mpv.set(voice->player, 0, "volume", mpvDouble, &value);
            voice->gain = gain;
        }
        bool failed = false;
        for (size_t events = 0; events < 64; ++events) {
            const auto* event = mpv.event(voice->player, 0);
            if (!event || !event->id) break;
            if (event->id == 8) voice->loaded = true;
            if (event->id == 7) { voice->loaded = false; voice->meter.levels = {}; }
            if (event->id == 1) failed = true;
            if (event->id == 3 && event->reply == 0x55474d) {
                voice->pendingMeter = false;
                const auto* property = static_cast<const MpvProperty*>(event->data);
                if (!event->error && property && property->format == mpvNode && property->data) {
                    radio::measureLevels(now, metadataLevels(*static_cast<const MpvNode*>(property->data)), voice->meter);
                    voice->lastAudio = now;
                }
            }
        }
        if (!voice->loaded || now - voice->lastAudio > 500 || request.gain <= 1e-5f) voice->meter.levels = {};
        if (voice->loaded && !request.paused && !voice->pendingMeter) {
            voice->pendingMeter = mpv.get(voice->player, 0x55474d, "af-metadata/ug_npc", mpvNode) >= 0;
        }
        publish(networkMeters, slot, request, voice->meter, now);
        if (failed || (!voice->loaded && now >= voice->loadedAt && now - voice->loadedAt > 30000)) {
            disconnect(*voice);
            voice->identity = request.identity;
            voice->epoch = request.epoch;
            voice->retryAt = now + 10000;
        }
    }
}
