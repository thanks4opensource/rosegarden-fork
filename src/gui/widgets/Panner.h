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

#ifndef RG_PANNER_H
#define RG_PANNER_H

#include <QAction>
#include <QGraphicsView>
#include <QRectF>
#include <QPixmap>
#include <QPointF>

class QGraphicsItem;
class QGraphicsScene;
class QMenu;
class QMouseEvent;
class QPainter;
class QPaintEvent;
class QResizeEvent;
class QStyleOptionGraphicsItem;
class QWheelEvent;


namespace Rosegarden
{


/// The light blue area that appears below the notation and matrix views.
/**
 * A Panner is the light blue navigation area below the notation and matrix
 * views.  It is to the left of the set of three thumbwheels that can be
 * used for panning and zooming.
 *
 * The viewport can be moved around by clicking and dragging in
 * the Panner.  Using the scroll wheel within the Panner zooms in and out.
 */
class Panner : public QGraphicsView
{
    Q_OBJECT

public:
    enum class FitMode {
        ALL,
        VISIBLE,
        LOOP,
    };

    Panner(bool inMatrixEditor=false);
    ~Panner() override { }

    virtual void setScene(QGraphicsScene *);

signals:
    void pannedRectChanged(QRectF);
    void pannerChanged(QRectF);
    void zoomIn();
    void zoomOut();
    void zoomFitNotes(const unsigned mode,
                      const bool fitHorizontal,
                      const bool ignorePercussion);
    void showContextHelp(const QString&);

public slots:
    void slotSetPannedRect(QRectF);
    void slotShowPositionPointer(float x); // scene coord; full height
    void slotShowPositionPointer(QPointF top, float height); // scene coords
    void slotHidePositionPointer();
    void slotLoopRangeActiveChanged();

protected slots:
    void slotSceneRectChanged(const QRectF &);

protected:
    QRectF   m_pannedRect;
    QMenu   *m_menu;
    QAction *m_fitVisibleAction,
            *m_fitAllAction,
            *m_fitLoopAction,
            *m_fitLoopBothAction,
            *m_fitAllBothAction,
            *m_fitIgnorePercussionAction;
    QPointF  m_pointerTop;
    bool     m_inMatrixEditor;
    float    m_pointerHeight;
    bool     m_pointerVisible,
             m_fitIgnorePercussion;

    void moveTo(QPoint);

    void paintEvent(QPaintEvent *) override;
    void mousePressEvent(QMouseEvent *e) override;
    void mouseMoveEvent(QMouseEvent *e) override;
    void mouseReleaseEvent(QMouseEvent *e) override;
    void mouseDoubleClickEvent(QMouseEvent *e) override;
    void wheelEvent(QWheelEvent *e) override;
    void resizeEvent(QResizeEvent *) override;
    void enterEvent(QEvent *e) override;

    virtual void updateScene(const QList<QRectF> &);
    void drawItems(QPainter *, int, QGraphicsItem *[],
                           const QStyleOptionGraphicsItem []) override;

    bool m_clicked;
    QRectF m_clickedRect;
    QPoint m_clickedPoint;

    QPixmap m_cache;
};


}

#endif
