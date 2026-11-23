# Underglow Garage 1.0.2 — ExtraOptions

Verified with official ANNA 1.0.2 automatic scrolling. See ANNA-1.0.2-Compatibility.txt for installation and tests.

# Edition: ExtraOptions

# Underglow Garage 1.0.2

Registers version 1.0.2 with GitHub update checks enabled for
Laur1sXd/UnderglowFH3. Mod Platform checks the latest published release; drafts
are not public update releases. The main Underglow On/Off row is not registered.

MODS part switches preview the highlighted row. Press A to save it; moving to
another row or pressing B restores any unaccepted preview. There is no separate
Apply Underglow Settings row. Official stock platform 1.4.2 keeps this behavior.
Part row order is Front, Rear, Left, Right in every edition.
The platform Load On/Off selector follows the same A-to-accept rule. Its
unaccepted value is only a selector preview: it shows the bottom notice but
does not write the load file. Moving away or B restores the accepted value
and clears a preview-only notice. After A, the notice remains until successful F9.
F9 reads only the accepted Load request, preserving any earlier accepted
pending request while a different value is previewed.

The effect-placement popup lists All, Front, Rear, Left Side and Right Side.
Use the existing B button to go back; there is no duplicate Back row.

## Load setting and F9

With your existing stock FH3 Mod Platform **DINPUT8.dll 1.4.2** and
`[dev] HotReload=1`, accept Load Underglow Off and press F9 to deactivate it;
accept On and press F9 to reactivate it. This behavior is implemented inside
Underglow.dll. No replacement or patch to DINPUT8.dll is required.

All four editions keep one small F9 listener resident while Off. The lighting,
radio workers and gameplay hooks are stopped. The listener uses the configured
ReloadKey (F9 by default), only in this process's foreground window. A real DLL
hot reload stops and joins the listener as part of `fh3mod_shutdown` too.
Each generation reenables only its own recorded hooks, avoiding duplicate old
hooks on stock platforms. Setups/history remain saved; HotReload=0 retains the
normal restart behavior. Close the Underglow editor/Options/popups before F9.
The loader's bottom pending-change notice compares its selected value with
the last state successfully applied by F9. It clears after either successful
switch and reappears for the opposite selection. Failed/deferred changes keep
the notice. Resident menu hooks update this on the native UI thread while Off.

The mod must have loaded once in the current session. If Azimuth starts with
Load Underglow already Off, the stock loader skips the DLL and it cannot listen
for F9: choose On and restart once in that case. Installing this updated DLL
over an already-running old build also needs one initial restart.

The older separate F9 platform build is optional and is not needed for this
workflow. If that host already handles runtime Load changes, the mod leaves
control with it instead of starting a competing listener.

This working copy extends UnderglowFH3 with the optional CarMeets.ParkedCars v1
interface. It retains the garage, effects, ANNA, radio and fitted-body geometry
code from the upstream source. See README-upstream.md and Licenses for credits.

Only the eleven NPC bays get randomized lighting, each with its own native
32-light pool, all four sides, stable random colors and an effect selected from
the complete catalog. The row includes one-color, two-color, multicolor and
radio-reactive families. Audio effects read the same active radio signal as
personal glow and go dark when the radio is silent.

Exit/audio fixes: native presets have unique registered names and cleanup
checks each preset's ownership before deleting it. This avoids the native
name-map erase failure when an unnamed allocation recycles a formerly named
slot, and preserves foreign presets if the engine reuses a slot. A live meet
continues sampling radio FFT or Horizon Radio Network bands even when the
meet UI reports a pause or its free-roam station flags are inactive. Normal
pause-menu frame holding is retained outside a meet.

The player's car keeps its exact equipped personal glow inside and outside the
meet. A car with no fitted glow stays unlit; Car Meets never forces random neon
onto the player's car. NPC colors/effects are transient and never written to
Garage entries or game saves.

There is no main Underglow On/Off row in MODS. Apply colors/effects through
Designs and Paints; No Underglow controls the personal car's installed setup.
Legacy [Underglow] Enabled is ignored, so an older Enabled=0 cannot hide a
newly applied garage setup. Individual part switches in MODS retain live
row previews, A to accept, and rollback when leaving an unaccepted row.
Without Car Meets this is ordinary personal Underglow.
If Underglow itself is absent or disabled by Mod Platform, Car Meets adds no
lights at all. Native car shadows and their resources are not replaced.

Build: run Build-Editions.cmd with Visual Studio C++ Build Tools installed.
It produces build/Standalone-Native, Standalone-Network, ExtraOptions-Native,
and ExtraOptions-Network. Choose exactly one edition for the corresponding
radio / ExtraOptions setup. Each DLL is named Underglow.dll. Keep the existing
Underglow.ini when upgrading; do not install two Underglow DLLs.

Tests: test.cmd compiles and runs native and network versions of the light-pool
and audio regression harness. The prior code fails the recycled-name fixture;
the current code passes, including preset reuse by a foreign owner and live
sampling under the meet's pause/station flags.
It uses stub native light allocation in a private process, not a running game.
The source includes the shared SDK contract in sdk/FH3CarMeets.h.

The user confirmed Network audio effects, meet exit and Achievements in the
combined test. Network audio requires the Network edition of Horizon 3 Extended
that publishes Horizon3Extended.RadioBands; a native-audio edition can play its
replacement songs without exporting those samples. The successful test used
the catalogue-session Network build, alongside Standalone-Network Underglow.

Supports the official FH3 Mod Platform 1.3.0 DINPUT8.dll build, including
settings selection, preview, A to save and B to discard. Existing checked
platform layouts remain supported. Keep Underglow.ini when upgrading.

Run build.cmd for this selected edition; Build-Editions.cmd builds all four.

## Random local and race Drivatars

Local AI in free roam, head-to-head races and other races each have a 50% chance
of receiving randomized underglow. The choice stays stable for that spawned car.
The renderer chooses from all 61 effects, including 15 audio effects, and the built-in palette,
with fitted-body geometry, a random brightness and a random effect speed.
It tracks up to 64 local AI cars concurrently. Ordinary traffic is excluded.

The native ForzaCar presentation handoff supplies live car identities and poses.
Replaced drivers, changed presentations, expired ticks and replaced scene pools
retire their cached lighting. Partial allocation failures roll back owned lamps
and presets, and ownership checks preserve lights belonging to other systems.
Car Meets actors continue to use the existing dedicated meet lighting.

Random AI setups are transient. Personal Garage setups and history are preserved.
Garage No Underglow changes the personal car only, not another car's random setup.

Run test.cmd for native/network regression tests, including driver replacement,
despawning, daylight upload, stable random choices and light-pool exhaustion.
These are native harness tests. An interactive head-to-head and race test is
still required to verify behavior while driving in Azimuth.

## Independent Drivatar radios

All four Garage editions give nearby audio-reactive local/race Drivatars their
own station playback and meter. Each spawn keeps its station choice from the
eight built-in stations. These cars do not read the player's signal, engine,
tires or festival music. Native playback uses an individual spatial FMOD event
and FFT; the rear reads bass, sides read low/mid bands and front reads treble.

When Horizon 3 Extended is installed and Horizon Radio Network is enabled,
NPCs use replacement music from its station folders, configured playlists or
station catalogues. This is also detected by the native-radio editions for
NPC playback. When HRN is off or absent they use native stations. The edition's
existing personal-car radio integration is unchanged.

Replacement playback dynamically uses the installed HRN libmpv-2.dll and
yt-dlp.exe; Underglow does not bundle or download players, songs or libraries.
Each NPC player has its own decoding and band filter. Missing/unavailable
tracks leave that car's audio effect dark while playback retries. The car's
own setup and Previous Underglows are never overwritten by an NPC.

Up to twelve nearby audio-effect radios play within 45 metres, nearest first.
Volume falls with distance and follows the game's radio/master music mix.
Playback is paused with the menu. Despawn, owner replacement, module disable and
switching native/HRN routes retire the affected voices and DSPs. Network work
and decoder destruction run away from the game's rendering/audio callbacks.

test.cmd checks all four compile configurations, independent native station
meters, cleanup/replacement, silence/player isolation, route selection and
voice limits. If HRN's libmpv is installed, it also decodes two generated local
test tones concurrently through the actual production filter with null audio
output. Set UG_TEST_MPV_ROOT to the folder containing libmpv-2.dll on other
machines. This test uses no internet and plays no audible music.

Live check, 13 November 2026: the isolated native-radio FH3 run captured a local
Drivatar, started its independent native station, and received music on that
event's own meter. All four editions also passed the real Mod Platform startup
harness with Extra Options/Horizon 3 Extended and Hot Reload on/off combinations.
Full interactive race-transition and live HRN catalogue-streaming checks remain
outside that result; the offline HRN test validates real local-track decoding.


## Save isolation

See `Save isolation.txt`. New careers do not inherit old garage IDs or unassigned legacy setups. Reapply desired old setups after updating.
