#pragma once

#include <QAbstractListModel>
#include <QList>
#include <QObject>
#include <QtQml/qqmlregistration.h>

// Quickshell's ObjectModel: a list of QObjects readable as `.values` (a JS
// array) or usable directly as a view/Instantiator model (role modelData).
class ObjectModel : public QAbstractListModel {
    Q_OBJECT
    QML_ANONYMOUS
    Q_PROPERTY(QList<QObject*> values READ values NOTIFY valuesChanged)

public:
    using QAbstractListModel::QAbstractListModel;

    QList<QObject*> values() const { return mValues; }

    int rowCount(const QModelIndex& parent = {}) const override { return parent.isValid() ? 0 : mValues.size(); }
    QVariant data(const QModelIndex& index, int role) const override {
        if (role != Qt::UserRole || !index.isValid() || index.row() >= mValues.size()) return {};
        return QVariant::fromValue(mValues.at(index.row()));
    }
    QHash<int, QByteArray> roleNames() const override { return {{Qt::UserRole, "modelData"}}; }

    void append(QObject* o) {
        beginInsertRows({}, mValues.size(), mValues.size());
        mValues.append(o);
        endInsertRows();
        emit valuesChanged();
    }

    void remove(QObject* o) {
        const int i = mValues.indexOf(o);
        if (i < 0) return;
        beginRemoveRows({}, i, i);
        mValues.removeAt(i);
        endRemoveRows();
        emit valuesChanged();
    }

    bool contains(QObject* o) const { return mValues.contains(o); }

signals:
    void valuesChanged();

private:
    QList<QObject*> mValues;
};
