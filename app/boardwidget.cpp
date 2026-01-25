#include "boardwidget.h"

#include "fen.h"

#include <QColor>
#include <QMouseEvent>
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
    m_selectedSq = -1;
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

    const int cellSize = side / 8;
    if (cellSize <= 0) {
        return;
    }
    const int boardSize = cellSize * 8;
    const int x = (width() - boardSize) / 2;
    const int y = (height() - boardSize) / 2;
    QRect boardRect(x, y, boardSize, boardSize);

    const QColor lightSquare(240, 217, 181);
    const QColor darkSquare(181, 136, 99);
    for (int rank = 0; rank < 8; ++rank) {
        for (int file = 0; file < 8; ++file) {
            const bool isLight = ((rank + file) % 2) == 0;
            const QColor color = isLight ? lightSquare : darkSquare;
            const int px = boardRect.left() + file * cellSize;
            const int py = boardRect.top() + (7 - rank) * cellSize;
            painter.fillRect(QRect(px, py, cellSize, cellSize), color);
        }
    }

    if (m_selectedSq >= 0) {
        const int selFile = m_selectedSq % 8;
        const int selRank = m_selectedSq / 8;
        const int px = boardRect.left() + selFile * cellSize;
        const int py = boardRect.top() + (7 - selRank) * cellSize;
        painter.fillRect(QRect(px, py, cellSize, cellSize), QColor(80, 120, 200, 120));
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

void BoardWidget::mousePressEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton) {
        QWidget::mousePressEvent(event);
        return;
    }

    int sq = -1;
    if (!squareFromPoint(event->pos(), sq)) {
        m_selectedSq = -1;
        update();
        return;
    }

    if (m_selectedSq == -1) {
        Color color = WHITE;
        const Piece piece = m_position.pieceAt(sq, color);
        if (piece == NO_PIECE || color != m_position.stm) {
            return;
        }
        m_selectedSq = sq;
        update();
        return;
    }

    if (m_selectedSq == sq) {
        m_selectedSq = -1;
        update();
        return;
    }

    Color fromColor = WHITE;
    const Piece fromPiece = m_position.pieceAt(m_selectedSq, fromColor);
    if (fromPiece == NO_PIECE) {
        m_selectedSq = -1;
        update();
        return;
    }

    Color toColor = WHITE;
    const Piece toPiece = m_position.pieceAt(sq, toColor);
    if (toPiece != NO_PIECE && toColor == fromColor) {
        m_selectedSq = -1;
        update();
        return;
    }

    Piece promoPiece = NO_PIECE;
    int flags = MOVE_FLAG_NONE;
    if (toPiece != NO_PIECE) {
        flags |= MOVE_FLAG_CAPTURE;
    }
    if (fromPiece == PAWN) {
        const int toRank = sq / 8;
        if ((fromColor == WHITE && toRank == 7)
            || (fromColor == BLACK && toRank == 0)) {
            promoPiece = QUEEN;
            flags |= MOVE_FLAG_PROMOTION;
        }
    }

    const Move move = make_move(m_selectedSq,
                                sq,
                                move_piece_code(fromPiece),
                                move_piece_code(toPiece),
                                move_piece_code(promoPiece),
                                flags);

    emit moveRequested(move);
    m_selectedSq = -1;
    update();
}

bool BoardWidget::squareFromPoint(const QPoint& point, int& outSq) const
{
    const int side = qMin(width(), height());
    if (side <= 0) {
        return false;
    }
    const int cellSize = side / 8;
    if (cellSize <= 0) {
        return false;
    }
    const int boardSize = cellSize * 8;
    const int x = (width() - boardSize) / 2;
    const int y = (height() - boardSize) / 2;
    const QRect boardRect(x, y, boardSize, boardSize);
    if (!boardRect.contains(point)) {
        return false;
    }

    const int file = (point.x() - boardRect.left()) / cellSize;
    const int rank = 7 - (point.y() - boardRect.top()) / cellSize;
    if (file < 0 || file >= 8 || rank < 0 || rank >= 8) {
        return false;
    }

    outSq = rank * 8 + file;
    return true;
}
