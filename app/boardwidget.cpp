#include "boardwidget.h"

#include "move.h"
#include "movegen.h"

#include <QColor>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QSizePolicy>
#include <QSvgRenderer>
#include <QVariantAnimation>

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

    m_moveAnimation = new QVariantAnimation(this);
    m_moveAnimation->setDuration(180);
    m_moveAnimation->setStartValue(0.0);
    m_moveAnimation->setEndValue(1.0);
    m_moveAnimation->setEasingCurve(QEasingCurve::OutCubic);
    connect(m_moveAnimation, &QVariantAnimation::valueChanged, this,
            [this](const QVariant& value) {
        m_animationProgress = value.toReal();
        update();
    });
    connect(m_moveAnimation, &QVariantAnimation::finished, this, [this]() {
        m_animationProgress = 1.0;
        m_animatedPieces.clear();
        update();
    });

    setFromFenString("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
}

void BoardWidget::setPosition(const Xake::Position& position, Xake::Move lastMove)
{
    const Position previousPosition = m_position;
    m_position = position;
    clearSelection();

    if (lastMove == NOMOVE) {
        stopMoveAnimation();
    } else {
        startMoveAnimation(previousPosition, lastMove);
    }
    update();
}

void BoardWidget::setMoveInputEnabled(bool enabled)
{
    if (m_moveInputEnabled == enabled) {
        return;
    }

    m_moveInputEnabled = enabled;
    if (!enabled) {
        clearSelection();
        update();
    }
}

bool BoardWidget::setFromFenString(const std::string& fen)
{
    stopMoveAnimation();
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

    painter.setRenderHint(QPainter::Antialiasing, true);
    bool paintedDestinations[SQ64_SIZE] = {};
    const QColor targetColor(38, 122, 105, 190);
    for (const Move move : m_legalMoves) {
        const int destination = move_to(move);
        if (paintedDestinations[destination]) {
            continue;
        }
        paintedDestinations[destination] = true;

        const int file = destination % 8;
        const int rank = destination / 8;
        const QRectF squareRect(boardRect.left() + file * cellSize,
                                boardRect.top() + (7 - rank) * cellSize,
                                cellSize,
                                cellSize);
        if (is_capture(move)) {
            QPen capturePen(targetColor, qMax(2, cellSize / 12));
            painter.setPen(capturePen);
            painter.setBrush(Qt::NoBrush);
            painter.drawEllipse(squareRect.adjusted(cellSize * 0.08,
                                                    cellSize * 0.08,
                                                    -cellSize * 0.08,
                                                    -cellSize * 0.08));
        } else {
            painter.setPen(Qt::NoPen);
            painter.setBrush(targetColor);
            const qreal radius = qMax<qreal>(3.0, cellSize * 0.13);
            painter.drawEllipse(squareRect.center(), radius, radius);
        }
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
                bool animatedDestination = false;
                for (const AnimatedPiece& animatedPiece : m_animatedPieces) {
                    if (animatedPiece.toSq == sq) {
                        animatedDestination = true;
                        break;
                    }
                }
                if (animatedDestination) {
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

            for (const AnimatedPiece& animatedPiece : m_animatedPieces) {
                const int col = pieceSpriteColumn(animatedPiece.piece);
                if (col < 0) {
                    continue;
                }

                const int row = (piece_color(animatedPiece.piece) == WHITE) ? 0 : 1;
                const QRectF source(col * tileW, row * tileH, tileW, tileH);
                const int fromFile = animatedPiece.fromSq % 8;
                const int fromRank = animatedPiece.fromSq / 8;
                const int toFile = animatedPiece.toSq % 8;
                const int toRank = animatedPiece.toSq / 8;
                const qreal file = fromFile + (toFile - fromFile) * m_animationProgress;
                const qreal rank = fromRank + (toRank - fromRank) * m_animationProgress;
                const qreal px = boardRect.left() + file * cell;
                const qreal py = boardRect.top() + (7.0 - rank) * cell;
                painter.drawPixmap(QRectF(px, py, cell, cell), m_pieceset, source);
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

void BoardWidget::selectSquare(int sq)
{
    clearSelection();

    const Piece piece = pieceAt(m_position, sq);
    if (piece == NO_PIECE || piece_color(piece) != m_position.get_side_to_move()) {
        return;
    }

    m_selectedSq = sq;
    MoveGen::MoveList moveList;
    MoveGen::generate_pseudo_moves(m_position, moveList);
    for (int index = 0; index < moveList.size; ++index) {
        const Move move = moveList.moves[index];
        if (move_from(move) != sq) {
            continue;
        }

        Position testPosition = m_position;
        if (testPosition.do_move(move)) {
            m_legalMoves.append(move);
        }
    }
}

void BoardWidget::clearSelection()
{
    m_selectedSq = -1;
    m_legalMoves.clear();
}

void BoardWidget::startMoveAnimation(const Position& previousPosition, Move move)
{
    stopMoveAnimation();

    const int fromSq = move_from(move);
    const int toSq = move_to(move);
    const Piece movingPiece = pieceAt(previousPosition, fromSq);
    if (movingPiece == NO_PIECE || pieceAt(m_position, toSq) == NO_PIECE) {
        update();
        return;
    }

    m_animatedPieces.append({movingPiece, fromSq, toSq});

    if (move_special(move) == CASTLE) {
        const bool kingSide = toSq > fromSq;
        const int rookFromSq = kingSide ? fromSq + 3 : fromSq - 4;
        const int rookToSq = kingSide ? fromSq + 1 : fromSq - 1;
        const Piece rook = pieceAt(previousPosition, rookFromSq);
        if (rook != NO_PIECE && pieceAt(m_position, rookToSq) != NO_PIECE) {
            m_animatedPieces.append({rook, rookFromSq, rookToSq});
        }
    }

    m_animationProgress = 0.0;
    m_moveAnimation->start();
    update();
}

void BoardWidget::stopMoveAnimation()
{
    if (m_moveAnimation) {
        m_moveAnimation->stop();
    }
    m_animationProgress = 1.0;
    m_animatedPieces.clear();
}

void BoardWidget::mousePressEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton) {
        QWidget::mousePressEvent(event);
        return;
    }
    if (!m_moveInputEnabled) {
        return;
    }

    int sq = -1;
    if (!squareFromPoint(event->pos(), sq)) {
        clearSelection();
        update();
        return;
    }

    if (m_selectedSq == -1) {
        selectSquare(sq);
        update();
        return;
    }

    if (m_selectedSq == sq) {
        clearSelection();
        update();
        return;
    }

    QVector<Move> destinationMoves;
    for (const Move move : m_legalMoves) {
        if (move_to(move) == sq) {
            destinationMoves.append(move);
        }
    }

    if (destinationMoves.isEmpty()) {
        const Piece clickedPiece = pieceAt(m_position, sq);
        if (clickedPiece != NO_PIECE
            && piece_color(clickedPiece) == m_position.get_side_to_move()) {
            selectSquare(sq);
            update();
        }
        return;
    }

    const Piece fromPiece = pieceAt(m_position, m_selectedSq);
    if (fromPiece == NO_PIECE) {
        clearSelection();
        update();
        return;
    }
    const Color fromColor = piece_color(fromPiece);

    PieceType promoPiece = NO_PIECE_TYPE;
    if (destinationMoves.size() > 1) {
        promoPiece = promptPromotion(fromColor, event->globalPosition().toPoint());
        if (promoPiece == NO_PIECE_TYPE) {
            return;
        }
    }

    Move selectedMove = NOMOVE;
    for (const Move move : destinationMoves) {
        if (promoted_piece(move) == promoPiece) {
            selectedMove = move;
            break;
        }
    }
    if (selectedMove == NOMOVE) {
        return;
    }

    emit moveRequested(selectedMove);
    clearSelection();
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
