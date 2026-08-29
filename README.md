# AnyDesk Wayland Bridge

[![Release](https://img.shields.io/github/v/release/RobloxExecutorV3/anydesk-wayland-bridge?display_name=tag&sort=semver)](https://github.com/RobloxExecutorV3/anydesk-wayland-bridge/releases/latest)
[![Stars](https://img.shields.io/github/stars/RobloxExecutorV3/anydesk-wayland-bridge?style=flat)](https://github.com/RobloxExecutorV3/anydesk-wayland-bridge/stargazers)
[![Issues](https://img.shields.io/github/issues/RobloxExecutorV3/anydesk-wayland-bridge)](https://github.com/RobloxExecutorV3/anydesk-wayland-bridge/issues)
![Arch Linux](https://img.shields.io/badge/Arch_Linux-supported-1793D1?logo=arch-linux&logoColor=white)
![Wayland](https://img.shields.io/badge/Wayland-Hyprland-FFBC00?logo=wayland&logoColor=black)


> Incoming AnyDesk remote control on **Hyprland/Wayland** without switching your desktop session to Xorg.

`anydesk-wayland-bridge` is an experimental compatibility layer that makes AnyDesk's Linux X11 backend work with a live Hyprland session.

It bridges the current Wayland desktop into a hidden X11 display and relays AnyDesk input back into Hyprland.

## What this fixes

Official AnyDesk for Linux supports outgoing connections from Wayland, but incoming remote control still expects an Xorg/X11 desktop.

This project works around that limitation by providing AnyDesk with a compatible virtual X11 session while the real desktop remains Wayland.

It currently handles:

- live Hyprland screen capture through PipeWire / XDG Desktop Portal
- incoming AnyDesk video through a hidden `Xvfb :99` display
- remote mouse movement and clicks
- remote keyboard input
- scrolling
- automatic initial cursor offset calibration
- AnyDesk's `systemd-logind` Wayland detection
- AnyDesk's primary-monitor requirement
- delayed AnyDesk startup until the virtual X server is ready

## Quick install

> [!NOTE]
> **AUR package is not published yet.** Until AUR registration is available, install directly from GitHub with `makepkg`.

```bash
git clone https://github.com/RobloxExecutorV3/anydesk-wayland-bridge.git
cd anydesk-wayland-bridge
makepkg -si
```

Then launch:

```bash
anydesk-wayland-fixed
```

### Prebuilt Arch package

A prebuilt package is also attached to each GitHub Release. Install `anydesk-bin` first, then install the bridge package:

```bash
yay -S anydesk-bin
curl -LO https://github.com/RobloxExecutorV3/anydesk-wayland-bridge/releases/download/v0.1.0/anydesk-wayland-bridge-0.1.0-1-x86_64.pkg.tar.zst
sudo pacman -U anydesk-wayland-bridge-0.1.0-1-x86_64.pkg.tar.zst
```

Then run:

```bash
anydesk-wayland-fixed
```

> `pacman -U` does not resolve AUR dependencies by itself, which is why `anydesk-bin` is installed with an AUR helper first.

On first launch, Polkit may ask once for permission to enable the system AnyDesk readiness path.

When the AUR package goes live, installation will become:

```bash
yay -S anydesk-wayland-bridge
```

`anydesk-bin` is declared as a dependency, so an AUR helper will pull AnyDesk automatically.

## How it works

```text
Hyprland / Wayland desktop
        │
        ├── XDG Desktop Portal + PipeWire
        │        │
        │        ▼
        │  patched xwaylandvideobridge
        │        │
        ▼        ▼
      Xvfb :99 ───────────────► AnyDesk X11 backend
        ▲                             │
        │                             │ remote input
        │                             ▼
        └──── XRecord ── input-relay ── /dev/uinput
                                      │
                                      ▼
                                   Hyprland
```

A small `LD_PRELOAD` shim also intercepts the `systemd-logind` properties queried by AnyDesk and reports:

```text
Session.Display = :99
Session.Type    = x11
```

Only the AnyDesk service sees those substituted values. The actual desktop remains a Wayland session.

## Components

### `libanydesk-wayland-shim.so`

Intercepts the GLib D-Bus calls AnyDesk uses to inspect the current logind session.

It substitutes the virtual display and X11 session type so AnyDesk does not reject the connection with:

```text
Remote display server is not supported (e.g. Wayland)
```

### Patched `xwaylandvideobridge`

The KDE bridge is patched to:

- initialize the portal capture automatically
- expose the captured desktop at `0,0`
- accept input while running in AnyDesk bridge mode
- fill the virtual X11 framebuffer

### `input-relay`

Uses XRecord to observe input delivered by AnyDesk to `Xvfb :99`, then recreates it using a virtual `/dev/uinput` keyboard/mouse visible to Hyprland.

The relay also automatically calibrates the cursor through the bottom-right screen edge when a new remote session begins. This fixes the common initial pointer-offset problem without requiring the remote user to manually move the cursor into a corner.

### systemd integration

`anydesk-wayland-bridge.service` starts the user-side bridge.

`anydesk-wayland-ready.path` starts AnyDesk only after `/tmp/.X11-unix/X99` exists, avoiding a boot race where AnyDesk starts before the virtual display.

## Usage

Normal use is just:

```bash
anydesk-wayland-fixed
```

Check the bridge:

```bash
systemctl --user status anydesk-wayland-bridge.service
```

Check AnyDesk:

```bash
anydesk --get-status
anydesk --get-id
```

The virtual display should report a primary output:

```bash
DISPLAY=:99 xrandr
```

Expected shape:

```text
screen connected primary 2560x1440+0+0
```

The actual resolution is detected from the current Hyprland monitor when the bridge starts.

## Troubleshooting

### `Remote display server is not supported`

Check that AnyDesk's service process is being spawned with `DPY: :99`:

```bash
sudo tail -n 100 /var/log/anydesk.trace
```

You should see something similar to:

```text
Spawning new control process with: UID: ..., DPY: :99
```

Also verify:

```bash
DISPLAY=:99 xrandr
```

The output must be marked `primary`.

### Remote screen is black

Check the video bridge log:

```bash
cat /tmp/anydesk-wayland-video.log
```

Also make sure these services are available in the Wayland session:

```bash
systemctl --user status xdg-desktop-portal.service
systemctl --user status xdg-desktop-portal-hyprland.service
```

### Mouse does not control Hyprland

Check:

```bash
cat /tmp/anydesk-wayland-input.log
hyprctl devices
```

You should see a virtual device named similar to:

```text
AnyDesk Wayland Bridge
```

### Cursor starts in the wrong position

Current versions automatically perform edge calibration on the first remote movement after a new/idle session:

```text
pointer edge-calibrated via <bottom-right> -> <remote position>
```

Check `/tmp/anydesk-wayland-input.log` if the offset remains.

## Requirements

This package currently targets:

- Arch Linux
- Hyprland
- AnyDesk Linux
- PipeWire
- `xdg-desktop-portal-hyprland`
- systemd

Other Wayland compositors are not currently supported because input injection and monitor discovery use Hyprland-specific commands.

## Security note

This project intentionally creates a virtual input device and uses an `LD_PRELOAD` shim for the AnyDesk service. Review the source before installing if you are using it on a sensitive machine.

The shim only changes the session properties observed by AnyDesk; it does not convert the real Wayland session into X11.

## Status

Tested with:

- Arch Linux
- Hyprland
- AnyDesk 8.0.4
- 2560x1440 / 180 Hz single-monitor setup

The package build has also been validated with `makepkg` and `namcap`.

Multi-monitor setups still need broader testing.

## Contributors

- [ItzShelfie](https://github.com/ItzShelfie)

## License

The bridge-specific code is MIT licensed. The patched `xwaylandvideobridge` remains under its upstream KDE licenses (GPL/LGPL as applicable).

## Disclaimer

This is an unofficial community workaround and is not affiliated with or supported by AnyDesk Software GmbH or KDE.

Test reports and fixes are welcome.
