#pragma once

#include <QProcess>

class QTextStream;

class Cmd : public QProcess
{
    Q_OBJECT
public:
    explicit Cmd(QObject *parent = nullptr);
    bool proc(const QString &cmd, const QStringList &args = {}, QString *output = nullptr,
              const QByteArray *input = nullptr, bool quiet = false, bool elevate = false);
    bool procAsRoot(const QString &cmd, const QStringList &args = {}, QString *output = nullptr,
                    const QByteArray *input = nullptr, bool quiet = false);
    bool run(const QString &cmd, QString *output = nullptr, const QByteArray *input = nullptr, bool quiet = false,
             bool elevate = false);
    bool runAsRoot(const QString &cmd, QString *output = nullptr, const QByteArray *input = nullptr,
                   bool quiet = false);
    [[nodiscard]] QString getOut(const QString &cmd, bool quiet = false, bool elevate = false);
    [[nodiscard]] QString getOutAsRoot(const QString &cmd, bool quiet = false);

signals:
    void done();
    void errorAvailable(const QString &err);
    void outputAvailable(const QString &out);

private:
    QString asRoot;
    QString helper;
    QString out_buffer;
    static constexpr int EXIT_CODE_COMMAND_NOT_FOUND = 127;
    static constexpr int EXIT_CODE_PERMISSION_DENIED = 126;
};
