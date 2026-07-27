#include "uci_client.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTextStream>

UciClient::UciClient(QObject *parent)
    : QObject(parent)
{
    m_process.setProcessChannelMode(QProcess::SeparateChannels);

    connect(&m_process, &QProcess::readyReadStandardOutput,
            this, &UciClient::onReadyReadStandardOutput);
    connect(&m_process, &QProcess::readyReadStandardError,
            this, &UciClient::onReadyReadStandardError);
    connect(&m_process, &QProcess::started, this, &UciClient::engineStarted);
    connect(&m_process, &QProcess::errorOccurred, this,
            [this](QProcess::ProcessError error) {
        emit processError(error, m_process.errorString());
    });
    connect(&m_process,
            QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &UciClient::onProcessFinished);
}

UciClient::~UciClient()
{
    if (m_process.state() != QProcess::NotRunning) {
        sendQuit();
        if (!m_process.waitForFinished(500)) {
            m_process.kill();
        }
    }
    disableLogging();
}

bool UciClient::start(const QString& program, const QStringList& args)
{
    if (m_process.state() != QProcess::NotRunning) {
        return false;
    }

    m_stdoutBuffer.clear();
    m_stderrBuffer.clear();
    logLine("##", QString("start %1 %2").arg(program, args.join(' ')).trimmed());
    m_process.start(program, args);
    return m_process.waitForStarted(3000);
}

void UciClient::stopProcess()
{
    if (m_process.state() == QProcess::NotRunning) {
        return;
    }

    m_process.terminate();
    if (!m_process.waitForFinished(500)) {
        m_process.kill();
    }
}

bool UciClient::isRunning() const
{
    return m_process.state() != QProcess::NotRunning;
}

QString UciClient::errorString() const
{
    return m_process.errorString();
}

bool UciClient::setLogFilePath(const QString& path)
{
    disableLogging();
    if (path.trimmed().isEmpty()) {
        return false;
    }

    QFileInfo info(path);
    if (!info.dir().exists()) {
        info.dir().mkpath(".");
    }

    m_logFile = new QFile(path, this);
    if (!m_logFile->open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
        delete m_logFile;
        m_logFile = nullptr;
        m_loggingEnabled = false;
        return false;
    }
    m_loggingEnabled = true;
    logLine("##", QString("logging to %1").arg(path));
    return true;
}

void UciClient::disableLogging()
{
    if (m_logFile) {
        if (m_logFile->isOpen()) {
            m_logFile->flush();
            m_logFile->close();
        }
        delete m_logFile;
        m_logFile = nullptr;
    }
    m_loggingEnabled = false;
}

void UciClient::sendUci()
{
    sendRawCommand("uci");
}

void UciClient::sendIsReady()
{
    sendRawCommand("isready");
}

void UciClient::sendSetOptionHash(int mb)
{
    if (mb <= 0) {
        return;
    }
    sendRawCommand(QString("setoption name Hash value %1").arg(mb));
}

void UciClient::sendNewGame()
{
    sendRawCommand("ucinewgame");
}

void UciClient::sendPositionStartpos(const QStringList& moves)
{
    QString cmd = "position startpos";
    if (!moves.isEmpty()) {
        cmd += " moves " + moves.join(' ');
    }
    sendRawCommand(cmd);
}

void UciClient::sendPositionFen(const QString& fen, const QStringList& moves)
{
    if (fen.trimmed().isEmpty()) {
        return;
    }
    QString cmd = "position fen " + fen;
    if (!moves.isEmpty()) {
        cmd += " moves " + moves.join(' ');
    }
    sendRawCommand(cmd);
}

void UciClient::sendGoDepth(int depth)
{
    if (depth <= 0) {
        return;
    }
    sendRawCommand(QString("go depth %1").arg(depth));
}

void UciClient::sendGoWtimeBtime(int wtimeMs, int btimeMs,
                                 int wincMs, int bincMs)
{
    const QString cmd = QString("go wtime %1 btime %2 winc %3 binc %4")
        .arg(wtimeMs)
        .arg(btimeMs)
        .arg(wincMs)
        .arg(bincMs);
    sendRawCommand(cmd);
}

void UciClient::sendGoMovetime(int ms)
{
    if (ms <= 0) {
        return;
    }
    sendRawCommand(QString("go movetime %1").arg(ms));
}

void UciClient::sendGoInfinite()
{
    sendRawCommand("go infinite");
}

void UciClient::sendGoPerft(int depth)
{
    if (depth <= 0) {
        return;
    }
    sendRawCommand(QString("go perft %1").arg(depth));
}

void UciClient::sendDebugDump()
{
    sendRawCommand("d");
}

void UciClient::sendStop()
{
    sendRawCommand("stop");
}

void UciClient::sendQuit()
{
    sendRawCommand("quit");
}

void UciClient::sendRawCommand(const QString& line)
{
    if (m_process.state() == QProcess::NotRunning) {
        return;
    }
    logLine(">>", line);
    QByteArray data = line.toUtf8();
    if (!data.endsWith('\n')) {
        data.append('\n');
    }
    const qint64 written = m_process.write(data);
    if (written != data.size()) {
        emit processError(QProcess::WriteError, m_process.errorString());
    }
}

void UciClient::onReadyReadStandardOutput()
{
    m_stdoutBuffer.append(m_process.readAllStandardOutput());
    int newlineIndex = -1;
    while ((newlineIndex = m_stdoutBuffer.indexOf('\n')) != -1) {
        QByteArray line = m_stdoutBuffer.left(newlineIndex);
        m_stdoutBuffer.remove(0, newlineIndex + 1);
        if (!line.isEmpty() && line.endsWith('\r')) {
            line.chop(1);
        }
        handleOutputLine(QString::fromUtf8(line));
    }
}

void UciClient::onReadyReadStandardError()
{
    m_stderrBuffer.append(m_process.readAllStandardError());
    int newlineIndex = -1;
    while ((newlineIndex = m_stderrBuffer.indexOf('\n')) != -1) {
        QByteArray line = m_stderrBuffer.left(newlineIndex);
        m_stderrBuffer.remove(0, newlineIndex + 1);
        if (!line.isEmpty() && line.endsWith('\r')) {
            line.chop(1);
        }
        logLine("!!", QString::fromUtf8(line));
        emit standardErrorOutput(QString::fromUtf8(line));
    }
}

void UciClient::onProcessFinished(int exitCode, QProcess::ExitStatus status)
{
    logLine("##", QString("exit %1 status %2").arg(exitCode).arg(static_cast<int>(status)));
    emit engineExited(exitCode, status);
}

void UciClient::handleOutputLine(const QString& line)
{
    logLine("<<", line);
    emit engineOutput(line);

    if (line == "uciok") {
        emit uciOk();
        return;
    }
    if (line == "readyok") {
        emit readyOk();
        return;
    }

    if (line.startsWith("id name ")) {
        emit idName(line.mid(8));
        return;
    }
    if (line.startsWith("id author ")) {
        emit idAuthor(line.mid(10));
        return;
    }
    if (line.startsWith("option ")) {
        emit optionLine(line);
        return;
    }
    if (line.startsWith("info ")) {
        emit infoLine(line);
        return;
    }
    if (line.startsWith("bestmove ")) {
        QString move;
        QString ponder;
        const QStringList parts = line.split(' ', Qt::SkipEmptyParts);
        if (parts.size() >= 2) {
            move = parts[1];
        }
        for (int i = 2; i + 1 < parts.size(); ++i) {
            if (parts[i] == "ponder") {
                ponder = parts[i + 1];
                break;
            }
        }
        emit bestMove(move, ponder);
        return;
    }
}

void UciClient::logLine(const QString& prefix, const QString& line)
{
    const QString trimmedLine = line.trimmed();
    emit communication(prefix, trimmedLine);

    if (!m_loggingEnabled || !m_logFile || !m_logFile->isOpen()) {
        return;
    }

    QTextStream stream(m_logFile);
    const QString ts = QDateTime::currentDateTime().toString(Qt::ISODateWithMs);
    stream << ts << " " << prefix << " " << trimmedLine << "\n";
    m_logFile->flush();
}
