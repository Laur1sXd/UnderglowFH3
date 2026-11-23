// A save-local identity in FH3's existing ContentSeen table. Using the native
// profile database makes the identity travel with FH3MP cloud saves, unlike a
// directory timestamp, XUID or the garage's repeatedly reused numeric car IDs.
// No game content uses this reserved range. The marker equips nothing itself.
namespace saveScope {
constexpr uint64_t markerFirst = 0x5547000000000000ull;
constexpr uint64_t markerLast = 0x5547ffffffffffffull;
using Row = int (*)(void*, int, char**, char**);
using Exec = int (*)(void*, const char*, Row, void*, char**);
using Prepare = int (*)(void*, const char*, int, unsigned, void**, const char**);
using Random = BOOLEAN (WINAPI*)(PVOID, ULONG);
Exec exec;
Prepare originalPrepare;
std::array<Prepare, 4> originalCalls{};
Random randomBytes;
std::atomic<bool> trackingReady{};
SRWLOCK lock = SRWLOCK_INIT;
std::wstring selected;
struct Exclusive {
    Exclusive() { AcquireSRWLockExclusive(&lock); }
    ~Exclusive() { ReleaseSRWLockExclusive(&lock); }
};
struct Snapshot { std::wstring key; };
Snapshot current() {
    AcquireSRWLockShared(&lock);
    Snapshot result{selected};
    ReleaseSRWLockShared(&lock);
    return result;
}

struct Markers { uint64_t value{}; unsigned count{}; std::vector<uint64_t> values; };
int markerRow(void* context, int count, char** values, char**) {
    auto& result = *static_cast<Markers*>(context);
    if (count != 1 || !values || !values[0]) return 1;
    char* end{};
    const auto value = std::strtoull(values[0], &end, 10);
    if (!end || *end || value < markerFirst || value > markerLast) return 1;
    if (!result.count) result.value = value;
    result.values.push_back(value); ++result.count;
    return 0;
}

thread_local bool querying{};
struct Querying {
    Querying() { querying = true; }
    ~Querying() { querying = false; }
};

// Called only inside the game's prepare callback, with its live connection.
// Never retain that connection or call it from a worker after the profile closes.
void observe(void* db) {
    Exclusive guard;
    Querying ownQuery;
    const std::string find = "SELECT ContentType_ContentId FROM Profile0_ContentSeen WHERE ContentType_ContentId BETWEEN " +
        std::to_string(markerFirst) + " AND " + std::to_string(markerLast) + " ORDER BY ContentType_ContentId ASC LIMIT 64";
    Markers markers;
    if (!exec || exec(db, find.c_str(), markerRow, &markers, nullptr) != 0 || markers.count >= 64) {
        selected.clear(); return;
    }
    if (!markers.count) {
        uint64_t token{};
        if (!randomBytes || !randomBytes(&token, sizeof(token))) { selected.clear(); return; }
        token = markerFirst | (token & 0x0000ffffffffffffull);
        // A marker identifies a career; it grants no colors, effects or ownership.
        const auto insert = "INSERT OR IGNORE INTO Profile0_ContentSeen (ContentType_ContentId) VALUES (" +
            std::to_string(token) + ")";
        if (exec(db, insert.c_str(), nullptr, nullptr, nullptr) != 0 ||
            exec(db, find.c_str(), markerRow, &markers, nullptr) != 0 || markers.count != 1) {
            selected.clear(); return;
        }
    }
    // A valid career can contain more than one earlier UG marker. Keep its
    // already-selected identity, or an existing saved setup, then fall back to
    // the smallest stored token. Never delete native content/marker rows.
    if(markers.count>1) {
        bool preferred=false;
        for(const auto value:markers.values)if(std::to_wstring(value)==selected) {markers.value=value;preferred=true;break;}
        if(!preferred)for(const auto value:markers.values) {
            const auto setupSection=L"Garage."+std::to_wstring(value);wchar_t probe[4]{};
            if(GetPrivateProfileSectionW(setupSection.c_str(),probe,4,ini)>0) {markers.value=value;break;}
        }
    }
    const auto next=std::to_wstring(markers.value);
    if(api && api->log && next!=selected)log(markers.count>1?"Save-local Garage identity selected from existing multiple markers.":"Save-local Garage identity selected.");
    selected=next;
}

int prepareThrough(Prepare original, void* db, const char* sql, int length, unsigned flags,
                   void** statement, const char** tail) {
    const int result = original(db, sql, length, flags, statement, tail);
    if (!trackingReady.load() || querying || !db || !sql) return result;
    const std::string_view query(sql, length < 0 ? std::strlen(sql) : static_cast<size_t>(length));
    if (query.find("Profile0_") == query.npos) return result;
    // Profile replacement must invalidate the cached glow before any new car
    // can reuse the old numeric garage ID, including on a recycled connection.
    if (query.starts_with("ATTACH") || query.starts_with("DETACH") || query.starts_with("DROP")) {
        Exclusive guard; selected.clear();
    } else if (result == 0) {
        try { observe(db); }
        catch (...) { Exclusive guard; selected.clear(); }
    }
    return result;
}
int prepare(void* db, const char* sql, int length, unsigned flags,
            void** statement, const char** tail) {
    return prepareThrough(originalPrepare, db, sql, length, flags, statement, tail);
}
template<size_t Index>
int prepareCall(void* db, const char* sql, int length, unsigned flags,
                void** statement, const char** tail) {
    return prepareThrough(originalCalls[Index], db, sql, length, flags, statement, tail);
}

bool install() {
    trackingReady = false;
    { Exclusive guard; selected.clear(); }
    // Mod Platform 1.4.2 installs its upgrade bridge directly on 0x440c0.
    // Do not reserve that function with MinHook: an earlier mod hook makes the
    // platform's own MH_CreateHook fail with MH_ERROR_ALREADY_CREATED.
    // Observe its four native CALL sites instead. Forward to each call's own
    // chain so the platform's root detour remains in the path in either order.
    exec = native<Exec>(0x421d0);
    static const auto library = LoadLibraryW(L"advapi32.dll");
    if (library) randomBytes = reinterpret_cast<Random>(GetProcAddress(library, "SystemFunction036"));
    if (!randomBytes || !api->hook_call || !api->enable_hooks || !api->disable_hooks) return false;
    constexpr size_t calls[]{0x43e8d, 0x44afe, 0x44b61, 0x44c7d};
    constexpr Prepare detours[]{prepareCall<0>, prepareCall<1>, prepareCall<2>, prepareCall<3>};
    for (const auto rva : calls) {
        std::array<Byte, 5> bytes{};
        if (!read(game + rva, bytes) || bytes[0] != 0xe8) return false;
        int32_t relative{}; std::memcpy(&relative, bytes.data() + 1, sizeof(relative));
        if (game + rva + 5 + relative != game + 0x440c0) return false;
    }
    std::array<FH3Hook*, std::size(calls)> hooks{};
    for (size_t i = 0; i < std::size(calls); ++i) {
        hooks[i] = api->hook_call(mod, "Underglow save-local profile query", game + calls[i],
            reinterpret_cast<void*>(detours[i]), reinterpret_cast<void**>(&originalCalls[i]));
        if (!hooks[i]) return false; // all remain disabled until every hook exists
    }
    // Identity tracking remains resident across internal F9 Off/On. The host
    // still owns/removes this hook on a full shutdown or module unload.
    for (size_t i = 0; i < hooks.size(); ++i) {
        if (api->enable_hooks(mod, hooks[i])) continue;
        for (size_t j = 0; j <= i; ++j) api->disable_hooks(mod, hooks[j]);
        return false;
    }
    trackingReady = true;
    return true;
}
} // namespace saveScope
