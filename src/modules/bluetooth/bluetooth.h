#pragma once

#include <QObject>
#include <QTimer>
#include <QtQml/qqmlregistration.h>

#include "objectmodel.h"

class BluetoothAdapter;

// Values match Quickshell.Bluetooth.
class BluetoothDeviceState : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("BluetoothDeviceState is an enum.")

public:
    enum Enum { Disconnected = 0, Connected = 1, Disconnecting = 2, Connecting = 3 };
    Q_ENUM(Enum)
};

class BluetoothAdapterState : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("BluetoothAdapterState is an enum.")

public:
    enum Enum { Disabled = 0, Enabled = 1, Enabling = 2, Disabling = 3, Blocked = 4 };
    Q_ENUM(Enum)
};

// One remote device, backed by an IOBluetoothDevice. macOS has no "trusted"
// or "blocked" flags: paired devices count as trusted and nothing is blocked;
// setting either is accepted and ignored.
class BluetoothDevice : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("BluetoothDevice comes from a BluetoothAdapter.")
    Q_PROPERTY(QString name READ name NOTIFY nameChanged)
    Q_PROPERTY(QString deviceName READ name NOTIFY nameChanged)
    Q_PROPERTY(QString address READ address CONSTANT)
    Q_PROPERTY(QString icon READ icon CONSTANT)
    Q_PROPERTY(bool connected READ connected NOTIFY stateChanged)
    Q_PROPERTY(int state READ state NOTIFY stateChanged)
    Q_PROPERTY(bool paired READ paired NOTIFY pairedChanged)
    Q_PROPERTY(bool bonded READ paired NOTIFY pairedChanged)
    Q_PROPERTY(bool pairing READ pairing NOTIFY pairingChanged)
    Q_PROPERTY(bool trusted READ paired WRITE setTrusted NOTIFY pairedChanged)
    Q_PROPERTY(bool blocked READ blocked WRITE setBlocked NOTIFY blockedChanged)
    Q_PROPERTY(bool batteryAvailable READ batteryAvailable NOTIFY batteryChanged)
    Q_PROPERTY(qreal battery READ battery NOTIFY batteryChanged)
    Q_PROPERTY(BluetoothAdapter* adapter READ adapter CONSTANT)

public:
    BluetoothDevice(void* ioDevice, BluetoothAdapter* adapter);
    ~BluetoothDevice() override;

    QString name() const { return mName; }
    QString address() const { return mAddress; }
    QString icon() const { return mIcon; }
    bool connected() const { return mState == BluetoothDeviceState::Connected; }
    int state() const { return mState; }
    bool paired() const { return mPaired; }
    bool pairing() const { return mPairing; }
    void setTrusted(bool) {}
    bool blocked() const { return false; }
    void setBlocked(bool) {}
    bool batteryAvailable() const { return mBattery >= 0; }
    qreal battery() const { return mBattery < 0 ? 0 : mBattery; }
    BluetoothAdapter* adapter() const { return mAdapter; }

    Q_INVOKABLE void connect();
    Q_INVOKABLE void disconnect();
    Q_INVOKABLE void pair();
    Q_INVOKABLE void cancelPair();
    Q_INVOKABLE void forget();

    // Re-reads name/connection/pairing/battery from IOBluetooth.
    void refresh();
    // Called from the Objective-C delegates.
    void connectFinished(bool ok);
    void pairFinished(bool ok);
    void* ioDevice() const { return mDevice; }

signals:
    void nameChanged();
    void stateChanged();
    void pairedChanged();
    void pairingChanged();
    void blockedChanged();
    void batteryChanged();

private:
    void setState(int s);

    void* mDevice;   // IOBluetoothDevice*, retained
    void* mDelegate; // connection/pairing delegate, retained
    void* mPairer = nullptr;
    BluetoothAdapter* mAdapter;
    QString mName, mAddress, mIcon;
    int mState = BluetoothDeviceState::Disconnected;
    bool mPaired = false;
    bool mPairing = false;
    qreal mBattery = -1;
};

// The Mac's single Bluetooth controller.
class BluetoothAdapter : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Use Bluetooth.defaultAdapter.")
    Q_PROPERTY(QString name READ name CONSTANT)
    Q_PROPERTY(QString adapterId READ name CONSTANT)
    Q_PROPERTY(bool enabled READ enabled WRITE setEnabled NOTIFY enabledChanged)
    Q_PROPERTY(int state READ state NOTIFY enabledChanged)
    Q_PROPERTY(bool discovering READ discovering WRITE setDiscovering NOTIFY discoveringChanged)
    Q_PROPERTY(bool discoverable READ discoverable WRITE setDiscoverable NOTIFY discoverableChanged)
    Q_PROPERTY(bool pairable READ pairable WRITE setPairable NOTIFY pairableChanged)
    Q_PROPERTY(ObjectModel* devices READ devices CONSTANT)

public:
    explicit BluetoothAdapter(QObject* parent = nullptr);
    ~BluetoothAdapter() override;

    QString name() const { return QStringLiteral("hci0"); }
    bool enabled() const { return mEnabled; }
    void setEnabled(bool e);
    int state() const { return mEnabled ? BluetoothAdapterState::Enabled : BluetoothAdapterState::Disabled; }
    bool discovering() const { return mDiscovering; }
    void setDiscovering(bool d);
    bool discoverable() const { return mDiscoverable; }
    void setDiscoverable(bool d);
    // macOS always accepts pairing requests while Bluetooth settings are open.
    bool pairable() const { return true; }
    void setPairable(bool) {}
    ObjectModel* devices() const { return mDevices; }

    // Called by the inquiry delegate and IOBluetooth notifications.
    void deviceSeen(void* ioDevice);
    void inquiryStopped();
    void refresh();
    void removeDevice(BluetoothDevice* d);

signals:
    void enabledChanged();
    void discoveringChanged();
    void discoverableChanged();
    void pairableChanged();

private:
    BluetoothDevice* find(void* ioDevice) const;

    ObjectModel* mDevices;
    QTimer mPoll;
    void* mInquiry = nullptr;
    void* mInquiryDelegate = nullptr;
    void* mConnectNotification = nullptr;
    void* mObserver = nullptr;
    bool mEnabled = false;
    bool mDiscovering = false;
    bool mDiscoverable = false;
};

class Bluetooth : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
    Q_PROPERTY(BluetoothAdapter* defaultAdapter READ defaultAdapter CONSTANT)
    Q_PROPERTY(ObjectModel* adapters READ adapters CONSTANT)
    Q_PROPERTY(ObjectModel* devices READ devices CONSTANT)

public:
    explicit Bluetooth(QObject* parent = nullptr);

    BluetoothAdapter* defaultAdapter() const { return mAdapter; }
    ObjectModel* adapters() const { return mAdapters; }
    ObjectModel* devices() const { return mAdapter->devices(); }

private:
    BluetoothAdapter* mAdapter;
    ObjectModel* mAdapters;
};
