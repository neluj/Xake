#include "uci_client.h"

UciClient::UciClient(QObject *parent)
    : QObject(parent)
{
    m_process.setProcessChannelMode(QProcess::SeparateChannels);

    connect(&m_process, &QProcess::readyReadStandardOutput,
            this, &UciClient::onReadyReadStandardOutput);
    connect(&m_process, &QProcess::readyReadStandardError,
            this, &UciClient::onReadyReadStandardError);
    connect(&m_process, &QProcess::started, this, &UciClient::engineStarted);
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
}

bool UciClient::start(const QString& program, const QStringList& args)
{
    if (m_process.state() != QProcess::NotRunning) {
        return false;
    }

    m_stdoutBuffer.clear();
    m_stderrBuffer.clear();
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
                                 int wincMs, int bincMs, int movestogo)
{
    QString cmd = QString("go wtime %1 btime %2 winc %3 binc %4")
        .arg(wtimeMs)
        .arg(btimeMs)
        .arg(wincMs)
        .arg(bincMs);
    if (movestogo > 0) {
        cmd += QString(" movestogo %1").arg(movestogo);
    }
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
    QByteArray data = line.toUtf8();
    if (!data.endsWith('\n')) {
        data.append('\n');
    }
    m_process.write(data);
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
        emit engineError(QString::fromUtf8(line));
    }
}

void UciClient::onProcessFinished(int exitCode, QProcess::ExitStatus status)
{
    emit engineExited(exitCode, status);
}

void UciClient::handleOutputLine(const QString& line)
{
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
