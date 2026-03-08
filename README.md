# Maestro

A lightweight Windows system-tray utility that provides global hotkeys for controlling Spotify playback and per-app volume.

## Features

- **Global hotkeys** — control Spotify from any application without alt-tabbing
- **Per-app volume** — adjusts only Spotify's mixer volume, not system-wide
- **System tray** — lives quietly in your tray; right-click to exit
- **Single instance** — prevents duplicate copies from running
- **Zero UI** — no visible window, no console, minimal resource footprint

## Default Hotkeys

| Shortcut | Action |
|---|---|
| `Ctrl + Shift + F5` | Play / Pause |
| `Ctrl + Shift + Page Up` | Volume Up (Spotify only) |
| `Ctrl + Shift + Page Down` | Volume Down (Spotify only) |
| `Ctrl + Shift + →` | Next Track |
| `Ctrl + Shift + ←` | Previous Track |

## Requirements

- Windows 10 or later
- Spotify desktop app running
- Visual Studio 2022+ with the **Desktop development with C++** workload

## Building

1. Open `maestro.slnx` in Visual Studio.
2. Select a configuration (`Debug` or `Release`) and platform (`x64` recommended).
3. Build the solution (**Ctrl + Shift + B**).

## Usage

1. Start Spotify.
2. Run `maestro.exe`.
3. A small icon appears in your system tray.
4. Use the hotkeys from any application.
5. Right-click the tray icon → **Exit** to quit.

## License

This project is licensed under the MIT License — see the [LICENSE](LICENSE.md) file for details.
