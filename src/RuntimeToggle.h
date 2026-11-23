#pragma once

// A resident controller lets a DLL which already loaded once honor its own
// Load setting on F9 without modifying the stock platform DLL or its memory.
SRWLOCK runtimeControlLock = SRWLOCK_INIT;
HANDLE runtimeThread{}, runtimeWake{};
std::atomic<bool> runtimeStop{}, runtimeActive{true};
bool builtinRuntime{}, runtimeDirty{};
UINT runtimeKey = VK_F9;
using RuntimeKeyState = SHORT (WINAPI*)(int);
using RuntimeForeground = HWND (WINAPI*)();
using RuntimeWindowPid = DWORD (WINAPI*)(HWND, LPDWORD);
RuntimeKeyState runtimeKeyState{};
RuntimeForeground runtimeForeground{};
RuntimeWindowPid runtimeWindowPid{};

bool hostHandlesRuntimeLoads() {
    HMODULE module{};
    if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
        reinterpret_cast<LPCWSTR>(api->register_mod), &module)) return false;
    const auto* base = reinterpret_cast<const Byte*>(module);
    const auto dos = read<IMAGE_DOS_HEADER>(base);
    const auto nt = read<IMAGE_NT_HEADERS64>(base + dos.e_lfanew);
    if (dos.e_magic != IMAGE_DOS_SIGNATURE || nt.Signature != IMAGE_NT_SIGNATURE) return false;
    const auto* sections = base + dos.e_lfanew + offsetof(IMAGE_NT_HEADERS64, OptionalHeader) + nt.FileHeader.SizeOfOptionalHeader;
    constexpr char marker[] = "Underglow F9 compatibility";
    for (unsigned i = 0; i < nt.FileHeader.NumberOfSections; ++i) {
        const auto imageSection = read<IMAGE_SECTION_HEADER>(sections + i * sizeof(IMAGE_SECTION_HEADER));
        if (!(imageSection.Characteristics & IMAGE_SCN_MEM_READ) || (imageSection.Characteristics & IMAGE_SCN_MEM_DISCARDABLE) ||
            imageSection.VirtualAddress >= nt.OptionalHeader.SizeOfImage) continue;
        const auto size = std::min(imageSection.Misc.VirtualSize, nt.OptionalHeader.SizeOfImage - imageSection.VirtualAddress);
        const auto* first = base + imageSection.VirtualAddress;
        const auto* last = first + size;
        if (std::search(first, last, reinterpret_cast<const Byte*>(marker),
            reinterpret_cast<const Byte*>(marker) + sizeof(marker) - 1) != last) return true;
    }
    return false;
}

bool resumeRuntime() {
    if (setupTouched.load() && !setupComplete.load()) {
        log("F9 could not resume a partially installed hook set. A restart is required for this failed startup.");
        return false;
    }
    stopEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (!stopEvent) return false;
    stopping = false;
    modEnabled = true;
    animationStart = GetTickCount64();
    if (setupComplete.load()) {
        if (!switchOwnHooks(true)) {
            stopping = true;
            modEnabled = false;
            CloseHandle(stopEvent);
            stopEvent = nullptr;
            return false;
        }
        if (!pluginAnna::startWorker()) {
            switchOwnHooks(false); stopping = true; modEnabled = false;
            CloseHandle(stopEvent); stopEvent = nullptr; return false;
        }
        npcRadio::start();
    } else {
        setupThread = CreateThread(nullptr, 0, installHooks, nullptr, 0, nullptr);
        if (!setupThread) {
            stopping = true;
            modEnabled = false;
            CloseHandle(stopEvent);
            stopEvent = nullptr;
            return false;
        }
    }
    return true;
}

bool changeRuntime(bool wanted) {
    __try {
        if (!wanted || runtimeDirty) {
            shutdown();
            if (!switchOwnHooks(false)) return false;
        }
        return !wanted || resumeRuntime();
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        log("F9 Load change is pending: return to the game, close the Underglow editor and press F9 again.");
        return false;
    }
}

bool applyRuntimeLoad() {
    if (!builtinRuntime || runtimeStop.load()) return false;
    AcquireSRWLockExclusive(&runtimeControlLock);
    const auto epoch = loadWriteEpoch.load();
    if (epoch & 1) { ReleaseSRWLockExclusive(&runtimeControlLock); return false; }
    const bool wanted = loaderRequestedOn(api->azimuth_dir);
    if (loadWriteEpoch.load() != epoch) { ReleaseSRWLockExclusive(&runtimeControlLock); return false; }
    bool ok = true;
    if (runtimeStop.load()) ok = false;
    else if (wanted != runtimeActive.load() || runtimeDirty) {
        ok = changeRuntime(wanted);
        runtimeDirty = !ok;
        if (ok) {
            runtimeActive = wanted;
            appliedLoadNotice = wanted ? 1 : 0;
            log(wanted ? "Stock-platform F9: Load ON applied by Underglow." :
                "Stock-platform F9: Load OFF applied; lighting/audio hooks and workers stopped. Only the F9 listener remains.");
        }
    }
    ReleaseSRWLockExclusive(&runtimeControlLock);
    return ok;
}

DWORD WINAPI runtimeMonitor(void*) {
    bool down = runtimeKeyState && (runtimeKeyState(static_cast<int>(runtimeKey)) & 0x8000) != 0;
    while (WaitForSingleObject(runtimeWake, 25) == WAIT_TIMEOUT && !runtimeStop.load()) {
        DWORD pid{};
        const HWND window = runtimeForeground();
        const bool front = window && runtimeWindowPid(window, &pid) && pid == GetCurrentProcessId();
        const bool pressed = front && (runtimeKeyState(static_cast<int>(runtimeKey)) & 0x8000) != 0;
        if (pressed && !down) applyRuntimeLoad();
        down = pressed;
    }
    return 0;
}

void startRuntimeMonitor(bool active) {
    runtimeActive = active;
    const auto config = std::filesystem::path(api->azimuth_dir) / L"fh3-mods/platform.ini";
    if (!GetPrivateProfileIntW(L"dev", L"HotReload", 0, config.c_str()) || hostHandlesRuntimeLoads()) return;
    wchar_t key[16]{};
    GetPrivateProfileStringW(L"dev", L"ReloadKey", L"F9", key, static_cast<DWORD>(std::size(key)), config.c_str());
    if ((key[0] == L'F' || key[0] == L'f') && key[1]) {
        const int number = _wtoi(key + 1);
        if (number >= 1 && number <= 12) runtimeKey = VK_F1 + number - 1;
    }
    const HMODULE user32 = GetModuleHandleW(L"user32.dll");
    if (!user32) return;
    runtimeKeyState = reinterpret_cast<RuntimeKeyState>(GetProcAddress(user32, "GetAsyncKeyState"));
    runtimeForeground = reinterpret_cast<RuntimeForeground>(GetProcAddress(user32, "GetForegroundWindow"));
    runtimeWindowPid = reinterpret_cast<RuntimeWindowPid>(GetProcAddress(user32, "GetWindowThreadProcessId"));
    if (!runtimeKeyState || !runtimeForeground || !runtimeWindowPid) return;
    runtimeWake = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (!runtimeWake) return;
    builtinRuntime = true;
    appliedLoadNotice = active ? 1 : 0;
    runtimeThread = CreateThread(nullptr, 0, runtimeMonitor, nullptr, 0, nullptr);
    if (!runtimeThread) {
        builtinRuntime = false;
        CloseHandle(runtimeWake);
        runtimeWake = nullptr;
        return;
    }
    log("Built-in F9 Load controller ready; the installed Mod Platform DLL is unchanged.");
}

void stopRuntimeMonitor() {
    runtimeStop = true;
    if (runtimeWake) SetEvent(runtimeWake);
    if (runtimeThread) {
        WaitForSingleObject(runtimeThread, INFINITE);
        CloseHandle(runtimeThread);
        runtimeThread = nullptr;
    }
    if (runtimeWake) { CloseHandle(runtimeWake); runtimeWake = nullptr; }
}
