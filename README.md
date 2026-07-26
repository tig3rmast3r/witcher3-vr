# The Witcher 3 VR

An unofficial work-in-progress mod that brings native Virtual Reality support
to the DirectX 12 version of **The Witcher 3: Wild Hunt**, using the latest
official PC release (currently Patch 4.04).

This is an independent implementation developed specifically for The Witcher 3.
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
| DLSS | Working, with a known initialization issue |
| DLAA | Working |
| Custom render resolutions | Working |
| 6DoF HMD free look | Synchronized with OpenXR |
| Crossbow HMD aiming | Working |
| Resizable HUD and menus | Working |
| Adjustable in-headset render window | Working |
| Near-camera view | Working |
| Experimental first-person view | Available through `F11` |
| Optional 5:4 cinema framing | Working |
| HMD-aware render-budget region | Implemented |
| 2D Overlay and world-space icons | Corrected for VR |

The ForceDLAA parameter-query and resolution-override approach is adapted from
[DLSSTweaks](https://github.com/emoose/DLSSTweaks) by emoose.

### Not yet implemented

- Ray tracing
- High Screen Space Reflections
- Proper VR loading-screen presentation
- Native support for canted displays

Headsets with canted displays currently require the manufacturer's
**Parallel Projection** mode.

### Tested configurations

| Headset | OpenXR path | Notes |
|---|---|---|
| Meta Quest 3 | Virtual Desktop | Primary development configuration |
| Pimax 5K | SteamVR OpenXR | Parallel Projection must be enabled |

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

- Some shadows may flicker in stereo.
- DLSS may occasionally initialize with incorrect left-eye history. Toggling
  Cinema Mode on and off by pressing `F10` twice may recover it.

## Requirements

- A legally owned copy of **The Witcher 3: Wild Hunt — Next-Gen Update**
- The latest official PC version of the game, currently
  [Patch 4.04](https://support.cdprojektred.com/en/witcher-3/pc/sp-technical/issue/2342/patch-4-04-download-now)
- The DirectX 12 version of the game
- A working OpenXR runtime
- A mouse and keyboard or gamepad
- An NVIDIA RTX GPU when using DLSS or DLAA

Older game versions and rollback branches are not supported.

## Installation

1. Download the latest package from the
   [Releases](https://github.com/tig3rmast3r/witcher3-vr/releases) page.
2. Extract the package into:

   ```text
   The Witcher 3\bin\x64_dx12\
   ```

3. Run `Witcher3VRLauncher.exe`.
4. Select the rendering mode and resolution.
5. Launch the game through the launcher.

If `witcher3vr.ini` does not exist, the launcher creates it automatically. The
first-run selection is **Stereo TAAU** at the **Ultra 2688 × 2784** preset;
nothing overwrites an existing INI.

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
| `F10` | Toggle 2D Cinema Mode |
| `F11` | Toggle the experimental First-Person view |

The First-Person view is experimental and uses a fixed camera placement rather
than a fully animation-driven body anchor. It is positioned far enough forward
to keep Geralt's head out of view during normal movement, but its placement may
not be ideal for every animation, contextual action, or horse-riding state.

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

The project owner's Ko-fi and GitHub Sponsors links will be added here before
the first public release.

## Credits

- [praydog / REFramework](https://github.com/praydog/REFramework) — DX12
  hooking and VR architecture reference
- [praydog / UEVR](https://github.com/praydog/UEVR) — additional VR
  implementation reference
- [emoose / DLSSTweaks](https://github.com/emoose/DLSSTweaks) — ForceDLAA
  parameter-query and resolution-override approach
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
requires a legally obtained copy of **The Witcher 3: Wild Hunt**.

The software is provided as-is and without warranty. Use it at your own risk
and keep backups of saves and configuration files.

This project is intended to comply with the
[CD PROJEKT RED Fan Content Guidelines](https://www.cdprojektred.com/en/fan-content).
