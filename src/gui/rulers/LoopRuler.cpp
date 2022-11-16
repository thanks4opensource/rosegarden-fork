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

#define RG_MODULE_STRING "[LoopRuler]"
#define RG_NO_DEBUG_PRINT

#include "LoopRuler.h"

#include "misc/Debug.h"
#include "base/RulerScale.h"
#include "base/SnapGrid.h"
#include "gui/application/RosegardenMainWindow.h"
#include "gui/general/GUIPalette.h"
#include "gui/general/RosegardenScrollView.h"
#include "document/RosegardenDocument.h"

#include <QAction>
#include <QApplication>
#include <QBrush>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPainter>
#include <QPainter>
#include <QPaintEvent>
#include <QRect>
#include <QSize>
#include <QToolTip>
#include <QWidget>

#include <utility>  // std::swap()

namespace Rosegarden
{

LoopRuler::LoopRuler(RosegardenDocument *doc,
                     RulerScale *rulerScale,
                     int height,
                     bool invert,
                     bool isForMainWindow,
                     QWidget *parent) :
    QWidget(parent),
    m_height(height),
    m_invert(invert),
    m_isForMainWindow(isForMainWindow),
    m_currentXOffset(0),
    m_width( -1),
    m_mouseButtonIsDown(false),
    m_mouseMoved(false),
    m_mouseXAtClick(0.0),
    m_lastMouseXPos(0.0),
    m_doc(doc),
    m_comp(doc->getComposition()),
    m_rulerScale(rulerScale),
    m_noneSnapGrid(rulerScale),
    m_beatSnapGrid(rulerScale),
    m_gridSnapGrid(&m_noneSnapGrid),
    m_isMatrixEditor(false),
    m_quickMarkerPen(QPen(GUIPalette::getColour(GUIPalette::QuickMarker), 4)),
    m_loopRangeSettingMode(false),
    m_startLoop(0),
    m_endLoop(0),
    m_startOrEnd(&m_endLoop),
    m_doubleClickTimer(nullptr),
    m_mouseEvent(nullptr),
    m_waitingForDoubleClick(false),
    m_didDoubleClick(false)
{
    // Always snap loop extents to beats.
    // Apply no snap to pointer position unless Ctrl key, except
    //   backwards of that for matrix editor.
    m_noneSnapGrid.setSnapTime(SnapGrid::NoSnap);
    m_beatSnapGrid.setSnapTime(SnapGrid::SnapToBeat);

    setCorrectToolTip();

    m_doubleClickTimer = new QTimer(this);
    connect(m_doubleClickTimer, &QTimer::timeout,
            this, &LoopRuler::doubleClickTimerTimeout);
}

LoopRuler::~LoopRuler()
{
    delete m_doubleClickTimer;
    delete m_mouseEvent;
}

void
LoopRuler::setSnapGrid(const SnapGrid *grid)
{
    if (grid) {
        m_gridSnapGrid = grid;
        m_isMatrixEditor = true;
    }
    setCorrectToolTip();
}

void
LoopRuler::setCorrectToolTip()
{
    if (m_isMatrixEditor)
        setToolTip(tr("<qt><p>Click and drag to move the playback pointer."
                      "</p><p>Right-click and drag to set a range within "
                              "segment limits for looping or editing."
                      "</p><p>Right-click to toggle the range."
                      "</p><p>Ctrl-click and drag to move the playback pointer "
                             "without snap to grid."
                      "</p><p>Double-click to start playback."
                      "</p></qt>"));
    else
        setToolTip(tr("<qt><p>Click and drag to move the playback pointer."
                      "</p><p>Right-click and drag to set a range within "
                              "segment limits for looping or editing."
                      "</p><p>Right-click to toggle the range."
                      "</p><p>Ctrl-click and drag to move the playback pointer "
                             "with snap to beat."
                      "</p><p>Double-click to start playback."
                      "</p></qt>"));
}

void LoopRuler::scrollHoriz(int x)
{
    // int w = width(); //, h = height();
    // int dx = x - ( -m_currentXOffset);
    m_currentXOffset = -x;

//    if (dx > w*3 / 4 || dx < -w*3 / 4) {
//        update();
//        return ;
//    }

/*### These bitBlts are not working
    RG_DEBUG << "LoopRuler::scrollHoriz > Dodgy bitBlt start?";
    if (dx > 0) { // moving right, so the existing stuff moves left
        bitBlt(this, 0, 0, this, dx, 0, w - dx, h);
        repaint(w - dx, 0, dx, h);
    } else {      // moving left, so the existing stuff moves right
        bitBlt(this, -dx, 0, this, 0, 0, w + dx, h);
        repaint(0, 0, -dx, h);
    }
    RG_DEBUG << "LoopRuler::scrollHoriz > Dodgy bitBlt end?";
*/
    update();
}

QSize LoopRuler::sizeHint() const
{
    double width =
        m_rulerScale->getBarPosition(m_rulerScale->getLastVisibleBar()) +
        m_rulerScale->getBarWidth(m_rulerScale->getLastVisibleBar());

    QSize res(std::max(int(width), m_width), m_height);

    return res;
}

QSize LoopRuler::minimumSizeHint() const
{
    double firstBarWidth = m_rulerScale->getBarWidth(0);

    QSize res = QSize(int(firstBarWidth), m_height);

    return res;
}

void LoopRuler::paintEvent(QPaintEvent* e)
{
    QPainter paint(this);

    paint.setClipRegion(e->region());
    paint.setClipRect(e->rect().normalized());

    // Display loop range or not
    if (m_comp.loopRangeIsActive() || m_loopRangeSettingMode)
        drawLoopMarker(&paint, e->rect().width());
    else
        paint.fillRect(e->rect(), QBrush(GUIPalette::getColour(
                                         GUIPalette::LoopRulerBackground)));

    paint.setBrush(palette().windowText());
    drawBarSections(&paint);

    if (m_isForMainWindow) {
        timeT tQM = m_doc->getQuickMarkerTime();
        if (tQM >= 0) {
            // draw quick marker
            double xQM = m_rulerScale->getXForTime(tQM)
                       + m_currentXOffset;

            paint.setPen(m_quickMarkerPen);

            // looks necessary to compensate for shift in the
            // CompositionView (cursor)
            paint.translate(1, 0);

            // draw red segment
            paint.drawLine(int(xQM), 1, int(xQM), m_height-1);
        }
    }
}

void LoopRuler::drawBarSections(QPainter* paint)
{
    QRect clipRect = paint->clipRegion().boundingRect();

    int firstBar = m_rulerScale->getBarForX(clipRect.x() -
                                            m_currentXOffset);
    int lastBar = m_rulerScale->getLastVisibleBar();
    if (firstBar < m_rulerScale->getFirstVisibleBar()) {
        firstBar = m_rulerScale->getFirstVisibleBar();
    }

    paint->setPen(GUIPalette::getColour(GUIPalette::LoopRulerForeground));

    for (int i = firstBar; i < lastBar; ++i) {

        double x = m_rulerScale->getBarPosition(i) + m_currentXOffset;
        if (x > clipRect.x() + clipRect.width())
            break;

        double width = m_rulerScale->getBarWidth(i);
        if (width == 0)
            continue;

        if (x + width < clipRect.x())
            continue;

        if (m_invert) {
            paint->drawLine(int(x), 0, int(x), 5*m_height / 7);
        } else {
            paint->drawLine(int(x), 2*m_height / 7, int(x), m_height);
        }

        double beatAccumulator = m_rulerScale->getBeatWidth(i);
        double inc = beatAccumulator;
        if (inc == 0)
            continue;

        for (; beatAccumulator < width; beatAccumulator += inc) {
            if (m_invert) {
                paint->drawLine(int(x + beatAccumulator), 0,
                                int(x + beatAccumulator), 2 * m_height / 7);
            } else {
                paint->drawLine(int(x + beatAccumulator), 5*m_height / 7,
                                int(x + beatAccumulator), m_height);
            }
        }
    }
}

void
LoopRuler::drawLoopMarker(QPainter *paint, unsigned width)
{
    double x1 = (int)m_rulerScale->getXForTime(m_startLoop);
    double x2 = (int)m_rulerScale->getXForTime(m_endLoop);

    if (x1 > x2)
        std::swap(x1, x2);

    x1 += m_currentXOffset;
    x2 += m_currentXOffset;

    // Qt fuckup, doesn't draw correctly if start of rectangle is
    // offscreen (outside clipRec??)
    if (x1 < 0) x1 = 0;
    if (x2 < 0) x2 = 0;

    paint->save();
    QBrush background = QBrush(GUIPalette::getColour(
                               GUIPalette::LoopBeforeAfterBackground));
    QBrush foreground;

    paint->fillRect(QRectF(0, 0, x1, m_height), background);

    if (x1 != 0) {  // Loop range is visible onscreen or is offscreen to right
                    // and doesn't cover entire visible area, so need to draw
                    // time range before it.
        foreground.setColor(GUIPalette::getColour(GUIPalette::LoopBefore));
        foreground.setStyle(Qt::DiagCrossPattern);
        paint->setBrush(foreground);
        paint->drawRect(QRectF(0, 0, x1, m_height));
    }

    if (x2 != 0) {  // Loop range is visible onscreen -- draw it.
        foreground.setColor(GUIPalette::getColour(GUIPalette::LoopHighlight));
        foreground.setStyle(Qt::SolidPattern);
        paint->setBrush(foreground);
        paint->drawRect(QRectF(x1, 0, x2, m_height));
    }

    if (x2 < width) {   // Loop range is visible or is offscreen to left and
                        // doesn't cover entire visible area, so need to draw
                        // draw time range after it.
        paint->fillRect(QRectF(x2, 0, width, m_height), background);
        foreground.setColor(GUIPalette::getColour(GUIPalette::LoopAfter));
        foreground.setStyle(Qt::DiagCrossPattern);
        paint->setBrush(foreground);
        paint->drawRect(QRectF(x2, 0, width, m_height));
    }

    paint->restore();
}

double
LoopRuler::mouseEventToSceneX(QMouseEvent *mE)
{
    double x = mE->pos().x() - m_currentXOffset;
    return x;
}

// The double-click timer code is necessary to implement starting playback
// on double click at the current playback cursor time without first doing
// the single click action of jumping the cursor to the mouse position
// in the ruler. (Applies only to left button click, not to right button
// setting loop range and active/inactive.)
// This keeps the user experience consistent: That starting playback is the
// always the same whether done by a "Play" button, a menu, or a double-click
// in a LoopRuler. In all cases playback starts at the same precise time.
// The downside (besides the code complexity) is a slight delay (the
// double click time period) when single-clicking to set the time cursor,
// either by an immediate click and release, or when starting a move/drag.
// The delay is almost imperceptible and considered acceptable given the
// benefit gained in the double-click interaction.
void
LoopRuler::doubleClickTimerTimeout()
{
    m_doubleClickTimer->stop();
    m_waitingForDoubleClick = false;
    doSingleClick();
}

void LoopRuler::mousePressEvent(QMouseEvent *mouseEvent)
{
    m_mouseButtonIsDown = true;
    m_mouseMoved = false;
    m_mouseXAtClick = mouseEventToSceneX(mouseEvent);

    delete m_mouseEvent;
    m_mouseEvent = new QMouseEvent(*mouseEvent);

    // Check if have to wait to see if is double click.
    if ( m_mouseEvent->button()                           == Qt::RightButton ||
        (m_mouseEvent->modifiers() & Qt::ShiftModifier)   != 0               ||
        (m_mouseEvent->modifiers() & Qt::ControlModifier) != 0) {
        // No, handle immediately
        doSingleClick();
    }
    else {
        // Yes, wait for possible 2nd click. See doubleClickTimerTimeout().
        m_waitingForDoubleClick = true;
        m_doubleClickTimer->start(QApplication::doubleClickInterval());
    }
}

void
LoopRuler::doSingleClick()
{
    const double x = mouseEventToSceneX(m_mouseEvent);

    const bool leftButton = (m_mouseEvent->button() == Qt::LeftButton);
    const bool rightButton = (m_mouseEvent->button() == Qt::RightButton);
    const bool shift = (m_mouseEvent->modifiers() & Qt::ShiftModifier) != 0;
    const bool control = (m_mouseEvent->modifiers() & Qt::ControlModifier) != 0;

    delete m_mouseEvent;
    m_mouseEvent = nullptr;

    // Check for quick press and release (without any mouse motion)
    if (!m_mouseButtonIsDown && !m_mouseMoved) {
        // Yes, so all that's needed is to accept it.
        emit setPointerPosition(snapX(x, control, CursorLoop::CURSOR));
        return;
    }

    // Check if starting to define loop range, but don't allow if  empty
    // composition with no segments (doesn't make sense, and causes problems
    // later in mouseReleaseEvent() when checking loop range against
    // start/end times of segments).
    if (((shift && leftButton) || rightButton)) {
        if (m_comp.getNbSegments() == 0) {
            QMessageBox::warning(this,
                                 tr("Loop Ruler"),
                                 tr("Can't define loop range without segments"));
            return;
        }

        // Start loop range definition
        bool adjustExisting = false;

        // New loop range or adjust start/end of current one?
        if ( m_comp.loopRangeIsActive() &&
            (m_comp.getLoopStart() != m_comp.getLoopEnd())) {

            // Have an existing loop range that is currently displayed.
            double xStart = m_rulerScale->getXForTime(m_comp.getLoopStart());
            double xEnd   = m_rulerScale->getXForTime(m_comp.getLoopEnd());

            if (fabs(x - xEnd) < static_cast<double>(CLOSE_PIXELS)) {
                adjustExisting = true;
                m_startOrEnd = &m_endLoop;
            } else if (fabs(x - xStart) < static_cast<double>(CLOSE_PIXELS)) {
                adjustExisting = true;
                m_startOrEnd = &m_startLoop;
            }
            *m_startOrEnd = snapX(x, control, CursorLoop::LOOP);
        }

        if (adjustExisting) {
            m_startLoop = m_comp.getLoopStart();
            m_endLoop   = m_comp.getLoopEnd();
        } else {
            // start new loop range definition
            m_startLoop = m_endLoop = snapX(x, control, CursorLoop::LOOP);
            m_startOrEnd = &m_endLoop;
        }

        m_loopRangeSettingMode = true;
        emit startMouseMove(FOLLOW_HORIZONTAL);
        return;
    }

    // Left button pointer drag
    if (leftButton) {
        // ??? This signal is never emitted with any other argument.
        //     Remove the parameter.  This gets a little tricky because
        //     some clients need this and share slots with other signal
        //     sources.  It would probably be best to connect this signal
        //     to a slot in the client that is specific to LoopRuler.

        // Check if mouse already moved pointer, and if so don't jump
        // it back to  where it was before double-click timeout.
        if (!m_mouseMoved) {
            m_lastMouseXPos = x;
            emit setPointerPosition(snapX(x, control, CursorLoop::CURSOR));
        }
        emit startMouseMove(FOLLOW_HORIZONTAL);
    }
}

void
LoopRuler::mouseReleaseEvent(QMouseEvent *mouseEvent)
{
    m_mouseButtonIsDown = false;

    if (m_waitingForDoubleClick) {
        // Waiting to see if double click, so do nothing
        //
        // Could possibly use m_doubleClickTimer->isActive() (i.e.
        // QTimer::isActive()) instead of managing own
        // bool m_waitingForDoubleClick but would open slight chance of
        // a race condition if timer has timed out but
        // doubleClickTimerTimeout() signal hasn't been emitted/received yet.
        return;
    }

    if (m_didDoubleClick) {
        // Don't do single-click release actions.
        m_didDoubleClick = false;
        return;
    }

    emit stopMouseMove();

    // If we were setting loop range
    if (m_loopRangeSettingMode) {
        m_loopRangeSettingMode = false;

        // If there was no drag, toggle the loop range active/inactive
        // instead of defining new range.
        if (m_endLoop == m_startLoop)
            m_doc->toggleLoopRangeIsActive();
        else {
            // Make sure start < end
            if (m_endLoop < m_startLoop) std::swap(m_startLoop, m_endLoop);
            m_doc->setLoopRange(m_startLoop, m_endLoop);
        }
    }

    if (mouseEvent->button() == Qt::LeftButton) {
        // we need to re-emit this signal so that when the user releases
        // the button after dragging the pointer, the pointer's position
        // is updated again in the other views (typically, in the seg.
        // canvas while the user has dragged the pointer in an edit view)
        emit setPointerPosition(snapX(m_lastMouseXPos,
                                      (mouseEvent->modifiers() &
                                            Qt::ControlModifier) != 0,
                                       CursorLoop::CURSOR));
    }
}

void
LoopRuler::mouseDoubleClickEvent(QMouseEvent *mouseEvent)
{
    const bool leftButton = (mouseEvent->button() == Qt::LeftButton);

    if (m_doubleClickTimer) m_doubleClickTimer->stop();

    m_waitingForDoubleClick = false;
    m_didDoubleClick = true;  // See mouseReleaseEvent()

    if (leftButton) RosegardenMainWindow::self()->slotPlay();
}

void
LoopRuler::mouseMoveEvent(QMouseEvent *mE)
{
    const bool control = (mE->modifiers() & Qt::ControlModifier) != 0;

    double x = mouseEventToSceneX(mE);
    if (x < 0) x = 0;

    // Need to tell doSingleClick that pointer has already moved so
    // it shouldn't jump it back to where it was when mouse button
    // was first clicked when it gets triggered by double-click timer
    // timout.
    // But wait for significant mouse movement to ignore inadvertent
    // movement during double-click timeout.
    if (!m_mouseMoved && fabs(x - m_mouseXAtClick) < CLOSE_PIXELS) {
        return;
    }
    m_mouseMoved = true;

    if (m_loopRangeSettingMode) {
        timeT snapped = snapX(x, control, CursorLoop::LOOP);
        if (snapped != *m_startOrEnd) {
            *m_startOrEnd = snapped;
            update();
        }
    }
    // Double check that isn't loop range definition aborted because no segments
    else if (mE->buttons() & Qt::LeftButton) {
        emit dragPointerToPosition(snapX(x, control, CursorLoop::CURSOR));
        m_lastMouseXPos = x;
    }

    emit mouseMove();
}

// Snap to:
//                  normal      control
// matrix
//      pointer     grid        none
//      loop        grid        beat
// other
//      pointer     none        beat
//      loop        beat        beat
// Snap cursor appropriately for matrix editor vs all others
timeT LoopRuler::snapX(
const double x,
const bool controlKey,
const CursorLoop cursorLoop)
{
    switch (cursorLoop) {
    case CursorLoop::CURSOR:
    default:  // supress compiler warning
        if (controlKey) {
            if (m_isMatrixEditor) return m_noneSnapGrid.snapX(x);
            else                  return m_beatSnapGrid.snapX(x);
        }
        else {
            if (m_isMatrixEditor) return m_gridSnapGrid->snapX(x);
            else                  return m_noneSnapGrid. snapX(x);
        }
    case CursorLoop::LOOP:
        if (m_isMatrixEditor && !controlKey)
            return m_gridSnapGrid->snapX(x);
        else
            return m_beatSnapGrid.snapX(x);
    }
}

// Another window or editor's ruler or a dialog/menu has changed the loop range.
void LoopRuler::slotSetLoopMarkerStartEnd(timeT startLoop, timeT endLoop)
{
    m_startLoop = startLoop;
    m_endLoop = endLoop;
    update();
}

// Another window or editor's ruler or a dialog/menu has changed loop active.
void LoopRuler::slotSetLoopMarkerActive()
{
    m_startLoop = m_comp.getLoopStart();
    m_endLoop = m_comp.getLoopEnd();
    update();
}

}  // namespace Rosegarden
