# The build environment for Maemo Leste's packages: Debian 12 (Devuan
# daedalus, which Leste is built on, has the same libraries), arm64, run on
# the PC through qemu's binfmt. Pure Maps' Qt 5 dependencies as its own
# development container installs them (.devcontainer/common in its tree).
FROM --platform=linux/arm64 debian:12
ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y --no-install-recommends \
	ca-certificates git build-essential cmake ninja-build pkg-config gettext \
	python3 dpkg-dev fakeroot file \
	zlib1g-dev libicu-dev libgl1-mesa-dev libgles2-mesa-dev libssl-dev \
	libcurl4-openssl-dev libjpeg-dev libpng-dev libwebp-dev libsqlite3-dev \
	libuv1-dev \
	qtbase5-dev qtbase5-dev-tools qtdeclarative5-dev qtdeclarative5-dev-tools \
	qtpositioning5-dev qtquickcontrols2-5-dev qttools5-dev qttools5-dev-tools \
	libqt5svg5-dev libqt5opengl5-dev \
	libs2-dev libabsl-dev \
	qml-module-io-thp-pyotherside qml-module-org-kde-kirigami2 \
	qml-module-qtpositioning qml-module-qtquick-controls2 \
	&& rm -rf /var/lib/apt/lists/*
# The positioning plugin (Leste's liblocation, from Leste's repository: not
# in Debian; it is not linked to the GLib, dbus-glib and GConf libraries it
# uses) and OSM Scout Server
RUN apt-get update && apt-get install -y --no-install-recommends \
	curl libglib2.0-dev libdbus-1-dev libdbus-glib-1-dev libgconf2-dev \
	libkyotocabinet-dev libmarisa-dev libmicrohttpd-dev liblz4-dev \
	libprotobuf-dev protobuf-compiler libboost-all-dev \
	&& rm -rf /var/lib/apt/lists/*
RUN set -e; cd /tmp; \
	u=https://maedevu.maemo.org/leste/pool/main/libl/liblocation; \
	curl -fsSL -o liblocation0_0.108-1+4m7_arm64.deb "$u/liblocation0_0.108-1%2b4m7_arm64.deb"; \
	curl -fsSL -o liblocation-dev_0.108-1+4m7_arm64.deb "$u/liblocation-dev_0.108-1%2b4m7_arm64.deb"; \
	printf '%s  %s\n' \
	    6ab23bd1989af40f56589a06d53145708d982a3545f708f99c5be99ff0eeb69a "liblocation0_0.108-1+4m7_arm64.deb" \
	    e47e8b04e03b1b338700522027ebec57b90cc5f40424eaae8a15eda651e813cf "liblocation-dev_0.108-1+4m7_arm64.deb" \
	    | sha256sum -c -; \
	for d in liblocation*.deb; do dpkg-deb -x "$d" /; done; rm liblocation*.deb
