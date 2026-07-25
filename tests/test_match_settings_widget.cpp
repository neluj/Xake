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

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    TestMatchSettingsWidget test;
    return QTest::qExec(&test, argc, argv);
}

#include "test_match_settings_widget.moc"
