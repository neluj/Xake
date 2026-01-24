#include "boardwidget.h"

#include "fen.h"

#include <QColor>
#include <QPainter>
#include <QSizePolicy>
#include <QSvgRenderer>

BoardWidget::BoardWidget(QWidget *parent)
    : QWidget(parent)
{
    // Let the layout expand the board widget.
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    const QString piecesPathPng = QStringLiteral(":/assets/pieces/pieceset.png");
    m_pieceset = QPixmap(piecesPathPng);
    if (m_pieceset.isNull()) {
        QSvgRenderer renderer(QStringLiteral(":/assets/pieces/pieceset.svg"));
        if (renderer.isValid()) {
            QSize size = renderer.defaultSize();
            if (size.isEmpty()) {
                size = QSize(270, 90);
            }
            QImage image(size, QImage::Format_ARGB32_Premultiplied);
            image.fill(Qt::transparent);
            QPainter painter(&image);
            renderer.render(&painter);
            m_pieceset = QPixmap::fromImage(image);
        }
    }

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

    // Fit the image into the largest square within the widget.
    const int side = qMin(width(), height());
    if (side <= 0) {
        return;
    }

    const int x = (width() - side) / 2;
    const int y = (height() - side) / 2;
    QRect boardRect(x, y, side, side);

    const QColor lightSquare(240, 217, 181);
    const QColor darkSquare(181, 136, 99);
    const int cellSize = side / 8;
    if (cellSize <= 0) {
        return;
    }
    for (int rank = 0; rank < 8; ++rank) {
        for (int file = 0; file < 8; ++file) {
            const bool isLight = ((rank + file) % 2) == 0;
            const QColor color = isLight ? lightSquare : darkSquare;
            const int px = boardRect.left() + file * cellSize;
            const int py = boardRect.top() + (7 - rank) * cellSize;
            painter.fillRect(QRect(px, py, cellSize, cellSize), color);
        }
    }

    if (m_pieceset.isNull()) {
        return;
    }

    const double tileW = static_cast<double>(m_pieceset.width()) / 6.0;
    const double tileH = static_cast<double>(m_pieceset.height()) / 2.0;
    if (tileW <= 0.0 || tileH <= 0.0) {
        return;
    }

    // Fit the pieces into the board square grid.
    const double cell = static_cast<double>(cellSize);
    static const int pieceToCol[PIECE_NB] = { 5, 3, 2, 4, 1, 0 };

    for (int sq = 0; sq < 64; ++sq) {
        Color color = WHITE;
        const Piece piece = m_position.pieceAt(sq, color);
        if (piece == NO_PIECE) {
            continue;
        }

        const int col = pieceToCol[piece];
        const int row = (color == WHITE) ? 0 : 1;
        const QRectF source(col * tileW, row * tileH, tileW, tileH);

        const int file = sq % 8;
        const int rank = sq / 8;
        const double px = boardRect.left() + file * cell;
        const double py = boardRect.top() + (7 - rank) * cell;
        const QRectF target(px, py, cell, cell);
        painter.drawPixmap(target, m_pieceset, source);
    }
}
