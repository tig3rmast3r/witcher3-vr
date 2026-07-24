# Witcher 3 VR

Native OpenXR rendering for The Witcher 3 next-gen DX12 using REDengine
geometry stereo.

This repository was initialized from the validated V602 renderer. It is kept
local/private while packaging, dependency licensing and public documentation are
prepared. Its GitHub `origin` is configured, but nothing is pushed until the
owner authorizes a release.

## Build

```powershell
.\scripts\setup-dependencies.ps1
cmake --preset release
cmake --build --preset release
ctest --preset release
```

The optimized `Release` build produces one `dxgi.dll`. The launcher's
`Diagnostic Logging` checkbox enables `witcher3vr.log` and detailed runtime
probes in that same DLL; no separate diagnostic binary is required.

Packaging a validated milestone produces one ZIP and a SHA-256 manifest:

```powershell
.\scripts\package-release.ps1 -Version V602
```

See `docs/RELEASE_WORKFLOW.md` before tagging or packaging a milestone.

No open-source license has been selected yet. Do not publish this repository
until the owner has completed the redistribution and licensing review.
