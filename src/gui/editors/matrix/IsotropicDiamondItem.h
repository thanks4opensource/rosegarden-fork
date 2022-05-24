/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*- vi:set ts=8 sts=4 sw=4: */

/*
    Rosegarden
    A MIDI and audio sequencer and musical notation editor.
    Copyright 2000-2022 the Rosegarden development team.

    Other copyrights also apply to some parts of this work.  Please
    see the AUTHORS file and individual file headers for details.

    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License as
    published by the Free Software Foundation; either version 2 of the
    License, or (at your option) any later version.  See the file
    COPYING included with this distribution for more information.
*/

#ifndef RG_ISOTROPICDIAMONDITEM_H
#define RG_ISOTROPICDIAMONDITEM_H

#include <QGraphicsPolygonItem>
#include <QString>
#include <QPainter>

namespace Rosegarden
{

class IsotropicDiamondItem : public QGraphicsPolygonItem {
  public:
    IsotropicDiamondItem() : m_firstBadPaint(true) {}

    ~IsotropicDiamondItem() {}

    void setSize(const qreal halfWidth, const qreal halfHeight)
    {
        m_halfWidth     = halfWidth;
        m_halfHeight    = halfHeight;
        m_firstBadPaint = true;
    }

    void setPos(const QPointF &point)
    {
        m_rawPos = point;
        m_firstBadPaint = true;
        QGraphicsPolygonItem::setPos(point);
    }

    void setPos(qreal x, qreal y)
    {
        m_rawPos = QPointF(x, y);
        m_firstBadPaint = true;
        QGraphicsPolygonItem::setPos(x, y);
    }

    void hide()
    {
        m_firstBadPaint = true;
        QGraphicsPolygonItem::hide();
    }

    void paint(QPainter *painter, const QStyleOptionGraphicsItem *item,
               QWidget *widget) override
    {
        const QTransform &t = painter->transform();
        qreal m11    = t.m11(),
              m22    = t.m22(),
              xScale = 1.0;

        if (m11 < 1.0 || m22 < 1.0) xScale = 1.0 / m11;
        else if (m11 > m22) xScale = m22 / m11;

#if 0
        qreal x   = m_rawPos.x()       -       BORDER_WIDTH * xScale,
              y   = m_rawPos.y()       -       BORDER_WIDTH * yScale,
              w   = m_rawPoly.width()  + 2.0 * BORDER_WIDTH * xScale,
              h   = m_rawPoly.height() + 2.0 * BORDER_WIDTH * yScale;
#endif

        qreal   w = m_halfWidth * xScale,  //    + BORDER_WIDTH * xScale,
                h = m_halfHeight; // + BORDER_WIDTH * yScale;
        QPolygonF polygon;
        polygon << QPointF( 0.0,  0.0    )
                << QPointF( w  ,  h * 0.5)
                << QPointF( 0.0,  h      )
                << QPointF(-w  ,  h * 0.5)
                << QPointF( 0.0,  0.0    );

        // Incredible hack
        if (m_firstBadPaint) {
            m_firstBadPaint = false;
        }

        // Must do this instead of painter->drawPolygon(), etc.
        setPolygon(polygon);
        QGraphicsPolygonItem::setPen(pen());
        QGraphicsPolygonItem::paint(painter, item, widget);
    }

  protected:
    static constexpr qreal  BORDER_WIDTH = 4.0;
    qreal m_halfWidth, m_halfHeight;
    QPointF m_rawPos;
    bool m_firstBadPaint;
};

}  // namespace Rosegarden

#endif  // #ifndef RG_ISOTROPICDIAMONDITEM
