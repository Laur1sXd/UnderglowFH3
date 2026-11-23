#define main meetRegressionTests
#include "test_meet_lights.cpp"
#undef main
#include "test_npc_radio.h"

int main() {
    meetRegressionTests();
    struct AiCar {
        std::array<Byte, 0x73b0> car{};
        std::array<Byte, 0x900> presentation{};
        Byte controller{};
    };
    assert(test::count() == 1); // Preserve the fixture's foreign lamp.
    const auto personal = savedSettings;
    std::array<AiCar, 12> opponents{};
    std::array<uint64_t, 12> identities{};
    for (auto& opponent : opponents) {
        test::put(opponent.car.data(), 0, game + 0x42e0650);
        test::put(opponent.car.data(), 0x7370, 1);
        test::put(opponent.car.data(), 0x7378, &opponent.controller);
        test::put(opponent.car.data(), 0x73a0, opponent.presentation.data());
        test::put(opponent.presentation.data(), 0, game + 0x468e848);
        test::put(opponent.presentation.data(), 0x10, test::cars[0].scene.data());
        XMFLOAT4X4 matrix;
        XMStoreFloat4x4(&matrix, XMMatrixTranslation(10, 280, -1400));
        test::put(opponent.presentation.data(), 0x1a0, matrix);
    }
    meetProvider = nullptr;
    modEnabled = true;
    drivatarSequence = 1000;
    const auto capture = [](AiCar& opponent) {
        FH3Context context{};
        context.rbx = reinterpret_cast<uintptr_t>(opponent.car.data());
        const auto before = context;
        captureDrivatar(&context, nullptr);
        assert(std::memcmp(&before, &context, sizeof(context)) == 0);
    };
    for (int excluded : {0, 2, 3}) {
        test::put(opponents[0].car.data(), 0x7370, excluded);
        capture(opponents[0]);
        assert(std::none_of(drivatarSnapshots.begin(), drivatarSnapshots.end(),
            [](const DrivatarSnapshot& frame) { return frame.identity != 0; }));
    }
    test::put(opponents[0].car.data(), 0x7370, 1);
    for (size_t i = 0; i < opponents.size(); ++i) {
        capture(opponents[i]);
        identities[i] = drivatarSnapshots[i].identity;
        assert(identities[i]);
    }
    size_t lit = 0;
    for (size_t i = 0; i < opponents.size(); ++i) {
        const auto settings = drivatarSettings(identities[i]);
        lit += settings.lightsEnabled;
        assert(settings.effects[0] >= 0 && settings.effects[0] < fx::kEffectCount);
        capture(opponents[i]);
        assert(drivatarSnapshots[i].identity == identities[i]);
    }
    assert(lit > 0 && lit < opponents.size());
    assert(updateDrivatarLights(test::manager.data()));
    assert(test::count() == 1 + lit * lightCount);
    for (size_t i = 0; i < opponents.size(); ++i)
        assert((drivatarWorlds[i].lights[0] != invalid) == drivatarSettings(identities[i]).lightsEnabled);

    daylightManager = test::manager.data();
    daylightUntil = GetTickCount64() + 10000;
    const Pool presets{test::styles.data(), test::styles.data() + test::styles.size()};
    DaylightUpload upload{presets};
    daylightUpload = &upload;
    std::array<Byte, maxOwnedLights * lightPacketSize> packets{};
    std::array<Byte, lightPacketSize> packet{};
    for (Id id = 0; id < test::capacity; ++id) {
        if (!test::usedStyles[id]) continue;
        test::put(packet.data(), 0, 1.0f);
        test::put(packet.data(), 0x20, id);
        copyLightPacket(packets.data(), packet.data(), packet.size());
    }
    assert(upload.slotCount == lit * lightCount && upload.packetCount == lit * lightCount);
    daylightUpload = nullptr;

    // A race / head-to-head can reuse a car address with a new driver.
    Byte replacementController{};
    for (auto& opponent : opponents)
        test::put(opponent.car.data(), 0x7378, &replacementController);
    assert(!updateDrivatarLights(test::manager.data()) && test::count() == 1);
    for (size_t i = 0; i < opponents.size(); ++i) {
        capture(opponents[i]);
        assert(drivatarSnapshots[i].identity != identities[i]);
    }
    updateDrivatarLights(test::manager.data());
    modEnabled = false;
    assert(!updateDrivatarLights(test::manager.data()) && test::count() == 1);
    modEnabled = true;

    test::lampLimit = 10;
    updateDrivatarLights(test::manager.data());
    assert(test::count() == 1 && test::namedStyles.empty());
    test::lampLimit = test::capacity;
    for (auto& owner : drivatarWorlds) owner.retryAfter = 0;
    updateDrivatarLights(test::manager.data());

    // Expired frames must retire safely even after the car memory is gone.
    for (auto& frame : drivatarSnapshots) {
        if (!frame.identity) continue;
        frame.pose.playerTick = GetTickCount64() - 1000;
        frame.pose.car = reinterpret_cast<Byte*>(1);
    }
    assert(!updateDrivatarLights(test::manager.data()) && test::count() == 1);
    assert(std::memcmp(&personal, &savedSettings, sizeof(personal)) == 0);
    std::set<int> effects, primaries;
    size_t enabled = 0;
    for (uint64_t identity = 1; identity <= 10000; ++identity) {
        const auto settings = drivatarSettings(identity);
        enabled += settings.lightsEnabled;
        effects.insert(settings.effects[0]);
        primaries.insert(settings.options[Primary]);
        assert(settings.effects[0] >= 0 && settings.effects[0] < fx::kEffectCount);
    }
    assert(enabled > 4500 && enabled < 5500);
    assert(effects.size() == fx::kEffectCount && primaries.size() == fx::kColorCount);
    testNpcRadio();
    std::cout << "PASS: local/race AI classification, stable random glow or none, daylight, "
        "head-to-head driver replacement, despawn, capacity rollback and personal settings.\n";
}
