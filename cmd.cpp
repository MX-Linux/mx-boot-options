#include "cmd.h"

#include <QApplication>
#include <QDebug>
#include <QEventLoop>
#include <QFileInfo>

#include <unistd.h>

Cmd::Cmd(QObject *parent)
    : QProcess(parent)
{
}

QString Cmd::getOut(const QString &cmd, bool quiet, bool asRoot)
{
    run(cmd, quiet, asRoot);
    return readAll();
}

QString Cmd::getOutAsRoot(const QString &cmd, bool quiet)
{
    return getOut(cmd, quiet, true);
}

bool Cmd::run(const QString &cmd, bool quiet, bool asRoot)
{
    if (state() != QProcess::NotRunning) {
        qDebug() << "Process already running:" << program() << arguments();
        return false;
    }
    if (!quiet) {
        qDebug().noquote() << cmd;
    }
    QEventLoop loop;
    connect(this, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), &loop, &QEventLoop::quit);
    if (asRoot && getuid() != 0) {
        if (QFile::exists("/usr/bin/pkexec")) {
            QString helper {"/usr/lib/" + QApplication::applicationName() + "/helper"};
            start("/usr/bin/pkexec", {helper, cmd});
        } else {
            start("/usr/bin/gksu", {cmd});
        }
    } else {
        start("/bin/bash", {"-c", cmd});
    }
    loop.exec();
    emit done();
    return (exitStatus() == QProcess::NormalExit && exitCode() == 0);
}

bool Cmd::runAsRoot(const QString &cmd, bool quiet)
{
    return run(cmd, quiet, true);
}
