/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*- vi:set ts=8 sts=4 sw=4: */

/*
    Rosegarden
    A MIDI and audio sequencer and musical notation editor.
    Copyright 2000-2022 the Rosegarden development team.
    Modifications and additions Copyright (c) 2022 Mark R. Rubin aka "thanks4opensource" aka "thanks4opensrc"

    Other copyrights also apply to some parts of this work.  Please
    see the AUTHORS file and individual file headers for details.

    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License as
    published by the Free Software Foundation; either version 2 of the
    License, or (at your option) any later version.  See the file
    COPYING included with this distribution for more information.
*/

#ifndef RG_ISOTROPICTEXTITEM_H
#define RG_ISOTROPICTEXTITEM_H

#include <QGraphicsSimpleTextItem>
#include <QString>
#include <QPainter>

namespace Rosegarden
{

// A QGraphicsSimpleTextItem that scales isotropically, maintaing the
// aspect ratio of the text characters without stretching, regardless
// if the QGraphicsScene containing the text is scaled anisotropically.
// Text only scales according to the QGraphicsScene vertical zoom factor.

class IsotropicTextItem : public QGraphicsSimpleTextItem {
  public:
    void setText(const QString &text, const QRectF &noteRect) {
        QGraphicsSimpleTextItem::setText(text);
        m_textWidth = QGraphicsSimpleTextItem::boundingRect().width();
        m_noteRect = noteRect;
        m_noteWidth = m_noteRect.width();
        m_badBoundingBox = true;
    }

    QRectF boundingRect() const override {
        return m_noteRect;
    }

    void paint(QPainter *painter, const QStyleOptionGraphicsItem *item,
               QWidget *widget) override
    {
        const QTransform &t = painter->transform();
        const qreal m11 = t.m11(),
                    m22 = t.m22(),
                    m31 = t.m31(),
                    m32 = t.m32();

        if (m_badBoundingBox) {
            // Total mystery hack. If not done, text width and thus
            // centering is off until note is moved or resized. But
            // not needed if at least percussion segment is included
            // in matrix editor and therefor  MatrixScene::getYResolution
            // is 11 instead of 8.
            m_textWidth = QGraphicsSimpleTextItem::boundingRect().width();
            m_badBoundingBox = false;
        }

        qreal xScale = m22,
              xPos   = m31;

        if (m_textWidth * m22 < m_noteWidth * m11)
            xPos = m31 + (m_noteWidth * m11 - m_textWidth * m22) * 0.5;
        else
            xScale = m11 * (m_noteWidth / m_textWidth);

        painter->setTransform(QTransform(xScale, 0.0, 0.0,
                                         0.0,    m22, 0.0,
                                         xPos,   m32, 1.0));

        // Must do this instead of painter->drawText(0, 11, text(), etc.
        // That works, but always draws text in QColor(0,0,0,0) black
        // regradless any calls to painter->setBrush(brush()),
        // setBrush(known_good_color), etc.
        QGraphicsSimpleTextItem::paint(painter, item, widget);
    }

  protected:
    QRectF m_noteRect;
    float m_textWidth, m_noteWidth;
    bool m_badBoundingBox;  // see "if (m_badBoundingBox)" above
};

}  // namespace Rosegarden

#endif  // #ifndef RG_ISOTROPICTEXTITEM
