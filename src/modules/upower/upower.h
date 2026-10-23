#pragma once

#include <QObject>
#include <QtQml/qqmlregistration.h>

// Quickshell.Services.UPower on macOS, backed by IOKit power sources.

class UPowerDeviceState : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("UPowerDeviceState is an enum.")

public:
    enum Enum { Unknown = 0, Charging, Discharging, Empty, FullyCharged, PendingCharge, PendingDischarge };
    Q_ENUM(Enum)
};

class UPowerDeviceType : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("UPowerDeviceType is an enum.")

public:
    enum Enum { Unknown = 0, LinePower, Battery, Ups };
    Q_ENUM(Enum)
};

class UPowerDevice : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("UPowerDevice comes from UPower.")
    Q_PROPERTY(bool ready READ isPresent NOTIFY changed)
    Q_PROPERTY(bool isPresent READ isPresent NOTIFY changed)
    Q_PROPERTY(bool isLaptopBattery READ isLaptopBattery NOTIFY changed)
    Q_PROPERTY(int type READ type NOTIFY changed)
    Q_PROPERTY(qreal percentage READ percentage NOTIFY changed)
    Q_PROPERTY(int state READ state NOTIFY changed)
    Q_PROPERTY(qreal timeToEmpty READ timeToEmpty NOTIFY changed)
    Q_PROPERTY(qreal timeToFull READ timeToFull NOTIFY changed)
    Q_PROPERTY(qreal healthPercentage READ healthPercentage NOTIFY changed)
    Q_PROPERTY(bool healthSupported READ healthSupported NOTIFY changed)
    Q_PROPERTY(QString nativePath READ nativePath CONSTANT)
    Q_PROPERTY(QString model READ model NOTIFY changed)
    Q_PROPERTY(QString iconName READ iconName NOTIFY changed)

public:
    using QObject::QObject;

    bool isPresent() const { return mPresent; }
    bool isLaptopBattery() const { return mPresent; }
    int type() const { return mPresent ? UPowerDeviceType::Battery : UPowerDeviceType::Unknown; }
    qreal percentage() const { return mPercentage; }
    int state() const { return mState; }
    qreal timeToEmpty() const { return mTimeToEmpty; }
    qreal timeToFull() const { return mTimeToFull; }
    qreal healthPercentage() const { return mHealth; }
    bool healthSupported() const { return mHealth > 0; }
    QString nativePath() const { return QStringLiteral("InternalBattery-0"); }
    QString model() const { return mModel; }
    QString iconName() const;

    void update(bool present, qreal pct, int state, qreal toEmpty, qreal toFull, qreal health, const QString& model);

signals:
    void changed();

private:
    bool mPresent = false;
    qreal mPercentage = 0;
    int mState = UPowerDeviceState::Unknown;
    qreal mTimeToEmpty = 0;
    qreal mTimeToFull = 0;
    qreal mHealth = 0;
    QString mModel;
};

class UPower : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
    Q_PROPERTY(UPowerDevice* displayDevice READ displayDevice CONSTANT)
    Q_PROPERTY(bool onBattery READ onBattery NOTIFY onBatteryChanged)

public:
    explicit UPower(QObject* parent = nullptr);
    ~UPower() override;

    UPowerDevice* displayDevice() const { return mDevice; }
    bool onBattery() const { return mOnBattery; }

    void refresh();

signals:
    void onBatteryChanged();

private:
    UPowerDevice* mDevice;
    bool mOnBattery = false;
    void* mSource = nullptr;
};
