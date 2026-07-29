#include "history_repository.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include <algorithm>

namespace {

struct JsonFile {
    bool exists = false;
    bool valid = false;
    QJsonObject root;
};

QDateTime timestamp(const QJsonValue& value)
{
    return QDateTime::fromString(value.toString(), Qt::ISODate);
}

QStringList stringArray(const QJsonValue& value)
{
    QStringList result;
    const QJsonArray values = value.toArray();
    result.reserve(values.size());
    for (const QJsonValue& item : values) {
        if (item.isString()) {
            result.append(item.toString());
        }
    }
    return result;
}

QString playerName(const QJsonValue& value)
{
    const QJsonObject player = value.toObject();
    const QString configuredName = player.value(QStringLiteral("name"))
                                       .toString()
                                       .trimmed();
    if (!configuredName.isEmpty()) {
        return configuredName;
    }

    const QString enginePath = player.value(QStringLiteral("enginePath")).toString();
    const QString executableName = QFileInfo(enginePath).completeBaseName();
    return executableName.isEmpty() ? QStringLiteral("Unknown") : executableName;
}

void parseGameSettings(const QJsonObject& game, HistoryEntry& entry)
{
    entry.timeControl = game.value(QStringLiteral("timeControl")).toString();
    entry.baseTimeSeconds =
        game.value(QStringLiteral("baseTimeSeconds")).toInt();
    entry.incrementSeconds =
        game.value(QStringLiteral("incrementSeconds")).toInt();
}

JsonFile readJson(const QString& path, QStringList& warnings)
{
    JsonFile result;
    result.exists = QFileInfo::exists(path);
    if (!result.exists) {
        return result;
    }

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        warnings.append(QStringLiteral("%1: %2").arg(path, file.errorString()));
        return result;
    }

    QJsonParseError parseError;
    const QJsonDocument document =
        QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        const QString reason =
            parseError.error == QJsonParseError::NoError
            ? QStringLiteral("the JSON root is not an object")
            : parseError.errorString();
        warnings.append(QStringLiteral("%1: %2").arg(path, reason));
        return result;
    }

    result.valid = true;
    result.root = document.object();
    return result;
}

QString firstSessionRecord(const QDir& directory)
{
    const QFileInfoList files = directory.entryInfoList(
        {QStringLiteral("session_*.json")},
        QDir::Files,
        QDir::Name);
    return files.isEmpty() ? QString() : files.first().absoluteFilePath();
}

HistoryEntry parseMatch(const QJsonObject& root,
                        const QString& directoryPath,
                        const QString& recordPath)
{
    HistoryEntry entry;
    entry.type = HistorySessionType::Match;
    entry.directoryPath = directoryPath;
    entry.recordFilePath = recordPath;
    entry.sessionTag = root.value(QStringLiteral("sessionTag")).toString();
    entry.status = root.value(QStringLiteral("status")).toString();
    entry.startedAt = timestamp(root.value(QStringLiteral("startTime")));
    entry.finishedAt = timestamp(root.value(QStringLiteral("finishedAt")));
    entry.startFen = root.value(QStringLiteral("startFen")).toString();
    entry.finalFen = root.value(QStringLiteral("finalFen")).toString();
    entry.moves = stringArray(root.value(QStringLiteral("moves")));

    const QJsonObject match = root.value(QStringLiteral("match")).toObject();
    entry.player1 = playerName(match.value(QStringLiteral("player1")));
    entry.player2 = playerName(match.value(QStringLiteral("player2")));
    parseGameSettings(match.value(QStringLiteral("game")).toObject(), entry);

    const QJsonObject opening = root.value(QStringLiteral("opening")).toObject();
    entry.openingName = opening.value(QStringLiteral("name")).toString();
    entry.openingMoveCount =
        stringArray(opening.value(QStringLiteral("moves"))).size();

    const QJsonObject result = root.value(QStringLiteral("result")).toObject();
    entry.result = result.value(QStringLiteral("notation")).toString();
    entry.termination = result.value(QStringLiteral("termination")).toString();
    entry.message = result.value(QStringLiteral("message")).toString();
    const QJsonObject abort =
        root.value(QStringLiteral("abort")).toObject();
    if (entry.termination.isEmpty()) {
        entry.termination =
            root.value(QStringLiteral("termination")).toString();
    }
    if (entry.termination.isEmpty()) {
        entry.termination = abort.value(QStringLiteral("title")).toString();
    }
    if (entry.message.isEmpty()) {
        entry.message = abort.value(QStringLiteral("message")).toString();
    }

    const QString pgnPath =
        QDir(directoryPath).filePath(QStringLiteral("game.pgn"));
    if (QFileInfo::exists(pgnPath)) {
        entry.pgnFilePath = pgnPath;
    }
    return entry;
}

HistoryGame parseTournamentGame(const QJsonObject& object)
{
    HistoryGame game;
    game.gameNumber = object.value(QStringLiteral("gameNumber")).toInt();
    game.status = object.value(QStringLiteral("status")).toString();
    game.startedAt = timestamp(object.value(QStringLiteral("startedAt")));
    game.finishedAt = timestamp(object.value(QStringLiteral("finishedAt")));
    game.white = playerName(object.value(QStringLiteral("white")));
    game.black = playerName(object.value(QStringLiteral("black")));
    game.result = object.value(QStringLiteral("result")).toString();
    game.termination = object.value(QStringLiteral("termination")).toString();
    game.message = object.value(QStringLiteral("message")).toString();
    if (game.termination.isEmpty()) {
        game.termination =
            object.value(QStringLiteral("abortTitle")).toString();
    }
    if (game.message.isEmpty()) {
        game.message = object.value(QStringLiteral("abortMessage")).toString();
    }
    game.moves = stringArray(object.value(QStringLiteral("moves")));

    const QJsonObject opening =
        object.value(QStringLiteral("opening")).toObject();
    game.openingName = opening.value(QStringLiteral("name")).toString();
    game.startFen = opening.value(QStringLiteral("startFen")).toString();
    game.openingMoveCount =
        stringArray(opening.value(QStringLiteral("moves"))).size();
    return game;
}

HistoryEntry parseTournamentReport(const QJsonObject& root,
                                   const QString& directoryPath,
                                   const QString& recordPath)
{
    HistoryEntry entry;
    entry.type = HistorySessionType::Tournament;
    entry.directoryPath = directoryPath;
    entry.recordFilePath = recordPath;
    entry.sessionTag = root.value(QStringLiteral("sessionTag")).toString();
    entry.status = root.value(QStringLiteral("status")).toString();
    entry.startedAt = timestamp(root.value(QStringLiteral("startedAt")));
    entry.finishedAt = timestamp(root.value(QStringLiteral("finishedAt")));
    entry.startFen = root.value(QStringLiteral("startFen")).toString();

    const QJsonObject tournament =
        root.value(QStringLiteral("tournament")).toObject();
    entry.player1 = playerName(tournament.value(QStringLiteral("player1")));
    entry.player2 = playerName(tournament.value(QStringLiteral("player2")));
    parseGameSettings(tournament.value(QStringLiteral("game")).toObject(), entry);

    const QJsonObject summary = root.value(QStringLiteral("summary")).toObject();
    entry.totalGames = summary.value(QStringLiteral("totalGames")).toInt();
    entry.completedGames =
        summary.value(QStringLiteral("completedGames")).toInt();
    entry.player1Wins = summary.value(QStringLiteral("player1Wins")).toInt();
    entry.player2Wins = summary.value(QStringLiteral("player2Wins")).toInt();
    entry.draws = summary.value(QStringLiteral("draws")).toInt();

    const QJsonArray games = root.value(QStringLiteral("games")).toArray();
    entry.games.reserve(games.size());
    for (const QJsonValue& value : games) {
        if (value.isObject()) {
            entry.games.append(parseTournamentGame(value.toObject()));
        }
    }

    const QString pgnPath =
        QDir(directoryPath).filePath(QStringLiteral("tournament.pgn"));
    if (QFileInfo::exists(pgnPath)) {
        entry.pgnFilePath = pgnPath;
    }
    return entry;
}

HistoryEntry parseTournamentSession(const QJsonObject& root,
                                    const QString& directoryPath,
                                    const QString& recordPath)
{
    HistoryEntry entry;
    entry.type = HistorySessionType::Tournament;
    entry.directoryPath = directoryPath;
    entry.recordFilePath = recordPath;
    entry.sessionTag = root.value(QStringLiteral("sessionTag")).toString();
    entry.status = root.value(QStringLiteral("status")).toString();
    entry.startedAt = timestamp(root.value(QStringLiteral("startTime")));
    entry.finishedAt = timestamp(root.value(QStringLiteral("finishedAt")));
    entry.startFen = root.value(QStringLiteral("startFen")).toString();

    const QJsonObject tournament =
        root.value(QStringLiteral("tournament")).toObject();
    const QJsonObject match = tournament.value(QStringLiteral("match")).toObject();
    entry.player1 = playerName(match.value(QStringLiteral("player1")));
    entry.player2 = playerName(match.value(QStringLiteral("player2")));
    parseGameSettings(match.value(QStringLiteral("game")).toObject(), entry);
    entry.totalGames = tournament.value(QStringLiteral("rounds")).toInt()
        * tournament.value(QStringLiteral("gamesPerPairing")).toInt();

    const QString pgnPath =
        QDir(directoryPath).filePath(QStringLiteral("tournament.pgn"));
    if (QFileInfo::exists(pgnPath)) {
        entry.pgnFilePath = pgnPath;
    }
    return entry;
}

QDateTime sortTimestamp(const HistoryEntry& entry)
{
    if (entry.startedAt.isValid()) {
        return entry.startedAt;
    }
    return QFileInfo(entry.directoryPath).lastModified();
}

Qt::CaseSensitivity pathCaseSensitivity()
{
#ifdef Q_OS_WIN
    return Qt::CaseInsensitive;
#else
    return Qt::CaseSensitive;
#endif
}

QString canonicalManagedSessionPath(const QString& sessionsDirectory,
                                    const QString& sessionDirectory,
                                    QString *error)
{
    const QFileInfo rootInfo(sessionsDirectory);
    if (!rootInfo.exists() || !rootInfo.isDir()) {
        if (error) {
            *error = QStringLiteral("The sessions directory is not available.");
        }
        return {};
    }

    const QFileInfo sessionInfo(sessionDirectory);
    if (!sessionInfo.exists() || !sessionInfo.isDir()) {
        if (error) {
            *error = QStringLiteral("The selected session directory is not available.");
        }
        return {};
    }
    if (sessionInfo.isSymLink()) {
        if (error) {
            *error = QStringLiteral("Symbolic links cannot be deleted from history.");
        }
        return {};
    }

    const QString rootPath = rootInfo.canonicalFilePath();
    const QString sessionPath = sessionInfo.canonicalFilePath();
    const QString parentPath = QFileInfo(sessionPath).absoluteDir().canonicalPath();
    if (rootPath.isEmpty() || sessionPath.isEmpty() || parentPath.isEmpty()
        || QString::compare(rootPath,
                            parentPath,
                            pathCaseSensitivity()) != 0) {
        if (error) {
            *error = QStringLiteral(
                "The selected directory is not a managed Xake session.");
        }
        return {};
    }

    return sessionPath;
}

} // namespace

HistoryLoadResult loadSessionHistory(const QString& sessionsDirectory)
{
    HistoryLoadResult result;
    const QDir root(sessionsDirectory);
    if (!root.exists()) {
        return result;
    }

    const QFileInfoList directories = root.entryInfoList(
        QDir::Dirs | QDir::NoDotAndDotDot,
        QDir::Name);
    result.entries.reserve(directories.size());

    for (const QFileInfo& directoryInfo : directories) {
        const QDir directory(directoryInfo.absoluteFilePath());
        const QString reportPath =
            directory.filePath(QStringLiteral("tournament_report.json"));
        const JsonFile report = readJson(reportPath, result.warnings);
        if (report.valid) {
            result.entries.append(parseTournamentReport(
                report.root, directory.absolutePath(), reportPath));
            continue;
        }

        const QString sessionPath = firstSessionRecord(directory);
        if (sessionPath.isEmpty()) {
            continue;
        }
        const JsonFile session = readJson(sessionPath, result.warnings);
        if (!session.valid) {
            continue;
        }

        if (session.root.value(QStringLiteral("sessionType")).toString()
            == QStringLiteral("tournament")) {
            result.entries.append(parseTournamentSession(
                session.root, directory.absolutePath(), sessionPath));
        } else {
            result.entries.append(parseMatch(
                session.root, directory.absolutePath(), sessionPath));
        }
    }

    std::sort(result.entries.begin(), result.entries.end(),
              [](const HistoryEntry& left, const HistoryEntry& right) {
        return sortTimestamp(left) > sortTimestamp(right);
    });
    return result;
}

bool isManagedHistorySessionDirectory(const QString& sessionsDirectory,
                                      const QString& sessionDirectory)
{
    return !canonicalManagedSessionPath(
                sessionsDirectory, sessionDirectory, nullptr)
                .isEmpty();
}

HistorySessionDeletionResult deleteHistorySession(
    const QString& sessionsDirectory,
    const QString& sessionDirectory)
{
    HistorySessionDeletionResult result;
    const QString managedPath = canonicalManagedSessionPath(
        sessionsDirectory, sessionDirectory, &result.error);
    if (managedPath.isEmpty()) {
        return result;
    }

    if (!QDir(managedPath).removeRecursively()) {
        result.error = QStringLiteral(
            "The selected session directory could not be deleted.");
        return result;
    }

    result.deleted = true;
    return result;
}
