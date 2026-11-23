#pragma once

std::atomic<bool> reportedDrivatarCapture{};

// ForzaCar kind 1 is local AI. Kind 0 is the player, kind 2 is traffic.
// Observe the native presentation handoff without changing race or AI state.
bool isDrivatar(Byte* car) {
    return car && read<Byte*>(car) == game + 0x42e0650 &&
        read<int>(car + 0x7370) == 1 && read<Byte*>(car + 0x7378);
}

void captureDrivatar(FH3Context* context, void*) {
    auto* car = reinterpret_cast<Byte*>(context->rbx);
    if (!modEnabled.load() || !isDrivatar(car)) return;
    auto* presentation = presentationFor(car);
    if (!presentation) return;
    auto* manager = managerFor(presentation);
    XMFLOAT4X4 matrix;
    if (!manager || !read(presentation + 0x1a0, matrix) || !validMatrix(matrix)) return;

    // A parked Car Meets actor is already owned by the meet renderer.
    const auto* provider = meetProvider.load();
    if (provider) {
        FH3MeetAppearance appearance{};
        appearance.size = sizeof(appearance);
        const float point[]{matrix._41, matrix._42, matrix._43};
        if (provider->lookup(presentation, point, &appearance)) return;
    }

    const auto now = GetTickCount64();
    auto* driver = read<Byte*>(car + 0x7378);
    Snapshot pose{};
    pose.car = car;
    pose.presentation = presentation;
    pose.manager = manager;
    pose.playerTick = now;
    pose.matrixTick = now;
    pose.matrix = matrix;
    if (autoFit) pose.extents = extentsFor(presentation);

    AcquireSRWLockExclusive(&snapshotLock);
    auto entry = std::find_if(drivatarSnapshots.begin(), drivatarSnapshots.end(),
        [car](const DrivatarSnapshot& frame) { return frame.pose.car == car; });
    if (entry == drivatarSnapshots.end()) {
        entry = std::find_if(drivatarSnapshots.begin(), drivatarSnapshots.end(),
            [now](const DrivatarSnapshot& frame) { return !frame.identity || now - frame.pose.playerTick > 750; });
    }
    if (entry != drivatarSnapshots.end()) {
        const bool sameSpawn = entry->identity && entry->driver == driver &&
            entry->pose.car == car && entry->pose.presentation == presentation &&
            entry->pose.manager == manager && now - entry->pose.playerTick <= 750;
        if (!sameSpawn) entry->identity = ++drivatarSequence;
        entry->pose = pose;
        entry->driver = driver;
    }
    ReleaseSRWLockExclusive(&snapshotLock);
    if (entry != drivatarSnapshots.end() && !reportedDrivatarCapture.exchange(true))
        log("Live local/race Drivatar presentation captured.");
}

bool currentDrivatarPose(const DrivatarSnapshot& frame, ULONGLONG now) {
    return frame.identity && frame.pose.matrixTick && now - frame.pose.playerTick <= 750 &&
        isDrivatar(frame.pose.car) && read<Byte*>(frame.pose.car + 0x7378) == frame.driver &&
        presentationFor(frame.pose.car) == frame.pose.presentation &&
        managerFor(frame.pose.presentation) == frame.pose.manager;
}

Settings drivatarSettings(uint64_t identity) {
    const auto seed = static_cast<uint32_t>(identity ^ (identity >> 32));
    const auto random = static_cast<uint32_t>(fx::hash01(seed) * 16777216);
    Settings settings{};
    settings.lightsEnabled = modEnabled.load() && (random & 1) != 0;
    for (size_t zone = 0; zone < zoneCount; ++zone) settings.options[Front + zone] = 1;
    settings.options[Primary] = static_cast<int>((random >> 1) % fx::kColorCount);
    settings.options[Secondary] = static_cast<int>((random >> 8) % fx::kColorCount);
    // Each audio effect reads this NPC's independent station signal.
    std::array<int, fx::kEffectCount> choices{};
    size_t count = 0;
    for (const auto entry : fx::kEffectOrder) choices[count++] = entry.effect;
    settings.effects.fill(choices[(random >> 3) % count]);
    settings.sliders[Brightness] = 0.5f + ((random >> 12) % 36) / 100.0f;
    settings.sliders[Speed] = 0.35f + ((random >> 16) % 116) / 100.0f;
    return settings;
}

bool updateDrivatarLights(Byte* manager) {
    std::array<DrivatarSnapshot, maxDrivatars> frames;
    AcquireSRWLockShared(&snapshotLock);
    frames = drivatarSnapshots;
    ReleaseSRWLockShared(&snapshotLock);
    const auto lights = readPool(manager, 0, lightStride);
    const auto presets = readPool(manager, 0x38, presetStride);
    const auto now = GetTickCount64();
    bool lit = false;
    for (size_t slot = 0; slot < frames.size(); ++slot) {
        const auto& frame = frames[slot];
        auto& owner = drivatarWorlds[slot];
        const auto settings = drivatarSettings(frame.identity);
        const bool active = settings.lightsEnabled && frame.pose.manager == manager && currentDrivatarPose(frame, now);
        if (!active && owner.manager != manager) continue;
        if (owner.manager != manager || owner.lightPool != lights.begin || owner.presetPool != presets.begin) {
            // A replaced scene owns its old pools. Never destroy through a stale manager.
            owner = World{};
            owner.manager = manager;
            owner.lightPool = lights.begin;
            owner.presetPool = presets.begin;
            drivatarIdentities[slot] = 0;
        }
        bool valid = owner.lights.front() != invalid;
        for (size_t i = 0; i < lightCount && valid; ++i)
            valid = ownsLight(lights, i, owner) && ownsPreset(presets, i, owner);
        if (!active || !valid || drivatarIdentities[slot] != frame.identity)
            removeLights(manager, lights, presets, false, owner);
        if (!active) continue;
        if (lights.begin && presets.begin && owner.lights.front() == invalid && now >= owner.retryAfter) {
            if (!makeLights(manager, lights, presets, owner)) {
                removeLights(manager, lights, presets, true, owner);
                owner.retryAfter = now + 2000;
                if (!owner.reportedCapacity) log("Waiting for free Drivatar underglow light slots.");
                owner.reportedCapacity = true;
                continue;
            }
            drivatarIdentities[slot] = frame.identity;
        }
        if (owner.lights.front() == invalid) continue;
        const auto audio = npcRadio::frame(frame.identity, now);
        renderLights(manager, presets, frame.pose, settings, now, owner, &audio);
        lit = true;
    }
    return lit;
}

bool installDrivatarHook() {
    constexpr Byte expected[]{0x48, 0x8b, 0x8b, 0xa0, 0x73, 0, 0};
    std::array<Byte, sizeof(expected)> actual{};
    if (!api->hook_instruction || !read(game + drivatarPresentationTick, actual) ||
        std::memcmp(actual.data(), expected, actual.size())) return false;
    auto* hook = api->hook_instruction(mod, "Local Drivatar presentation", game + drivatarPresentationTick,
        captureDrivatar, nullptr);
    return hook && enableOwnHook(hook);
}
