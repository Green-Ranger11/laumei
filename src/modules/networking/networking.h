#pragma once

#include <QObject>
#include <QSet>
#include <QTimer>
#include <QtQml/qqmlregistration.h>

#include "objectmodel.h"

class WifiDevice;

class DeviceType : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("DeviceType is an enum.")

public:
    enum Enum { None = 0, Wifi = 1, Wired = 2 };
    Q_ENUM(Enum)
};

class ConnectionState : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("ConnectionState is an enum.")

public:
    enum Enum { Unknown = 0, Connecting, Connected, Disconnecting, Disconnected };
    Q_ENUM(Enum)
};

class WifiSecurityType : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("WifiSecurityType is an enum.")

public:
    enum Enum {
        Wpa3SuiteB192 = 0, Sae, Wpa2Eap, Wpa2Psk, WpaEap, WpaPsk,
        StaticWep, DynamicWep, Leap, Owe, Open, Unknown
    };
    Q_ENUM(Enum)
};

class ConnectionFailReason : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("ConnectionFailReason is an enum.")

public:
    enum Enum { Unknown = 0, NoSecrets, WifiClientDisconnected, WifiClientFailed, WifiAuthTimeout, WifiNetworkLost };
    Q_ENUM(Enum)
};

// One SSID in range (the strongest BSS seen for it).
class WifiNetwork : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("WifiNetwork comes from WifiDevice.networks.")
    Q_PROPERTY(QString name READ name CONSTANT)
    Q_PROPERTY(qreal signalStrength READ signalStrength NOTIFY signalStrengthChanged)
    Q_PROPERTY(bool connected READ connected NOTIFY connectedChanged)
    Q_PROPERTY(bool known READ known NOTIFY knownChanged)
    Q_PROPERTY(int security READ security NOTIFY securityChanged)
    Q_PROPERTY(bool stateChanging READ stateChanging NOTIFY stateChangingChanged)

public:
    WifiNetwork(const QString& ssid, WifiDevice* device);

    QString name() const { return mName; }
    qreal signalStrength() const { return mSignal; }
    bool connected() const { return mConnected; }
    bool known() const { return mKnown; }
    int security() const { return mSecurity; }
    bool stateChanging() const { return mStateChanging; }

    // Saved or open networks join with what macOS already has.
    Q_INVOKABLE void connect();
    Q_INVOKABLE void connectWithPsk(const QString& psk);
    Q_INVOKABLE void disconnect();
    // Removing a saved network needs an administrator password on macOS.
    Q_INVOKABLE void forget();

    void update(qreal signal, int security, bool connected, bool known);
    void setStateChanging(bool c);

signals:
    void signalStrengthChanged();
    void connectedChanged();
    void knownChanged();
    void securityChanged();
    void stateChangingChanged();
    void connectionFailed(int reason);

private:
    void join(const QString& password);

    WifiDevice* mDevice;
    QString mName;
    qreal mSignal = 0;
    bool mConnected = false;
    bool mKnown = false;
    int mSecurity = WifiSecurityType::Unknown;
    bool mStateChanging = false;
};

// The Wi-Fi interface (usually en0).
class WifiDevice : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("WifiDevice comes from Networking.devices.")
    Q_PROPERTY(QString name READ name CONSTANT)
    Q_PROPERTY(int type READ type CONSTANT)
    Q_PROPERTY(bool connected READ connected NOTIFY connectedChanged)
    Q_PROPERTY(int state READ state NOTIFY connectedChanged)
    // The joined network, once a scan has seen it (scans need Location access).
    Q_PROPERTY(WifiNetwork* network READ network NOTIFY networkChanged)
    Q_PROPERTY(ObjectModel* networks READ networks CONSTANT)
    Q_PROPERTY(bool scannerEnabled READ scannerEnabled WRITE setScannerEnabled NOTIFY scannerEnabledChanged)
    // macOS auto-joins saved networks per network, with no device-wide switch;
    // this is remembered but has no effect.
    Q_PROPERTY(bool autoconnect MEMBER mAutoconnect NOTIFY autoconnectChanged)

public:
    explicit WifiDevice(const QString& interfaceName, QObject* parent = nullptr);

    QString name() const { return mName; }
    int type() const { return DeviceType::Wifi; }
    // Associated, judged by the link (RSSI) so it works without Location
    // access, which hides the SSID.
    bool connected() const { return mLinkUp; }
    int state() const { return mLinkUp ? ConnectionState::Connected : ConnectionState::Disconnected; }
    WifiNetwork* network() const { return mLinkUp ? find(mCurrentSsid) : nullptr; }
    ObjectModel* networks() const { return mNetworks; }
    bool scannerEnabled() const { return mScannerEnabled; }
    void setScannerEnabled(bool e);

    // Re-reads the current network and saved networks; scans if enabled.
    void refresh();
    void scan();

signals:
    void connectedChanged();
    void networkChanged();
    void scannerEnabledChanged();
    void autoconnectChanged();

private:
    struct Seen {
        QString ssid;
        qreal signal;
        int security;
    };
    void applyScan(const QList<Seen>& seen);
    WifiNetwork* find(const QString& ssid) const;

    QString mName;
    QString mCurrentSsid;
    bool mLinkUp = false;
    QSet<QString> mKnown;
    ObjectModel* mNetworks;
    QTimer mScanTimer;
    bool mScannerEnabled = false;
    bool mScanning = false;
    bool mAutoconnect = true;
};

class Networking : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
    Q_PROPERTY(ObjectModel* devices READ devices CONSTANT)
    Q_PROPERTY(bool wifiEnabled READ wifiEnabled WRITE setWifiEnabled NOTIFY wifiEnabledChanged)
    Q_PROPERTY(bool wifiHardwareEnabled READ wifiHardwareEnabled CONSTANT)

public:
    explicit Networking(QObject* parent = nullptr);
    ~Networking() override;

    ObjectModel* devices() const { return mDevices; }
    bool wifiEnabled() const { return mWifiEnabled; }
    void setWifiEnabled(bool e);
    bool wifiHardwareEnabled() const { return mWifi != nullptr; }

    // Called from CoreWLAN's event delegate.
    void wifiEvent();

signals:
    void wifiEnabledChanged();

private:
    ObjectModel* mDevices;
    WifiDevice* mWifi = nullptr;
    void* mEventDelegate = nullptr;
    void* mLocation = nullptr;
    bool mWifiEnabled = false;
};
