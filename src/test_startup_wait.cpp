#include "Underglow.cpp"
#include <cstdio>

std::atomic<unsigned> startupWaitCalls{};
void* rejectUntilCancelled(uint32_t) {
    if (++startupWaitCalls == 4) stopping = true;
    return nullptr;
}
void* rejectForever(uint32_t) { ++startupWaitCalls; return nullptr; }
void* readyImmediately(uint32_t) {
    ++startupWaitCalls;
    return reinterpret_cast<void*>(1); // supported() safely rejects this test image
}
static void prepare(FH3ModApi& host) {
    host = {};
    host.log = +[](FH3Mod*, const char*) {};
    api = &host;
    stopping = false;
    game = nullptr;
    startupWaitCalls = 0;
    stopEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
}
static void cleanup() { CloseHandle(stopEvent); stopEvent = nullptr; }
int main() {
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
    FH3ModApi host{};
    prepare(host);
    if (!stopEvent) return 2;
    host.wait_for_code = rejectUntilCancelled;
    auto began = GetTickCount64();
    installHooks(nullptr);
    auto elapsed = GetTickCount64() - began;
    cleanup();
    if (startupWaitCalls != 4 || elapsed < 100 || elapsed > 1500) {
        std::printf("FAIL: rejected-title polling made %u calls in %llu ms; expected throttled retries.\n",
                    startupWaitCalls.load(), elapsed);
        return 1;
    }
    std::printf("PASS: immediate NULL responses are throttled (%llu ms for four calls).\n", elapsed);

    prepare(host);
    host.wait_for_code = rejectForever;
    HANDLE worker = CreateThread(nullptr, 0, installHooks, nullptr, 0, nullptr);
    if (!worker) return 2;
    auto deadline = GetTickCount64() + 500;
    while (!startupWaitCalls && GetTickCount64() < deadline) Sleep(1);
    began = GetTickCount64();
    stopping = true;
    SetEvent(stopEvent);
    DWORD result = WaitForSingleObject(worker, 500);
    elapsed = GetTickCount64() - began;
    CloseHandle(worker);
    cleanup();
    if (result != WAIT_OBJECT_0 || elapsed > 250) {
        std::printf("FAIL: cancellation did not wake the polling worker promptly.\n"); return 1;
    }
    std::printf("PASS: cancellation wakes the worker in %llu ms.\n", elapsed);

    prepare(host);
    host.wait_for_code = readyImmediately;
    began = GetTickCount64();
    installHooks(nullptr);
    elapsed = GetTickCount64() - began;
    cleanup();
    if (startupWaitCalls != 1 || game != reinterpret_cast<Byte*>(1) || elapsed > 100) {
        std::printf("FAIL: an already-ready game was delayed or retried.\n"); return 1;
    }
    std::printf("PASS: an already-ready game is forwarded without a polling delay.\n");
    return 0;
}
