# The upstream revisions the packages are built from
MAPLIBRE_URL=https://github.com/maplibre/maplibre-native-qt.git
MAPLIBRE_REV=v3.0.0
MAPLIBRE_VERSION=3.0.0
MAPBOXGLQML_URL=https://github.com/rinigus/mapbox-gl-qml.git
MAPBOXGLQML_REV=79e0ccc
MAPBOXGLQML_VERSION=3.0.0+git79e0ccc
PUREMAPS_URL=https://github.com/rinigus/pure-maps.git
PUREMAPS_REV=3.5.1
PUREMAPS_VERSION=3.5.1
# the packages' own revision (a change here, not upstream)
PKGREV=1
NEMODBUS_URL=https://github.com/sailfishos/nemo-qml-plugin-dbus.git
NEMODBUS_REV=2.1.27
NEMODBUS_VERSION=2.1.27
# the Qt Positioning plugin of this repository (leste/qtposition-leste)
QTPOSITIONLESTE_VERSION=0.1.0
# OSM Scout Server and libpostal as its packagings pin it (Flathub's)
OSMSCOUT_URL=https://github.com/rinigus/osmscout-server.git
OSMSCOUT_REV=3.1.7
OSMSCOUT_VERSION=3.1.7
LIBPOSTAL_URL=https://github.com/rinigus/pkg-libpostal.git
LIBPOSTAL_REV=69d0561b3d367c2db1dd7db1c4d652b865b8f83c
# Valhalla (OSM Scout Server's routing) as its packagings build it
# (rinigus' pkg-valhalla-lite 3.4.0.5, Flathub's valhalla-lite)
VALHALLA_URL=https://github.com/valhalla/valhalla.git
VALHALLA_REV=3.4.0
VALHALLA_VERSION=3.4.0
# OSM Scout Server's own package revision: 2 with Valhalla (the other
# packages stay at PKGREV)
OSMSCOUT_PKGREV=2
