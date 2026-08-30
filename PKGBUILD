# Maintainer: LimeiksYT

pkgname=anydesk-wayland-bridge
pkgver=0.1.1
pkgrel=1
pkgdesc='Incoming AnyDesk remote control bridge for Hyprland/Wayland'
arch=('x86_64')
url='https://aur.archlinux.org/packages/anydesk-wayland-bridge'
license=('MIT' 'GPL-2.0-or-later' 'LGPL-2.1-or-later')
install=anydesk-wayland-bridge.install

# anydesk-bin is intentionally a dependency rather than bundled here.
depends=(
  'anydesk-bin'
  'glib2'
  'hyprland'
  'kcoreaddons'
  'ki18n'
  'kpipewire'
  'kstatusnotifieritem'
  'kwindowsystem'
  'libx11'
  'libxcb'
  'libxtst'
  'pipewire'
  'polkit'
  'qt6-base'
  'qt6-declarative'
  'systemd'
  'xdg-desktop-portal'
  'xdg-desktop-portal-hyprland'
  'xorg-server-xvfb'
  'xorg-xrandr'
)
makedepends=(
  'cmake'
  'extra-cmake-modules'
  'gcc'
  'kdoctools'
  'knotifications'
  'pkgconf'
)
provides=('anydesk-wayland-fix')

source=(
  'https://download.kde.org/stable/xwaylandvideobridge/xwaylandvideobridge-0.4.0.tar.xz'
  'input_relay.c'
  'logind_shim.c'
  'run-bridge.sh'
  'anydesk-wayland-fixed'
  'anydesk-wayland-bridge.service'
  'anydesk-wayland-ready.path'
  'anydesk-wayland-bridge.conf'
  'anydesk-wayland-fixed.desktop'
  'anydesk-wayland-bridge.install'
  '70-anydesk-wayland-uinput.rules'
  'xwaylandvideobridge-anydesk.patch'
  'LICENSE'
)
sha256sums=(
  'ea72ac7b2a67578e9994dcb0619602ead3097a46fb9336661da200e63927ebe6'
  'f8d35b3ef7403cd23514550306a8a709975ffc028aeffb365fecf47a495452e1'
  '447dff3022bad73aab31ceb5d6a19538776794f6d62b4cdcc084115b8b2fccdc'
  '0d3126ef9c7aaee87471bf7ab490bcea41863cb811b59d9fcf989a6053a722bb'
  'c8a576ef6248983bde8c0d2fe7cabb3cf49f90bbaee60ddf431674c16ee50d78'
  '6069c23aa10f2c8a1ac22488f10a0f2995feb7d6dec89cb3fdf23ffb39aa7324'
  '2eed4f60b0a3744966f455d7f283737016d20b5ea695270aea0ec1f0011e2f04'
  'be45e3be96b050906d2b12f27f678c4f187400ed58b1e32b5697572d538a2635'
  '308c7ec0b6ca41148c472d209c9048e0ed441f5941038e644c535c1f6220f85e'
  '6cf0c5dd67965192ac0ed866bc84ba9cea9f43f8374a6e0490b5680c26cd912a'
  'b481f41f10cedbd34cfe7b99a3defe0c9667e55bd6800dfa1ff05b1998e46d81'
  '17c57ee2a10fd32e49fd328e4b6521498fe6dab7e288e29d17b2a364273b3047'
  'afecd3cd4a59618aaff9e68e406caa74015c87964d5332633ac73b8b115b41e7'
)

prepare() {
  cd "xwaylandvideobridge-0.4.0"
  patch -Np1 -i "$srcdir/xwaylandvideobridge-anydesk.patch"
}

build() {
  gcc $CFLAGS $CPPFLAGS \
    "$srcdir/input_relay.c" \
    -o input-relay \
    $LDFLAGS -lX11 -lXtst

  gcc $CFLAGS $CPPFLAGS -shared -fPIC \
    "$srcdir/logind_shim.c" \
    -o libanydesk-wayland-shim.so \
    $LDFLAGS $(pkg-config --cflags --libs gio-2.0) -ldl

  cmake -B build -S "xwaylandvideobridge-0.4.0" \
    -DBUILD_TESTING=OFF \
    -DQT_MAJOR_VERSION=6 \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX=/usr
  cmake --build build
}

package() {
  install -Dm755 input-relay \
    "$pkgdir/usr/lib/$pkgname/input-relay"
  install -Dm755 libanydesk-wayland-shim.so \
    "$pkgdir/usr/lib/$pkgname/libanydesk-wayland-shim.so"
  install -Dm755 build/bin/xwaylandvideobridge \
    "$pkgdir/usr/lib/$pkgname/xwaylandvideobridge"
  install -Dm755 "$srcdir/run-bridge.sh" \
    "$pkgdir/usr/lib/$pkgname/run-bridge.sh"

  install -Dm755 "$srcdir/anydesk-wayland-fixed" \
    "$pkgdir/usr/bin/anydesk-wayland-fixed"
  install -Dm644 "$srcdir/anydesk-wayland-fixed.desktop" \
    "$pkgdir/usr/share/applications/anydesk-wayland-fixed.desktop"

  install -Dm644 "$srcdir/anydesk-wayland-bridge.service" \
    "$pkgdir/usr/lib/systemd/user/anydesk-wayland-bridge.service"
  install -Dm644 "$srcdir/anydesk-wayland-ready.path" \
    "$pkgdir/usr/lib/systemd/system/anydesk-wayland-ready.path"
  install -Dm644 "$srcdir/anydesk-wayland-bridge.conf" \
    "$pkgdir/usr/lib/systemd/system/anydesk.service.d/wayland-bridge.conf"
  install -Dm644 "$srcdir/70-anydesk-wayland-uinput.rules" \
    "$pkgdir/usr/lib/udev/rules.d/70-anydesk-wayland-uinput.rules"

  install -Dm644 "$srcdir/LICENSE" \
    "$pkgdir/usr/share/licenses/$pkgname/LICENSE"
}
