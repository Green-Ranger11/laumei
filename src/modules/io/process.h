#pragma once

#include <QObject>
#include <QPointer>
#include <QProcess>
#include <QStringList>
#include <QVariantMap>
#include <QtQml/qqmlparserstatus.h>
#include <QtQml/qqmlregistration.h>

#include "datastream.h"

// Darwin's <stdio.h> defines stdout/stderr as macros, which would rename the
// Process.stdout/.stderr properties. Files that include this header can't use
// the C stdio streams by name.
#undef stdout
#undef stderr

// Quickshell.Io Process: runs `command` while `running` is true.
class Process : public QObject, public QQmlParserStatus {
    Q_OBJECT
    QML_ELEMENT
    Q_INTERFACES(QQmlParserStatus)
    Q_PROPERTY(QStringList command READ command WRITE setCommand NOTIFY commandChanged)
    Q_PROPERTY(QString workingDirectory READ workingDirectory WRITE setWorkingDirectory NOTIFY workingDirectoryChanged)
    Q_PROPERTY(QVariantMap environment READ environment WRITE setEnvironment NOTIFY environmentChanged)
    Q_PROPERTY(bool clearEnvironment READ clearEnvironment WRITE setClearEnvironment NOTIFY clearEnvironmentChanged)
    Q_PROPERTY(bool running READ isRunning WRITE setRunning NOTIFY runningChanged)
    Q_PROPERTY(QVariant processId READ processId NOTIFY processIdChanged)
    Q_PROPERTY(DataStreamParser* stdout READ stdoutParser WRITE setStdoutParser NOTIFY stdoutParserChanged)
    Q_PROPERTY(DataStreamParser* stderr READ stderrParser WRITE setStderrParser NOTIFY stderrParserChanged)
    Q_PROPERTY(bool stdinEnabled READ stdinEnabled WRITE setStdinEnabled NOTIFY stdinEnabledChanged)

public:
    explicit Process(QObject* parent = nullptr);
    ~Process() override;

    void classBegin() override {}
    void componentComplete() override;

    QStringList command() const { return mCommand; }
    void setCommand(const QStringList& c);
    QString workingDirectory() const { return mWorkingDirectory; }
    void setWorkingDirectory(const QString& d);
    QVariantMap environment() const { return mEnvironment; }
    void setEnvironment(const QVariantMap& e);
    bool clearEnvironment() const { return mClearEnvironment; }
    void setClearEnvironment(bool c);
    bool isRunning() const;
    void setRunning(bool r);
    QVariant processId() const;
    DataStreamParser* stdoutParser() const { return mStdout; }
    void setStdoutParser(DataStreamParser* p);
    DataStreamParser* stderrParser() const { return mStderr; }
    void setStderrParser(DataStreamParser* p);
    bool stdinEnabled() const { return mStdinEnabled; }
    void setStdinEnabled(bool e);

    // exec(["cmd", "arg"]) or exec({ command, environment, workingDirectory,
    // clearEnvironment }): replaces the context and (re)starts the process.
    Q_INVOKABLE void exec(const QVariant& context);
    Q_INVOKABLE void startDetached();
    Q_INVOKABLE void signal(int sig);
    Q_INVOKABLE void write(const QString& data);

    // Shared with Quickshell.execDetached.
    static bool launchDetached(const QStringList& command, const QVariantMap& environment,
                               bool clearEnvironment, const QString& workingDirectory);

signals:
    void commandChanged();
    void workingDirectoryChanged();
    void environmentChanged();
    void clearEnvironmentChanged();
    void runningChanged();
    void processIdChanged();
    void stdoutParserChanged();
    void stderrParserChanged();
    void stdinEnabledChanged();
    void started();
    void exited(int exitCode, int exitStatus);

private:
    void start();
    void onFinished(int exitCode, QProcess::ExitStatus status);

    QStringList mCommand;
    QString mWorkingDirectory;
    QVariantMap mEnvironment;
    bool mClearEnvironment = false;
    bool mWantRunning = false;
    bool mComplete = false;
    bool mStdinEnabled = false;
    QPointer<DataStreamParser> mStdout;
    QPointer<DataStreamParser> mStderr;
    QProcess* mProcess = nullptr;
};
