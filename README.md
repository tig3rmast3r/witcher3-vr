# The Witcher 3 VR

An unofficial work-in-progress mod that brings native Virtual Reality support
to the DirectX 12 version of *The Witcher 3: Wild Hunt*, using the latest
official PC release (currently Patch 4.04).

This is an independent implementation developed specifically for
*The Witcher 3*.
Its DX12 and VR architecture was informed by
[REFramework](https://github.com/praydog/REFramework) and
[UEVR](https://github.com/praydog/UEVR) by praydog; neither project is bundled
as a runtime dependency.

> [!WARNING]
> This project is under active development. Features may be incomplete,
> unstable, or incompatible with some hardware and game configurations.

> [!IMPORTANT]
> Gameplay currently requires a mouse and keyboard or a gamepad. VR motion
> controllers are not supported and are not currently planned.

## Current status

### Working

| Feature | Status |
|---|---|
| Mono rendering | Working |
| Same-tick geometry stereo | Working |
| No AA / FXAA | Working |
| TAAU | Working |
| DLSS | Working |
| DLAA | Working |
| Custom render resolutions | Working |
| 6DoF HMD free look | Synchronized with OpenXR |
| Crossbow HMD aiming | Working |
| Resizable HUD and menus | Working |
| Adjustable in-headset render window | Working |
| Near-camera view | Working |
| Experimental first-person exploration view | Available through `F11`, with adaptive on-foot and horse placement plus stationary view-following body rotation |
| First-person snap turn and headset-based movement | Optional 30/45/60-degree gamepad snap turn with continuous HMD-directed movement |
| First-person combat handoff | Optional automatic switch to third person in combat |
| Faster movement transitions | Optional and enabled by default in both third and first person |
| 5:4 cinema framing | Enabled |
| HMD-aware render-budget region | Implemented |
| 2D overlays and world-space icons | Corrected for VR; optional "Steady Icons" mode |
| Cinema Mode and cutscenes | Mono or stereoscopic, according to the selected rendering mode |
| Optional automatic cutscenes in Full VR | Available; manual `F10` Cinema Mode remains independent |
| World-locked menus, inventory, and Cinema Mode | Free HMD rotation while the screen remains stationary |
| First-person aiming handoff | Temporarily switches to third person while aiming |

The ForceDLAA parameter-query and resolution-override approach is adapted from
[DLSSTweaks](https://github.com/emoose/DLSSTweaks) by emoose.

### Not yet implemented

- Ray tracing
- Screen Space Reflections (High)
- Native support for canted displays
- Far/Distante game camera modes for exploration, combat, and horse riding; only close/near cameras are currently corrected

Headsets with canted displays currently require the manufacturer's
**Parallel Projection** mode.

### Tested configurations

| Headset | OpenXR path | Notes |
|---|---|---|
| Meta Quest 3 | Virtual Desktop | Primary development configuration |
| Pimax 5K | OpenXR via SteamVR | Parallel Projection must be enabled |

### Not supported or not fully tested

- Motion blur
- Bloom
- Lens flare
- Third-party mod compatibility
- Other headset and OpenXR-path combinations

You are free to use other mods, but third-party mod compatibility is not
supported during this stage of development. Before reporting a Witcher 3 VR
bug, disable all other mods and reproduce the problem on an otherwise supported
installation. Please do not open issues for problems that occur only while
another mod is installed. This is a temporary development-scope limitation, not
a restriction on using mods.

## Known issues

- Some rare shadows may flicker in stereo.
- Loading screens may change size, appear blank, or display duplicated images.
- Black borders may be visible at the sides; depending on the headset, they may
  also appear at the top or bottom of the image.

## Requirements

- A legally owned copy of *The Witcher 3: Wild Hunt — Next-Gen Update*
- The latest official PC version of the game, currently
  [Patch 4.04](https://support.cdprojektred.com/en/witcher-3/pc/sp-technical/issue/2342/patch-4-04-download-now)
- The DirectX 12 version of the game
- A working OpenXR runtime
- A mouse and keyboard or gamepad
- An NVIDIA RTX GPU when using DLSS or DLAA

Older game versions and rollback branches are not supported.

## Installation

This mod uses a launcher, mainly because every mode has its own hooks,
and some of them must be on at startup.
Keeping all hooks enabled degrades performance, so the launcher enables
only the hooks required for the selected mode.

1. Download the latest package from the
   [Releases](https://github.com/tig3rmast3r/witcher3-vr/releases) page.
2. Extract the package directly into the game root:

   ```text
   The Witcher 3\
   ```

   The archive already contains the correct `bin\x64_dx12`, `mods`, `dlc`, and
   `Witcher3VR` folders, including the required OpenXR loader.
3. Run `bin\x64_dx12\Witcher3VRLauncher.exe`.
4. Select the rendering mode and resolution.
5. Launch the game through the launcher.

The package also installs the bundled `modWitcher3VRStateBridge` script under
the game's `mods` directory. It supplies the instantaneous locomotion and
combat state used by the experimental First-Person view and is part of
Witcher 3 VR.

The bundled Fast Transitions DLC remains installed, while the launcher option
**Faster Movement Transitions (Recommended)** controls REDengine's native DLC
enable flag. It is enabled by default and can be disabled without removing or
renaming any files; the change applies on the next game launch.

If `witcher3vr.ini` does not exist, the launcher creates it automatically. The
first run inherits the game's current supported AA choice as **Stereo No AA /
FXAA**, **Stereo TAAU**, or **Stereo DLSS**. An unknown AA mode falls back to
**Stereo No AA / FXAA**. Resolution defaults to **Ultra 2688 × 2784** and 5:4
Cinema framing is enabled. Older INIs receive a one-time configuration update
that preserves the selected rendering mode, resolution, Full VR cutscene
choice, and unrelated custom settings. Later manual tuning is not overwritten
at launcher startup.

The launcher includes three resolution presets intended for Quest 3 with
Virtual Desktop. If you use a different headset or runtime, select **Custom**
and enter the recommended per-eye resolution reported by your OpenXR or
SteamVR runtime.

For the current release, preserve the recommended aspect ratio. A different
resolution with the same ratio can be used to trade image quality for
performance; an incorrect ratio may distort screen-space elements or the HUD.

## Recommended game settings

The **Configure Settings for VR** button applies the settings used during
development. These provide a tested compromise between performance and image
quality.

The launcher creates a backup before changing the game profile. Use
**Restore Original Settings** to restore it.

If you prefer to configure the game manually, use the following settings:

### Display

| Setting | Value |
|---|---|
| Resolution | Controlled by the launcher |
| VSync | Off |
| Maximum FPS | Unlimited / `0` |
| NVIDIA Reflex | Off recommended |
| Anti-aliasing mode | Controlled by the launcher |
| DLSS quality mode | Controlled by the launcher |

Reflex On or Boost has caused stuttering on the development system and is not
currently recommended.

### Graphics

| Setting | Value |
|---|---|
| Ray tracing | Off |
| Screen Space Reflections | Off or Low |
| Motion blur | Off |

The launcher warns when Ray Tracing or SSR High is detected. These settings
are not changed automatically: disable Ray Tracing and set Screen Space
Reflections to Low or Off before launching VR.

All remaining settings can be adjusted to preference, but configurations
different from the provided VR preset may not have been tested.

### Texture quality and LOD

The Texture Quality setting also affects LOD behaviour. Higher values keep
detailed geometry and textures visible farther away, but may increase distant
shimmering.

Suggested values:

- **No AA / FXAA:** Medium
- **TAAU / DLSS:** Medium or High
- **DLAA:** Higher values may be practical if performance allows

## Keyboard shortcuts

| Key | Action |
|---|---|
| `F8` | Toggle between Standard and Near views |
| `F9` | Recenter the VR view |
| `F10` | Toggle Cinema Mode; mono in Mono modes, stereoscopic in Stereo modes |
| `F11` | Toggle First Person (Experimental) |

The First-Person view is experimental and intended for exploration only. It
uses DLL-controlled placement profiles for idle movement, walking, sprinting,
and horse states rather than a fully animation-driven body anchor. Its
placement may not be ideal for every animation or contextual action; Standard
third-person view is recommended for combat.

The launcher option **Gamepad Snap Turn + Head Follow (First Person Only,
Experimental)** enables both gamepad snap turning and continuous headset-based
movement direction while `F11` First Person is active. The snap-turn angle is
selectable as 30, 45, or 60 degrees and defaults to 45 degrees. The option is
disabled by default and does not affect Standard, Near, or Cinema views.

When First Person is stationary on foot, Geralt follows the viewing direction
so targets can be acquired without first starting a movement animation. The
view remains detached from the body turn, and normal free look resumes as soon
as locomotion begins. Combat, horse riding, swimming, diving, menus, Cinema
Mode, and cutscenes are excluded from this behavior.

The launcher option **Auto switch to third person during combats (First Person
Only, experimental)** automatically switches from First Person to Standard
third-person view when combat begins. After combat has remained inactive for
10 seconds, it returns to First Person. Manual view changes cancel the pending
automatic return. This option is also disabled by default.

While First Person is active, aiming temporarily uses the Standard third-person
camera so the weapon trajectory remains aligned with the crosshair. The view
returns to First Person when aiming ends.

Menus, inventory screens, and Cinema Mode remain stationary in the virtual
world, allowing free headset rotation instead of moving with the headset.
Cinema Mode follows the selected rendering mode: Mono modes produce a mono
screen, while Stereo modes produce a stereoscopic screen.

The launcher option **Show Automatic Cutscenes in Full VR (Experimental)**
keeps automatic cutscenes in the normal VR camera instead of switching them to
Cinema Mode. It does not change manual `F10` Cinema Mode. Both routes follow the
selected Mono or Stereo rendering mode.

Cinema Mode can also be used as a temporary workaround for sections that do
not render correctly or are difficult to play in VR.

## Troubleshooting and bug reports

Use [GitHub Issues](https://github.com/tig3rmast3r/witcher3-vr/issues) for
reproducible bugs and feature requests.

Reports from modded installations must first be reproduced with all third-party
mods disabled. Mod-specific conflicts and compatibility problems are not
supported at this stage and should not be submitted as issues.

When reporting a bug, include:

- A clear description of the problem
- Steps needed to reproduce it
- Headset model and connection method
- OpenXR runtime
- GPU model
- Rendering and anti-aliasing mode
- `witcher3vr.ini`
- The diagnostic log
- A save file, if the problem depends on a specific scene or location

To create a diagnostic log:

1. Enable **Diagnostic Logging** in the launcher.
2. Reproduce the problem.
3. Close the game normally.
4. Attach the generated `witcher3vr.log` file to the issue.

Please do not open feature requests for motion-controller support. The request
is understood, but motion controls are not currently part of the development
plan.

For general troubleshooting, you can also use the
[Flat2VR community](https://www.flat2vr.com/).

## Support

The mod is free and publicly available. Donations are entirely optional and
never provide exclusive builds, features, or support.

If you would like to support development, you can do so through
[Ko-fi](https://ko-fi.com/tig3rmast3r) or by using the Sponsor button at the
top of the repository.

## Credits

- [praydog / REFramework](https://github.com/praydog/REFramework) — DX12
  hooking and VR architecture reference
- [praydog / UEVR](https://github.com/praydog/UEVR) — additional VR
  implementation reference
- [emoose / DLSSTweaks](https://github.com/emoose/DLSSTweaks) — ForceDLAA
  parameter-query and resolution-override approach
- [Next Gen Movement Input Lag Fix — Fumio Edition](https://www.nexusmods.com/witcher3/mods/7586)
  — behavior-graph foundation for the optional Fast Transitions DLC
- [MinHook](https://github.com/TsudaKageyu/minhook) — Windows API hooking
  library
- [Khronos OpenXR SDK](https://github.com/KhronosGroup/OpenXR-SDK)
- [Microsoft DirectX-Headers](https://github.com/microsoft/DirectX-Headers)
- [NVIDIA NGX SDK](https://github.com/NVIDIA-RTX/Streamline/blob/main/external/ngx-sdk/license.txt)

See [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md) for copyright notices and
third-party licensing information.

## Building from source

The dependency setup script checks out the exact revisions used by the project.

```powershell
.\scripts\setup-dependencies.ps1
cmake --preset release
cmake --build --preset release
ctest --preset release
```

The optimized Release build produces one `dxgi.dll`. The launcher's
**Diagnostic Logging** checkbox enables logging and detailed runtime probes in
that same DLL; no separate diagnostic binary is required.

See [CONTRIBUTING.md](CONTRIBUTING.md) before proposing a code change.

## License

Original Witcher 3 VR code is released under the [MIT License](LICENSE), unless
otherwise stated.

Third-party software remains the property of its respective authors and is
distributed under its own license terms. The project's MIT license does not
relicense third-party components or NVIDIA SDK materials.

## Disclaimer

> This is an unofficial fan work and is not approved or endorsed by
> CD PROJEKT RED.

This project is not affiliated with CD PROJEKT RED, CD PROJEKT S.A., NVIDIA, or
the authors of the third-party projects listed above.

All trademarks, game content, and related intellectual property belong to
their respective owners. This project does not distribute game assets and
requires a legally obtained copy of *The Witcher 3: Wild Hunt*.

The software is provided as-is and without warranty. Use it at your own risk
and keep backups of saves and configuration files.

This project is intended to comply with the
[CD PROJEKT RED Fan Content Guidelines](https://www.cdprojektred.com/en/fan-content).
