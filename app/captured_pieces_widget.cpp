#include "captured_pieces_widget.h"

#include <QPainter>
#include <QPalette>
#include <QSizePolicy>

using namespace Xake;

namespace {

constexpr int kOuterMargin = 8;
constexpr int kRowGap = 7;
constexpr int kIconGap = 3;
constexpr int kMaximumIconSize = 38;
constexpr PieceType kCapturedPieceOrder[] = {
    QUEEN,
    ROOK,
    BISHOP,
    KNIGHT,
    PAWN,
    KING
};

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

CapturedPiecesWidget::CapturedPiecesWidget(QWidget *parent)
    : QWidget(parent)
    , m_pieceset(QStringLiteral(":/assets/pieces/pieceset.png"))
{
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    setMinimumHeight(minimumSizeHint().height());
    setAccessibleName(tr("Captured pieces"));
}

void CapturedPiecesWidget::setCapturedPieces(
    const QVector<Piece>& pieces)
{
    m_capturedPieces.clear();
    m_capturedPieces.reserve(pieces.size());
    for (const Piece piece : pieces) {
        if (piece == NO_PIECE) {
            continue;
        }
        const Color color = piece_color(piece);
        if (color == WHITE || color == BLACK) {
            m_capturedPieces.append(piece);
        }
    }
    update();
}

QVector<Piece> CapturedPiecesWidget::piecesForColor(Color color) const
{
    QVector<Piece> pieces;
    for (const PieceType type : kCapturedPieceOrder) {
        for (const Piece piece : m_capturedPieces) {
            if (piece_color(piece) == color && piece_type(piece) == type) {
                pieces.append(piece);
            }
        }
    }
    return pieces;
}

QSize CapturedPiecesWidget::sizeHint() const
{
    return QSize(480, 104);
}

QSize CapturedPiecesWidget::minimumSizeHint() const
{
    return QSize(280, 88);
}

void CapturedPiecesWidget::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);

    const int availableHeight =
        qMax(2, height() - 2 * kOuterMargin - kRowGap);
    const int rowHeight = availableHeight / 2;
    const int dividerY = kOuterMargin + rowHeight + kRowGap / 2;

    painter.setPen(palette().color(QPalette::Mid));
    painter.drawLine(kOuterMargin,
                     dividerY,
                     width() - kOuterMargin,
                     dividerY);

    const auto drawRow =
        [this, &painter, rowHeight](Color color, int top) {
        const QVector<Piece> pieces = piecesForColor(color);
        const int iconsLeft = kOuterMargin;
        const int iconsWidth =
            qMax(0, width() - iconsLeft - kOuterMargin);
        if (pieces.isEmpty()) {
            return;
        }

        const int gapsWidth = kIconGap * qMax(0, pieces.size() - 1);
        const int fitSize =
            qMax(1, (iconsWidth - gapsWidth) / pieces.size());
        const int iconSize =
            qMin(kMaximumIconSize, qMin(rowHeight - 6, fitSize));
        const qreal tileWidth =
            m_pieceset.isNull() ? 0.0 : m_pieceset.width() / 6.0;
        const qreal tileHeight =
            m_pieceset.isNull() ? 0.0 : m_pieceset.height() / 2.0;
        int x = iconsLeft;
        const int y = top + (rowHeight - iconSize) / 2;

        for (const Piece piece : pieces) {
            const QRect tileRect(x, y, iconSize, iconSize);
            QColor tileColor = palette().color(QPalette::AlternateBase);
            tileColor.setAlpha(180);
            painter.setBrush(tileColor);
            painter.setPen(palette().color(QPalette::Mid));
            painter.drawRoundedRect(tileRect, 4, 4);

            const int column = pieceSpriteColumn(piece);
            if (column >= 0 && tileWidth > 0.0 && tileHeight > 0.0) {
                const int row = piece_color(piece) == WHITE ? 0 : 1;
                const QRectF source(column * tileWidth,
                                    row * tileHeight,
                                    tileWidth,
                                    tileHeight);
                const QRectF target = QRectF(tileRect).adjusted(2, 2, -2, -2);
                painter.drawPixmap(target, m_pieceset, source);
            } else {
                painter.setPen(palette().color(QPalette::WindowText));
                painter.drawText(tileRect,
                                 Qt::AlignCenter,
                                 QString::fromLatin1(
                                     PIECE_NAMES.data() + int(piece),
                                     1).toUpper());
            }
            x += iconSize + kIconGap;
        }
    };

    drawRow(WHITE, kOuterMargin);
    drawRow(BLACK, kOuterMargin + rowHeight + kRowGap);
}
