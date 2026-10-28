/*
 * A Qt Positioning source on Maemo Leste's liblocation: the GPS fixes of
 * gpsd, which Leste's location-daemon sends on the system bus. Through
 * liblocation the source keeps to Leste's location settings (the method,
 * GPS on or off), Leste asks for the privacy disclaimer the first time,
 * and the daemon and gpsd stop when no program wants a position.
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License version 2.1 or
 * (at your option) any later version.
 */
#ifndef LESTE_POSITIONSOURCE_H
#define LESTE_POSITIONSOURCE_H

#include <QGeoPositionInfoSource>
#include <QTimer>

struct _LocationGPSDControl;
struct _LocationGPSDevice;

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
    void requestTimedOut();

private:
    static void deviceChanged(_LocationGPSDevice *device, void *self);
    static void controlError(_LocationGPSDControl *control, int error, void *self);
    static void gpsdRunning(_LocationGPSDControl *control, void *self);

    QGeoPositionInfo fix(bool current) const;
    void fixChanged();
    bool startControl();
    void stopControl();
    void releaseControl();
    void setError(Error error);

    _LocationGPSDControl *m_control;    // while positions are wanted
    bool m_controlStarted;
    _LocationGPSDevice *m_device;
    QGeoPositionInfo m_last;
    bool m_running;         // startUpdates
    bool m_requested;       // requestUpdate
    QTimer m_requestTimer;
    Error m_error;
};

#endif
