#define main meetRegressionTests
#include "test_meet_lights.cpp"
#undef main
#include "test_npc_radio.h"

std::atomic<int> destroyedPlayers{};
int silentPlayer(void*, uint64_t, const char*, int, void*) { return 0; }
void destroyPlayer(void*) { ++destroyedPlayers; }
DWORD WINAPI shutdownTestThread(void*) { fh3mod_shutdown(); return 0; }

#ifndef UG_SHUTDOWN_TEST_ENTRY
#define UG_SHUTDOWN_TEST_ENTRY main
#endif
int UG_SHUTDOWN_TEST_ENTRY() {
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
    meetRegressionTests();
    static FH3ModApi host{};
    host.log = +[](FH3Mod*, const char*) {};
    host.azimuth_dir = L".";
    api = &host;
    originalSceneLights = test::noop;
    radio::originalTick = test::radioTick;
    npcTest::stubs();
    static const FH3CarMeetsV1 provider{sizeof(provider), test::lookup};
    meetProvider = &provider;
    static std::array<Byte, 0xaa000> engine{};
    test::put(game, radio::engineGlobal, engine.data());
    test::put(engine.data(), 0xa9fe8, engine.data());
    radio::tap = {};
    for (unsigned cycle = 0; cycle < 3; ++cycle) {
        stopping = false;
        modEnabled = true;
        sceneCleanupDone = true;
        audioCleanupDone = true;
        stopEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
        test::fill();
        assert(updateMeetLights(test::manager.data()));
        assert(test::count() > 1);
        npcRadio::nativeStudio = engine.data();
        npcRadio::description = &npcTest::events;
        std::array<npcRadio::Request, npcRadio::voiceCount> requests{};
        requests[0].identity = 1;
        requests[0].station = 1;
        requests[0].mix = 1;
        npcRadio::updateNative(requests, GetTickCount64());
        assert(npcTest::events.size() == 1 && npcTest::meters.size() == 1);
        npcRadio::mpv.set = silentPlayer;
        npcRadio::mpv.destroy = destroyPlayer;
        npcRadio::networkVoices[0].player = &engine;
        npcRadio::networkThread = CreateThread(nullptr, 0, npcRadio::networkWorker, nullptr, 0, nullptr);
        assert(npcRadio::networkThread);
        HANDLE thread = CreateThread(nullptr, 0, shutdownTestThread, nullptr, 0, nullptr);
        assert(thread);
        const auto until = GetTickCount64() + 3000;
        while (WaitForSingleObject(thread, 0) != WAIT_OBJECT_0 && GetTickCount64() < until) {
            if (!stopping.load()) { Sleep(1); continue; }
            sceneLights(test::manager.data());
            radio::tick(nullptr, .016f);
            Sleep(1);
        }
        assert(WaitForSingleObject(thread, 0) == WAIT_OBJECT_0);
        CloseHandle(thread);
        assert(test::count() == 1); // the foreign lamp is preserved
        assert(test::namedStyles.empty());
        assert(npcTest::events.empty() && npcTest::meters.empty());
        assert(!setupThread && !npcRadio::networkThread && !stopEvent);
        assert(sceneCleanupDone && audioCleanupDone);
        assert(destroyedPlayers == static_cast<int>(cycle + 1));
        fh3mod_shutdown(); // repeated shutdown is safe
    }
    garage::runtimePageOpen = true;
    assert(fh3mod_can_shutdown() == 0);
    garage::runtimePageOpen = false;
    assert(fh3mod_can_shutdown() == 1);
    std::cout << "PASS: shutdown retires owned lights on render callbacks, native DSP/events on audio callbacks, "
        "joins the network worker, destroys its players, preserves foreign resources and is repeatable.\n";
    return 0;
}
