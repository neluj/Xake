#pragma once

#include <string>

#include <QPoint>
#include <QVector>
#include <QWidget>

#include "move.h"
#include "position.h"
#include "types.h"

class QVariantAnimation;
class TestBoardWidget;

// Renders a square chessboard background and pieces from Qt resources.
class BoardWidget : public QWidget
{
    Q_OBJECT

public:
    explicit BoardWidget(QWidget *parent = nullptr);
    void setPosition(const Xake::Position& position, Xake::Move lastMove = Xake::NOMOVE);
    bool setFromFenString(const std::string& fen);
    void setMoveInputEnabled(bool enabled);

signals:
    void moveRequested(Xake::Move move);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;

private:
    friend class TestBoardWidget;

    struct AnimatedPiece {
        Xake::Piece piece = Xake::NO_PIECE;
        int fromSq = -1;
        int toSq = -1;
    };

    bool boardGeometry(QRect& boardRect, int& cellSize, int& labelMargin) const;
    bool squareFromPoint(const QPoint& point, int& outSq) const;
    Xake::PieceType promptPromotion(Xake::Color color, const QPoint& globalPos) const;
    void selectSquare(int sq);
    void clearSelection();
    void startMoveAnimation(const Xake::Position& previousPosition, Xake::Move move);
    void stopMoveAnimation();

    // Rendered sprite sheet of chess pieces (6 columns x 2 rows).
    QPixmap m_pieceset;
    Xake::Position m_position;
    QVariantAnimation *m_moveAnimation = nullptr;
    QVector<AnimatedPiece> m_animatedPieces;
    QVector<Xake::Move> m_legalMoves;
    qreal m_animationProgress = 1.0;
    int m_selectedSq = -1;
    bool m_moveInputEnabled = false;
};
