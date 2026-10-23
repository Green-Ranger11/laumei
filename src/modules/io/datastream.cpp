#include "datastream.h"

void StdioCollector::parseBytes(const QByteArray& incoming) {
    mBuffer += incoming;
    if (!mWaitForEnd) {
        mData = mBuffer;
        emit dataChanged();
    }
}

void StdioCollector::streamEnd() {
    mData = mBuffer;
    mBuffer.clear();
    emit dataChanged();
    emit streamFinished();
}

void SplitParser::setSplitMarker(const QString& m) {
    if (m == mMarker) return;
    mMarker = m;
    emit splitMarkerChanged();
}

void SplitParser::parseBytes(const QByteArray& incoming) {
    mBuffer += incoming;
    if (mMarker.isEmpty()) return;
    const QByteArray marker = mMarker.toUtf8();
    qsizetype at;
    while ((at = mBuffer.indexOf(marker)) != -1) {
        const QByteArray piece = mBuffer.left(at);
        mBuffer.remove(0, at + marker.size());
        emit read(QString::fromUtf8(piece));
    }
}

void SplitParser::streamEnd() {
    if (!mBuffer.isEmpty()) emit read(QString::fromUtf8(mBuffer));
    mBuffer.clear();
}
