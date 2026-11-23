#define UG_SHUTDOWN_TEST_ENTRY shutdownRegressionTests
#include "test_shutdown.cpp"
#undef UG_SHUTDOWN_TEST_ENTRY

std::atomic<bool> simulatedKey{}, simulatedForeground{true};
std::set<FH3Hook*> enabledTestHooks;
SHORT WINAPI keyState(int key) { return key == VK_F9 && simulatedKey.load() ? static_cast<SHORT>(0x8000) : 0; }
HWND WINAPI foreground() { return reinterpret_cast<HWND>(1); }
DWORD WINAPI windowPid(HWND, LPDWORD pid) { *pid = simulatedForeground ? GetCurrentProcessId() : GetCurrentProcessId() + 1; return 1; }
int enableTestHook(FH3Mod*, FH3Hook* hook) { assert(hook); enabledTestHooks.insert(hook); return 1; }
int disableTestHook(FH3Mod*, FH3Hook* hook) { assert(hook); enabledTestHooks.erase(hook); return 1; }

int main() {
    shutdownRegressionTests();
    static FH3ModApi host{};
    static const auto directory = std::filesystem::current_path() / L"runtime-toggle-fixture";
    static const auto directoryName = directory.wstring();
    std::filesystem::create_directories(directory / L"fh3-mods");
    host.azimuth_dir = directoryName.c_str();
    host.log = +[](FH3Mod*, const char*) {};
    host.enable_hooks = enableTestHook;
    host.disable_hooks = disableTestHook;
    api = &host;
    auto* foreign = reinterpret_cast<FH3Hook*>(100);
    enabledTestHooks.insert(foreign);
    ownHookTargets = {reinterpret_cast<FH3Hook*>(1), reinterpret_cast<FH3Hook*>(2)};
    for (auto* hook : ownHookTargets) enabledTestHooks.insert(hook);
    builtinRuntime = true;
    runtimeStop = false;
    runtimeActive = true;
    runtimeDirty = false;
    setupComplete = setupTouched = true;
    stopping = false;
    modEnabled = true;
    sceneCleanupDone = audioCleanupDone = true;
    stopEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    runtimeKeyState = keyState;
    runtimeForeground = foreground;
    runtimeWindowPid = windowPid;
    runtimeKey = VK_F9;
    runtimeWake = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    runtimeThread = CreateThread(nullptr, 0, runtimeMonitor, nullptr, 0, nullptr);
    assert(runtimeThread);
    Sleep(60);
    const auto saveLoad = [](bool enabled) {
        std::ofstream file(directory / L"fh3-mods/platform-disabled.txt");
        if (!enabled) file << "underglow.dll\n";
    };
    const auto pump = [](bool expected) {
        const auto until = GetTickCount64() + 2500;
        do {
            if (stopping.load()) { sceneLights(test::manager.data()); radio::tick(nullptr, .016f); }
            if (runtimeActive.load() == expected) break;
            Sleep(5);
        } while (GetTickCount64() < until);
        assert(runtimeActive.load() == expected);
    };
    saveLoad(false);
    loadWriteEpoch = 1;
    assert(!applyRuntimeLoad() && runtimeActive.load());
    loadWriteEpoch = 2; // A concurrent acceptance must complete before F9 can read it.
    saveLoad(true);
    for (unsigned cycle = 0; cycle < 3; ++cycle) {
        saveLoad(false);
        simulatedForeground = false;
        simulatedKey = true;
        Sleep(100);
        assert(runtimeActive && enabledTestHooks.size() == 3);
        simulatedKey = false; Sleep(60);
        simulatedForeground = true;
        simulatedKey = true;
        pump(false);
        assert(enabledTestHooks.size() == 1 && enabledTestHooks.contains(foreign));
        simulatedKey = false; Sleep(60);
        saveLoad(true);
        simulatedKey = true;
        pump(true);
        assert(enabledTestHooks.size() == 3 && enabledTestHooks.contains(foreign));
        simulatedKey = false; Sleep(60);
    }
    stopRuntimeMonitor();
    assert(!runtimeThread && !runtimeWake);
    assert(fh3mod_apply_runtime_load() == 0);
    fh3mod_shutdown();
    std::cout << "PASS: real F9-listener loop consumes rising edges only in its foreground process, "
        "toggles repeatedly, preserves foreign hooks and stops cleanly for a DLL reload.\n";
}
