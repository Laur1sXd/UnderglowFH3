// Garage input guard only. This does not intercept any ANNA command or list.
namespace garageInput {
using Input = unsigned char (*)(void*, unsigned, unsigned, unsigned char);
Input original;
unsigned char onInput(void* self, unsigned context, unsigned action, unsigned char flag) {
    const auto pressed = original(self, context, action, flag);
    constexpr unsigned uiContext = 7, viewPress = 0x57, viewRepeat = 0x59;
    if (pressed && context == uiContext && action >= viewPress && action <= viewRepeat && garage::underglowPage)
        return 0;
    return pressed;
}
bool install() {
    constexpr size_t rva = 0x1a198f0;
    constexpr char prefix[] = "\x48\x89\x5c\x24\x08";
    if (!api->hook_function || std::memcmp(game + rva, prefix, sizeof(prefix) - 1)) return false;
    auto* hook = api->hook_function(mod, "Underglow Garage View/save-livery guard", game + rva,
        reinterpret_cast<void*>(onInput), reinterpret_cast<void**>(&original));
    return hook && enableOwnHook(hook);
}
}
