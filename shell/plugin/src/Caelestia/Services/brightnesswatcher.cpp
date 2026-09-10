// SPDX-License-Identifier: GPL-3.0-only
#include "brightnesswatcher.hpp"

#include <qloggingcategory.h>
#include <QGuiApplication>
#include <qpa/qplatformnativeinterface.h>
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
    : QWaylandClientExtensionTemplate<KdeOutputDeviceRegistry>(23) {
}

void KdeOutputDeviceRegistry::kde_output_device_registry_v2_output(struct ::kde_output_device_v2* output) {
    auto* dev = new KdeOutputDevice(output);
    emit deviceAdded(dev);
}


KdeOutputManagement::KdeOutputManagement(QObject* parent)
    : QWaylandClientExtensionTemplate<KdeOutputManagement>(21) {
}


BrightnessWatcher::BrightnessWatcher(QObject* parent)
    : QObject(parent) {
    m_registry = new KdeOutputDeviceRegistry(this);
    connect(m_registry, &KdeOutputDeviceRegistry::deviceAdded, this, &BrightnessWatcher::onDeviceAdded);
    
    m_management = new KdeOutputManagement(this);

    // QtWayland requires us to explicitly check if the extension was successfully bound.
    // However, it binds asynchronously. If QGuiApplication is already running, it binds immediately.

    setupLegacyOutputDeviceDiscovery();
}

void BrightnessWatcher::handleRegistryGlobal(void* data, struct wl_registry* registry, uint32_t name,
                                              const char* interface, uint32_t version) {
    if (qstrcmp(interface, "kde_output_device_v2") != 0)
        return;

    auto* self = static_cast<BrightnessWatcher*>(data);
    uint32_t bindVersion = qMin<uint32_t>(version, static_cast<uint32_t>(kde_output_device_v2_interface.version));
    auto* object = static_cast<struct ::kde_output_device_v2*>(
        wl_registry_bind(registry, name, &kde_output_device_v2_interface, bindVersion));

    auto* dev = new KdeOutputDevice(object);
    self->onDeviceAdded(dev);
}

void BrightnessWatcher::handleRegistryGlobalRemove(void*, struct wl_registry*, uint32_t) {
    // Each output announces its own removal via kde_output_device_v2's "removed"
    // event (KdeOutputDevice::kde_output_device_v2_removed), which onDeviceAdded()
    // already wires up regardless of how the device was discovered. Nothing to do here.
}

void BrightnessWatcher::setupLegacyOutputDeviceDiscovery() {
    auto* native = qApp->platformNativeInterface();
    auto* display = native ? static_cast<struct wl_display*>(
                                  native->nativeResourceForIntegration(QByteArrayLiteral("wl_display")))
                            : nullptr;
    if (!display) {
        qCWarning(lcBrightnessWatcher) << "No wl_display available; cannot discover legacy kde_output_device_v2 outputs";
        return;
    }

    static const struct wl_registry_listener listener = {
        &BrightnessWatcher::handleRegistryGlobal,
        &BrightnessWatcher::handleRegistryGlobalRemove,
    };

    m_wlRegistry = wl_display_get_registry(display);
    wl_registry_add_listener(m_wlRegistry, &listener, this);
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
