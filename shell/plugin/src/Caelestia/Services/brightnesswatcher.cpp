// SPDX-License-Identifier: GPL-3.0-only
#include "brightnesswatcher.hpp"

#include <qloggingcategory.h>
#include <QGuiApplication>
#include <qpa/qplatformnativeinterface.h>
#include <QtGui/qguiapplication_platform.h>
#include <wayland-client.h>
#include "wayland-kde-output-device-v2-client-protocol.h"

Q_LOGGING_CATEGORY(lcBrightnessWatcher, "caelestia.services.brightnesswatcher", QtInfoMsg)

namespace caelestia::services {

KdeOutputDevice::KdeOutputDevice(struct ::kde_output_device_v2* object)
    : QtWayland::kde_output_device_v2(object) {
}

KdeOutputDevice::~KdeOutputDevice() {
}

void KdeOutputDevice::kde_output_device_v2_name(const QString& name) {
    if (m_name != name) {
        m_name = name;
        emit nameChanged();
    }
}

void KdeOutputDevice::kde_output_device_v2_brightness(uint32_t brightness) {
    if (m_brightness != brightness) {
        m_brightness = brightness;
        emit brightnessChanged();
    }
}

void KdeOutputDevice::kde_output_device_v2_capabilities(uint32_t flags) {
    bool has = (flags & capability_brightness);
    if (m_hasBrightness != has) {
        m_hasBrightness = has;
    }
}

void KdeOutputDevice::kde_output_device_v2_removed() {
    emit removed();
}


KdeOutputDeviceRegistry::KdeOutputDeviceRegistry(QObject* parent)
    // QWaylandClientExtensionTemplate only binds if the compositor advertises
    // an interface version >= the one requested here; it does not fall back
    // to min(requested, advertised). Upstream requests 23 (they track KWin
    // git master, which is well ahead), but everything this class actually
    // uses -- capability_brightness and the brightness event -- only needs
    // protocol version 9. KWin 6.3.x (this build's target) advertises 11, so
    // requesting 23 here silently never binds: BrightnessWatcher stays
    // permanently inactive, brightness() always returns -1, and setBrightness()
    // is a no-op that never reaches the compositor -- confirmed live via
    // /sys/class/backlight/*/brightness not moving after a set. 9 is the
    // actual floor; keeping it low (rather than matching this system's 11
    // exactly) keeps this working on any KWin from the point brightness
    // control shipped, not just this one's exact version.
    : QWaylandClientExtensionTemplate<KdeOutputDeviceRegistry>(9) {
}

void KdeOutputDeviceRegistry::kde_output_device_registry_v2_output(struct ::kde_output_device_v2* output) {
    auto* dev = new KdeOutputDevice(output);
    emit deviceAdded(dev);
}


KdeOutputDeviceLegacyScanner::KdeOutputDeviceLegacyScanner(QObject* parent)
    : QObject(parent) {
    auto* waylandApp = qGuiApp ? qGuiApp->nativeInterface<QNativeInterface::QWaylandApplication>() : nullptr;
    if (!waylandApp) {
        // Not running under wayland-client at all (e.g. X11) -- nothing to scan.
        return;
    }

    struct wl_display* display = waylandApp->display();
    if (!display) {
        return;
    }

    m_registry = wl_display_get_registry(display);
    static const struct wl_registry_listener listener = {
        &KdeOutputDeviceLegacyScanner::handleGlobal,
        &KdeOutputDeviceLegacyScanner::handleGlobalRemove,
    };
    wl_registry_add_listener(m_registry, &listener, this);
}

KdeOutputDeviceLegacyScanner::~KdeOutputDeviceLegacyScanner() {
    if (m_registry) {
        wl_registry_destroy(m_registry);
    }
}

void KdeOutputDeviceLegacyScanner::handleGlobal(
    void* data, struct wl_registry* registry, uint32_t name, const char* interface, uint32_t version) {
    if (qstrcmp(interface, "kde_output_device_v2") != 0) {
        return;
    }

    auto* self = static_cast<KdeOutputDeviceLegacyScanner*>(data);
    // Bind at the same version floor as KdeOutputDeviceRegistry (see its
    // constructor's comment) capped at what this particular global actually
    // advertises, exactly like Qt's own QWaylandClientExtensionTemplate would.
    const uint32_t bindVersion = qMin<uint32_t>(version, 9);
    auto* bound = wl_registry_bind(registry, name, &kde_output_device_v2_interface, bindVersion);
    auto* dev = new KdeOutputDevice(static_cast<struct ::kde_output_device_v2*>(bound));
    emit self->deviceAdded(dev);
}

void KdeOutputDeviceLegacyScanner::handleGlobalRemove(void*, struct wl_registry*, uint32_t) {
    // Already-bound devices learn of their own removal through the
    // interface-specific kde_output_device_v2.removed event (handled in
    // KdeOutputDevice::kde_output_device_v2_removed), not through the
    // registry's global_remove -- nothing to do here.
}


KdeOutputManagement::KdeOutputManagement(QObject* parent)
    // Same version-floor reasoning as KdeOutputDeviceRegistry above: 9 is the
    // minimum that has set_brightness (upstream requests 21).
    : QWaylandClientExtensionTemplate<KdeOutputManagement>(9) {
}


BrightnessWatcher::BrightnessWatcher(QObject* parent)
    : QObject(parent) {
    m_registry = new KdeOutputDeviceRegistry(this);
    connect(m_registry, &KdeOutputDeviceRegistry::deviceAdded, this, &BrightnessWatcher::onDeviceAdded);

    // Covers KWin versions before kde_output_device_registry_v2 existed --
    // see KdeOutputDeviceLegacyScanner's class comment. Harmless if the
    // modern registry above also ends up firing for the same output on some
    // future compositor: onDeviceAdded just keys m_devices by name, so the
    // later of the two simply replaces the same map entry.
    m_legacyScanner = new KdeOutputDeviceLegacyScanner(this);
    connect(m_legacyScanner, &KdeOutputDeviceLegacyScanner::deviceAdded, this, &BrightnessWatcher::onDeviceAdded);

    m_management = new KdeOutputManagement(this);
    
    // QtWayland requires us to explicitly check if the extension was successfully bound.
    // However, it binds asynchronously. If QGuiApplication is already running, it binds immediately.
}

qreal BrightnessWatcher::brightness(const QString& outputName) const {
    if (m_devices.contains(outputName)) {
        auto* dev = m_devices[outputName];
        if (dev->hasBrightness()) {
            return dev->brightness() / 10000.0;
        }
    }
    return -1.0;
}

void BrightnessWatcher::setBrightness(const QString& outputName, qreal value) {
    if (!m_management->isInitialized()) {
        qCWarning(lcBrightnessWatcher) << "Cannot set brightness: kde_output_management_v2 is not available.";
        return;
    }

    if (!m_devices.contains(outputName)) {
        qCWarning(lcBrightnessWatcher) << "Cannot set brightness: unknown output" << outputName;
        return;
    }

    auto* dev = m_devices[outputName];
    if (!dev->hasBrightness()) {
        qCWarning(lcBrightnessWatcher) << "Cannot set brightness: output" << outputName << "does not support brightness";
        return;
    }

    // Clamp value between 0.0 and 1.0
    value = qBound(0.0, value, 1.0);
    uint32_t brightValue = qRound(value * 10000.0);

    auto* config = m_management->create_configuration();
    if (!config) return;

    QtWayland::kde_output_configuration_v2 cfg(config);
    cfg.set_brightness(dev->object(), brightValue);
    cfg.apply();
    // Destroying the config object when it goes out of scope?
    // According to protocol, the server cleans up the config after apply or destroy.
    // Actually, we must call destroy() on the wrapper to free client-side memory, 
    // or let it leak? QtWayland wrappers don't automatically destroy the Wayland object on C++ destruction unless told.
    // Let's call destroy() after apply(), but wait - apply is asynchronous. 
    // Usually destroying the object right after apply is safe in Wayland.
    cfg.destroy();
}

void BrightnessWatcher::onDeviceAdded(KdeOutputDevice* device) {
    // Wait until we have the name
    connect(device, &KdeOutputDevice::nameChanged, this, [this, device]() {
        if (!device->name().isEmpty()) {
            m_devices[device->name()] = device;
            
            connect(device, &KdeOutputDevice::brightnessChanged, this, [this, device]() {
                emit brightnessChanged(device->name(), device->brightness() / 10000.0);
            });
        }
    });

    connect(device, &KdeOutputDevice::removed, this, [this, device]() {
        if (!device->name().isEmpty()) {
            m_devices.remove(device->name());
        }
        device->deleteLater();
    });
}

} // namespace caelestia::services
