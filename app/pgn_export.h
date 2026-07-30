#pragma once

#include <QString>
#include <QStringList>
#include <QVector>

struct PgnGameRecord {
    QString event;
    QString site = QStringLiteral("?");
    QString date;
    QString round = QStringLiteral("1");
    QString white;
    QString black;
    QString result = QStringLiteral("*");
    QString termination;
    QString startFen;
    QString opening;
    QString timeControl;
    QStringList movesUci;
    int openingMoveCount = 0;
};

QString pgnText(const QVector<PgnGameRecord>& games, QString* errorOut = nullptr);
bool writePgnFile(const QVector<PgnGameRecord>& games,
                  const QString& filePath,
                  QString* errorOut = nullptr);
