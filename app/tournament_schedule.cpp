#include "tournament_schedule.h"

#include "match_settings_validation.h"

#include <QVector>

#include <limits>

namespace {

struct RoundRobinPairing {
    int round = 0;
    int ordinal = 0;
    QString firstParticipantId;
    QString secondParticipantId;
};

QVector<RoundRobinPairing> roundRobinPairings(
    const QVector<TournamentParticipant>& participants,
    int *roundCount)
{
    QVector<int> rotation;
    rotation.reserve(participants.size() + 1);
    for (qsizetype index = 0; index < participants.size(); ++index) {
        rotation.append(static_cast<int>(index));
    }
    if ((rotation.size() % 2) != 0) {
        rotation.append(-1);
    }

    const int rounds = rotation.size() - 1;
    const int pairingsPerRound = rotation.size() / 2;
    QVector<RoundRobinPairing> pairings;
    pairings.reserve(rounds * pairingsPerRound);
    const int orientationSize =
        (participants.size() % 2) == 0
        ? static_cast<int>(participants.size()) + 1
        : static_cast<int>(participants.size());
    int ordinal = 0;
    for (int round = 0; round < rounds; ++round) {
        for (int index = 0; index < pairingsPerRound; ++index) {
            const int left = rotation.at(index);
            const int right = rotation.at(rotation.size() - 1 - index);
            if (left < 0 || right < 0) {
                continue;
            }

            const int clockwiseDistance =
                (right - left + orientationSize)
                % orientationSize;
            const bool leftIsWhite =
                clockwiseDistance
                <= (orientationSize - 1) / 2;
            const int first = leftIsWhite ? left : right;
            const int second = leftIsWhite ? right : left;
            pairings.append({
                round,
                ordinal++,
                participants.at(first).id,
                participants.at(second).id
            });
        }

        const int last = rotation.takeLast();
        rotation.insert(1, last);
    }

    if (roundCount) {
        *roundCount = rounds;
    }
    return pairings;
}

bool wouldOverflow(qint64 games)
{
    return games < 1 || games > std::numeric_limits<int>::max();
}

} // namespace

TournamentScheduleResult buildTournamentSchedule(
    const TournamentConfig& sourceConfig)
{
    TournamentConfig config = sourceConfig;
    normalizeTournamentConfig(config);
    const ValidationError validation = validateTournamentConfig(config);
    if (validation != ValidationError::None) {
        return {{}, 0, validationErrorMessage(validation)};
    }

    TournamentScheduleResult result;
    int gameNumber = 1;
    const int openingGroupsPerPairing =
        (config.gamesPerPairing + 1) / 2;

    if (config.format == TournamentFormat::Gauntlet) {
        QVector<TournamentParticipant> opponents;
        for (const TournamentParticipant& participant : config.participants) {
            if (participant.id != config.gauntletParticipantId) {
                opponents.append(participant);
            }
        }
        const qint64 totalGames =
            static_cast<qint64>(config.rounds)
            * opponents.size()
            * config.gamesPerPairing;
        if (wouldOverflow(totalGames)) {
            result.error = QStringLiteral(
                "The tournament contains too many games.");
            return result;
        }

        for (int cycle = 0; cycle < config.rounds; ++cycle) {
            for (int repeat = 0;
                 repeat < config.gamesPerPairing;
                 ++repeat) {
                for (qsizetype opponentIndex = 0;
                     opponentIndex < opponents.size();
                     ++opponentIndex) {
                    const bool mainIsWhite =
                        ((cycle + repeat + opponentIndex) % 2) == 0;
                    const QString opponentId =
                        opponents.at(opponentIndex).id;
                    const int pairingOrdinal =
                        cycle * opponents.size() + opponentIndex;
                    result.games.append({
                        gameNumber,
                        gameNumber,
                        cycle + 1,
                        repeat + 1,
                        pairingOrdinal * openingGroupsPerPairing
                            + repeat / 2,
                        mainIsWhite
                            ? config.gauntletParticipantId
                            : opponentId,
                        mainIsWhite
                            ? opponentId
                            : config.gauntletParticipantId
                    });
                    ++gameNumber;
                }
            }
        }
        result.roundCount = result.games.size();
        return result;
    }

    int roundsPerCycle = 0;
    const QVector<RoundRobinPairing> pairings =
        roundRobinPairings(config.participants, &roundsPerCycle);
    const qint64 totalGames =
        static_cast<qint64>(config.rounds)
        * pairings.size()
        * config.gamesPerPairing;
    if (wouldOverflow(totalGames)) {
        result.error = QStringLiteral(
            "The tournament contains too many games.");
        return result;
    }

    for (int cycle = 0; cycle < config.rounds; ++cycle) {
        for (int repeat = 0;
             repeat < config.gamesPerPairing;
             ++repeat) {
            for (int round = 0; round < roundsPerCycle; ++round) {
                for (const RoundRobinPairing& pairing : pairings) {
                    if (pairing.round != round) {
                        continue;
                    }
                    const bool reverse = ((cycle + repeat) % 2) != 0;
                    const int pairingOrdinal =
                        cycle * pairings.size() + pairing.ordinal;
                    result.games.append({
                        gameNumber++,
                        (cycle * config.gamesPerPairing + repeat)
                            * roundsPerCycle + round + 1,
                        cycle + 1,
                        repeat + 1,
                        pairingOrdinal * openingGroupsPerPairing
                            + repeat / 2,
                        reverse
                            ? pairing.secondParticipantId
                            : pairing.firstParticipantId,
                        reverse
                            ? pairing.firstParticipantId
                            : pairing.secondParticipantId
                    });
                }
            }
        }
    }
    result.roundCount =
        config.rounds * config.gamesPerPairing * roundsPerCycle;
    return result;
}
