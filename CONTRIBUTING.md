# Contributing

Thanks for helping test and improve AnyDesk Wayland Bridge.

## Before opening an issue

Please include your distro, Hyprland version, AnyDesk version, GPU/driver, monitor layout, resolution and scaling.

Useful diagnostics:

```bash
systemctl --user status anydesk-wayland-bridge.service
systemctl status anydesk.service
systemctl status anydesk-wayland-ready.path
hyprctl monitors
hyprctl devices
cat /tmp/anydesk-wayland-video.log
cat /tmp/anydesk-wayland-input.log
```

Do **not** post passwords, unattended-access credentials, tokens, private keys, or other secrets.

## Testing we especially need

- AMD and Intel GPUs
- multi-monitor setups
- fractional scaling
- 1080p and 4K displays
- non-US keyboard layouts
- newer AnyDesk versions

## Pull requests

1. Fork the repository and create a focused branch.
2. Keep changes small and explain the problem they solve.
3. Run:

```bash
makepkg -f
```

4. If `namcap` is available, also run:

```bash
namcap PKGBUILD
namcap anydesk-wayland-bridge-*.pkg.tar.zst
```

5. Do not commit generated `src/`, `pkg/`, or package archives.

The current project scope is **Hyprland/Wayland incoming AnyDesk control**. Support for other compositors is welcome as long as it does not break the tested Hyprland path.
