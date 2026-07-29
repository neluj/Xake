#include "game_replay.h"

#include "movegen.h"
#include "opening_book.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>

using namespace Xake;

namespace {

constexpr char kStartFen[] =
    "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";

QString normalizedStartFen(const QString& fen)
{
    const QString trimmed = fen.trimmed();
    return trimmed.isEmpty()
        || trimmed.compare(QStringLiteral("startpos"), Qt::CaseInsensitive) == 0
        ? QString::fromLatin1(kStartFen)
        : trimmed;
}

QStringList stringArray(const QJsonValue& value)
{
    QStringList result;
    const QJsonArray values = value.toArray();
    result.reserve(values.size());
    for (const QJsonValue& item : values) {
        if (item.isString()) {
            result.append(item.toString().trimmed().toLower());
        }
    }
    return result;
}

QString playerName(const QJsonValue& value)
{
    if (value.isString()) {
        return value.toString().trimmed();
    }

    const QJsonObject player = value.toObject();
    const QString name =
        player.value(QStringLiteral("name")).toString().trimmed();
    if (!name.isEmpty()) {
        return name;
    }
    return QFileInfo(
        player.value(QStringLiteral("enginePath")).toString())
        .completeBaseName();
}

PieceType promotionType(QChar character)
{
    switch (character.toLower().toLatin1()) {
    case 'n':
        return KNIGHT;
    case 'b':
        return BISHOP;
    case 'r':
        return ROOK;
    case 'q':
        return QUEEN;
    default:
        return NO_PIECE_TYPE;
    }
}

Move resolveUciMove(const Position& position, const QString& notation)
{
    const QByteArray bytes = notation.trimmed().toLower().toLatin1();
    if (bytes.size() != 4 && bytes.size() != 5) {
        return NOMOVE;
    }
    if (bytes[0] < 'a' || bytes[0] > 'h'
        || bytes[2] < 'a' || bytes[2] > 'h'
        || bytes[1] < '1' || bytes[1] > '8'
        || bytes[3] < '1' || bytes[3] > '8') {
        return NOMOVE;
    }

    const Square64 from =
        Square64((bytes[1] - '1') * 8 + bytes[0] - 'a');
    const Square64 to =
        Square64((bytes[3] - '1') * 8 + bytes[2] - 'a');
    const PieceType promotion = bytes.size() == 5
        ? promotionType(QLatin1Char(bytes[4]))
        : NO_PIECE_TYPE;
    if (bytes.size() == 5 && promotion == NO_PIECE_TYPE) {
        return NOMOVE;
    }

    MoveGen::MoveList moves;
    MoveGen::generate_pseudo_moves(position, moves);
    for (int index = 0; index < moves.size; ++index) {
        const Move candidate = moves.moves[index];
        if (move_from(candidate) != from
            || move_to(candidate) != to
            || promoted_piece(candidate) != promotion) {
            continue;
        }
        Position legalPosition = position;
        if (legalPosition.do_move(candidate)) {
            return candidate;
        }
    }
    return NOMOVE;
}

QVector<MoveRecord> recordsForMoves(const QJsonObject& object,
                                    const QStringList& moves,
                                    int openingMoveCount)
{
    QVector<MoveRecord> records = moveRecordsFromJson(
        object.value(QStringLiteral("moveRecords")).toArray());
    if (records.size() != moves.size()) {
        records.clear();
        records.reserve(moves.size());
        for (qsizetype index = 0; index < moves.size(); ++index) {
            MoveRecord record;
            record.uci = moves.at(index);
            record.origin = index < openingMoveCount
                ? MoveOrigin::Opening
                : MoveOrigin::Imported;
            records.append(record);
        }
        return records;
    }

    for (qsizetype index = 0; index < records.size(); ++index) {
        records[index].uci = moves.at(index);
        if (index < openingMoveCount) {
            records[index].origin = MoveOrigin::Opening;
        }
    }
    return records;
}

ReplayGame sessionGame(const QJsonObject& root, const QString& filePath)
{
    ReplayGame game;
    game.sourcePath = filePath;
    game.title = QFileInfo(filePath).completeBaseName();
    game.event = QStringLiteral("Xake game");
    game.startFen = normalizedStartFen(
        root.value(QStringLiteral("startFen")).toString());
    game.movesUci = stringArray(root.value(QStringLiteral("moves")));

    const QJsonObject match =
        root.value(QStringLiteral("match")).toObject();
    game.white = playerName(match.value(QStringLiteral("player1")));
    game.black = playerName(match.value(QStringLiteral("player2")));
    const QJsonObject result =
        root.value(QStringLiteral("result")).toObject();
    game.result = result.value(QStringLiteral("notation")).toString();

    const QJsonObject opening =
        root.value(QStringLiteral("opening")).toObject();
    game.openingName =
        opening.value(QStringLiteral("name")).toString().trimmed();
    game.openingMoveCount =
        stringArray(opening.value(QStringLiteral("moves"))).size();
    game.moveRecords =
        recordsForMoves(root, game.movesUci, game.openingMoveCount);

    const QJsonObject clocks =
        root.value(QStringLiteral("clocks")).toObject();
    if (!game.moveRecords.isEmpty()) {
        MoveRecord& last = game.moveRecords.last();
        if (last.whiteTimeAfterMs < 0
            && clocks.value(QStringLiteral("whiteMs")).isDouble()) {
            last.whiteTimeAfterMs =
                clocks.value(QStringLiteral("whiteMs")).toInteger(-1);
        }
        if (last.blackTimeAfterMs < 0
            && clocks.value(QStringLiteral("blackMs")).isDouble()) {
            last.blackTimeAfterMs =
                clocks.value(QStringLiteral("blackMs")).toInteger(-1);
        }
    }
    return game;
}

ReplayGame tournamentGame(const QJsonObject& object,
                          const QJsonObject& root,
                          const QString& filePath,
                          int fallbackNumber)
{
    ReplayGame game;
    game.gameNumber =
        object.value(QStringLiteral("gameNumber")).toInt(fallbackNumber);
    game.sourcePath = filePath;
    game.event = QStringLiteral("Xake tournament");
    game.white = playerName(object.value(QStringLiteral("white")));
    game.black = playerName(object.value(QStringLiteral("black")));
    game.result = object.value(QStringLiteral("result")).toString();

    const QJsonObject opening =
        object.value(QStringLiteral("opening")).toObject();
    game.openingName =
        opening.value(QStringLiteral("name")).toString().trimmed();
    game.startFen = normalizedStartFen(
        opening.value(QStringLiteral("startFen"))
            .toString(root.value(QStringLiteral("startFen")).toString()));
    game.openingMoveCount =
        stringArray(opening.value(QStringLiteral("moves"))).size();
    game.movesUci = stringArray(object.value(QStringLiteral("moves")));
    game.moveRecords =
        recordsForMoves(object, game.movesUci, game.openingMoveCount);
    game.title = QStringLiteral("Game %1: %2 - %3")
        .arg(game.gameNumber)
        .arg(game.white, game.black);
    return game;
}

ReplayLoadResult loadJsonReplay(const QString& filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return {{}, QStringLiteral("Could not open '%1': %2")
                         .arg(filePath, file.errorString())};
    }

    QJsonParseError parseError;
    const QJsonDocument document =
        QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError
        || !document.isObject()) {
        const QString reason = parseError.error == QJsonParseError::NoError
            ? QStringLiteral("the JSON root is not an object")
            : parseError.errorString();
        return {{}, QStringLiteral("Could not read '%1': %2")
                         .arg(filePath, reason)};
    }

    const QJsonObject root = document.object();
    if (root.value(QStringLiteral("games")).isArray()) {
        ReplayLoadResult result;
        const QJsonArray games =
            root.value(QStringLiteral("games")).toArray();
        result.games.reserve(games.size());
        for (int index = 0; index < games.size(); ++index) {
            if (games.at(index).isObject()) {
                result.games.append(tournamentGame(
                    games.at(index).toObject(),
                    root,
                    filePath,
                    index + 1));
            }
        }
        if (result.games.isEmpty()) {
            result.error =
                QStringLiteral("The tournament report does not contain any games.");
        }
        return result;
    }

    if (root.value(QStringLiteral("sessionType")).toString()
        == QStringLiteral("tournament")) {
        const QDir directory = QFileInfo(filePath).dir();
        const QString report =
            directory.filePath(QStringLiteral("tournament_report.json"));
        if (QFileInfo(report).absoluteFilePath()
                != QFileInfo(filePath).absoluteFilePath()
            && QFileInfo::exists(report)) {
            const ReplayLoadResult reportResult = loadReplayFile(report);
            if (reportResult.success()) {
                return reportResult;
            }
        }
        const QString pgn =
            directory.filePath(QStringLiteral("tournament.pgn"));
        if (QFileInfo::exists(pgn)) {
            const ReplayLoadResult pgnResult = loadReplayFile(pgn);
            if (pgnResult.success()) {
                return pgnResult;
            }
        }
        return {{}, QStringLiteral(
                         "This tournament session file has no replayable games. "
                         "Open a valid tournament_report.json or tournament.pgn.")};
    }

    if (!root.contains(QStringLiteral("match"))
        && !root.contains(QStringLiteral("moves"))
        && !root.contains(QStringLiteral("startFen"))) {
        return {{}, QStringLiteral(
                         "The JSON file is not a Xake game or tournament record.")};
    }
    return {{sessionGame(root, filePath)}, {}};
}

ReplayLoadResult loadOpeningReplay(const QString& filePath)
{
    QVector<OpeningEntry> entries;
    QString error;
    if (!loadOpeningFile(filePath, &entries, &error)) {
        return {{}, error};
    }

    ReplayLoadResult result;
    result.games.reserve(entries.size());
    for (const OpeningEntry& entry : entries) {
        ReplayGame game;
        game.gameNumber = entry.sourceIndex;
        game.sourcePath = filePath;
        game.title = entry.event.isEmpty() ? entry.name : entry.event;
        game.event = entry.event;
        game.white = entry.white;
        game.black = entry.black;
        game.result = entry.result;
        game.openingName = entry.name;
        game.startFen = normalizedStartFen(entry.startFen);
        game.openingMoveCount = entry.openingMoveCount;
        game.movesUci = entry.movesUci;
        game.moveRecords.reserve(game.movesUci.size());
        for (qsizetype index = 0; index < game.movesUci.size(); ++index) {
            MoveRecord record;
            record.uci = game.movesUci.at(index);
            record.origin = index < game.openingMoveCount
                ? MoveOrigin::Opening
                : MoveOrigin::Imported;
            game.moveRecords.append(record);
        }
        result.games.append(game);
    }
    return result;
}

} // namespace

ReplayLoadResult loadReplayFile(const QString& filePath)
{
    const QFileInfo info(filePath);
    if (!info.exists() || !info.isFile()) {
        return {{}, QStringLiteral("Replay file does not exist: %1")
                         .arg(filePath)};
    }

    const QString suffix = info.suffix().toLower();
    if (suffix == QStringLiteral("json")) {
        return loadJsonReplay(info.absoluteFilePath());
    }
    if (suffix == QStringLiteral("pgn")
        || suffix == QStringLiteral("epd")
        || suffix == QStringLiteral("edp")) {
        return loadOpeningReplay(info.absoluteFilePath());
    }
    return {{}, QStringLiteral(
                     "Unsupported replay format '.%1'. Use JSON, PGN or EPD.")
                     .arg(suffix)};
}

GameReplay::GameReplay()
{
    clear();
}

bool GameReplay::load(const ReplayGame& game, QString* errorOut)
{
    const QString startFen = normalizedStartFen(game.startFen);
    Position start;
    if (!start.set_FEN(startFen.toStdString())) {
        if (errorOut) {
            *errorOut = QStringLiteral("The replay has an invalid start FEN.");
        }
        return false;
    }
    if (game.movesUci.size() > MAX_GAME_MOVES - start.get_ply()) {
        if (errorOut) {
            *errorOut = QStringLiteral(
                "The replay contains too many moves (%1; maximum %2).")
                .arg(game.movesUci.size())
                .arg(MAX_GAME_MOVES - start.get_ply());
        }
        return false;
    }

    Position validator = start;
    QVector<MoveRecord> records;
    records.reserve(game.movesUci.size());
    for (qsizetype index = 0; index < game.movesUci.size(); ++index) {
        const QString uci = game.movesUci.at(index).trimmed().toLower();
        const Move move = resolveUciMove(validator, uci);
        if (move == NOMOVE) {
            if (errorOut) {
                *errorOut = QStringLiteral(
                    "Replay move %1 is invalid or illegal: %2")
                    .arg(index + 1)
                    .arg(uci);
            }
            return false;
        }

        MoveRecord record;
        if (index < game.moveRecords.size()
            && game.moveRecords.at(index).uci.compare(
                   uci, Qt::CaseInsensitive) == 0) {
            record = game.moveRecords.at(index);
        }
        record.move = move;
        record.uci = QString::fromStdString(algebraic_move(move));
        record.movedPiece = validator.get_mailbox_piece(move_from(move));
        record.capturedPiece = captured_piece(move);
        if (move_special(move) == ENPASSANT) {
            record.capturedPiece =
                make_piece(~validator.get_side_to_move(), PAWN);
        }
        if (index < game.openingMoveCount) {
            record.origin = MoveOrigin::Opening;
        }
        if (!validator.do_move(move)) {
            if (errorOut) {
                *errorOut = QStringLiteral(
                    "Replay move %1 could not be applied: %2")
                    .arg(index + 1)
                    .arg(uci);
            }
            return false;
        }
        records.append(record);
    }

    m_game = game;
    m_game.startFen = startFen;
    m_game.movesUci.clear();
    for (const MoveRecord& record : records) {
        m_game.movesUci.append(record.uci);
    }
    m_game.moveRecords = records;
    m_records = records;
    m_position = start;
    m_currentPly = 0;
    return true;
}

void GameReplay::clear()
{
    m_game = ReplayGame{};
    m_records.clear();
    m_currentPly = 0;
    m_position.set_FEN(kStartFen);
}

bool GameReplay::goToPly(int ply)
{
    if (ply < 0 || ply > m_records.size()) {
        return false;
    }

    while (m_currentPly > ply) {
        m_position.undo_move();
        --m_currentPly;
    }
    while (m_currentPly < ply) {
        if (!m_position.do_move(m_records.at(m_currentPly).move)) {
            return false;
        }
        ++m_currentPly;
    }
    return true;
}

const ReplayGame& GameReplay::game() const
{
    return m_game;
}

const Position& GameReplay::position() const
{
    return m_position;
}

Move GameReplay::lastMove() const
{
    return m_currentPly > 0
        ? m_records.at(m_currentPly - 1).move
        : NOMOVE;
}

int GameReplay::currentPly() const
{
    return m_currentPly;
}

int GameReplay::totalPly() const
{
    return static_cast<int>(m_records.size());
}

QStringList GameReplay::visibleMoves() const
{
    return m_game.movesUci.mid(0, m_currentPly);
}

QVector<Piece> GameReplay::capturedPieces() const
{
    return capturedPiecesFromMoveRecords(m_records, m_currentPly);
}

qint64 GameReplay::whiteTimeMs() const
{
    return clockAtCurrentPly(true);
}

qint64 GameReplay::blackTimeMs() const
{
    return clockAtCurrentPly(false);
}

qint64 GameReplay::clockAtCurrentPly(bool white) const
{
    const auto before = [white](const MoveRecord& record) {
        return white ? record.whiteTimeBeforeMs
                     : record.blackTimeBeforeMs;
    };
    const auto after = [white](const MoveRecord& record) {
        return white ? record.whiteTimeAfterMs
                     : record.blackTimeAfterMs;
    };

    if (m_currentPly > 0) {
        const qint64 value = after(m_records.at(m_currentPly - 1));
        if (value >= 0) {
            return value;
        }
    }
    if (m_currentPly < m_records.size()) {
        const qint64 value = before(m_records.at(m_currentPly));
        if (value >= 0) {
            return value;
        }
    }
    for (int index = m_currentPly - 1; index >= 0; --index) {
        const qint64 value = after(m_records.at(index));
        if (value >= 0) {
            return value;
        }
    }
    for (int index = m_currentPly; index < m_records.size(); ++index) {
        const qint64 value = before(m_records.at(index));
        if (value >= 0) {
            return value;
        }
    }
    return -1;
}
