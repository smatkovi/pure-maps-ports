# Pure Maps for Maemo Leste and FreeBSD

Packages of [Pure Maps](https://github.com/rinigus/pure-maps) (maps and
navigation on OpenStreetMap data) and
[OSM Scout Server](https://github.com/rinigus/osmscout-server) (its offline
maps and search) for **Maemo Leste** (arm64, Hildon) and **FreeBSD**
(arm64, MeeGo's home and Hildon; see [FreeBSD](#freebsd)), with what the
distributions do not have. For Leste:

| Package (Leste) | What it is |
| --- | --- |
| `pure-maps` | Pure Maps 3.5.1, the Kirigami version, with a Hildon menu entry |
| `qml-module-mapboxmap` | [mapbox-gl-qml](https://github.com/rinigus/mapbox-gl-qml), the QML map item |
| `libqmaplibre3`, `libqmaplibre-dev` | [MapLibre Native Qt](https://github.com/maplibre/maplibre-native-qt) 3.0.0, the map renderer |
| `qml-module-nemo-dbus` | [Nemo.DBus](https://github.com/sailfishos/nemo-qml-plugin-dbus) 2.1.27 (D-Bus for QML) |
| `libqt5positioning5-plugin-leste` | GPS for Qt programs from Leste's location-daemon (written here) |
| `osmscout-server` | OSM Scout Server 3.1.7, the Kirigami version, with a Hildon menu entry |

Everything else Pure Maps needs (Kirigami, PyOtherSide, Qt's QML modules,
s2geometry) comes from Leste's own repositories; `apt` installs it.

The packages are in the [releases](https://github.com/smatkovi/pure-maps-ports/releases).
On Leste:

```sh
sudo apt install ./libqmaplibre3_*.deb ./qml-module-mapboxmap_*.deb \
    ./qml-module-nemo-dbus_*.deb ./libqt5positioning5-plugin-leste_*.deb \
    ./pure-maps_*.deb
```

Pure Maps is then in the Hildon menu. Its default map (OpenFreeMap) and
search (Photon) work without an account. Its default router, Stadia Maps,
needs an API key of your own (entered in Pure Maps' preferences). OSRM
routes without one.

## Offline maps: OSM Scout Server

```sh
sudo apt install ./osmscout-server_*.deb
```

Its menu entry opens the server's settings and map manager. A first-run
wizard chooses the languages for search and then the countries to download.
The data is large: for Austria about 2.8 GB of vector tiles, 330 MB for
search and 26 MB of address-parser data, plus 106 MB of the address parser's
global data once. Pure Maps uses the server when its "Offline" profile is
selected (or for single services in "Mixed"). The server starts by itself
(D-Bus activation) and listens on `127.0.0.1:8553`.

This build has offline maps (vector tiles for MapLibre) and offline search.
Offline routing (Valhalla) and raster tiles (Mapnik) are not in it yet. Pure
Maps' "Offline" profile routes through the server too, so it cannot route
yet. Its "Mixed" profile can take the map and search from the server and
route online, for example with OSRM. [libpostal](https://github.com/openvenues/libpostal)
(the address parser, in rinigus' pinned 2017 version) is linked into the
server statically.

## FreeBSD

The FreeBSD packages (FreeBSD 15, arm64) are in the same releases:
`maplibre-native-qt`, `mapbox-gl-qml`, `nemo-qml-plugin-dbus` and `pure-maps`
in Pure Maps' release, `osmscout-server` in its own. They are built in
[pro1x-maemo-leste](https://github.com/smatkovi/pro1x-maemo-leste)'s
FreeBSD cross build (`freebsd/hildon/apps.sh`), where FreeBSD's own
packages provide the rest (Kirigami, PyOtherSide, s2, libpostal,
kyotocabinet, marisa-trie, libmicrohttpd). Install their dependencies
from FreeBSD's repository, then the packages:

```sh
pkg install python312 pyotherside-qt5 kf5-kirigami2 s2 qt5-location \
    qt5-quickcontrols qt5-quickcontrols2 qt5-graphicaleffects qt5-multimedia \
    qt5-sensors qt5-sqldrivers-sqlite3 espeak-ng kyotocabinet marisa-trie \
    libmicrohttpd libpostal
pkg add maplibre-native-qt-3.0.0.pkg nemo-qml-plugin-dbus-2.1.27.pkg \
    mapbox-gl-qml-3.0.0.79.pkg pure-maps-3.5.1.pkg osmscout-server-3.1.7.pkg
```

Both apps have menu entries for MeeGo's home (the normal XDG ones) and for
Hildon. They start `pure-maps-mobile` and `osmscout-server-mobile`, with
Kirigami's touch layout. `pure-maps-mobile` also preloads libpython:
FreeBSD's Python modules are not linked to libpython, and PyOtherSide
loads libpython only for itself, so `math` and `datetime` would not load.
Tested in a FreeBSD 15.1 arm64 VM: Pure Maps shows the map, OSM Scout
Server's settings open and the server listens on port 8553. Positioning
on FreeBSD is Qt's own (GeoClue 2 if it runs).

## What is different on Leste

- **GPS.** Leste reads the GPS through gpsd and its own location-daemon,
  which sends the fixes on the system bus; programs use them through
  liblocation, as on Maemo 5. There is no Qt Positioning plugin for it, and
  GeoClue has no gpsd source, so this repository has one:
  `leste/qtposition-leste`. It is a liblocation client like Leste's own
  programs. It keeps to the location settings, the first time it shows
  Leste's location privacy disclaimer, and the daemon and gpsd stop when no
  program wants a position. It turns liblocation's fixes into Qt positions
  with speed, direction and accuracy. Where it finds no location-daemon it
  makes no source, so Qt uses the next plugin.

  Leste's liblocation (0.108) has two quirks the plugin works around.
  `liblocation.so.0` is not linked to the GLib, dbus-glib and GConf
  libraries it uses, so the plugin links them. And when a liblocation
  client starts while the disclaimer dialog is still open, liblocation
  takes the dialog's unanswered state for "accepted". So the plugin keeps
  its control until the dialog is answered instead of making a new one,
  and gpsd does not run before that. Other programs that start
  liblocation while the dialog is open can still set the disclaimer to
  accepted.
- **Screen blanking.** While Pure Maps navigates it keeps the display on.
  Upstream does this through `org.freedesktop.ScreenSaver`, which Leste does
  not have. The Leste package asks MCE (`req_display_blanking_pause`)
  instead (`leste/patches/pure-maps`).
- **Hildon.** A menu entry in `/usr/share/applications/hildon` starts
  `pure-maps-hildon`, which sets Kirigami's touch layout
  (`QT_QUICK_CONTROLS_MOBILE=1`) and the Material style. Both can be
  overridden from the environment.

## Building

The Leste packages are cross compiled on an x86-64 Linux PC with clang
against an arm64 Debian 12 root. Leste's libraries are Devuan daedalus',
which are Debian 12's. The root is made from `leste/Dockerfile` (Docker
with qemu-user for arm64). The `.deb` files are made in that root, with
their dependencies found by `dpkg -S`:

```sh
bash leste/build.sh            # everything, packages in work/out
bash leste/build.sh pure-maps  # one part
```

Needed on the PC: Docker with arm64 emulation (`qemu-user-static`
binfmt), clang and lld, CMake, Ninja, and Qt 5.15's host tools (moc, rcc,
lrelease, qmake). The upstream revisions are in `versions.sh`.

## Licenses

The packaged programs keep their own licenses:

- Pure Maps: GPL-3.0
- MapLibre Native Qt: BSD-2-Clause, with the parts listed in its `LICENSES`
- mapbox-gl-qml: LGPL-3.0
- Nemo.DBus: LGPL-2.1

The positioning plugin and the build scripts of this repository are
LGPL-2.1-or-later.
