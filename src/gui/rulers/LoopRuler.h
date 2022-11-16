
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

#ifndef RG_LOOPRULER_H
#define RG_LOOPRULER_H

#include "base/SnapGrid.h"
#include <QSize>
#include <QWidget>
#include <QPen>
#include "base/Event.h"


class QPaintEvent;
class QPainter;
class QMouseEvent;


namespace Rosegarden
{

class RulerScale;
class RosegardenDocument;
class Composition;


/**
 * LoopRuler is a widget that shows bar and beat durations on a
 * ruler-like scale, and reacts to mouse clicks by sending relevant
 * signals to modify position pointer and playback/looping states.
 */
class LoopRuler : public QWidget
{
    Q_OBJECT

public:
    LoopRuler(RosegardenDocument *doc,
              RulerScale *rulerScale,
              int height = 0,
              bool invert = false,
              bool isForMainWindow = false,
              QWidget* parent = nullptr);

    ~LoopRuler() override;

    void setSnapGrid(const SnapGrid *grid);

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

    void scrollHoriz(int x);

signals:
    /// Set the pointer position on mouse single click
    void setPointerPosition(timeT);

    /// Set the pointer position on mouse drag
    void dragPointerToPosition(timeT);
    void startMouseMove(int directionConstraint);
    void stopMouseMove();
    void mouseMove();

public slots:
    void slotSetLoopMarkerStartEnd(timeT startLoop, timeT endLoop);
    void slotSetLoopMarkerActive();

protected:
    // QWidget overrides
    void mousePressEvent(QMouseEvent *) override;
    void mouseReleaseEvent(QMouseEvent *) override;
    void mouseDoubleClickEvent(QMouseEvent *) override;
    void mouseMoveEvent(QMouseEvent *) override;
    void paintEvent(QPaintEvent *) override;

private:
    // Allow slight movement during double click, and select of start/end
    // to resize instead of start new range definition.
    static const unsigned CLOSE_PIXELS = 10;

    enum class CursorLoop {
        CURSOR = 0,
        LOOP,
    };

    void setCorrectToolTip();
    double mouseEventToSceneX(QMouseEvent *mouseEvent);

    void drawBarSections(QPainter*);
    void drawLoopMarker(QPainter*, unsigned);  // before/loop/after sections
    void doubleClickTimerTimeout();
    void doSingleClick();

    timeT snapX(const double x, const bool, const CursorLoop);

    //--------------- Data members ---------------------------------
    int  m_height;
    bool m_invert;
    bool m_isForMainWindow;
    int  m_currentXOffset;
    int  m_width;
    bool m_mouseButtonIsDown;
    bool m_mouseMoved;
    double m_mouseXAtClick;
    // Remember the mouse x pos in mousePressEvent and in mouseMoveEvent so
    // that we can emit it in mouseReleaseEvent to update the pointer position
    // in other views
    double m_lastMouseXPos;

    RosegardenDocument *m_doc;
    Composition &m_comp;

    RulerScale *m_rulerScale;
    SnapGrid m_noneSnapGrid;
    SnapGrid m_beatSnapGrid;
    const SnapGrid *m_gridSnapGrid;

    bool m_isMatrixEditor;
    QPen m_quickMarkerPen;

    bool m_loopRangeSettingMode;
    timeT m_startLoop;
    timeT m_endLoop;
    timeT *m_startOrEnd;

    QTimer *m_doubleClickTimer;
    QMouseEvent *m_mouseEvent;
    bool m_waitingForDoubleClick;
    bool m_didDoubleClick;
};


}

#endif
