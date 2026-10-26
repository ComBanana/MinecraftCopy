# MinecraftCopy

A small voxel sandbox built in C++17 with OpenGL. The first release is `0.0.1-alpha`.

## Features

- Finite and infinite worlds
- Block placement and breaking
- Keyboard and mouse controls
- A Windows launcher with world-mode selection

## Build on Windows

Install CMake, Git, and Visual Studio Build Tools with the C++ workload and Windows SDK. Configuration downloads the pinned WebView2 SDK package, so the first configure needs network access.

```powershell
cmake -S . -B build "-DMINECRAFTCOPY_VERSION:STRING=0.0.1-alpha"
cmake --build build --config Release --parallel 1
ctest --test-dir build -C Release --output-on-failure
```

Run `build/bin/Release/MinecraftCopyLauncher.exe`. The launcher requires the Microsoft Edge WebView2 Evergreen Runtime. To start the game directly, run `build/bin/Release/MinecraftCopy.exe`; pass `--world=infinite` for an infinite world or `--world=finite` for a finite world. `--version` prints the game build version.

## Install and update

Download `MinecraftCopyLauncher-windows-x64.zip` from the [GitHub Releases](https://github.com/ComBanana/MinecraftCopy/releases) page, extract the complete ZIP, and run `MinecraftCopyLauncher.exe`. On first launch, install the game from the launcher. The launcher checks published GitHub releases, compares semantic versions, and downloads the Windows game files into your local application data folder. It verifies each file against the release manifest's SHA-256 hash before activating it. The repository must be public for downloads to work.

The launcher updates the game. Updating the launcher itself requires downloading and replacing its ZIP package. To publish the first release, commit and push the source changes (including `.github/workflows/release.yml`), make the repository public, then push the tag `v0.0.1-alpha`. GitHub Actions builds the Windows x64 release, runs the test suite, creates the launcher ZIP and game manifest, and publishes the prerelease. You can create the same release files locally with:

```powershell
./scripts/package-release.ps1 -Version 0.0.1-alpha
```

This creates ignored release files under `build/release-assets/v0.0.1-alpha`.

## Controls

- `W`, `A`, `S`, `D`: move
- Mouse: look around
- `Space`: jump
- Left mouse button: break a block
- Right mouse button: place a grass block
- `F5`: switch world modes
- `Escape`: quit

The finite world is 25 by 25 chunks. Infinite mode streams nearby chunks. Terrain is deterministic for the configured seed.

## License

MinecraftCopy is released under the MIT License. See [LICENSE](LICENSE) and [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).

MinecraftCopy is an independent project and is not affiliated with or endorsed by Mojang Studios or Microsoft.
