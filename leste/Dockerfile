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
