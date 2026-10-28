#!/bin/bash
# Pure Maps for Maemo Leste (arm64), with what Devuan daedalus does not
# have: MapLibre Native Qt (the map renderer) and mapbox-gl-qml (its QML
# map item). Cross compiled on the PC with clang against an arm64 Debian 12
# root (Leste's libraries are Devuan daedalus', which are Debian 12's),
# made from leste/Dockerfile; the packages are made in that image, through
# qemu, with their dependencies from dpkg -S. The .deb files go to work/out.
#
#   bash leste/build.sh [maplibre-native-qt|mapbox-gl-qml|pure-maps ...]
set -euo pipefail
TOP=$(cd "$(dirname "$0")/.." && pwd)
. "$TOP/versions.sh"
W=$TOP/work
export LESTE_SYSROOT=$W/sysroot
IMAGE=pure-maps-leste-build
JOBS=${JOBS:-$(nproc)}
log() { echo ">> $*"; }

# The arm64 root to build against (and to make the packages in)
# (made again when the Dockerfile changes: Docker reuses its unchanged steps)
mkdir -p "$W"
if ! docker image inspect "$IMAGE" > /dev/null 2>&1 ||
    ! cmp -s "$TOP/leste/Dockerfile" "$W/image.Dockerfile"; then
	log "image $IMAGE"
	docker build --platform linux/arm64 -t "$IMAGE" "$TOP/leste" > "$W/image.log" 2>&1 ||
	    { tail -20 "$W/image.log"; exit 1; }
	cp "$TOP/leste/Dockerfile" "$W/image.Dockerfile"
	rm -f "$LESTE_SYSROOT/.done"
fi
if [ ! -f "$LESTE_SYSROOT/.done" ]; then
	rm -rf "${LESTE_SYSROOT:?}"
	log "sysroot"
	mkdir -p "$LESTE_SYSROOT"
	c=$(docker create --platform linux/arm64 "$IMAGE")
	docker export "$c" | tar -x -C "$LESTE_SYSROOT" --anchored --exclude=proc --exclude=sys --exclude=dev
	docker rm "$c" > /dev/null
	# links to /lib/... (libz.so, libdbus-1.so) would name the PC's files:
	# relative ones, within the root
	find "$LESTE_SYSROOT" -type l -lname '/*' -print0 |
	    while IFS= read -r -d '' l; do
		ln -sfnr "$LESTE_SYSROOT$(readlink "$l")" "$l"
	done
	# with the parts built before
	for r in "$W"/root/*/; do
		[ -d "$r" ] && cp -a "$r." "$LESTE_SYSROOT/"
	done
	touch "$LESTE_SYSROOT/.done"
fi
export PKG_CONFIG_SYSROOT_DIR=$LESTE_SYSROOT
export PKG_CONFIG_LIBDIR=$LESTE_SYSROOT/usr/lib/aarch64-linux-gnu/pkgconfig:$LESTE_SYSROOT/usr/lib/pkgconfig:$LESTE_SYSROOT/usr/share/pkgconfig

src() {
	local name=$1 url=$2 rev=$3 d=$W/src/$1
	[ -d "$d/.git" ] || git clone -q "$url" "$d"
	git -C "$d" cat-file -e "$rev^{commit}" 2> /dev/null || git -C "$d" fetch -q --tags origin
	git -C "$d" checkout -q -f "$rev"
	# only the pinned commits: maplibre-native's full history is over 6 GB
	git -C "$d" submodule update -q --init --recursive --depth 1 --jobs 8
	# what Leste needs changed (checkout -f above took the last run's back)
	local p
	for p in "$TOP/leste/patches/$name"/*.patch; do
		[ -f "$p" ] && git -C "$d" apply "$p"
	done
	return 0
}

# cmake_part NAME [cmake arguments]: build, install into work/root/NAME and
# into the sysroot (the parts after it build on it)
cmake_part() {
	local name=$1 b=$W/build/$1 r=$W/root/$1
	shift
	log "$name"
	cmake -S "$W/src/$name" -B "$b" -G Ninja \
	    -DCMAKE_TOOLCHAIN_FILE="$TOP/leste/cmake/toolchain.cmake" \
	    -DCMAKE_PROJECT_INCLUDE="$TOP/leste/cmake/hosttools.cmake" \
	    -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr \
	    -DCMAKE_INSTALL_LIBDIR=lib/aarch64-linux-gnu "$@" > "$W/$name.cmake.log" 2>&1 ||
	    { tail -30 "$W/$name.cmake.log"; exit 1; }
	cmake --build "$b" --parallel "$JOBS" > "$W/$name.build.log" 2>&1 ||
	    { grep -m25 -E "error:|Error [0-9]|FAILED" "$W/$name.build.log"; exit 1; }
	rm -rf "$r"
	DESTDIR=$r cmake --install "$b" > "$W/$name.install.log" 2>&1 ||
	    { tail -20 "$W/$name.install.log"; exit 1; }
	cp -a "$r/." "$LESTE_SYSROOT/"
}

# qmake for the arm64 root: the PC's qmake (Qt 5.15, as the root's) with
# the root's paths and mkspecs (qt.conf) and clang for arm64; the PC's Qt
# tools (moc, rcc, lrelease) are the host binaries
qmake_setup() {
	local q=$W/qmake m=$LESTE_SYSROOT/usr/lib/aarch64-linux-gnu/qt5/mkspecs
	mkdir -p "$q/leste-clang"
	cat > "$q/qt.conf" <<-CONF
	[Paths]
	Prefix=/usr
	Headers=/usr/include/aarch64-linux-gnu/qt5
	Libraries=/usr/lib/aarch64-linux-gnu
	ArchData=/usr/lib/aarch64-linux-gnu/qt5
	Data=/usr/share/qt5
	Binaries=/usr/lib/qt5/bin
	Plugins=/usr/lib/aarch64-linux-gnu/qt5/plugins
	Imports=/usr/lib/aarch64-linux-gnu/qt5/imports
	Qml2Imports=/usr/lib/aarch64-linux-gnu/qt5/qml
	Translations=/usr/share/qt5/translations
	Sysroot=$LESTE_SYSROOT
	SysrootifyPrefix=true
	HostPrefix=/usr
	HostBinaries=/usr/bin
	HostLibraries=/usr/lib
	HostData=$LESTE_SYSROOT/usr/lib/aarch64-linux-gnu/qt5
	TargetSpec=linux-clang
	HostSpec=linux-g++
	CONF
	cat > "$q/leste-clang/qmake.conf" <<-SPEC
	include($m/linux-clang/qmake.conf)
	QMAKE_CC = clang --target=aarch64-linux-gnu --sysroot=$LESTE_SYSROOT
	QMAKE_CXX = clang++ --target=aarch64-linux-gnu --sysroot=$LESTE_SYSROOT
	QMAKE_LINK = \$\$QMAKE_CXX
	QMAKE_LINK_SHLIB = \$\$QMAKE_CXX
	QMAKE_LINK_C = \$\$QMAKE_CC
	QMAKE_LINK_C_SHLIB = \$\$QMAKE_CC
	QMAKE_LFLAGS += -fuse-ld=lld
	QMAKE_AR = llvm-ar cqs
	QMAKE_STRIP = llvm-strip
	QMAKE_OBJCOPY = llvm-objcopy
	QMAKE_NM = llvm-nm -P
	SPEC
	echo "#include \"$m/linux-clang/qplatformdefs.h\"" > "$q/leste-clang/qplatformdefs.h"
	cat > "$q/qmake" <<-QMAKE
	#!/bin/sh
	if [ "\$1" = -query ]; then shift; exec /usr/bin/qmake-qt5 -query -qtconf "$q/qt.conf" "\$@"; fi
	exec /usr/bin/qmake-qt5 -qtconf "$q/qt.conf" -spec "$q/leste-clang" "\$@"
	QMAKE
	chmod 755 "$q/qmake"
}

# qmake_part NAME PROJECT [qmake arguments]: as cmake_part, for qmake
qmake_part() {
	local name=$1 pro=$2 b=$W/build/$1 r=$W/root/$1
	shift 2
	qmake_setup
	log "$name"
	mkdir -p "$b"
	(cd "$b" && "$W/qmake/qmake" "$W/src/$name/$pro" "$@") > "$W/$name.qmake.log" 2>&1 ||
	    { tail -30 "$W/$name.qmake.log"; exit 1; }
	make -C "$b" -j"$JOBS" > "$W/$name.build.log" 2>&1 ||
	    { grep -m25 -E "error:|Error [0-9]" "$W/$name.build.log"; exit 1; }
	rm -rf "$r"
	make -C "$b" install INSTALL_ROOT="$r" > "$W/$name.install.log" 2>&1 ||
	    { tail -20 "$W/$name.install.log"; exit 1; }
	# install paths from \$\$[QT_INSTALL_*] name the sysroot
	if [ -d "$r$LESTE_SYSROOT" ]; then
		local top=${LESTE_SYSROOT#/}
		cp -a "$r$LESTE_SYSROOT/." "$r/"
		rm -r "${r:?}/${top%%/*}"
	fi
	cp -a "$r/." "$LESTE_SYSROOT/"
}

# deb NAME PACKAGE VERSION SECTION "extra Depends" "Description" [files...]:
# a package of work/root/NAME (or only the files given, relative to it);
# the libraries the ELF files need are found with dpkg -S in the image
deb() {
	local name=$1 pkg=$2 version=$3 section=$4 extra=$5 desc=$6 d=$W/pkg/$2
	shift 6
	log "package $pkg"
	rm -rf "$d"
	mkdir -p "$d/DEBIAN"
	if [ $# -gt 0 ]; then
		(cd "$W/root/$name" && cp -a --parents "$@" "$d/")
	else
		cp -a "$W/root/$name/." "$d/"
	fi
	[ -d "$TOP/leste/extra/$pkg" ] && cp -a "$TOP/leste/extra/$pkg/." "$d/"
	find "$d" -type f ! -path "$d/DEBIAN/*" -exec sh -c \
	    'for f; do file -b "$f" | grep -q "^ELF" && llvm-strip --strip-unneeded "$f"; done; true' sh {} +
	cat > "$d/DEBIAN/control" <<-CONTROL
	Package: $pkg
	Version: $version-$PKGREV
	Architecture: arm64
	Section: $section
	Priority: optional
	Maintainer: Sebastian Matkovich <sebastian.matkovich@gmail.com>
	Homepage: https://github.com/smatkovi/pure-maps-ports
	Installed-Size: $(du -sk --exclude=DEBIAN "$d" | cut -f1)
	Depends: @SHLIBS@${extra:+, $extra}
	${RECOMMENDS:+Recommends: $RECOMMENDS}
	Description: $desc
	CONTROL
	sed -i '/^$/d' "$d/DEBIAN/control"
	mkdir -p "$W/out"
	docker run --rm -i -u "$(id -u):$(id -g)" --platform linux/arm64 -v "$W:/work" "$IMAGE" \
	    sh -s "$pkg" "$version-$PKGREV" < "$TOP/leste/mkdeb.sh"
}

want() { [ $# -eq 1 ] && return 0; case " ${*:2} " in *" $1 "*) return 0 ;; esac; return 1; }

if want maplibre-native-qt "$@"; then
	src maplibre-native-qt "$MAPLIBRE_URL" "$MAPLIBRE_REV"
	# as Pure Maps' own containers build it (Qt 5: no widgets, no QtLocation)
	cmake_part maplibre-native-qt -DMLN_QT_WITH_WIDGETS=OFF -DMLN_QT_WITH_LOCATION=OFF \
	    -DMLN_QT_WITH_INTERNAL_ICU=ON -DMLN_WITH_WERROR=OFF -DCMAKE_CXX_FLAGS="-include cstdint"
	# its tile cache is an SQLite database (Qt SQL)
	deb maplibre-native-qt libqmaplibre3 "$MAPLIBRE_VERSION" libs "libqt5sql5-sqlite" \
	    "MapLibre Native for Qt (the map renderer of Pure Maps)" \
	    usr/lib/aarch64-linux-gnu/libQMapLibre.so.3 usr/lib/aarch64-linux-gnu/libQMapLibre.so.$MAPLIBRE_VERSION
	deb maplibre-native-qt libqmaplibre-dev "$MAPLIBRE_VERSION" libdevel \
	    "libqmaplibre3 (= $MAPLIBRE_VERSION-$PKGREV), qtbase5-dev" \
	    "MapLibre Native for Qt: headers and CMake files" \
	    usr/include usr/lib/aarch64-linux-gnu/libQMapLibre.so usr/lib/aarch64-linux-gnu/cmake
fi
if want nemo-qml-plugin-dbus "$@"; then
	# Pure Maps' Kirigami pages talk D-Bus through it (the screen saver)
	src nemo-qml-plugin-dbus "$NEMODBUS_URL" "$NEMODBUS_REV"
	qmake_part nemo-qml-plugin-dbus dbus.pro VERSION="$NEMODBUS_VERSION"
	deb nemo-qml-plugin-dbus qml-module-nemo-dbus "$NEMODBUS_VERSION" libs "" \
	    "Nemo.DBus: D-Bus for QML (from Sailfish OS)" \
	    usr/lib/aarch64-linux-gnu/libnemodbus.so.2 usr/lib/aarch64-linux-gnu/libnemodbus.so.$NEMODBUS_VERSION \
	    usr/lib/aarch64-linux-gnu/qt5/qml/Nemo
fi
if want qtposition-leste "$@"; then
	# GPS for Qt programs from Leste's location-daemon (written here)
	rm -rf "$W/src/qtposition-leste"
	mkdir -p "$W/src"
	cp -a "$TOP/leste/qtposition-leste" "$W/src/"
	cmake_part qtposition-leste -DQT_INSTALL_PLUGINS=/usr/lib/aarch64-linux-gnu/qt5/plugins
	# liblocation is Leste's (dpkg -S in the image does not know it)
	deb qtposition-leste libqt5positioning5-plugin-leste "$QTPOSITIONLESTE_VERSION" libs \
	    "liblocation0 (>= 0.108), location-daemon" \
	    "Qt Positioning plugin for Maemo Leste's location-daemon (GPS through gpsd)"
fi
if want mapbox-gl-qml "$@"; then
	src mapbox-gl-qml "$MAPBOXGLQML_URL" "$MAPBOXGLQML_REV"
	cmake_part mapbox-gl-qml -DQT_VERSION_MAJOR=5 \
	    -DQT_INSTALL_QML=/usr/lib/aarch64-linux-gnu/qt5/qml
	deb mapbox-gl-qml qml-module-mapboxmap "$MAPBOXGLQML_VERSION" libs \
	    "libqmaplibre3 (= $MAPLIBRE_VERSION-$PKGREV)" \
	    "QML map item on MapLibre Native Qt (MapboxMap)"
fi
if want pure-maps "$@"; then
	src pure-maps "$PUREMAPS_URL" "$PUREMAPS_REV"
	cmake_part pure-maps -DFLAVOR=kirigami -Ds2_DIR="$TOP/leste/cmake/s2" \
	    -DPython3_EXECUTABLE=/usr/bin/python3
	# the QML modules its pages import; a speech engine for the voice
	RECOMMENDS="espeak-ng | flite" deb pure-maps pure-maps "$PUREMAPS_VERSION" utils \
	    "qml-module-mapboxmap (= $MAPBOXGLQML_VERSION-$PKGREV), qml-module-nemo-dbus (= $NEMODBUS_VERSION-$PKGREV), python3, qml-module-io-thp-pyotherside, qml-module-org-kde-kirigami2, qml-module-qtpositioning, libqt5positioning5-plugin-leste (>= $QTPOSITIONLESTE_VERSION), qml-module-qtquick2, qml-module-qtquick-controls2, qml-module-qtquick-dialogs, qml-module-qtquick-layouts, qml-module-qtquick-window2, qml-module-qtgraphicaleffects, qml-module-qt-labs-settings, qml-module-qtmultimedia, qml-module-qtsensors" \
	    "Maps and navigation (OpenStreetMap, online or with OSM Scout Server)"
fi
log "done: $W/out"
