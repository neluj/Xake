#pragma once

#include <QString>
#include <QStringList>
#include <QVector>

struct OpeningEntry {
    int sourceIndex = 0;
    QString name;
    QString startFen;
    QString finalFen;
    QStringList movesUci;
};

bool loadOpeningFile(const QString& filePath,
                     QVector<OpeningEntry>* openings,
                     QString* errorOut = nullptr);
bool parsePgnOpenings(const QString& contents,
                      QVector<OpeningEntry>* openings,
                      QString* errorOut = nullptr);
bool parseEpdOpenings(const QString& contents,
                      QVector<OpeningEntry>* openings,
                      QString* errorOut = nullptr);
