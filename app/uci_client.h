#pragma once

#include <QObject>
#include <QProcess>
#include <QString>
#include <QStringList>
#include <QFile>

class UciClient : public QObject
{
    Q_OBJECT

public:
    explicit UciClient(QObject *parent = nullptr);
    ~UciClient() override;

    bool start(const QString& program, const QStringList& args = {});
    void stopProcess();
    bool isRunning() const;
    QString errorString() const;

    bool setLogFilePath(const QString& path);
    void disableLogging();

    void sendUci();
    void sendIsReady();
    void sendSetOptionHash(int mb);
    void sendNewGame();
    void sendPositionStartpos(const QStringList& moves = {});
    void sendPositionFen(const QString& fen, const QStringList& moves = {});
    void sendGoDepth(int depth);
    void sendGoWtimeBtime(int wtimeMs, int btimeMs, int wincMs, int bincMs, int movestogo = -1);
    void sendGoMovetime(int ms);
    void sendGoInfinite();
    void sendGoPerft(int depth);
    void sendDebugDump();
    void sendStop();
    void sendQuit();
    void sendRawCommand(const QString& line);

signals:
    void engineStarted();
    void engineExited(int exitCode, QProcess::ExitStatus status);
    void engineError(const QString& line);
    void processError(QProcess::ProcessError error, const QString& detail);
    void engineOutput(const QString& line);
    void communication(const QString& prefix, const QString& line);
    void uciOk();
    void readyOk();
    void idName(const QString& name);
    void idAuthor(const QString& author);
    void optionLine(const QString& line);
    void infoLine(const QString& line);
    void bestMove(const QString& move, const QString& ponder);

private slots:
    void onReadyReadStandardOutput();
    void onReadyReadStandardError();
    void onProcessFinished(int exitCode, QProcess::ExitStatus status);

private:
    void handleOutputLine(const QString& line);
    void logLine(const QString& prefix, const QString& line);

    QProcess m_process;
    QByteArray m_stdoutBuffer;
    QByteArray m_stderrBuffer;
    QFile *m_logFile = nullptr;
    bool m_loggingEnabled = false;
};
