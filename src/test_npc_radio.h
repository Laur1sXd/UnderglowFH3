#pragma once

namespace npcTest {
struct Event { int station{}; bool started{}, paused{}, silent{}; npcRadio::Attributes attributes{}; };
struct Fft { Event* event{}; std::array<float, 1024> bins{}; radio::Spectrum spectrum{}; };
std::set<Event*> events;
std::set<Fft*> meters;
int created{}, released{};
int lookup(void*, const char*, void** out) { *out = &events; return 0; }
int create(void*, void** out) { auto* event = new Event; events.insert(event); *out = event; ++created; return 0; }
int station(void* p, const char* name, float value) {
    assert(std::strcmp(name, "PGG_RADIO_Station") == 0); static_cast<Event*>(p)->station = int(value); return 0;
}
int position(void* p, const npcRadio::Attributes* attributes) { static_cast<Event*>(p)->attributes = *attributes; return 0; }
int property(void*, int which, float value) { assert((which == 3 && value == 2) || (which == 4 && value == 45)); return 0; }
int volume(void*, float value) { assert(value >= 0 && value <= npcRadio::playbackVolume); return 0; }
int start(void* p) { static_cast<Event*>(p)->started = true; return 0; }
int pause(void* p, bool value) { static_cast<Event*>(p)->paused = value; return 0; }
int stop(void* p, int mode) { assert(mode == 1); static_cast<Event*>(p)->started = false; return 0; }
int release(void* p) { assert(events.erase(static_cast<Event*>(p)) == 1); delete static_cast<Event*>(p); ++released; return 0; }
int pointer(void* p, void** out) { *out = p; return 0; }
int bus(void* p, const char*, void** out) { *out = p; return 0; }
float slider(void*, const SmallString* name) { assert(std::string(name->text, name->length) == "AOPT_Radio"); return .5f; }
int gain(void*, float* out) { *out = .8f; return 0; }
int createFft(void*, int type, void** out) { assert(type == radio::fftType); auto* fft = new Fft; meters.insert(fft); *out = fft; return 0; }
int addFft(void* group, int index, void* p, void**) { assert(index == -1); static_cast<Fft*>(p)->event = static_cast<Event*>(group); return 0; }
int removeFft(void*, void*) { return 0; }
int releaseFft(void* p) { assert(meters.erase(static_cast<Fft*>(p)) == 1); delete static_cast<Fft*>(p); return 0; }
int active(void*, bool value) { assert(value); return 0; }
int window(void*, int, int* out, char*, int) { *out = 2048; return 0; }
int format(void*, int* rate, int* mode, int* speakers) { *rate = 48000; *mode = 0; *speakers = 2; return 0; }
int spectrum(void* p, int, void** out, unsigned* bytes, char*, int) {
    auto& fft = *static_cast<Fft*>(p); fft.bins.fill(0);
    if (!fft.event->silent && !fft.event->paused) fft.bins[fft.event->station == 1 ? 4 : 256] = .8f;
    fft.spectrum.length = 1024; fft.spectrum.channels = 1; fft.spectrum.bins[0] = fft.bins.data();
    *out = &fft.spectrum; *bytes = sizeof(fft.spectrum); return 0;
}
void stubs() {
    const auto stub = [](size_t rva, auto fn) { test::stub(rva, reinterpret_cast<void*>(fn)); };
    stub(npcRadio::eventLookup, lookup); stub(npcRadio::eventCreate, create); stub(npcRadio::eventGroup, pointer);
    stub(npcRadio::eventStation, station); stub(npcRadio::eventPosition, position); stub(npcRadio::eventProperty, property);
    stub(npcRadio::eventVolume, volume); stub(npcRadio::eventStart, start); stub(npcRadio::eventPause, pause);
    stub(npcRadio::eventStop, stop); stub(npcRadio::eventRelease, release); stub(npcRadio::removeDsp, removeFft);
    stub(0x28aa280, pointer); stub(0x27be190, createFft); stub(0x27c1910, addFft); stub(0x27c5710, releaseFft);
    stub(0x27c58b0, active); stub(0x27c53d0, window); stub(0x27bf4f0, format); stub(0x27c5100, spectrum);
    stub(0x28a9840, bus); stub(0x28a9920, pointer); stub(0x197b4f0, slider); stub(0x27c1b50, gain);
}
// Run the real libmpv decoder/filter with silent output, never a fake meter.
decltype(npcRadio::mpv.option) realOption;
int silentOption(void* player, const char* name, const char* value) {
    if (!std::strcmp(name, "config")) assert(realOption(player, "ao", "null") >= 0);
    return realOption(player, name, value);
}
void wave(const std::filesystem::path& path, double hz) {
    std::filesystem::create_directories(path.parent_path()); std::ofstream file(path, std::ios::binary);
    constexpr uint32_t frames = 48000 * 5, dataBytes = frames * 4;
    const auto put = [&](auto value) { file.write(reinterpret_cast<const char*>(&value), sizeof(value)); };
    file.write("RIFF", 4); put(dataBytes + 36); file.write("WAVEfmt ", 8); put(uint32_t{16});
    put(uint16_t{1}); put(uint16_t{2}); put(uint32_t{48000}); put(uint32_t{192000}); put(uint16_t{4}); put(uint16_t{16});
    file.write("data", 4); put(dataBytes);
    for (uint32_t i = 0; i < frames; ++i) {
        const auto value = static_cast<int16_t>(12000 * std::sin(double(i) * hz * 6.283185307179586 / 48000));
        put(value); put(value);
    }
}
}

void testNpcRadio() {
    std::cout.setf(std::ios::unitbuf);
    _set_error_mode(_OUT_TO_STDERR);
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
    using namespace npcRadio;
    npcTest::stubs();
    FH3ModApi host{}; host.log = +[](FH3Mod*, const char*) {};
    const auto directory = std::filesystem::absolute("npc-radio-fixture");
    const auto directoryName = directory.wstring(); host.azimuth_dir = directoryName.c_str(); api = &host;
    std::vector<Byte> engine(0xaa000); test::put(game, radio::engineGlobal, engine.data());
    test::put(engine.data(), 0xa9fe8, engine.data());
    std::array<Byte, 16> mixer{}; test::put(mixer.data(), 0, game + 0x4639028);
    test::put(game, 0x4e72918, mixer.data());
    assert(std::abs(musicMix() - .1f) < .00001f);
    std::array<Request, voiceCount> current{};
    const auto epoch = routeEpoch.load();
    current[0] = {101, epoch, 1}; current[1] = {202, epoch, 2};
    current[0].matrix._41 = 12; current[1].matrix._41 = 25;
    auto now = GetTickCount64();
    updateNative(current, now);
    assert(npcTest::events.size() == 2 && npcTest::meters.size() == 2);
    auto bass = frame(101, now), treble = frame(202, now);
    assert(bass.lvl[2] > .1f && bass.lvl[0] == 0 && treble.lvl[0] > .1f && treble.lvl[2] == 0);
    // The player's loud signal must not reach a silent NPC's meter.
    radio::sample.last = now; std::fill(std::begin(radio::sample.lvl), std::end(radio::sample.lvl), 1.f);
    for (auto* event : npcTest::events) if (event->station == 1) event->silent = true;
    updateNative(current, now + 1000);
    bass = frame(101, now + 1000); treble = frame(202, now + 1000);
    assert(bass.lvl[2] == 0 && treble.lvl[0] > .1f);
    assert(frame(202, now + 1600).last == 0);
    // Replacing a driver and changing radio route retire only the old owners.
    current[0].identity = 303; current[1].network = true;
    updateNative(current, now + 1100);
    assert(npcTest::events.size() == 1 && npcTest::meters.size() == 1);
    assert(npcTest::released == 2 && npcTest::created == 3);
    for (auto* event : npcTest::events) assert(event->attributes.position.x == 12);
    current = {}; updateNative(current, now + 1200);
    assert(npcTest::events.empty() && npcTest::meters.empty() && npcTest::created == npcTest::released);
    ++routeEpoch; assert(frame(101, now).last == 0); assert(frame(202, now).last == 0);
    requests = {}; requestedAt = now; requests[0].identity = 42;
    assert(currentRequests(now)[0].identity == 42 && currentRequests(now - 1)[0].identity == 42 && !currentRequests(now + 751)[0].identity);
    std::set<int> chosen; for (uint64_t id = 1; id <= 1000; ++id) chosen.insert(stationFor(id));
    assert(chosen.size() == 8);
    // Real request selection keeps only nearby audio-effect AI, sorted by
    // distance. Neither an ordinary static glow nor a stale car starts music.
    struct Car { std::array<Byte, 0x73b0> data{}; std::array<Byte, 0x900> presentation{}; Byte driver{}; };
    std::vector<Car> fixtures(30);
    drivatarSnapshots = {}; snapshot = {}; snapshot.manager = test::manager.data();
    XMStoreFloat4x4(&snapshot.matrix, XMMatrixIdentity()); snapshot.matrixTick = now;
    uint64_t identity = 1;
    for (size_t i = 0; i < fixtures.size(); ++i) {
        while (!drivatarSettings(identity).lightsEnabled || !isAudioEffect(drivatarSettings(identity).effects[0])) ++identity;
        auto& car = fixtures[i]; auto& cached = drivatarSnapshots[i];
        test::put(car.data.data(), 0, game + 0x42e0650); test::put(car.data.data(), 0x7370, 1);
        test::put(car.data.data(), 0x7378, &car.driver); test::put(car.data.data(), 0x73a0, car.presentation.data());
        test::put(car.presentation.data(), 0, game + 0x468e848);
        test::put(car.presentation.data(), 0x10, test::cars[0].scene.data());
        cached.identity = identity++; cached.driver = &car.driver;
        cached.pose.car = car.data.data(); cached.pose.presentation = car.presentation.data();
        cached.pose.manager = test::manager.data(); cached.pose.playerTick = cached.pose.matrixTick = now;
        XMStoreFloat4x4(&cached.pose.matrix, XMMatrixTranslation(float(i * 2), 0, 0));
    }
    updateRequests(now);
    auto selected = currentRequests(now);
    assert(std::count_if(selected.begin(), selected.end(), [](const Request& r) { return r.identity != 0; }) == voiceCount);
    for (size_t i = 0; i < selected.size(); ++i) {
        assert(selected[i].identity == drivatarSnapshots[i].identity && selected[i].distance == i * 2);
        assert(selected[i].gain <= playbackVolume * .1f + .00001f);
    }
    modEnabled = false; updateRequests(now); assert(!currentRequests(now)[0].identity); modEnabled = true;
    const auto config = directory / "Horizon3Extended/Horizon3Extended.ini";
    std::filesystem::create_directories(config.parent_path());
    host.mod_version = +[](const char*) -> const char* { return "1.0.0"; };
    WritePrivateProfileStringW(L"Audio", L"HorizonRadioNetwork", L"1", config.c_str());
    assert(networkEnabled(now + 1000));
    WritePrivateProfileStringW(L"Audio", L"HorizonRadioNetwork", L"0", config.c_str());
    assert(!networkEnabled(now + 2000));
    WritePrivateProfileStringW(L"Audio", L"HorizonRadioNetwork", L"1", config.c_str()); host.mod_version = nullptr;
    assert(!networkEnabled(now + 3000));
    std::cout << "PASS: nearest-car voice cap, module disable cleanup, game radio-volume mix and HRN on/off/absent routing.\n";
    std::cout << "PASS: independent native NPC stations/meters, player isolation, silence, freshness, replacement and DSP/event cleanup.\n";

    const auto media = directory / "Horizon3Extended/HorizonRadioNetwork";
    npcTest::wave(media / "Radio/Pulse/bass.wav", 80);
    npcTest::wave(media / "Radio/BassArena/treble.wav", 6000);
    assert(stationTracks(media, 1, 101).size() == 1 && stationTracks(media, 2, 202).size() == 1);
    wchar_t configuredRoot[32768]{};
    const auto rootLength = GetEnvironmentVariableW(L"UG_TEST_MPV_ROOT", configuredRoot, static_cast<DWORD>(std::size(configuredRoot)));
    const std::filesystem::path decoderRoot = rootLength && rootLength < std::size(configuredRoot) ? configuredRoot : L"C:/Mano/Azimuth/Horizon3Extended/HorizonRadioNetwork";
    if (!std::filesystem::is_regular_file(decoderRoot / L"libmpv-2.dll")) {
        std::cout << "SKIP: real HRN decoding requires UG_TEST_MPV_ROOT to contain libmpv-2.dll.\n";
        return;
    }
    assert(mpv.load(decoderRoot));
    npcTest::realOption = mpv.option; mpv.option = npcTest::silentOption;
    current[0] = {101, routeEpoch.load(), 1}; current[1] = {202, routeEpoch.load(), 2};
    for (size_t i = 0; i < 2; ++i) { current[i].network = true; current[i].gain = .05f; }
    bool heard = false;
    const auto deadline = GetTickCount64() + 10000;
    while (GetTickCount64() < deadline) {
        now = GetTickCount64(); updateNetwork(current, now);
        bass = frame(101, now); treble = frame(202, now);
        if (bass.lvl[2] > .1f && treble.lvl[0] > .1f) { heard = true; break; }
        Sleep(50);
    }
    std::cout << "MPV bands bass " << bass.lvl[0] << ',' << bass.lvl[1] << ',' << bass.lvl[2] << ',' << bass.lvl[3]
        << " treble " << treble.lvl[0] << ',' << treble.lvl[1] << ',' << treble.lvl[2] << ',' << treble.lvl[3] << '\n';
    assert(heard && bass.lvl[2] > bass.lvl[0] * 3 && treble.lvl[0] > treble.lvl[2] * 3);
    current = {}; updateNetwork(current, GetTickCount64());
    for (const auto& voice : networkVoices) assert(!voice.player && !voice.identity);
    mpv.option = npcTest::realOption;
    std::cout << "PASS: real libmpv concurrent station decoding, independent band filters and player destruction; null audio output.\n";
}
