#include "ipchandler.h"

#include <QJSEngine>
#include <QJSValue>
#include <QMetaMethod>
#include <QQmlEngine>

namespace {

QList<QPointer<IpcHandler>>& registry() {
    static QList<QPointer<IpcHandler>> r;
    return r;
}

// Functions the config declared on the handler, i.e. past IpcHandler's own.
QList<QMetaMethod> declaredFunctions(const QObject* o) {
    QList<QMetaMethod> out;
    const QMetaObject* mo = o->metaObject();
    for (int i = IpcHandler::staticMetaObject.methodCount(); i < mo->methodCount(); i++) {
        const QMetaMethod m = mo->method(i);
        if (m.methodType() == QMetaMethod::Method || m.methodType() == QMetaMethod::Slot) out << m;
    }
    return out;
}

QString typeName(int id) {
    switch (id) {
    case QMetaType::Void: return "void";
    case QMetaType::QString: return "string";
    case QMetaType::Int: return "int";
    case QMetaType::Bool: return "bool";
    case QMetaType::Double:
    case QMetaType::Float: return "real";
    case QMetaType::QColor: return "color";
    default: return QMetaType(id).name() ? QString::fromLatin1(QMetaType(id).name()) : "var";
    }
}

} // namespace

IpcHandler::IpcHandler(QObject* parent) : QObject(parent) { registry().append(this); }

IpcHandler::~IpcHandler() { registry().removeAll(this); }

void IpcHandler::setTarget(const QString& t) {
    if (t == mTarget) return;
    mTarget = t;
    emit targetChanged();
}

void IpcHandler::setEnabled(bool e) {
    if (e == mEnabled) return;
    mEnabled = e;
    emit enabledChanged();
}

QList<IpcHandler*> IpcHandler::handlers() {
    QList<IpcHandler*> out;
    for (const auto& h : registry())
        if (h && h->mEnabled && !h->mTarget.isEmpty()) out << h;
    return out;
}

IpcHandler* IpcHandler::find(const QString& target) {
    for (IpcHandler* h : handlers())
        if (h->mTarget == target) return h;
    return nullptr;
}

QStringList IpcHandler::describeFunctions() const {
    QStringList out;
    for (const QMetaMethod& m : declaredFunctions(this)) {
        QStringList params;
        const auto names = m.parameterNames();
        for (int i = 0; i < m.parameterCount(); i++)
            params << QString::fromUtf8(names.value(i)) + ": " + typeName(m.parameterType(i));
        out << QString("function %1(%2): %3")
                   .arg(QString::fromUtf8(m.name()), params.join(", "), typeName(m.returnType()));
    }
    return out;
}

bool IpcHandler::call(const QString& function, const QStringList& args, QString* result, QString* error) {
    QMetaMethod method;
    for (const QMetaMethod& m : declaredFunctions(this))
        if (QString::fromUtf8(m.name()) == function) method = m;
    if (!method.isValid()) {
        *error = QString("target %1 has no function %2").arg(mTarget, function);
        return false;
    }
    if (args.size() != method.parameterCount()) {
        *error = QString("%1 takes %2 argument(s), got %3").arg(function).arg(method.parameterCount()).arg(args.size());
        return false;
    }

    QJSEngine* engine = qjsEngine(this);
    if (!engine) {
        *error = "handler has no QML engine";
        return false;
    }

    QJSValueList jsArgs;
    for (int i = 0; i < args.size(); i++) {
        const QString& a = args[i];
        switch (method.parameterType(i)) {
        case QMetaType::Int: {
            bool ok;
            const int v = a.toInt(&ok);
            if (!ok) { *error = QString("argument %1 is not an int: %2").arg(i + 1).arg(a); return false; }
            jsArgs << QJSValue(v);
            break;
        }
        case QMetaType::Double:
        case QMetaType::Float: {
            bool ok;
            const double v = a.toDouble(&ok);
            if (!ok) { *error = QString("argument %1 is not a real: %2").arg(i + 1).arg(a); return false; }
            jsArgs << QJSValue(v);
            break;
        }
        case QMetaType::Bool: jsArgs << QJSValue(a == "true" || a == "1"); break;
        default: jsArgs << QJSValue(a);
        }
    }

    QJSValue self = engine->toScriptValue(static_cast<QObject*>(this));
    QJSValue ret = self.property(function).callWithInstance(self, jsArgs);
    if (ret.isError()) {
        *error = ret.toString();
        return false;
    }
    if (method.returnType() != QMetaType::Void && !ret.isUndefined()) *result = ret.toString();
    return true;
}
