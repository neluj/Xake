#pragma once

#include <string>

#include <QPoint>
#include <QWidget>

#include "position.h"
#include "types.h"

// Renders a square chessboard background and pieces from Qt resources.
class BoardWidget : public QWidget
{
    Q_OBJECT

public:
    explicit BoardWidget(QWidget *parent = nullptr);
    void setPosition(const Position& position);
    bool setFromFenString(const std::string& fen);
    bool movePiece(int fromSq, int toSq);

signals:
    void moveRequested(Move move);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;

private:
    bool squareFromPoint(const QPoint& point, int& outSq) const;

    // Rendered sprite sheet of chess pieces (6 columns x 2 rows).
    QPixmap m_pieceset;
    Position m_position;
    int m_selectedSq = -1;
};
