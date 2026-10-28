/*
 * A Qt Positioning source on Maemo Leste's location-daemon: the GPS fixes
 * gpsd has, as the daemon sends them on the system bus
 * (org.maemo.LocationDaemon), the way liblocation reads them.
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License version 2.1 or
 * (at your option) any later version.
 */
#ifndef LESTE_POSITIONSOURCE_H
#define LESTE_POSITIONSOURCE_H

#include <QGeoPositionInfoSource>
#include <QTimer>

class QDBusServiceWatcher;

class LestePositionSource : public QGeoPositionInfoSource
{
    Q_OBJECT

public:
    explicit LestePositionSource(QObject *parent = nullptr);
    ~LestePositionSource();

    // whether there is a location-daemon to start
    static bool available();

    QGeoPositionInfo lastKnownPosition(bool fromSatellitePositioningMethodsOnly = false) const override;
    PositioningMethods supportedPositioningMethods() const override;
    int minimumUpdateInterval() const override;
    Error error() const override;

public slots:
    void startUpdates() override;
    void stopUpdates() override;
    void requestUpdate(int timeout = 0) override;

private slots:
    void fixStatusChanged(uchar mode);
    void timeChanged(qlonglong sec, qlonglong nsec);
    void positionChanged(double lat, double lon, double alt);
    void courseChanged(double speed, double track, double climb);
    void accuracyChanged(double ept, double epv, double epd, double eps, double epc, double eph);
    void flush();
    void requestTimedOut();
    void daemonGone();

private:
    bool startDaemon();
    void stopDaemon();
    void setError(Error error);

    QGeoPositionInfo m_last;
    // what the daemon sent for the gpsd report being read
    int m_mode;
    QDateTime m_time;
    double m_lat, m_lon, m_alt;
    double m_speed, m_track, m_climb;
    double m_epv, m_eph;
    bool m_havePosition;

    bool m_running;         // startUpdates
    bool m_requested;       // requestUpdate
    int m_lockFd;           // shared lock: the daemon stays while it is held
    QTimer m_flush;         // one report's signals come together
    QTimer m_requestTimer;
    QDBusServiceWatcher *m_watcher;
    Error m_error;
};

#endif
