#include <QCheckBox>
#include <QComboBox>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QApplication>
#include <QtTest/QtTest>

#include "match_settings_widget.h"

class TestMatchSettingsWidget : public QObject
{
    Q_OBJECT

private slots:
    void presetDisablesManualTimeFields();
    void customRestoresManualTimeFields();
    void openingFileDisablesManualPosition();
    void restoresSavedConfiguration();
};

void TestMatchSettingsWidget::presetDisablesManualTimeFields()
{
    MatchSettingsWidget widget;

    auto *timeControlCombo = widget.findChild<QComboBox*>(QStringLiteral("timeControlCombo"));
    auto *baseTimeSpin = widget.findChild<QSpinBox*>(QStringLiteral("baseTimeSpin"));
    auto *incrementSpin = widget.findChild<QSpinBox*>(QStringLiteral("incrementSpin"));
    auto *movesToGoSpin = widget.findChild<QSpinBox*>(QStringLiteral("movesToGoSpin"));

    QVERIFY(timeControlCombo);
    QVERIFY(baseTimeSpin);
    QVERIFY(incrementSpin);
    QVERIFY(movesToGoSpin);

    QCOMPARE(timeControlCombo->currentText(), QStringLiteral("Bullet"));
    QCOMPARE(baseTimeSpin->value(), 60);
    QCOMPARE(incrementSpin->value(), 0);
    QCOMPARE(movesToGoSpin->value(), 0);
    QCOMPARE(baseTimeSpin->isEnabled(), false);
    QCOMPARE(incrementSpin->isEnabled(), false);
    QCOMPARE(movesToGoSpin->isEnabled(), false);

    timeControlCombo->setCurrentText(QStringLiteral("Blitz"));
    QCOMPARE(baseTimeSpin->value(), 300);
    QCOMPARE(incrementSpin->value(), 0);
    QCOMPARE(movesToGoSpin->value(), 0);

    timeControlCombo->setCurrentText(QStringLiteral("Rapid"));
    QCOMPARE(baseTimeSpin->value(), 600);
    QCOMPARE(incrementSpin->value(), 0);
    QCOMPARE(movesToGoSpin->value(), 0);

    const GameConfig config = widget.gameConfig();
    QCOMPARE(config.timeControl, QStringLiteral("Rapid"));
    QCOMPARE(config.baseTimeSeconds, 600);
    QCOMPARE(config.incrementSeconds, 0);
    QCOMPARE(config.movesToGo, 0);
}

void TestMatchSettingsWidget::customRestoresManualTimeFields()
{
    MatchSettingsWidget widget;

    auto *timeControlCombo = widget.findChild<QComboBox*>(QStringLiteral("timeControlCombo"));
    auto *baseTimeSpin = widget.findChild<QSpinBox*>(QStringLiteral("baseTimeSpin"));
    auto *incrementSpin = widget.findChild<QSpinBox*>(QStringLiteral("incrementSpin"));
    auto *movesToGoSpin = widget.findChild<QSpinBox*>(QStringLiteral("movesToGoSpin"));

    QVERIFY(timeControlCombo);
    QVERIFY(baseTimeSpin);
    QVERIFY(incrementSpin);
    QVERIFY(movesToGoSpin);

    timeControlCombo->setCurrentText(QStringLiteral("Custom"));
    QVERIFY(baseTimeSpin->isEnabled());
    QVERIFY(incrementSpin->isEnabled());
    QVERIFY(movesToGoSpin->isEnabled());

    baseTimeSpin->setValue(123);
    incrementSpin->setValue(7);
    movesToGoSpin->setValue(18);

    timeControlCombo->setCurrentText(QStringLiteral("Blitz"));
    QCOMPARE(baseTimeSpin->isEnabled(), false);
    QCOMPARE(incrementSpin->isEnabled(), false);
    QCOMPARE(movesToGoSpin->isEnabled(), false);
    QCOMPARE(baseTimeSpin->value(), 300);
    QCOMPARE(incrementSpin->value(), 0);
    QCOMPARE(movesToGoSpin->value(), 0);

    timeControlCombo->setCurrentText(QStringLiteral("Custom"));
    QCOMPARE(baseTimeSpin->value(), 123);
    QCOMPARE(incrementSpin->value(), 7);
    QCOMPARE(movesToGoSpin->value(), 18);

    const GameConfig config = widget.gameConfig();
    QCOMPARE(config.timeControl, QStringLiteral("Custom"));
    QCOMPARE(config.baseTimeSeconds, 123);
    QCOMPARE(config.incrementSeconds, 7);
    QCOMPARE(config.movesToGo, 18);
}

void TestMatchSettingsWidget::openingFileDisablesManualPosition()
{
    MatchSettingsWidget widget;

    auto *openingCheck =
        widget.findChild<QCheckBox*>(QStringLiteral("useOpeningFileCheckBox"));
    auto *openingEdit =
        widget.findChild<QLineEdit*>(QStringLiteral("openingFileEdit"));
    auto *openingBrowse =
        widget.findChild<QPushButton*>(QStringLiteral("openingFileBrowseButton"));
    auto *startPosCheck =
        widget.findChild<QCheckBox*>(QStringLiteral("useStartPosCheckBox"));
    auto *startPosition =
        widget.findChild<QLineEdit*>(QStringLiteral("startPositionEdit"));

    QVERIFY(openingCheck);
    QVERIFY(openingEdit);
    QVERIFY(openingBrowse);
    QVERIFY(startPosCheck);
    QVERIFY(startPosition);
    QVERIFY(!openingEdit->isEnabled());
    QVERIFY(!openingBrowse->isEnabled());

    openingCheck->setChecked(true);
    QVERIFY(openingEdit->isEnabled());
    QVERIFY(openingBrowse->isEnabled());
    QVERIFY(!startPosCheck->isEnabled());
    QVERIFY(!startPosition->isEnabled());

    openingEdit->setText(QStringLiteral("C:/openings/book.pgn"));
    const GameConfig config = widget.gameConfig();
    QVERIFY(config.useOpeningFile);
    QCOMPARE(config.openingFilePath, QStringLiteral("C:/openings/book.pgn"));
}

void TestMatchSettingsWidget::restoresSavedConfiguration()
{
    MatchSettingsWidget widget;
    MatchConfig expected;
    expected.player1.type = PlayerType::Engine;
    expected.player1.enginePath =
        QStringLiteral("C:/engines/stockfish.exe");
    expected.player2.type = PlayerType::Human;
    expected.player2.name = QStringLiteral("Player");
    expected.game.timeControl = QStringLiteral("Custom");
    expected.game.baseTimeSeconds = 75;
    expected.game.incrementSeconds = 3;
    expected.game.movesToGo = 20;
    expected.game.useStartPos = false;
    expected.game.startPosition =
        QStringLiteral("8/8/8/8/8/8/4K3/7k w - - 0 1");
    expected.game.useOpeningFile = true;
    expected.game.openingFilePath =
        QStringLiteral("C:/openings/book.pgn");

    widget.setConfig(expected);

    const PlayerConfig player1 = widget.player1Config();
    const PlayerConfig player2 = widget.player2Config();
    const GameConfig game = widget.gameConfig();
    QCOMPARE(player1.type, PlayerType::Engine);
    QCOMPARE(player1.enginePath, expected.player1.enginePath);
    QCOMPARE(player2.type, PlayerType::Human);
    QCOMPARE(player2.name, expected.player2.name);
    QCOMPARE(game.timeControl, expected.game.timeControl);
    QCOMPARE(game.baseTimeSeconds, expected.game.baseTimeSeconds);
    QCOMPARE(game.incrementSeconds, expected.game.incrementSeconds);
    QCOMPARE(game.movesToGo, expected.game.movesToGo);
    QCOMPARE(game.useOpeningFile, true);
    QCOMPARE(game.openingFilePath, expected.game.openingFilePath);
}

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    TestMatchSettingsWidget test;
    return QTest::qExec(&test, argc, argv);
}

#include "test_match_settings_widget.moc"
