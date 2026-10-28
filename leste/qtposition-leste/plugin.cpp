/*
 * The "leste" Qt Positioning plugin: the default position source where
 * Maemo Leste's location-daemon is (other plugins are tried elsewhere).
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License version 2.1 or
 * (at your option) any later version.
 */
#include "positionsource.h"

#include <QGeoPositionInfoSourceFactory>
#include <QObject>

class LestePositionFactory : public QObject, public QGeoPositionInfoSourceFactory
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "org.qt-project.qt.position.sourcefactory/5.0" FILE "plugin.json")
    Q_INTERFACES(QGeoPositionInfoSourceFactory)

public:
    QGeoPositionInfoSource *positionInfoSource(QObject *parent) override
    {
        return LestePositionSource::available() ? new LestePositionSource(parent) : nullptr;
    }

    QGeoSatelliteInfoSource *satelliteInfoSource(QObject *) override
    {
        return nullptr;
    }

    QGeoAreaMonitorSource *areaMonitor(QObject *) override
    {
        return nullptr;
    }
};

#include "plugin.moc"
