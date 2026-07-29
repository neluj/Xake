#pragma once

#include "types.h"

#include <QPixmap>
#include <QVector>
#include <QWidget>

class CapturedPiecesWidget : public QWidget
{
public:
    explicit CapturedPiecesWidget(QWidget *parent = nullptr);

    void setCapturedPieces(const QVector<Xake::Piece>& pieces);
    QVector<Xake::Piece> piecesForColor(Xake::Color color) const;

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QVector<Xake::Piece> m_capturedPieces;
    QPixmap m_pieceset;
};
