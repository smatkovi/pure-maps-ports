/*
 * A Qt Positioning source on Maemo Leste's liblocation (see
 * positionsource.h).
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License version 2.1 or
 * (at your option) any later version.
 */
// GLib's headers before Qt's (Qt's "signals" keyword)
#include <gconf/gconf-client.h>
#include <location/location-gps-device.h>
#include <location/location-gpsd-control.h>

#include "positionsource.h"

#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QtMath>

namespace {
const char Service[] = "org.maemo.LocationDaemon";
// a cold GPS can take this long for its first fix
const int DefaultRequestTimeout = 120000;

// whether Leste's location privacy disclaimer was accepted (liblocation
// shows it, through location-ui, when a control starts before)
bool disclaimerAccepted()
{
    GConfClient *client = gconf_client_get_default();
    const gboolean accepted = gconf_client_get_bool(
        client, "/system/nokia/location/disclaimer-accepted", NULL);
    g_object_unref(client);
    return accepted;
}

// liblocation's intervals, the shortest one not shorter than msec
LocationGPSDControlInterval controlInterval(int msec)
{
    static const LocationGPSDControlInterval intervals[] = {
        LOCATION_INTERVAL_1S, LOCATION_INTERVAL_2S, LOCATION_INTERVAL_5S,
        LOCATION_INTERVAL_10S, LOCATION_INTERVAL_20S, LOCATION_INTERVAL_30S,
        LOCATION_INTERVAL_60S, LOCATION_INTERVAL_120S
    };
    for (LocationGPSDControlInterval i : intervals)
        if (msec <= int(i))
            return i;
    return LOCATION_INTERVAL_120S;
}
}

LestePositionSource::LestePositionSource(QObject *parent)
    : QGeoPositionInfoSource(parent)
    , m_control(nullptr)
    , m_controlStarted(false)
    , m_device(nullptr)
    , m_running(false)
    , m_requested(false)
    , m_error(NoError)
{
    m_requestTimer.setSingleShot(true);
    connect(&m_requestTimer, &QTimer::timeout, this, &LestePositionSource::requestTimedOut);

    // the device follows the daemon's signals (through GLib's main loop,
    // which is Qt's here) and starts with the last known fix
    m_device = LOCATION_GPS_DEVICE(g_object_new(LOCATION_TYPE_GPS_DEVICE, NULL));
    g_signal_connect(m_device, "changed", G_CALLBACK(deviceChanged), this);
    m_last = fix(false);
}

LestePositionSource::~LestePositionSource()
{
    releaseControl();
    g_signal_handlers_disconnect_by_data(m_device, this);
    g_object_unref(m_device);
}

bool LestePositionSource::available()
{
    QDBusConnection bus = QDBusConnection::systemBus();
    if (!bus.isConnected())
        return false;
    QDBusConnectionInterface *i = bus.interface();
    return i->isServiceRegistered(Service).value()
        || i->activatableServiceNames().value().contains(Service);
}

QGeoPositionInfo LestePositionSource::lastKnownPosition(bool) const
{
    return m_last;
}

QGeoPositionInfoSource::PositioningMethods LestePositionSource::supportedPositioningMethods() const
{
    return SatellitePositioningMethods;
}

int LestePositionSource::minimumUpdateInterval() const
{
    return 1000;
}

QGeoPositionInfoSource::Error LestePositionSource::error() const
{
    return m_error;
}

void LestePositionSource::setError(Error error)
{
    m_error = error;
    emit QGeoPositionInfoSource::error(error);
}

void LestePositionSource::startUpdates()
{
    if (startControl())
        m_running = true;
}

void LestePositionSource::stopUpdates()
{
    m_running = false;
    if (!m_requested)
        stopControl();
}

void LestePositionSource::requestUpdate(int timeout)
{
    if (timeout == 0)
        timeout = DefaultRequestTimeout;
    if (timeout < minimumUpdateInterval()) {
        emit updateTimeout();
        return;
    }
    if (!startControl())
        return;
    m_requested = true;
    m_requestTimer.start(timeout);
}

void LestePositionSource::requestTimedOut()
{
    m_requested = false;
    if (!m_running)
        stopControl();
    emit updateTimeout();
}

bool LestePositionSource::startControl()
{
    if (!m_control) {
        // a control holds liblocation's lock from its creation to its
        // end: the daemon and gpsd stay while one is there
        m_control = location_gpsd_control_get_default();
        if (!m_control) {
            setError(UnknownSourceError);
            return false;
        }
        g_signal_connect(m_control, "error-verbose", G_CALLBACK(controlError), this);
        g_signal_connect(m_control, "gpsd-running", G_CALLBACK(gpsdRunning), this);
    }
    m_error = NoError;
    if (!m_controlStarted) {
        g_object_set(m_control,
                     "preferred-method", LOCATION_METHOD_USER_SELECTED,
                     "preferred-interval", controlInterval(updateInterval()),
                     NULL);
        // asks for the disclaimer or for GPS to be switched on, if needed
        location_gpsd_control_start(m_control);
        m_controlStarted = true;
    }
    return true;
}

void LestePositionSource::stopControl()
{
    m_requestTimer.stop();
    m_requested = false;
    // Leste's liblocation takes the disclaimer dialog's state, when a
    // control starts while the dialog is still there, for the user's
    // answer, and that is "accepted" while it is unanswered. So a control
    // that may have shown the dialog stays (started) until the answer:
    // gpsd does not run before, so its lock costs nothing.
    if (m_controlStarted && !disclaimerAccepted())
        return;
    releaseControl();
}

void LestePositionSource::releaseControl()
{
    if (!m_control)
        return;
    g_signal_handlers_disconnect_by_data(m_control, this);
    if (m_controlStarted)
        location_gpsd_control_stop(m_control);
    g_object_unref(m_control);
    m_control = nullptr;
    m_controlStarted = false;
}

// (liblocation goes on with the control after its signals: it is let go
// after them)
void LestePositionSource::gpsdRunning(_LocationGPSDControl *, void *self)
{
    // gpsd started for a control kept for the disclaimer's answer
    LestePositionSource *s = static_cast<LestePositionSource *>(self);
    QMetaObject::invokeMethod(s, [s]() {
        if (!s->m_running && !s->m_requested)
            s->releaseControl();
    }, Qt::QueuedConnection);
}

void LestePositionSource::controlError(_LocationGPSDControl *, int error, void *self)
{
    LestePositionSource *s = static_cast<LestePositionSource *>(self);
    // the control does not start gpsd (a dialog was rejected): a start
    // after this one asks again
    s->m_controlStarted = false;
    QMetaObject::invokeMethod(s, [s]() {
        if (!s->m_running && !s->m_requested)
            s->releaseControl();
    }, Qt::QueuedConnection);
    switch (error) {
    case LOCATION_ERROR_USER_REJECTED_DIALOG:
    case LOCATION_ERROR_USER_REJECTED_SETTINGS:
    case LOCATION_ERROR_METHOD_NOT_ALLOWED_IN_OFFLINE_MODE:
        s->setError(AccessError);
        break;
    default:
        s->setError(ClosedError);
        break;
    }
}

void LestePositionSource::deviceChanged(_LocationGPSDevice *, void *self)
{
    static_cast<LestePositionSource *>(self)->fixChanged();
}

// the device's fix; only a current one (a 2D or 3D fix) if current
QGeoPositionInfo LestePositionSource::fix(bool current) const
{
    const LocationGPSDeviceFix *f = m_device->fix;
    if (!f || !(f->fields & LOCATION_GPS_DEVICE_LATLONG_SET)
        || !qIsFinite(f->latitude) || !qIsFinite(f->longitude))
        return QGeoPositionInfo();
    const bool fix3d = f->mode == LOCATION_GPS_DEVICE_MODE_3D;
    if (current && !fix3d && f->mode != LOCATION_GPS_DEVICE_MODE_2D)
        return QGeoPositionInfo();

    QGeoCoordinate c(f->latitude, f->longitude);
    if (fix3d && (f->fields & LOCATION_GPS_DEVICE_ALTITUDE_SET) && qIsFinite(f->altitude))
        c.setAltitude(f->altitude);
    QDateTime t;
    if ((f->fields & LOCATION_GPS_DEVICE_TIME_SET) && qIsFinite(f->time))
        t = QDateTime::fromMSecsSinceEpoch(qint64(f->time * 1000), Qt::UTC);
    else if (current)
        t = QDateTime::currentDateTimeUtc();
    else
        return QGeoPositionInfo();

    QGeoPositionInfo info(c, t);
    if ((f->fields & LOCATION_GPS_DEVICE_SPEED_SET) && qIsFinite(f->speed))
        info.setAttribute(QGeoPositionInfo::GroundSpeed, f->speed / 3.6);  // km/h
    if ((f->fields & LOCATION_GPS_DEVICE_TRACK_SET) && qIsFinite(f->track))
        info.setAttribute(QGeoPositionInfo::Direction, f->track);
    if (fix3d && (f->fields & LOCATION_GPS_DEVICE_CLIMB_SET) && qIsFinite(f->climb))
        info.setAttribute(QGeoPositionInfo::VerticalSpeed, f->climb);
    // Leste's liblocation passes gpsd's metres (Maemo 5's was centimetres)
    if (qIsFinite(f->eph))
        info.setAttribute(QGeoPositionInfo::HorizontalAccuracy, f->eph);
    if (fix3d && qIsFinite(f->epv))
        info.setAttribute(QGeoPositionInfo::VerticalAccuracy, f->epv);
    return info;
}

void LestePositionSource::fixChanged()
{
    // the device follows the daemon also when another program started it
    if (!m_control)
        return;
    const QGeoPositionInfo info = fix(true);
    if (!info.isValid())
        return;

    if (m_last.isValid()) {
        if (m_last.coordinate() == info.coordinate() && m_last.timestamp() == info.timestamp())
            return;
        // not more often than asked for
        if (!m_requested && updateInterval() > minimumUpdateInterval()
            && m_last.timestamp().msecsTo(info.timestamp()) < updateInterval())
            return;
    }
    m_last = info;

    if (m_requested) {
        m_requested = false;
        m_requestTimer.stop();
        if (!m_running)
            stopControl();
        emit positionUpdated(info);
    } else if (m_running) {
        emit positionUpdated(info);
    }
}
