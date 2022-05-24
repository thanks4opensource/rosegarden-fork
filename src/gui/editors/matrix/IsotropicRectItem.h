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

#ifndef RG_ISOTROPICRECTITEM_H
#define RG_ISOTROPICRECTITEM_H

#include <QGraphicsRectItem>
#include <QString>
#include <QPainter>

namespace Rosegarden
{

// A QGraphicsRectItem that scales isotropically, maintaing the
// aspect ratio of the rect characters without stretching, regardless
// if the QGraphicsScene containing the rect is scaled anisotropically.
// Rect only scales according to the QGraphicsScene vertical zoom factor.

class IsotropicRectItem : public QGraphicsRectItem {
  public:
    IsotropicRectItem() : QGraphicsRectItem(), m_firstBadPaint(true)
    {
        setPen(Qt::NoPen);
    }

    void setRect(const QRectF &inRect)
    {
        m_rawRect = inRect;
        m_firstBadPaint = true;
        QGraphicsRectItem::setRect(inRect);
    }

    void setPos(const QPointF &point)
    {
        m_rawPos = point;
        m_firstBadPaint = true;
        QGraphicsRectItem::setPos(point);
    }

    void hide()
    {
        m_firstBadPaint = true;
        QGraphicsRectItem::hide();
    }

    void paint(QPainter *painter, const QStyleOptionGraphicsItem *item,
               QWidget *widget) override
    {
        const QTransform &t = painter->transform();
        qreal m11    = t.m11(),
              m22    = t.m22(),
              xScale = 1.0,
              yScale = 1.0;

        if (m11 < 1.0 || m22 < 1.0) {
            xScale = 1.0 / m11;
            yScale = 1.0 / m22;
        } else {
            if (m11 > m22) xScale = m22 / m11;
            else           yScale = m11 / m22;
        }

        qreal x   = m_rawPos.x()       -       BORDER_WIDTH * xScale,
              y   = m_rawPos.y()       -       BORDER_WIDTH * yScale,
              w   = m_rawRect.width()  + 2.0 * BORDER_WIDTH * xScale,
              h   = m_rawRect.height() + 2.0 * BORDER_WIDTH * yScale;

        qreal rect_x = 0.0,
              rect_y = 0.0;

        // Incredible hack
        if (m_firstBadPaint) {
            rect_x = -BORDER_WIDTH * xScale;
            rect_y = -BORDER_WIDTH * yScale;
            m_firstBadPaint = false;
        }

        QGraphicsRectItem::setPos(x, y);
        QGraphicsRectItem::setRect(QRectF(rect_x, rect_y, w, h));

        // Must do this instead of painter->drawRect()
        QGraphicsRectItem::paint(painter, item, widget);
    }

  protected:
    static constexpr qreal  BORDER_WIDTH = 4.0;

    QRectF m_rawRect;
    QPointF m_rawPos;
    bool m_firstBadPaint;
};

}  // namespace Rosegarden

#endif  // #ifndef RG_ISOTROPICRECTITEM
