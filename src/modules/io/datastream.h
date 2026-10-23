#pragma once

#include <QByteArray>
#include <QObject>
#include <QtQml/qqmlregistration.h>

// Base for Process.stdout / Process.stderr parsers. The process feeds it raw
// chunks as they arrive and calls streamEnd() when the pipe closes.
class DataStreamParser : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("DataStreamParser is abstract; use StdioCollector or SplitParser.")

public:
    using QObject::QObject;
    virtual void parseBytes(const QByteArray& incoming) = 0;
    virtual void streamEnd() {}
};

// Collects a whole stream. `text` is filled once the stream ends, or live
// with waitForEnd: false.
class StdioCollector : public DataStreamParser {
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(QString text READ text NOTIFY dataChanged)
    Q_PROPERTY(QByteArray data READ data NOTIFY dataChanged)
    Q_PROPERTY(bool waitForEnd MEMBER mWaitForEnd NOTIFY waitForEndChanged)

public:
    using DataStreamParser::DataStreamParser;
    void parseBytes(const QByteArray& incoming) override;
    void streamEnd() override;

    QString text() const { return QString::fromUtf8(mData); }
    QByteArray data() const { return mData; }

signals:
    void dataChanged();
    void waitForEndChanged();
    void streamFinished();

private:
    QByteArray mBuffer;
    QByteArray mData;
    bool mWaitForEnd = true;
};

// Splits a stream on `splitMarker` (default newline) and emits read() per
// piece. A trailing piece without a marker is emitted when the stream ends.
class SplitParser : public DataStreamParser {
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(QString splitMarker READ splitMarker WRITE setSplitMarker NOTIFY splitMarkerChanged)

public:
    using DataStreamParser::DataStreamParser;
    void parseBytes(const QByteArray& incoming) override;
    void streamEnd() override;

    QString splitMarker() const { return mMarker; }
    void setSplitMarker(const QString& m);

signals:
    void read(const QString& data);
    void splitMarkerChanged();

private:
    QByteArray mBuffer;
    QString mMarker = QStringLiteral("\n");
};
