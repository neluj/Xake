#include "boardwidget.h"

#include "move.h"

#include <QColor>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QSizePolicy>
#include <QSvgRenderer>

using namespace Xake;

namespace {

Piece pieceAt(const Position& position, int sq)
{
    if (sq < 0 || sq >= SQ64_SIZE) {
        return NO_PIECE;
    }
    return position.get_mailbox_piece(Square64(sq));
}

int pieceSpriteColumn(Piece piece)
{
    switch (piece_type(piece)) {
    case PAWN:
        return 5;
    case KNIGHT:
        return 3;
    case BISHOP:
        return 2;
    case ROOK:
        return 4;
    case QUEEN:
        return 1;
    case KING:
        return 0;
    default:
        return -1;
    }
}

SpecialMove promotionMove(PieceType pieceType)
{
    switch (pieceType) {
    case KNIGHT:
        return PROMOTION_KNIGHT;
    case BISHOP:
        return PROMOTION_BISHOP;
    case ROOK:
        return PROMOTION_ROOK;
    case QUEEN:
        return PROMOTION_QUEEN;
    default:
        return NO_SPECIAL;
    }
}

} // namespace

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

void BoardWidget::setPosition(const Xake::Position& position)
{
    m_position = position;
    m_selectedSq = -1;
    update();
}

bool BoardWidget::setFromFenString(const std::string& fen)
{
    if (!m_position.set_FEN(fen)) {
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
    painter.setRenderHint(QPainter::TextAntialiasing, true);

    QRect boardRect;
    int cellSize = 0;
    int labelMargin = 0;
    if (!boardGeometry(boardRect, cellSize, labelMargin)) {
        return;
    }

    const QColor lightSquare(240, 217, 181);
    const QColor darkSquare(181, 136, 99);
    for (int rank = 0; rank < 8; ++rank) {
        for (int file = 0; file < 8; ++file) {
            const bool isLight = ((rank + file) % 2) != 0;
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

    if (!m_pieceset.isNull()) {
        const double tileW = static_cast<double>(m_pieceset.width()) / 6.0;
        const double tileH = static_cast<double>(m_pieceset.height()) / 2.0;
        if (tileW > 0.0 && tileH > 0.0) {
            // Fit the pieces into the board square grid.
            const double cell = static_cast<double>(cellSize);

            for (int sq = 0; sq < 64; ++sq) {
                const Piece piece = pieceAt(m_position, sq);
                if (piece == NO_PIECE) {
                    continue;
                }

                const int col = pieceSpriteColumn(piece);
                if (col < 0) {
                    continue;
                }
                const int row = (piece_color(piece) == WHITE) ? 0 : 1;
                const QRectF source(col * tileW, row * tileH, tileW, tileH);

                const int file = sq % 8;
                const int rank = sq / 8;
                const double px = boardRect.left() + file * cell;
                const double py = boardRect.top() + (7 - rank) * cell;
                const QRectF target(px, py, cell, cell);
                painter.drawPixmap(target, m_pieceset, source);
            }
        }
    }

    QFont coordinateFont = painter.font();
    coordinateFont.setBold(true);
    coordinateFont.setPixelSize(qBound(8, cellSize / 5, 16));
    painter.setFont(coordinateFont);
    painter.setPen(palette().color(QPalette::WindowText));

    for (int file = 0; file < 8; ++file) {
        const QRect labelRect(boardRect.left() + file * cellSize,
                              boardRect.bottom() + 1,
                              cellSize,
                              labelMargin);
        painter.drawText(labelRect,
                         Qt::AlignHCenter | Qt::AlignVCenter,
                         QString(QChar('a' + file)));
    }

    for (int rank = 0; rank < 8; ++rank) {
        const QRect labelRect(boardRect.left() - labelMargin,
                              boardRect.top() + (7 - rank) * cellSize,
                              labelMargin,
                              cellSize);
        painter.drawText(labelRect,
                         Qt::AlignHCenter | Qt::AlignVCenter,
                         QString::number(rank + 1));
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
        const Piece piece = pieceAt(m_position, sq);
        if (piece == NO_PIECE || piece_color(piece) != m_position.get_side_to_move()) {
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

    const Piece fromPiece = pieceAt(m_position, m_selectedSq);
    if (fromPiece == NO_PIECE) {
        m_selectedSq = -1;
        update();
        return;
    }
    const Color fromColor = piece_color(fromPiece);

    const Piece toPiece = pieceAt(m_position, sq);
    if (toPiece != NO_PIECE && piece_color(toPiece) == fromColor) {
        m_selectedSq = -1;
        update();
        return;
    }

    PieceType promoPiece = NO_PIECE_TYPE;
    if (piece_type(fromPiece) == PAWN) {
        const int toRank = sq / 8;
        if ((fromColor == WHITE && toRank == 7)
            || (fromColor == BLACK && toRank == 0)) {
            promoPiece = promptPromotion(fromColor, event->globalPosition().toPoint());
            if (promoPiece == NO_PIECE_TYPE) {
                m_selectedSq = -1;
                update();
                return;
            }
        }
    }

    const Move move = make_quiet_move(Square64(m_selectedSq),
                                      Square64(sq),
                                      promotionMove(promoPiece));

    emit moveRequested(move);
    m_selectedSq = -1;
    update();
}

bool BoardWidget::boardGeometry(QRect& boardRect, int& cellSize, int& labelMargin) const
{
    const int side = qMin(width(), height());
    if (side <= 0) {
        return false;
    }

    labelMargin = qBound(12, side / 24, 24);
    const int availableSide = side - 2 * labelMargin;
    cellSize = availableSide / 8;
    if (cellSize <= 0) {
        return false;
    }

    const int boardSize = cellSize * 8;
    const int contentSize = boardSize + 2 * labelMargin;
    const int x = (width() - contentSize) / 2 + labelMargin;
    const int y = (height() - contentSize) / 2 + labelMargin;
    boardRect = QRect(x, y, boardSize, boardSize);
    return true;
}

bool BoardWidget::squareFromPoint(const QPoint& point, int& outSq) const
{
    QRect boardRect;
    int cellSize = 0;
    int labelMargin = 0;
    if (!boardGeometry(boardRect, cellSize, labelMargin)) {
        return false;
    }
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

Xake::PieceType BoardWidget::promptPromotion(Xake::Color color, const QPoint& globalPos) const
{
    Q_UNUSED(color);

    QMenu menu;
    QAction *queen = menu.addAction(tr("Queen"));
    QAction *rook = menu.addAction(tr("Rook"));
    QAction *bishop = menu.addAction(tr("Bishop"));
    QAction *knight = menu.addAction(tr("Knight"));

    QAction *chosen = menu.exec(globalPos);
    if (!chosen) {
        return NO_PIECE_TYPE;
    }

    if (chosen == queen) {
        return QUEEN;
    }
    if (chosen == rook) {
        return ROOK;
    }
    if (chosen == bishop) {
        return BISHOP;
    }
    if (chosen == knight) {
        return KNIGHT;
    }

    return NO_PIECE_TYPE;
}
