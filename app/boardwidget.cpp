#include "boardwidget.h"

#include "move.h"

#include <QColor>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QSizePolicy>
#include <QSvgRenderer>

using namespace ChessGame;

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

void BoardWidget::setPosition(const ChessGame::Position& position)
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

ChessGame::PieceType BoardWidget::promptPromotion(ChessGame::Color color, const QPoint& globalPos) const
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
