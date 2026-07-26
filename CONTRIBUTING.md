# Contributing to Witcher 3 VR

Thanks for helping improve Witcher 3 VR.

## Bug reports

Use the
[bug report form](https://github.com/tig3rmast3r/witcher3-vr/issues/new?template=bug_report.yml)
and provide enough information to reproduce the problem.

Bug reports must be reproduced on the latest official PC version of the game.
Older versions and rollback branches are outside the support scope.

Third-party mods may be used, but mod compatibility is outside the current
support scope. Disable all other mods and reproduce the problem before opening
an issue. Reports for problems that occur only in a modded configuration will
be closed until the base project is stable enough for compatibility work.

For renderer bugs, include:

- GPU and driver version
- Headset, connection method, and OpenXR runtime
- Render mode and anti-aliasing backend
- Exact reproduction steps
- `witcher3vr.ini`
- A diagnostic log captured with **Diagnostic Logging** enabled
- A save file when the issue depends on a specific location or scene

Logs can contain system information and local file paths. Review attachments
before publishing them.

## Feature requests

Search existing issues before opening a request. Motion-controller support is
not currently planned, so duplicate requests for it will be closed.

## Code contributions

Discuss substantial renderer changes in an issue before starting a pull
request. Renderer fixes require in-game evidence; successful compilation alone
is not sufficient validation.

Before submitting:

1. Run `scripts/setup-dependencies.ps1`.
2. Configure and build the Release preset.
3. Run the automated tests.
4. Test the affected render routes in game.
5. Keep each change focused and document its runtime result.

```powershell
.\scripts\setup-dependencies.ps1
cmake --preset release
cmake --build --preset release
ctest --preset release
```

Do not commit:

- Game files or assets
- NVIDIA or game runtime DLLs
- Personal configuration
- Save files
- Logs, dumps, symbols, or reverse-engineering artifacts
- Compiled DLLs, executables, or release archives

By contributing code, you agree that your contribution may be distributed
under the repository's MIT License. Third-party code must retain its original
license and be recorded in `THIRD_PARTY_NOTICES.md`.
