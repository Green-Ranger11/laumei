#include "process.h"

#include <QProcessEnvironment>
#include <QStandardPaths>
#include <signal.h>

#include "paths.h"

namespace {

QProcessEnvironment buildEnvironment(const QVariantMap& env, bool clear) {
    QProcessEnvironment pe = clear ? QProcessEnvironment() : QProcessEnvironment::systemEnvironment();
    pe.insert("PATH", QsPaths::childPath());
    for (auto it = env.cbegin(); it != env.cend(); ++it) {
        // A null value removes the variable, like Quickshell.
        if (it.value().isNull()) pe.remove(it.key());
        else pe.insert(it.key(), it.value().toString());
    }
    return pe;
}

// QProcess searches the *parent's* PATH; resolve against the child PATH so
// compat shims and Homebrew tools are found even when launched by launchd.
QString resolveProgram(const QString& program, const QProcessEnvironment& env) {
    if (program.contains('/')) return program;
    const QString found = QStandardPaths::findExecutable(program, env.value("PATH").split(':'));
    return found.isEmpty() ? program : found;
}

} // namespace

Process::Process(QObject* parent) : QObject(parent) {}

Process::~Process() {
    if (mProcess && mProcess->state() != QProcess::NotRunning) {
        mProcess->disconnect(this);
        mProcess->kill();
        mProcess->waitForFinished(500);
    }
}

void Process::componentComplete() {
    mComplete = true;
    if (mWantRunning) start();
}

void Process::setCommand(const QStringList& c) {
    if (c == mCommand) return;
    mCommand = c;
    emit commandChanged();
}

void Process::setWorkingDirectory(const QString& d) {
    if (d == mWorkingDirectory) return;
    mWorkingDirectory = d;
    emit workingDirectoryChanged();
}

void Process::setEnvironment(const QVariantMap& e) {
    if (e == mEnvironment) return;
    mEnvironment = e;
    emit environmentChanged();
}

void Process::setClearEnvironment(bool c) {
    if (c == mClearEnvironment) return;
    mClearEnvironment = c;
    emit clearEnvironmentChanged();
}

bool Process::isRunning() const { return mProcess && mProcess->state() != QProcess::NotRunning; }

void Process::setRunning(bool r) {
    mWantRunning = r;
    if (!mComplete) return;
    if (r && !isRunning()) start();
    else if (!r && isRunning()) mProcess->terminate();
}

QVariant Process::processId() const {
    if (!isRunning()) return {};
    return QVariant::fromValue(mProcess->processId());
}

void Process::setStdoutParser(DataStreamParser* p) {
    if (p == mStdout) return;
    mStdout = p;
    emit stdoutParserChanged();
}

void Process::setStderrParser(DataStreamParser* p) {
    if (p == mStderr) return;
    mStderr = p;
    emit stderrParserChanged();
}

void Process::setStdinEnabled(bool e) {
    if (e == mStdinEnabled) return;
    mStdinEnabled = e;
    emit stdinEnabledChanged();
    if (!e && isRunning()) mProcess->closeWriteChannel();
}

void Process::start() {
    if (mCommand.isEmpty()) {
        qWarning() << "Process: cannot start with an empty command";
        mWantRunning = false;
        emit runningChanged();
        return;
    }

    auto* proc = new QProcess(this);
    mProcess = proc;
    const QProcessEnvironment env = buildEnvironment(mEnvironment, mClearEnvironment);
    proc->setProcessEnvironment(env);
    if (!mWorkingDirectory.isEmpty()) proc->setWorkingDirectory(mWorkingDirectory);

    connect(proc, &QProcess::readyReadStandardOutput, this, [this, proc] {
        const QByteArray d = proc->readAllStandardOutput();
        if (mStdout) mStdout->parseBytes(d);
    });
    connect(proc, &QProcess::readyReadStandardError, this, [this, proc] {
        const QByteArray d = proc->readAllStandardError();
        if (mStderr) mStderr->parseBytes(d);
    });
    connect(proc, &QProcess::started, this, [this] {
        emit runningChanged();
        emit processIdChanged();
        emit started();
    });
    connect(proc, &QProcess::finished, this, &Process::onFinished);
    connect(proc, &QProcess::errorOccurred, this, [this, proc](QProcess::ProcessError err) {
        if (err != QProcess::FailedToStart) return;
        qWarning().noquote() << "Process: failed to start" << mCommand.join(' ') << "-" << proc->errorString();
        onFinished(-1, QProcess::CrashExit);
    });

    proc->start(resolveProgram(mCommand.first(), env), mCommand.mid(1));
    if (!mStdinEnabled) proc->closeWriteChannel();
}

void Process::onFinished(int exitCode, QProcess::ExitStatus status) {
    auto* proc = mProcess;
    if (!proc) return;
    mProcess = nullptr;
    // Drain anything still buffered before telling parsers the stream ended.
    if (mStdout) {
        mStdout->parseBytes(proc->readAllStandardOutput());
        mStdout->streamEnd();
    }
    if (mStderr) {
        mStderr->parseBytes(proc->readAllStandardError());
        mStderr->streamEnd();
    }
    proc->disconnect(this);
    proc->deleteLater();
    mWantRunning = false;
    emit runningChanged();
    emit processIdChanged();
    emit exited(exitCode, status == QProcess::NormalExit ? 0 : 1);
}

void Process::exec(const QVariant& context) {
    if (context.canConvert<QVariantMap>() && context.typeId() == QMetaType::QVariantMap) {
        const QVariantMap m = context.toMap();
        if (m.contains("command")) setCommand(m.value("command").toStringList());
        if (m.contains("environment")) setEnvironment(m.value("environment").toMap());
        if (m.contains("workingDirectory")) setWorkingDirectory(m.value("workingDirectory").toString());
        if (m.contains("clearEnvironment")) setClearEnvironment(m.value("clearEnvironment").toBool());
    } else {
        setCommand(context.toStringList());
    }

    if (isRunning()) {
        mProcess->disconnect(this);
        mProcess->kill();
        mProcess->deleteLater();
        mProcess = nullptr;
    }
    mWantRunning = true;
    mComplete = true;
    start();
}

bool Process::launchDetached(const QStringList& command, const QVariantMap& environment,
                             bool clearEnvironment, const QString& workingDirectory) {
    if (command.isEmpty()) return false;
    QProcess p;
    const QProcessEnvironment env = buildEnvironment(environment, clearEnvironment);
    p.setProcessEnvironment(env);
    p.setProgram(resolveProgram(command.first(), env));
    p.setArguments(command.mid(1));
    if (!workingDirectory.isEmpty()) p.setWorkingDirectory(workingDirectory);
    p.setStandardInputFile(QProcess::nullDevice());
    return p.startDetached();
}

void Process::startDetached() {
    launchDetached(mCommand, mEnvironment, mClearEnvironment, mWorkingDirectory);
}

void Process::signal(int sig) {
    if (isRunning()) ::kill(static_cast<pid_t>(mProcess->processId()), sig);
}

void Process::write(const QString& data) {
    if (isRunning() && mStdinEnabled) mProcess->write(data.toUtf8());
}
