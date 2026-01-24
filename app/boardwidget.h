#pragma once

#include <string>

#include <QWidget>

#include "position.h"

// Renders a square chessboard background and pieces from Qt resources.
class BoardWidget : public QWidget
{
public:
    explicit BoardWidget(QWidget *parent = nullptr);
    void setPosition(const Position& position);
    bool setFromFenString(const std::string& fen);
    bool movePiece(int fromSq, int toSq);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    // Rendered sprite sheet of chess pieces (6 columns x 2 rows).
    QPixmap m_pieceset;
    Position m_position;
};
