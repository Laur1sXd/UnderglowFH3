#pragma once

#include "pe.hpp"

#include <cstdint>
#include <cstring>

namespace core::platform_menu {

// Optional host extension: explicit callbacks remove dependency on its linker
// layout. Stock platforms continue through the checked layouts below.
struct SettingsPreviewV1 {
    uint32_t size, version;
    void* (*page)(const void* mod);
    int (*selected)();
    void* buttons;
    void* row_set;
    void* selection_set;
    void (*notify)(void* model, void* descriptor);
    void* (*load_row)(const void* mod);
    void* (*model)();
    void* (*notice)();
};

struct PlatformMenu {
    uint32_t stamp, size;
    uint32_t button, row_get, row_set, notify, rows, installed_page, view, entries, in_hub;
    uint32_t row_actions, view_kinds, pages, settings_lock;
    const char* actions_prologue;
    const char* button_prologue;
    const char* row_get_prologue;
    const char* row_set_prologue;
    const char* notify_prologue;
};

// These checked Mod Platform builds use the same ModEntry layout.
inline constexpr PlatformMenu menus[] = {
    // Official Mod Platform 1.4.2 / DINPUT8.dll (API 17).
    // Checked local 1.4.2 save-handoff / shared SQL build.
    {0x6afdfcfb, 0x27f000, 0xeac10, 0xf07f0, 0xf0920, 0xcce80, 0x255258,
     0x24a2d0, 0x2552b8, 0x255db0, 0x2558b0, 0, 0, 0, 0,
     "\x41\x57\x41\x56\x41\x55\x41\x54\x56\x57\x55\x53",
     "\x41\x57\x41\x56\x41\x54\x56\x57\x55\x53",
     "\x56\x57\x53\x48\x83\xec\x30\x48\x89\xce",
     "\x41\x57\x41\x56\x41\x55\x41\x54\x56\x57\x55\x53",
     "\x55\x56\x57\x53\x48\x81\xec\xc8\x00\x00\x00"},
    {0x6afccb6a, 0x27f000, 0xea660, 0xf0200, 0xf0330, 0xcc8e0, 0x255248,
     0x24a2c0, 0x2552a8, 0x255da0, 0x2558a0, 0, 0, 0, 0,
     "\x41\x57\x41\x56\x41\x55\x41\x54\x56\x57\x55\x53",
     "\x41\x57\x41\x56\x41\x54\x56\x57\x53\x48",
     "\x56\x57\x53\x48\x83\xec\x30\x48\x89\xce",
     "\x41\x57\x41\x56\x41\x55\x41\x54\x56\x57\x55\x53",
     "\x55\x56\x57\x53\x48\x81\xec\xc8\x00\x00\x00"},
    {0x6af85a9e, 0x26b000, 0xda1a0, 0xdfd40, 0xdfe70, 0xbc420, 0x241248,
     0x2362c0, 0x2412a8, 0x241da0, 0x2418a0, 0, 0, 0, 0,
     "\x41\x57\x41\x56\x41\x55\x41\x54\x56\x57\x55\x53",
     "\x41\x57\x41\x56\x41\x54\x56\x57\x53\x48",
     "\x56\x57\x53\x48\x83\xec\x30\x48\x89\xce",
     "\x41\x57\x41\x56\x41\x55\x41\x54\x56\x57\x55\x53",
     "\x55\x56\x57\x53\x48\x81\xec\xc8\x00\x00\x00"},
    // Official Mod Platform 1.3.0 / DINPUT8.dll (API 16).
    {0x6af5f686, 0x26a000, 0xd93c0, 0xde3b0, 0xde4e0, 0xbb650, 0x240210,
     0x235280, 0x240268, 0x240d60, 0x240860, 0xbe170, 0x234778, 0x240340, 0x235790,
     "\x41\x57\x41\x56\x41\x55\x41\x54\x56\x57\x55\x53",
     "\x41\x57\x41\x56\x41\x54\x56\x57\x53\x48",
     "\x56\x57\x53\x48\x83\xec\x30\x48\x89\xce",
     "\x41\x57\x41\x56\x41\x55\x41\x54\x56\x57\x55\x53",
     "\x55\x56\x57\x53\x48\x81\xec\xc8\x00\x00\x00"},
    {0x6aee7575, 0x253000, 0xc80c0, 0xcd0b0, 0xcd1e0, 0xaa0f0, 0x229ff8,
     0x21f150, 0x22a048, 0x22aad0, 0x22a5e8, 0xace70, 0x21e698, 0x22a100, 0x21f620,
     "\x41\x57\x41\x56\x41\x55\x41\x54\x56\x57\x55\x53",
     "\x41\x57\x41\x56\x41\x54\x56\x57\x53\x48",
     "\x56\x57\x53\x48\x83\xec\x30\x48\x89\xce",
     "\x41\x57\x41\x56\x41\x55\x41\x54\x56\x57\x55\x53",
     "\x55\x56\x57\x53\x48\x81\xec\xc8\x00\x00\x00"},
    {0x47a22c2d, 0x24c000, 0xbbc10, 0xc0a10, 0xc0b40, 0xa1400, 0x2204a8,
     0x215610, 0x2204f8, 0x220f80, 0x220a98, 0xa4250, 0x214b68, 0x2205b0, 0x215ae0,
     "\x55\x41\x57\x41\x56\x41\x55\x41\x54\x56\x57\x53",
     "\x55\x41\x57\x41\x56\x41\x54\x56\x57\x53",
     "\x55\x56\x57\x53\x48\x83\xec\x28\x48\x8d",
     "\x55\x41\x57\x41\x56\x41\x55\x41\x54\x56\x57\x53",
     "\x55\x56\x57\x53\x48\x81\xec\xb8\x00\x00\x00"},
    {0x6ae83e9d, 0x21a000, 0x9e010, 0xa2910, 0xa2a40, 0x86fc0, 0x1f0b08,
     0x1e5d28, 0x1f0b40, 0x1f1540, 0x1f10c0, 0x89180, 0x1e5308, 0x1f0bf0, 0x1e61e8,
     "\x41\x57\x41\x56\x41\x55\x41\x54\x56\x57\x55\x53",
     "\x41\x57\x41\x56\x41\x54\x56\x57\x53\x48",
     "\x56\x57\x53\x48\x83\xec\x30\x48\x89\xce",
     "\x41\x57\x41\x56\x41\x55\x41\x54\x56\x57\x55\x53",
     "\x55\x56\x57\x53\x48\x81\xec\xb8\x00\x00\x00"},
    // Mod Platform 1.2.1 (API 16).
    {0x6aef5530, 0x268000, 0xd8580, 0xdd570, 0xdd6a0, 0xba810, 0x23e210,
     0x233280, 0x23e268, 0x23ed60, 0x23e860, 0xbd330, 0x232778, 0x23e340, 0x233790,
     "\x41\x57\x41\x56\x41\x55\x41\x54\x56\x57\x55\x53",
     "\x41\x57\x41\x56\x41\x54\x56\x57\x53\x48",
     "\x56\x57\x53\x48\x83\xec\x30\x48\x89\xce",
     "\x41\x57\x41\x56\x41\x55\x41\x54\x56\x57\x55\x53",
     "\x55\x56\x57\x53\x48\x81\xec\xc8\x00\x00\x00"},
    // Alternate Mod Platform 1.2.1 (API 16) build.
    {0x6aef54cf, 0x250000, 0xcee50, 0xd3f00, 0xd4030, 0xb40b0, 0x2271c0,
     0x21c230, 0x227218, 0x227d10, 0x227810, 0xb6170, 0x21b728, 0x2272f0, 0x21c740,
     "\x41\x57\x41\x56\x41\x55\x41\x54\x56\x57\x55\x53",
     "\x41\x57\x41\x56\x41\x54\x56\x57\x53\x48",
     "\x56\x57\x53\x48\x83\xec\x30\x48\x89\xce",
     "\x41\x57\x41\x56\x41\x55\x41\x54\x56\x57\x55\x53",
     "\x55\x56\x57\x53\x48\x81\xec\xb8\x00\x00\x00"},
};

inline bool matches(const uint8_t* code, const char* expected, size_t size)
{
    return core::pe::readable(code, size) && std::memcmp(code, expected, size) == 0;
}

inline bool matches_button(const uint8_t* code, const char* expected)
{
    constexpr size_t prologue_size = 10;
    constexpr size_t jump_size = 5;
    if (matches(code, expected, prologue_size)) return true;

    // Other mods can register an Accept handler before this worker runs.
    // Recognize the Platform's MinHook relay and let hook_function join its chain.
    if (!matches(code + jump_size, expected + jump_size, prologue_size - jump_size) ||
        !core::pe::readable(code, jump_size) || code[0] != 0xe9) return false;
    int32_t displacement;
    std::memcpy(&displacement, code + 1, sizeof displacement);
    const auto address = reinterpret_cast<intptr_t>(code + jump_size) + displacement;
    return matches(reinterpret_cast<const uint8_t*>(address), "\xff\x25\x00\x00\x00\x00", 6);
}

inline const PlatformMenu* find_platform_menu(const uint8_t* base)
{
    const auto* pe = core::pe::headers(base);
    if (!pe) return nullptr;
    for (const auto& candidate : menus) {
        if (pe->FileHeader.TimeDateStamp == candidate.stamp &&
            pe->OptionalHeader.SizeOfImage == candidate.size &&
            matches_button(base + candidate.button, candidate.button_prologue))
            return &candidate;
    }
    return nullptr;
}


} // namespace core::platform_menu
