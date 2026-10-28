/*
 * A Qt Positioning source on Maemo Leste's location-daemon (see
 * positionsource.h).
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License version 2.1 or
 * (at your option) any later version.
 */
#include "positionsource.h"

#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusMessage>
#include <QDBusServiceWatcher>
#include <QtMath>

#include <fcntl.h>
#include <sys/file.h>
#include <unistd.h>

namespace {
const char Service[] = "org.maemo.LocationDaemon";
const char Path[] = "/org/maemo/LocationDaemon";
// liblocation's lock: the daemon exits when nobody holds it shared
const char LockPath[] = "/run/lock/location-daemon.lock";
// gpsd's fix modes, as the daemon sends them
enum { ModeNotSeen, ModeNoFix, Mode2D, Mode3D };
// a cold GPS can take this long for its first fix
const int DefaultRequestTimeout = 120000;

QString interface(const char *name)
{
    return QString::fromLatin1(Service) + QLatin1Char('.') + QLatin1String(name);
}
}

LestePositionSource::LestePositionSource(QObject *parent)
    : QGeoPositionInfoSource(parent)
    , m_mode(ModeNotSeen)
    , m_lat(qQNaN()), m_lon(qQNaN()), m_alt(qQNaN())
    , m_speed(qQNaN()), m_track(qQNaN()), m_climb(qQNaN())
    , m_epv(qQNaN()), m_eph(qQNaN())
    , m_havePosition(false)
    , m_running(false)
    , m_requested(false)
    , m_lockFd(-1)
    , m_watcher(nullptr)
    , m_error(NoError)
{
    // the daemon sends one gpsd report as several signals, one after another
    m_flush.setSingleShot(true);
    m_flush.setInterval(50);
    connect(&m_flush, &QTimer::timeout, this, &LestePositionSource::flush);
    m_requestTimer.setSingleShot(true);
    connect(&m_requestTimer, &QTimer::timeout, this, &LestePositionSource::requestTimedOut);
}

LestePositionSource::~LestePositionSource()
{
    stopDaemon();
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
    if (startDaemon())
        m_running = true;
}

void LestePositionSource::stopUpdates()
{
    m_running = false;
    if (!m_requested)
        stopDaemon();
}

void LestePositionSource::requestUpdate(int timeout)
{
    if (timeout == 0)
        timeout = DefaultRequestTimeout;
    if (timeout < minimumUpdateInterval()) {
        emit updateTimeout();
        return;
    }
    if (!startDaemon())
        return;
    m_requested = true;
    m_requestTimer.start(timeout);
}

void LestePositionSource::requestTimedOut()
{
    m_requested = false;
    if (!m_running)
        stopDaemon();
    emit updateTimeout();
}

bool LestePositionSource::startDaemon()
{
    if (m_lockFd >= 0)
        return true;
    m_lockFd = ::open(LockPath, O_CREAT | O_RDONLY | O_CLOEXEC, 0666);
    if (m_lockFd >= 0 && ::flock(m_lockFd, LOCK_SH) != 0) {
        ::close(m_lockFd);
        m_lockFd = -1;
    }
    if (m_lockFd < 0) {
        setError(AccessError);
        return false;
    }

    QDBusConnection bus = QDBusConnection::systemBus();
    bus.connect(Service, Path, interface("Device"), "FixStatusChanged",
                this, SLOT(fixStatusChanged(uchar)));
    bus.connect(Service, Path, interface("Time"), "TimeChanged",
                this, SLOT(timeChanged(qlonglong, qlonglong)));
    bus.connect(Service, Path, interface("Position"), "PositionChanged",
                this, SLOT(positionChanged(double, double, double)));
    bus.connect(Service, Path, interface("Course"), "CourseChanged",
                this, SLOT(courseChanged(double, double, double)));
    bus.connect(Service, Path, interface("Accuracy"), "AccuracyChanged",
                this, SLOT(accuracyChanged(double, double, double, double, double, double)));

    m_watcher = new QDBusServiceWatcher(Service, bus, QDBusServiceWatcher::WatchForUnregistration, this);
    connect(m_watcher, &QDBusServiceWatcher::serviceUnregistered, this, &LestePositionSource::daemonGone);

    // a call to it starts it (D-Bus activation), as liblocation does
    bus.send(QDBusMessage::createMethodCall(Service, Path, Service, "start"));
    m_error = NoError;
    return true;
}

void LestePositionSource::stopDaemon()
{
    if (m_lockFd < 0)
        return;
    QDBusConnection bus = QDBusConnection::systemBus();
    bus.disconnect(Service, Path, interface("Device"), "FixStatusChanged",
                   this, SLOT(fixStatusChanged(uchar)));
    bus.disconnect(Service, Path, interface("Time"), "TimeChanged",
                   this, SLOT(timeChanged(qlonglong, qlonglong)));
    bus.disconnect(Service, Path, interface("Position"), "PositionChanged",
                   this, SLOT(positionChanged(double, double, double)));
    bus.disconnect(Service, Path, interface("Course"), "CourseChanged",
                   this, SLOT(courseChanged(double, double, double)));
    bus.disconnect(Service, Path, interface("Accuracy"), "AccuracyChanged",
                   this, SLOT(accuracyChanged(double, double, double, double, double, double)));
    delete m_watcher;
    m_watcher = nullptr;
    m_flush.stop();
    m_requestTimer.stop();
    m_requested = false;
    // closing it lets the lock go: the daemon (and gpsd's watch) end
    // when no other program holds one
    ::close(m_lockFd);
    m_lockFd = -1;
}

void LestePositionSource::daemonGone()
{
    // it went away while wanted (it crashed, or it was restarted):
    // start it again
    if (m_lockFd < 0)
        return;
    QTimer::singleShot(2000, this, [this]() {
        if (m_lockFd >= 0)
            QDBusConnection::systemBus().send(
                QDBusMessage::createMethodCall(Service, Path, Service, "start"));
    });
}

void LestePositionSource::fixStatusChanged(uchar mode)
{
    // the first signal of a report: the one before is complete
    if (m_flush.isActive()) {
        m_flush.stop();
        flush();
    }
    m_mode = mode;
    m_time = QDateTime();
    m_havePosition = false;
    m_lat = m_lon = m_alt = qQNaN();
    m_speed = m_track = m_climb = qQNaN();
    m_epv = m_eph = qQNaN();
    m_flush.start();
}

void LestePositionSource::timeChanged(qlonglong sec, qlonglong nsec)
{
    m_time = QDateTime::fromMSecsSinceEpoch(sec * 1000 + nsec / 1000000, Qt::UTC);
    m_flush.start();
}

void LestePositionSource::positionChanged(double lat, double lon, double alt)
{
    m_lat = lat;
    m_lon = lon;
    m_alt = alt;
    m_havePosition = true;
    m_flush.start();
}

void LestePositionSource::courseChanged(double speed, double track, double climb)
{
    m_speed = speed;
    m_track = track;
    m_climb = climb;
    m_flush.start();
}

void LestePositionSource::accuracyChanged(double, double epv, double, double, double, double eph)
{
    m_epv = epv;
    m_eph = eph;
    m_flush.start();
}

void LestePositionSource::flush()
{
    if (!m_havePosition || m_mode < Mode2D || !qIsFinite(m_lat) || !qIsFinite(m_lon))
        return;

    QGeoCoordinate c(m_lat, m_lon);
    if (m_mode == Mode3D && qIsFinite(m_alt))
        c.setAltitude(m_alt);
    const QDateTime t = m_time.isValid() ? m_time : QDateTime::currentDateTimeUtc();

    if (m_last.isValid()) {
        // gpsd reports a fix again with each message it reads
        if (m_last.coordinate() == c && m_last.timestamp() == t)
            return;
        // not more often than asked for
        if (!m_requested && updateInterval() > minimumUpdateInterval()
            && m_last.timestamp().msecsTo(t) < updateInterval())
            return;
    }

    QGeoPositionInfo info(c, t);
    if (qIsFinite(m_speed))
        info.setAttribute(QGeoPositionInfo::GroundSpeed, m_speed);
    if (qIsFinite(m_track))
        info.setAttribute(QGeoPositionInfo::Direction, m_track);
    if (qIsFinite(m_climb) && m_mode == Mode3D)
        info.setAttribute(QGeoPositionInfo::VerticalSpeed, m_climb);
    if (qIsFinite(m_eph))
        info.setAttribute(QGeoPositionInfo::HorizontalAccuracy, m_eph);
    if (qIsFinite(m_epv) && m_mode == Mode3D)
        info.setAttribute(QGeoPositionInfo::VerticalAccuracy, m_epv);
    m_last = info;

    if (m_requested) {
        m_requested = false;
        m_requestTimer.stop();
        if (!m_running)
            stopDaemon();
        emit positionUpdated(info);
    } else if (m_running) {
        emit positionUpdated(info);
    }
}
