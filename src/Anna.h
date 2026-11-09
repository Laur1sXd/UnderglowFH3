// FH3's native ANNA menu adapter. All state below lives on the HUD thread.
namespace anna {
struct GameString {
    union { char text[16]; const char* heap; } data{};
    size_t size{};
    size_t capacity{15};
};
static_assert(sizeof(GameString) == 32);

enum Page { Native, Car, Underglow, Targets, Patterns, Lights };
enum Action { Open, PickTarget, PickEffect, TurnPage, ToggleGlow };
enum class HostPage { none, car, lights };
constexpr size_t menuOffset = 0xf8;
constexpr size_t pageSize = 7;
constexpr int underglowCarRow = supportExtraOptions ? 1 : 0;

constexpr int targetOrder[] = {0, fx::kZoneFront + 1, fx::kZoneRear + 1, fx::kZoneLeft + 1, fx::kZoneRight + 1};
constexpr int targetCount = static_cast<int>(std::size(targetOrder));
using Add = void (*)(void*, const GameString*);
using Clear = void (*)(void*);
using List = void (*)(void*, const GameString*);
using Refresh = void (*)(void*, unsigned char);
using Selection = GameString* (*)(void*, GameString*);
using Label = void** (*)(void*, void**, const GameString*);
using Input = unsigned char (*)(void*, unsigned, unsigned, unsigned char);
using Move = unsigned char (*)(void*);
using Dispatch = void (*)(void*);
Add addOriginal;
Clear clearOriginal;
List listOriginal;
Refresh refreshOriginal;
Selection selectionOriginal;
Label labelOriginal;
Input inputOriginal;
Move downOriginal, upOriginal;
Dispatch dispatchOriginal;

struct Menu {
    void* component{};
    Byte* car{};
    Page page{Native};
    int target{}, offset{}, targetRow{};
    bool carrier{}, hostCar{}, inserted{}, standalone{}, rootBuild{};
    bool reportedRoot{};
    HostPage hostPage{};
    void* rootList{};
    size_t revision{};
    void* rebuildingMenu{};
    std::array<GameString, 16> hostNames{}, returnNames{};
    size_t hostCount{}, returnCount{};
} menu;

Byte* playerCar() {
    AcquireSRWLockShared(&snapshotLock);
    auto* car = snapshot.car;
    const bool current = snapshot.matrixTick && GetTickCount64() - snapshot.playerTick < 1000;
    ReleaseSRWLockShared(&snapshotLock);
    return current && isLocalDriver(car) ? car : nullptr;
}

bool named(const GameString* name, const char* value) {
    const auto length = std::strlen(value);
    return name && name->capacity == 15 && name->size == length &&
        std::memcmp(name->data.text, value, length) == 0;
}

bool hostPrefix(const GameString* name) {
    return name && name->capacity == 15 && name->size >= 3 && name->size < sizeof(name->data.text) &&
        ((supportExtraOptions && std::memcmp(name->data.text, "EO_", 3) == 0) ||
         (supportHorizon3Extended && std::memcmp(name->data.text, "HX_", 3) == 0));
}

bool hostEntry(const GameString* name, const char* label) {
    const auto length = std::strlen(label);
    if (!hostPrefix(name) || name->size < length + 3) return false;
    return !std::memcmp(name->data.text + 3, label, length) &&
        (name->size == length + 3 || (name->size == length + 4 && name->data.text[length + 3] == '+'));
}

bool decode(const GameString* name, int& action, int& value, int& state) {
    if (!name || name->capacity != 15 || name->size < 6 || name->size >= sizeof(name->data.text) ||
        std::memcmp(name->data.text, "UG_", 3) || name->data.text[name->size] != '\0') return false;
    return sscanf_s(name->data.text + 3, "%d_%d_%d", &action, &value, &state) == 3;
}

bool disabledCommand(const GameString& name) {
    int action, value, state;
    return decode(&name, action, value, state) && !state &&
        ((action == Open && (value == Underglow || value == Lights)) || action == ToggleGlow);
}

bool effectSelected(int value) {
    const auto record = garage::current();
    if (menu.target) return garage::zoneEffect(record.audio, menu.target - 1) == value;
    for (size_t zone = 0; zone < zoneCount; ++zone)
        if (garage::zoneEffect(record.audio, zone) != value) return false;
    return true;
}

bool underglowAvailable() {
    const auto record = garage::current();
    return record.state == garage::State::installed && !record.lightsOff;
}

bool underglowInstalled() {
    return garage::current().state == garage::State::installed;
}

bool targetAvailable(int value) {
    return value >= 0 && value <= static_cast<int>(zoneCount) &&
        garage::audioTargetAvailable(garage::current(), value - 1);
}

int nextTargetRow(const garage::CarSetup& record, int row, int direction) {
    for (int next = row + direction; next >= 0 && next < targetCount; next += direction)
        if (garage::audioTargetAvailable(record, targetOrder[next] - 1)) return next;
    return -1;
}

void emit(void* list, Action action, int value) {
    GameString key;
    int state = 0;
    if (action == Open && value == Underglow) state = underglowAvailable();
    if (action == Open && value == Lights) state = underglowInstalled();
    if (action == ToggleGlow) {
        const auto record = garage::current();
        if (record.state == garage::State::installed) state = record.lightsOff ? 2 : 1;
    }
    if (action == PickTarget) state = targetAvailable(value);
    if (action == PickEffect) state = effectSelected(value);
    key.size = static_cast<size_t>(sprintf_s(key.data.text, "UG_%d_%d_%d", action, value, state));
    addOriginal(list, &key);
}

void* listOf(void* component) {
    return component ? read<void*>(static_cast<Byte*>(component) + menuOffset) : nullptr;
}

struct Rows {
    Byte** items{};
    int count{};

    Byte* at(int index) const { return read<Byte*>(items + index); }
};

Rows rowsOf(void* list) {
    if (!list) return {};
    auto* collection = read<Byte*>(static_cast<Byte*>(list) + 0x210);
    const int count = collection ? garage::collectionCount(collection) : 0;
    auto** items = count > 0 && count <= 64 ? garage::collectionEntries(collection) : nullptr;
    return items ? Rows{items, count} : Rows{};
}

int highlightedRow(const Rows& rows) {
    for (int row = 0; row < rows.count; ++row) {
        const auto* item = rows.at(row);
        if (item && read<Byte>(item + 0x40)) return row;
    }
    return -1;
}

bool disabledRow(const Rows& rows, int index) {
    const auto* item = rows.at(index);
    return item && disabledCommand(read<GameString>(item + 0x48));
}

bool showsUnderglowRow(void* list) {
    if (!list) return false;
    if (menu.page == Car || menu.page == Lights) return list == listOf(menu.component);
    return menu.page == Native && menu.inserted && menu.hostPage != HostPage::none &&
        list == menu.rebuildingMenu;
}

unsigned char moveHighlight(void* list, int direction, Move move) {
    if (!showsUnderglowRow(list)) return move(list);
    if (menu.page == Car && !underglowInstalled()) return 0;
    const auto rows = rowsOf(list);
    const int current = highlightedRow(rows);
    if (current < 0) return move(list);
    int next = current + direction;
    if (next < 0 || next >= rows.count || !disabledRow(rows, next)) return move(list);
    while (next >= 0 && next < rows.count && disabledRow(rows, next)) next += direction;
    if (next < 0 || next >= rows.count) return 0;
    unsigned char moved = 0;
    for (int row = current; row != next && move(list); row += direction) moved = 1;
    return moved;
}

unsigned char moveDown(void* list) { return moveHighlight(list, 1, downOriginal); }
unsigned char moveUp(void* list) { return moveHighlight(list, -1, upOriginal); }

bool paint(int first = 0, bool hostLights = false) {
    auto* list = static_cast<Byte*>(listOf(menu.component));
    if (!list || !menu.car || playerCar() != menu.car) return false;
    const bool installed = underglowInstalled();
    if (menu.page == Targets) {
        const auto record = garage::current();
        if (first < 0 || first >= targetCount || !garage::audioTargetAvailable(record, targetOrder[first] - 1))
            first = nextTargetRow(record, -1, 1);
        menu.targetRow = first;
        // An index past the end suppresses the native first-row highlight when no target is available.
        if (first < 0) first = targetCount;
    } else if (menu.page == Car && !installed) {
        first = underglowCarRow + 1;
    } else if (menu.page == Lights && !installed) {
        first = 1;
    }
    clearOriginal(list);
    const Byte restore = 1;
    std::memcpy(list + 0x21c, &first, sizeof(first));
    std::memcpy(list + 0x21a, &restore, sizeof(restore));
    if (menu.page == Native) {
        const auto& names = hostLights ? menu.hostNames : menu.returnNames;
        const size_t count = hostLights ? menu.hostCount : menu.returnCount;
        for (size_t i = 0; i < count; ++i) addOriginal(list, &names[i]);
        if (hostLights) emit(list, ToggleGlow, 0);
        else emit(list, Open, Underglow);
    } else if (menu.page == Car) {
        if (supportExtraOptions) emit(list, Open, Lights);
        emit(list, Open, Underglow);
    } else if (menu.page == Lights) {
        emit(list, ToggleGlow, 0);
    } else if (menu.page == Underglow) {
        emit(list, Open, Targets);
    } else if (menu.page == Targets) {
        for (int target : targetOrder) emit(list, PickTarget, target);
    } else if (menu.page == Patterns) {
        const size_t count = effectChoiceCounts[fx::kGroupAudio] + 1;
        menu.offset = std::clamp(menu.offset, 0, static_cast<int>((count - 1) / pageSize));
        const auto start = static_cast<size_t>(menu.offset) * pageSize;
        const auto end = std::min(start + pageSize, count);
        if (start) emit(list, TurnPage, 0);
        for (size_t i = start; i < end; ++i)
            emit(list, PickEffect, i ? effectIds[fx::kGroupAudio][i - 1] : -1);
        if (end < count) emit(list, TurnPage, 1);
    }
    return true;
}

void setPage(Page page, int first = 0) {
    menu.page = page;
    menu.offset = 0;
    paint(first);
}

void onClear(void* list) {
    clearOriginal(list);
    menu.rebuildingMenu = list;
    menu.hostCount = 0;
    menu.hostPage = HostPage::none;
    menu.hostCar = menu.inserted = false;
    ++menu.revision;
}

void onAdd(void* list, const GameString* name) {
    if (menu.rebuildingMenu == list && menu.rootBuild && menu.rootList == list) {
        if (hostEntry(name, "Car")) menu.hostCar = true;
        else if (!menu.hostCar && !menu.inserted && playerCar()) {
            menu.inserted = true;
            emit(list, Open, Car);
            if (!menu.reportedRoot) log("ANNA Car entry added to the native menu.");
            menu.reportedRoot = true;
        }
    }
    addOriginal(list, name);
    if (menu.rebuildingMenu != list) return;
    if (!hostPrefix(name) || hostEntry(name, "Car")) return;
    if (menu.hostCount < menu.hostNames.size()) menu.hostNames[menu.hostCount++] = *name;
    if (hostEntry(name, "Autodrive") || hostEntry(name, "Lights") || hostEntry(name, "Doors"))
        menu.hostPage = HostPage::car;
    else if (hostEntry(name, "Headlights") || hostEntry(name, "Hazards") ||
        hostEntry(name, "SignalLeft") || hostEntry(name, "SignalRight"))
        menu.hostPage = HostPage::lights;
}

void finishHostMenu(void* component) {
    auto* list = listOf(component);
    if (menu.page != Native || !list || list != menu.rebuildingMenu ||
        menu.hostPage == HostPage::none || menu.inserted || !playerCar()) return;
    if (!supportExtraOptions && menu.hostPage == HostPage::lights) return;
    menu.inserted = true;
    if (menu.hostPage == HostPage::lights) emit(list, ToggleGlow, 0);
    else emit(list, Open, Underglow);
}

bool topLevel(void* component) {
    auto* bytes = static_cast<Byte*>(component);
    const auto begin = read<uintptr_t>(bytes + 0xb8);
    const auto end = read<uintptr_t>(bytes + 0xc0);
    if (!begin || end < begin || (end - begin) % sizeof(GameString) || end - begin > 64 * sizeof(GameString)) return false;
    for (auto entry = begin; entry < end; entry += sizeof(GameString)) {
        const auto name = read<GameString>(reinterpret_cast<void*>(entry));
        if (named(&name, "WhatShouldIDo") || named(&name, "RouteToNearest")) return true;
    }
    return false;
}

void afterRebuild(void* component, size_t previous, bool root) {
    if (previous == menu.revision) return;
    auto* car = playerCar();
    if (!car) { menu.page = Native; menu.carrier = false; return; }
    if (root) {
        menu.page = Native;
        menu.carrier = false;
    } else if (menu.carrier && menu.component == component && menu.car == car) {
        menu.carrier = false;
        setPage(Car);
    } else if (menu.page != Native && menu.component == component && menu.car == car) {
        paint(menu.page == Targets ? menu.targetRow : 0);
    } else finishHostMenu(component);
}

void onList(void* component, const GameString* title) {
    const auto previous = menu.revision;
    const bool root = topLevel(component);
    menu.rootBuild = root;
    menu.rootList = listOf(component);
    listOriginal(component, title);
    menu.rootBuild = false;
    afterRebuild(component, previous, root);
}

void onRefresh(void* component, unsigned char flag) {
    const auto previous = menu.revision;
    const bool root = topLevel(component);
    menu.rootBuild = root;
    menu.rootList = listOf(component);
    refreshOriginal(component, flag);
    menu.rootBuild = false;
    afterRebuild(component, previous, root);
}

void onDispatch(void* hud) {
    const auto previous = menu.revision;
    dispatchOriginal(hud);
    // The host can rebuild Lights or Car directly during input dispatch.
    if (previous != menu.revision) finishHostMenu(read<void*>(static_cast<Byte*>(hud) + 0x118));
}

GameString* onSelection(void* component, GameString* out) {
    selectionOriginal(component, out);
    int action, value, state;
    auto* car = playerCar();
    if (!car || !decode(out, action, value, state)) return out;
    menu.component = component;
    menu.car = car;
    if (action == Open && value == Car) {
        constexpr char followup[] = "StartMatchmakingFor";
        GameString command;
        command.data.heap = followup;
        command.size = command.capacity = sizeof(followup) - 1;
        native<void (*)(GameString*, const GameString*, size_t, size_t)>(0x245070)(out, &command, 0, SIZE_MAX);
        menu.carrier = menu.standalone = true;
        menu.returnCount = 0;
        log("ANNA Car opened from the native list.");
        return out;
    }

    if (action == Open && value == Lights) {
        if (supportExtraOptions && state && underglowInstalled()) setPage(Lights);
    } else if (action == ToggleGlow) {
        if (supportExtraOptions && state && garage::toggleLights()) {
            if (menu.page == Lights) paint();
            else if (menu.page == Native && menu.hostPage == HostPage::lights)
                paint(static_cast<int>(menu.hostCount), true);
        }
    } else if (disabledCommand(*out) || !underglowAvailable()) {
        out->data.text[0] = '\0';
        out->size = 0;
        return out;
    } else if (action == Open && (value == Underglow || value == Targets)) {
        if (value == Underglow && menu.page == Native) {
            menu.standalone = false;
            menu.returnNames = menu.hostNames;
            menu.returnCount = menu.hostCount;
        }
        setPage(static_cast<Page>(value));
    } else if (action == PickTarget && targetAvailable(value)) {
        menu.target = value;
        setPage(Patterns);
    } else if (action == TurnPage) {
        menu.offset += value ? 1 : -1;
        paint();
    } else if (action == PickEffect && (value == -1 || isAudioEffect(value)) && targetAvailable(menu.target)) {
        if (garage::setAudio(value, menu.target - 1)) {
            const auto& ids = effectIds[fx::kGroupAudio];
            const auto end = ids.begin() + effectChoiceCounts[fx::kGroupAudio];
            const int index = value == -1 ? 0 : static_cast<int>(std::find(ids.begin(), end, value) - ids.begin()) + 1;
            const int start = menu.offset * static_cast<int>(pageSize);
            paint(std::max(0, index - start + (start > 0)));
            log("ANNA Underglow audio effect applied.");
        } else log("Could not save the ANNA Underglow audio effect.");
    }
    out->data.text[0] = '\0';
    out->size = 0;
    return out;
}

void** onLabel(void* self, void** out, const GameString* name) {
    int action, value, state;
    if (!decode(name, action, value, state)) return labelOriginal(self, out, name);
    std::wstring label = L"Underglow";
    if (action == Open && value == Car) label = L"Car";
    if (supportExtraOptions && action == Open && value == Lights) label = state ? L"Lights" : L"Lights (Off)";
    if (action == Open && value == Underglow && !state) label = L"Underglow (Off)";
    if (action == ToggleGlow) label = state == 1 ? L"Underglow Off" :
        state == 2 ? L"Underglow On" : L"Underglow (Off)";
    if (action == Open && value == Targets) label = L"Audio Visualizer";
    if (action == PickTarget && value >= 0 && value <= static_cast<int>(zoneCount)) {
        label = garage::wideName(effectTargets[value]);
        if (!state) label += L" (Off)";
    }
    if (action == PickEffect && (value == -1 || isAudioEffect(value))) {
        label = value == -1 ? L"Off" : garage::wideName(fx::kEffects[value]);
        if (state) label.insert(0, L"\u00bb ");
    }
    if (action == TurnPage) label = value ? L"Next" : L"Previous";
    native<void** (*)(void**)>(0x1214480)(out);
    if (auto* result = static_cast<Byte*>(*out)) {
        const auto length = static_cast<unsigned>(label.size());
        const size_t bytes = (length + 1) * sizeof(wchar_t);
        if (auto* copy = native<void* (*)(size_t)>(0x1edbd00)(bytes)) {
            std::memcpy(copy, label.c_str(), bytes);
            std::memcpy(result + 0x28, &copy, sizeof(copy));
            std::memcpy(result + 0x30, &length, sizeof(length));
        }
    }
    return out;
}

bool patchMarkup(std::string_view source, std::string& output) {
    if (source.find("x:Class=\"ForzaUI.HUDAnnaControl\"") == source.npos) return false;
    constexpr std::string_view templateStart = "<DataTemplate x:Key=\"A_DataTemplate_SpeechItem\">";
    constexpr std::string_view label = "<local:FZTextBlock Text=\"{Binding Command}\"";
    const auto begin = source.find(templateStart);
    if (begin == source.npos || source.find("UG_AnnaLabel") != source.npos) return false;
    const auto end = source.find("</DataTemplate>", begin);
    const auto triggers = source.find("</DataTemplate.Triggers>", begin);
    const auto text = source.find(label, begin);
    if (end == source.npos || triggers >= end || text >= triggers) return false;

    std::string disabled;
    const auto addDisabledTrigger = [&](std::string_view name) {
        disabled.append(R"XAML(
        <DataTrigger Binding="{Binding Command}" Value=")XAML").append(name);
        disabled.append(R"XAML( (Off)">
            <Setter TargetName="UG_AnnaLabel" Property="Text" Value=")XAML").append(name);
        disabled.append(R"XAML("/>
            <Setter TargetName="UG_AnnaLabel" Property="Color" Value="#FF8A8A8A"/>
            <Setter TargetName="Item" Property="Color" Value="#FF333333"/>
            <Setter TargetName="Dpad" Property="Visibility" Value="Hidden"/>
        </DataTrigger>
    )XAML");
    };
    if (supportExtraOptions) addDisabledTrigger("Lights");
    addDisabledTrigger("Underglow");
    for (const auto* target : effectTargets) addDisabledTrigger(target);

    output.assign(source);
    output.insert(triggers, disabled);
    output.insert(text + sizeof("<local:FZTextBlock") - 1, " x:Name=\"UG_AnnaLabel\"");
    return true;
}

unsigned char onInput(void* input, unsigned context, unsigned action, unsigned char consume) {
    const auto pressed = inputOriginal(input, context, action, consume);
    if (!pressed) return 0;
    // FH3's UI_BACK commands are View. B uses the separate UI_CANCEL commands.
    // The Underglow picker stays alive while its Options or other dialogs are open.
    constexpr unsigned uiContext = 7, viewPress = 0x57, viewRepeat = 0x59;
    if (context == uiContext && action >= viewPress && action <= viewRepeat && garage::underglowPage) {
        if (action == viewPress) log("Underglow ignored the View livery-save command.");
        return 0;
    }
    if (context != 0xf) return pressed;
    if (action == 0xd9) {
        if ((menu.page == Car || menu.page == Lights) && menu.car && playerCar() == menu.car &&
            !underglowInstalled()) return 0;
        if (showsUnderglowRow(menu.rootList)) {
            const auto rows = rowsOf(menu.rootList);
            const int selected = highlightedRow(rows);
            if (selected >= 0 && disabledRow(rows, selected)) return 0;
        }
        return pressed;
    }
    if (menu.page == Native ||
        (action != 0xd7 && action != 0xd8 && action != 0xda) ||
        !menu.car || playerCar() != menu.car) return pressed;
    // ANNA advances through every native row. Keep the unavailable rows visible and skip them.
    if (action == 0xd7 || action == 0xd8) {
        if (menu.page != Targets) return pressed;
        const int next = nextTargetRow(garage::current(), menu.targetRow, action == 0xd7 ? 1 : -1);
        if (next >= 0) paint(next);
        return 0;
    }
    switch (menu.page) {
    case Car: menu.page = Native; return pressed;
    case Lights: setPage(Car); break;
    case Underglow: setPage(menu.standalone ? Car : Native,
        menu.standalone ? (underglowAvailable() ? underglowCarRow : 0) : static_cast<int>(menu.returnCount)); break;
    case Targets: setPage(Underglow); break;
    case Patterns: setPage(Targets, static_cast<int>(
        std::find(std::begin(targetOrder), std::end(targetOrder), menu.target) - std::begin(targetOrder))); break;
    default: return pressed;
    }
    return 0;
}

bool install() {
    struct Hook {
        size_t rva;
        const char* prefix;
        void* callback;
        void** original;
    };
    const Hook hooks[] = {
        {0x10b0da0, "\x40\x57\x48\x83\xec\x50", reinterpret_cast<void*>(onAdd), reinterpret_cast<void**>(&addOriginal)},
        {0x10b2810, "\x48\x8b\x89\x10\x02", reinterpret_cast<void*>(onClear), reinterpret_cast<void**>(&clearOriginal)},
        {0xab3fb0, "\x40\x55\x48\x83\xec\x20", reinterpret_cast<void*>(onList), reinterpret_cast<void**>(&listOriginal)},
        {0xab7f60, "\x48\x89\x5c\x24\x18", reinterpret_cast<void*>(onRefresh), reinterpret_cast<void**>(&refreshOriginal)},
        {0xaaa710, "\x40\x53\x48\x83\xec\x30", reinterpret_cast<void*>(onSelection), reinterpret_cast<void**>(&selectionOriginal)},
        {0x10b9870, "\x40\x57\x48\x83\xec\x50", reinterpret_cast<void*>(onLabel), reinterpret_cast<void**>(&labelOriginal)},
        {0x1a198f0, "\x48\x89\x5c\x24\x08", reinterpret_cast<void*>(onInput), reinterpret_cast<void**>(&inputOriginal)},
        {0x10b9970, "\x40\x57\x41\x56\x41\x57", reinterpret_cast<void*>(moveDown), reinterpret_cast<void**>(&downOriginal)},
        {0x10b9ae0, "\x40\x57\x41\x56\x41\x57", reinterpret_cast<void*>(moveUp), reinterpret_cast<void**>(&upOriginal)},
        {0xa01ba0, "\x48\x8b\xc4\x55\x41\x56", reinterpret_cast<void*>(onDispatch), reinterpret_cast<void**>(&dispatchOriginal)},
    };
    if (!api->hook_function) return false;
    const size_t count = std::size(hooks) - (supportExtraOptions || supportHorizon3Extended ? 0 : 1);
    for (size_t i = 0; i < count; ++i) {
        const auto& hook = hooks[i];
        if (std::memcmp(game + hook.rva, hook.prefix, std::strlen(hook.prefix))) return false;
    }
    std::array<FH3Hook*, std::size(hooks)> installed{};
    for (size_t i = 0; i < count; ++i) {
        const auto& hook = hooks[i];
        installed[i] = api->hook_function(mod, "FH3 ANNA Underglow audio", game + hook.rva, hook.callback, hook.original);
        if (!installed[i] || !api->enable_hooks(mod, installed[i])) {
            while (i) api->disable_hooks(mod, installed[--i]);
            return false;
        }
    }
    return true;
}
} // namespace anna
