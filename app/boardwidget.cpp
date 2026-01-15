#include "boardwidget.h"

#include "fen.h"

#include <QPainter>
#include <QSizePolicy>

BoardWidget::BoardWidget(QWidget *parent)
    : QWidget(parent)
    , m_boardPixmap(":/assets/boards/board.png")
    , m_pieceset(":/assets/pieces/pieceset_64.png")
{
    // Let the layout expand the board widget.
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    setFromFenString("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
}

void BoardWidget::setPosition(const Position& position)
{
    m_position = position;
    update();
}

bool BoardWidget::setFromFenString(const std::string& fen)
{
    if (!setFromFen(m_position, fen)) {
        return false;
    }
    update();
    return true;
}

bool BoardWidget::movePiece(int fromSq, int toSq)
{
    if (!m_position.movePiece(fromSq, toSq)) {
        return false;
    }
    update();
    return true;
}

void BoardWidget::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);

    QPainter painter(this);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);

    if (m_boardPixmap.isNull()) {
        painter.fillRect(rect(), QColor(30, 30, 30));
        return;
    }

    // Fit the image into the largest square within the widget.
    const int side = qMin(width(), height());
    if (side <= 0) {
        return;
    }

    const int x = (width() - side) / 2;
    const int y = (height() - side) / 2;
    QRect boardRect(x, y, side, side);
    painter.drawPixmap(boardRect, m_boardPixmap, m_boardPixmap.rect());

    if (m_pieceset.isNull()) {
        return;
    }

    const double tileW = static_cast<double>(m_pieceset.width()) / 6.0;
    const double tileH = static_cast<double>(m_pieceset.height()) / 2.0;
    if (tileW <= 0.0 || tileH <= 0.0) {
        return;
    }

    // Fit the pieces into the board square grid.
    const double cell = static_cast<double>(boardRect.width()) / 8.0;
    static const int pieceToCol[PIECE_NB] = { 5, 1, 2, 0, 3, 4 };

    for (int sq = 0; sq < 64; ++sq) {
        Color color = WHITE;
        const Piece piece = m_position.pieceAt(sq, color);
        if (piece == NO_PIECE) {
            continue;
        }

        const int col = pieceToCol[piece];
        const int row = (color == WHITE) ? 1 : 0;
        const QRectF source(col * tileW, row * tileH, tileW, tileH);

        const int file = sq % 8;
        const int rank = sq / 8;
        const double px = boardRect.left() + file * cell;
        const double py = boardRect.top() + (7 - rank) * cell;
        const QRectF target(px, py, cell, cell);
        painter.drawPixmap(target, m_pieceset, source);
    }
}
