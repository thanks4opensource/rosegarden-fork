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

#define RG_MODULE_STRING "[Panner]"
#define RG_NO_DEBUG_PRINT 1

#include "Panner.h"

#include "gui/general/GUIPalette.h"
#include "misc/Debug.h"
#include "base/Profiler.h"
#include "document/RosegardenDocument.h"

#include <QAction>
#include <QColor>
#include <QGraphicsScene>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPaintEvent>
#include <QPen>
#include <QPoint>
#include <QPolygon>
#include <QTransform>
#include <QWheelEvent>


namespace Rosegarden
{


class PannerScene : public QGraphicsScene
{
public:
    friend class Panner;
};

Panner::Panner(bool inMatrixEditor) :
    m_inMatrixEditor(inMatrixEditor),
    m_pointerHeight(0),
    m_pointerVisible(false),
    m_fitIgnorePercussion(true),
    m_clicked(false)
{
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setOptimizationFlags(QGraphicsView::DontSavePainterState
#if QT_VERSION >= 0x040600
                         |QGraphicsView::IndirectPainting
#endif
                        );
    setMouseTracking(true);
#if 0   // Old code. Documented to disable mouse events, but getting
        // even with, so removed because unwanted and doing nothing.
    setInteractive(false);
#endif

    const RosegardenDocument *document = RosegardenDocument::currentDocument;
    const Composition &composition(document->getComposition());

    m_menu = new QMenu(tr("Fit notes"), this);

    m_fitVisibleAction = m_menu->addAction("fit_visible");
    m_fitVisibleAction->setText(tr("Fit visible measures' notes vertical"));
    connect(m_fitVisibleAction,
            &QAction::triggered,
            this,
            [this]
            {
                emit zoomFitNotes(static_cast<unsigned>(FitMode::VISIBLE),
                                  false,
                                  this->m_fitIgnorePercussion);
                this->viewport()->update();
            });

    m_fitAllAction = m_menu->addAction("fit_all");
    m_fitAllAction->setText(tr("Fit all notes vertical"));
    connect(m_fitAllAction,
            &QAction::triggered,
            this,
            [this]
            {
                emit zoomFitNotes(static_cast<unsigned>(FitMode::ALL),
                                  false,
                                  this->m_fitIgnorePercussion);
                this->viewport()->update();
            });

    m_fitLoopAction = m_menu->addAction("fit_loop");
    m_fitLoopAction->setEnabled(composition.loopRangeIsActive());
    m_fitLoopAction->setText(tr("Fit loop range notes vertical"));
    connect(m_fitLoopAction,
            &QAction::triggered,
            this,
            [this]
            {
                emit zoomFitNotes(static_cast<unsigned>(FitMode::LOOP),
                                  false,
                                  this->m_fitIgnorePercussion);
                this->viewport()->update();
            });

    m_fitLoopBothAction = m_menu->addAction("fit_loop_both");
    m_fitLoopBothAction->setEnabled(composition.loopRangeIsActive());
    m_fitLoopBothAction->setText(tr("Fit loop range notes"));
    connect(m_fitLoopBothAction,
            &QAction::triggered,
            this,
            [this]
            {
                emit zoomFitNotes(static_cast<unsigned>(FitMode::LOOP),
                                  true,
                                  this->m_fitIgnorePercussion);
                this->viewport()->update();
            });

    m_fitAllBothAction = m_menu->addAction("fit_all");
    m_fitAllBothAction->setText(tr("Fit all notes"));
    connect(m_fitAllBothAction,
            &QAction::triggered,
            this,
            [this]
            {
                emit zoomFitNotes(static_cast<unsigned>(FitMode::ALL),
                                  true,
                                  this->m_fitIgnorePercussion);
                this->viewport()->update();
            });

    m_fitIgnorePercussionAction = m_menu->addAction("ignore_percussion");
    m_fitIgnorePercussionAction->setCheckable(true);
    m_fitIgnorePercussionAction->setChecked(m_fitIgnorePercussion);
    m_fitIgnorePercussionAction->setText(tr("Ignore percussion"));
    connect(m_fitIgnorePercussionAction,
            &QAction::triggered,
            this,
            [this]
            {
                  this->m_fitIgnorePercussion
                = this->m_fitIgnorePercussionAction->isChecked();
            });

    // Only enable loop range options if loop range is active
    connect(document,   &RosegardenDocument::loopRangeActiveChanged,
            this,       &Panner::slotLoopRangeActiveChanged);
}

void
Panner::setScene(QGraphicsScene *s)
{
    if (scene()) {
        disconnect(scene(), &QGraphicsScene::sceneRectChanged,
                   this, &Panner::slotSceneRectChanged);
    }
    QGraphicsView::setScene(s);
    if (scene()) fitInView(sceneRect(), Qt::KeepAspectRatio);
    m_cache = QPixmap();
    connect(scene(), &QGraphicsScene::sceneRectChanged,
            this, &Panner::slotSceneRectChanged);
    RG_DEBUG << "Panner::setScene: scene is " << scene();
}

void
Panner::slotSetPannedRect(QRectF rect)
{
    RG_DEBUG << "Panner::slotSetPannedRect(" << rect << ")";

    m_pannedRect = rect;
    viewport()->update();
}

void
Panner::slotShowPositionPointer(float x) // scene coord; full height
{
    m_pointerVisible = true;
    m_pointerTop = QPointF(x, 0);
    m_pointerHeight = 0;
    viewport()->update(); //!!! should update old and new pointer areas only, really
}

void
Panner::slotShowPositionPointer(QPointF top, float height) // scene coords
{
    m_pointerVisible = true;
    m_pointerTop = top;
    m_pointerHeight = height;
    viewport()->update(); //!!! should update old and new pointer areas only, really
}

void
Panner::slotHidePositionPointer()
{
    m_pointerVisible = false;
    viewport()->update(); //!!! should update old pointer area only, really
}

void
Panner::resizeEvent(QResizeEvent *)
{
    if (scene()) fitInView(sceneRect(), Qt::KeepAspectRatio);
    m_cache = QPixmap();
}

void
Panner::slotSceneRectChanged(const QRectF &newRect)
{
    if (!scene()) return; // spurious
    fitInView(newRect, Qt::KeepAspectRatio);
    m_cache = QPixmap();
    viewport()->update();
}

void
Panner::slotLoopRangeActiveChanged()
{
    bool active =   RosegardenDocument
                  ::currentDocument
                  ->getComposition()
                   .loopRangeIsActive();

    m_fitLoopAction    ->setEnabled(active);
    m_fitLoopBothAction->setEnabled(active);
}

void
Panner::paintEvent(QPaintEvent *e)
{
    RG_DEBUG << "paintEvent()";
    RG_DEBUG << "  region bounding rect:" << e->region().boundingRect();

    Profiler profiler("Panner::paintEvent");

    QPaintEvent *e2 = new QPaintEvent(e->region().boundingRect());
    QGraphicsView::paintEvent(e2);
    delete e2;
    e2 = nullptr;

    QPainter paint;
    paint.begin(viewport());
    paint.setClipRegion(e->region());

    QPainterPath path;
    path.addRect(rect());
    path.addPolygon(mapFromScene(m_pannedRect));

    QColor c(GUIPalette::getColour(GUIPalette::PannerOverlay));
    c.setAlpha(80);
    paint.setPen(Qt::NoPen);
    paint.setBrush(c);
    paint.drawPath(path);

    paint.setBrush(Qt::NoBrush);
    paint.setPen(QPen(GUIPalette::getColour(GUIPalette::PannerOverlay), 0));
    paint.drawConvexPolygon(mapFromScene(m_pannedRect));

    if (m_pointerVisible && scene()) {
        QPoint top = mapFromScene(m_pointerTop);
        float height = m_pointerHeight;
        if (height == 0.f) height = scene()->height();
        QPoint bottom = mapFromScene
            (QPointF(m_pointerTop.x(), m_pointerTop.y() + height));
        paint.setPen(QPen(GUIPalette::getColour(GUIPalette::Pointer), 2));
        paint.drawLine(top, bottom);
    }

    RG_DEBUG << "draw polygon: " << mapFromScene(m_pannedRect);
    paint.end();

    emit pannerChanged(m_pannedRect);
}

void
Panner::updateScene(const QList<QRectF> &rects)
{
    // ??? This is not virtual in QGraphicsView, so this is not actually
    //     an override.  It never gets called.  At least I can't get it to
    //     be called.

    RG_DEBUG << "updateScene()";

    if (!m_cache.isNull()) m_cache = QPixmap();
    QGraphicsView::updateScene(rects);
}

void
Panner::drawItems(QPainter *painter, int numItems,
                  QGraphicsItem *items[],
                  const QStyleOptionGraphicsItem options[])
{
    RG_DEBUG << "drawItems()";

    Profiler profiler("Panner::drawItems");

    if (m_cache.size() != viewport()->size()) {

        QGraphicsScene *s = scene();
        if (!s) return;
        PannerScene *ps = static_cast<PannerScene *>(s);

        m_cache = QPixmap(viewport()->size());
        m_cache.fill(Qt::transparent);
        QPainter cachePainter;
        cachePainter.begin(&m_cache);
        cachePainter.setTransform(viewportTransform());
        ps->drawItems(&cachePainter, numItems, items, options);
        cachePainter.end();
    }

    painter->save();
    painter->setTransform(QTransform());
    painter->drawPixmap(0, 0, m_cache);
    painter->restore();
}

void
Panner::mousePressEvent(QMouseEvent *e)
{
    if (m_inMatrixEditor && e->button() == Qt::RightButton) {
        m_menu->exec(QCursor::pos());
        return;
    }

    if (e->button() != Qt::LeftButton) {
        QGraphicsView::mouseDoubleClickEvent(e);
        return;
    }
    m_clicked = true;
    m_clickedRect = m_pannedRect;
    m_clickedPoint = e->pos();
}

void
Panner::mouseDoubleClickEvent(QMouseEvent *e)
{
    if (e->button() != Qt::LeftButton) {
        QGraphicsView::mouseDoubleClickEvent(e);
        return;
    }

    moveTo(e->pos());
}

void
Panner::mouseMoveEvent(QMouseEvent *e)
{
    if (!m_clicked) return;
    QPointF cp = mapToScene(m_clickedPoint);
    QPointF mp = mapToScene(e->pos());
    QPointF delta = mp - cp;
    const QRectF sceneRect = scene()->sceneRect();
    QRectF nr = m_clickedRect;

    // If we are going right
    if (delta.x() > 0.0) {
        // If we are going past the right edge, don't.
        if (nr.right() + delta.x() > sceneRect.right()) {
            double dx = sceneRect.right() - nr.right();
            if (dx < 0.0) dx = 0.0;
            delta.setX(dx);
        }
    }

    // If we are going left
    if (delta.x() < 0.0) {
        // If we are going past the left edge, don't.
        if (nr.left() + delta.x() < sceneRect.left()) {
            double dx = sceneRect.left() - nr.left();
            if (dx > 0.0) dx = 0.0;
            delta.setX(dx);
        }
    }

    // If we are going down
    if (delta.y() > 0.0) {
        // If we are going past the bottom edge, don't.
        if (nr.bottom() + delta.y() > sceneRect.bottom()) {
            double dy = sceneRect.bottom() - nr.bottom();
            if (dy < 0.0) dy = 0.0;
            delta.setY(dy);
        }
    }

    // If we are going up
    if (delta.y() < 0.0) {
        // If we are going past the top edge, don't.
        if (nr.top() + delta.y() < sceneRect.top()) {
            double dy = sceneRect.top() - nr.top();
            if (dy > 0.0) dy = 0.0;
            delta.setY(dy);
        }
    }

    nr.translate(delta);
    slotSetPannedRect(nr);
    emit pannedRectChanged(m_pannedRect);
    viewport()->update();
}

void
Panner::mouseReleaseEvent(QMouseEvent *e)
{
    if (e->button() != Qt::LeftButton) {
        QGraphicsView::mouseDoubleClickEvent(e);
        return;
    }

    if (m_clicked) {
        mouseMoveEvent(e);
    }

    m_clicked = false;
    viewport()->update();
}

void
Panner::wheelEvent(QWheelEvent *e)
{
    // We'll handle this.  Don't pass to parent.
    e->accept();

    if (e->angleDelta().y() > 0)
        emit zoomOut();
    else if (e->angleDelta().y() < 0)
        emit zoomIn();
}

void
Panner::enterEvent(QEvent *e)
{
    if (m_inMatrixEditor) {
        QString help(tr("Click-drag to move, double-click to jump, "
                        "right-click for fit view menu."));
        emit showContextHelp(help);
    }
    QGraphicsView::enterEvent(e);
}

void
Panner::moveTo(QPoint p)
{
    QPointF sp = mapToScene(p);
    QRectF nr = m_pannedRect;
    double d = sp.x() - nr.center().x();
    nr.translate(d, 0);
    slotSetPannedRect(nr);
    emit pannedRectChanged(m_pannedRect);
    viewport()->update();
}

}  // namespace Rosegarden
