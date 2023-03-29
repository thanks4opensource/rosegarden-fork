/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*- vi:set ts=8 sts=4 sw=4: */

/*
    Rosegarden
    A MIDI and audio sequencer and musical notation editor.
    Copyright 2000-2023 the Rosegarden development team.
    Modifications and additions Copyright (c) 2022,2023 Mark R. Rubin aka "thanks4opensource" aka "thanks4opensrc"

    Other copyrights also apply to some parts of this work.  Please
    see the AUTHORS file and individual file headers for details.

    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License as
    published by the Free Software Foundation; either version 2 of the
    License, or (at your option) any later version.  See the file
    COPYING included with this distribution for more information.
*/

#define RG_MODULE_STRING "[MatrixPainter]"
#define RG_NO_DEBUG_PRINT

#include "MatrixPainter.h"

#include "base/BaseProperties.h"
#include "base/Event.h"
#include "base/NotationTypes.h"
#include "base/SegmentMatrixHelper.h"
#include "base/SnapGrid.h"
#include "base/ViewElement.h"
#include "commands/matrix/MatrixInsertionCommand.h"
#include "commands/edit/EraseCommand.h"
#include "commands/matrix/MatrixPercussionInsertionCommand.h"
#include "document/CommandHistory.h"
#include "gui/editors/notation/NotePixmapFactory.h"
#include "MatrixElement.h"
#include "MatrixMouseEvent.h"
#include "MatrixMultiTool.h"
#include "MatrixScene.h"
#include "MatrixSegment.h"
#include "MatrixSelect.h"
#include "MatrixToolBox.h"
#include "MatrixTool.h"
#include "MatrixViewSegment.h"
#include "MatrixWidget.h"

#include "misc/Debug.h"

#include <QCursor>
#include <Qt>

#include <cmath>

namespace Rosegarden
{

QCursor *MatrixPainter::m_cursor(nullptr);
QCursor *MatrixPainter::m_selectCursor(nullptr);


MatrixPainter::MatrixPainter(MatrixWidget *widget, MatrixToolBox *toolbox) :
    MatrixTool("MatrixPainter", widget, toolbox),
    m_clickTime(0),
    m_currentElement(nullptr),
    m_currentViewSegment(nullptr),
    m_doubleClickSelection(nullptr)
{
    createMenu();
    if (!m_cursor)       m_cursor       = makeCursor("pencil", 1, 21);
    if (!m_selectCursor) m_selectCursor = makeCursor("painter_select_cursor",
                                                     6, 25);
}

void MatrixPainter::handleMidButtonPress(const MatrixMouseEvent *event)
{
    // note: middle button == third button (== left+right at the same time)

    MatrixTool *multiTool = dynamic_cast<MatrixTool*>(
                            m_toolbox->getTool(MatrixMultiTool::ToolName()));
    if (!multiTool) return;  // shouldn't ever happen
    multiTool->setRole(Role::SINGLE_ACTION);
    m_toolbox->appendDispatchTool(multiTool, event);

    // MultiTool has done appendDispatchTool() of Select, Segment, Move,
    // Resize, or Velocity.
    m_toolbox->getActiveTool()->handleLeftButtonPress(event);
}

void MatrixPainter::handleMouseDoubleClick(const MatrixMouseEvent*)
{
    if (!m_doubleClickSelection) return;

    // Delete events in m_doubleClickSelection() which was just created
    // and populated in handleLeftButtonPress() at first click of
    // double click and contains note that was just created there plus
    // any that were selected before.
    if (!m_doubleClickSelection->empty()) {
        EraseCommand *command = new EraseCommand(m_doubleClickSelection);
        CommandHistory::getInstance()->addCommand(command);
    }

    // No longer needed, so clean up here even though would also be
    // deleted next time handleLeftButtonPress() called.
    delete m_doubleClickSelection;
    m_doubleClickSelection = nullptr;
}

void MatrixPainter::handleLeftButtonPress(const MatrixMouseEvent *e)
{
    if (!e) return;

    m_isFinished = false;

    m_currentViewSegment = e->viewSegment;
    if (!m_currentViewSegment) return;

    // Save selection for possible double-click to follow
    delete m_doubleClickSelection;  // Previous one, in likely case of
                                    // no double-click.
    Event *eventUnderMouse =   (e->element && e->element->event())
                             ?  e->element->event()
                             :  nullptr;

    if (m_scene->getSelection() &&
        eventUnderMouse &&
        m_scene->getSelection()->contains(eventUnderMouse))
        m_doubleClickSelection = new EventSelection(*(m_scene->getSelection()));
    else {
        m_doubleClickSelection = new EventSelection(m_currentViewSegment->
                                                    getSegment());
        if (eventUnderMouse)
            m_doubleClickSelection->addEvent(eventUnderMouse);
    }

    // Grid needed for the event duration rounding

    int velocity = m_widget->getCurrentVelocity();

    RG_DEBUG << "handleLeftButtonPress(): velocity = " << velocity;

    m_clickTime = e->snappedLeftTime;

    // When entering notes, what you click on in concert pitch is what you
    // should see and hear, so we have to alter the event's physical pitch to
    // compensate.  In a concert pitch view of an Eb segment in -9, if you click
    // on a Bb, you should get what will be represented in notation as a G.
    long pitchOffset = m_currentViewSegment->getSegment().getTranspose();
    long adjustedPitch = e->pitch + (pitchOffset * -1);

    timeT snappedTime = e->snappedLeftTime;
    if (m_widget->isDrumMode() && !m_widget->getShowPercussionDurations()) {
        // Snap to closest grid time, left *or* right, because percussion
        // "notes" in normal/non-duration display mode conceptually fall
        // on the beat, not as a duration starting at the beat like pitched
        // notes or percussion ones in "Show percussion duration" mode.
        if (fabs(e->snappedRightTime - e->time) <
            fabs(e->snappedLeftTime  - e->time)) {
            snappedTime = e->snappedRightTime;
        }
    }

    Event *ev = new Event(Note::EventType, snappedTime, e->snapUnit);
    ev->set<Int>(BaseProperties::PITCH, adjustedPitch);
    ev->set<Int>(BaseProperties::VELOCITY, velocity);

    RG_DEBUG << "handleLeftButtonPress(): I'm working from segment \"" << m_currentViewSegment->getSegment().getLabel() << "\"" << "  clicked pitch: " << e->pitch << " adjusted pitch: " << adjustedPitch;

    m_currentElement = new MatrixElement(m_scene, ev, m_widget->isDrumMode(),
                                         pitchOffset,
                                         m_scene->getCurrentSegment(),
                                         m_scene->getCurrentSegmentIndex());

    // preview
    m_scene->playNote(m_currentViewSegment->getSegment(),
                      adjustedPitch,
                      velocity);
}

FollowMode
MatrixPainter::handleMouseMove(const MatrixMouseEvent *e)
{
    if (!e) return NO_FOLLOW;

    if ( e->buttons == Qt::NoButton &&
        (e->modifiers & Qt::ControlModifier)) {
        if (overlaySelectOrSegment(e))
            return m_toolbox->getActiveTool()->handleMouseMove(e);
    }

    setContextHelpForPos(e);

    // sanity check
    if (!m_currentElement) return NO_FOLLOW;

    timeT time = m_clickTime;
    timeT endTime = e->snappedRightTime;
    if (endTime <= time && e->snappedLeftTime < time) endTime = e->snappedLeftTime;
    if (endTime == time) endTime = time + e->snapUnit;
    if (time > endTime) std::swap(time, endTime);

    //RG_DEBUG << "handleMouseMove(): pitch = " << e->pitch << "time = " << time << ", end time = " << endTime;

    using BaseProperties::PITCH;

    // we need the same pitch/height corrections for dragging notes as for
    // entering new ones
    m_currentViewSegment = e->viewSegment;
    if (!m_currentViewSegment) return NO_FOLLOW;
    long pitchOffset = m_currentViewSegment->getSegment().getTranspose();
    long adjustedPitch = e->pitch + (pitchOffset * -1);

    long velocity = m_widget->getCurrentVelocity();
    m_currentElement->event()->get<Int>(BaseProperties::VELOCITY, velocity);

    timeT duration = endTime - time;
    if (m_widget->isDrumMode() && !m_widget->getShowPercussionDurations()) {
        // Just move, don't adjust duration.
        // See "Snap to closest grid time" comment above for why
        // left-or-right snapping.
        if (fabs(e->snappedRightTime - e->time) <
            fabs(e->snappedLeftTime  - e->time)) {
            time = e->snappedRightTime;
        }
        else {
            time = e->snappedLeftTime;
        }
        duration = e->snapUnit;
    }

    Event *ev = new Event(Note::EventType, time, duration);
    ev->set<Int>(BaseProperties::PITCH, adjustedPitch);
    ev->set<Int>(BaseProperties::VELOCITY, velocity);

    bool preview = false;
    if (m_currentElement->event()->has(PITCH) &&
        adjustedPitch != m_currentElement->event()->get<Int>(PITCH)) {
        preview = true;
    }

    Event *oldEv = m_currentElement->event();
    delete m_currentElement;
    delete oldEv;

    // const Segment *segment = e->element ? e->element->getSegment() : nullptr;
    m_currentElement = new MatrixElement(m_scene, ev, m_widget->isDrumMode(),
                                         pitchOffset,
                                         m_scene->getCurrentSegment(),
                                         m_scene->getCurrentSegmentIndex());

    if (preview) {
        m_scene->playNote(m_currentViewSegment->getSegment(), adjustedPitch, velocity);
    }

    return (FOLLOW_HORIZONTAL | FOLLOW_VERTICAL);
}

void MatrixPainter::handleMouseRelease(const MatrixMouseEvent *e)
{
    // Have to do this here instead of at exit because move of
    // MatrixElement and/or replacement with new one from command
    // execution can result in e->element pointing to deleted object
    setContextHelpForPos(e);

    m_isFinished = true;

    // This can happen in case of screen/window capture -
    // we only get a mouse release, the window snapshot tool
    // got the mouse down
    if (!m_currentElement) return;

    Event* ev = nullptr;

    timeT time = m_clickTime;
    if (m_widget->isDrumMode()) {
        if (m_widget->getShowPercussionDurations()) {
            // Same as normal pitched segment/notes, below.
            timeT endTime = e->snappedRightTime;
            if (endTime <= time && e->snappedLeftTime < time) {
                endTime = e->snappedLeftTime;
            }
            if (endTime == time) endTime = time + e->snapUnit;
            if (time > endTime) std::swap(time, endTime);
        } else {
            // Closest grid time, left or right
            // See "Snap to closest grid time" comment in
            // handleLeftButtonPress() above for why left-or-right snapping.
            if (fabs(e->snappedRightTime - e->time) <
                fabs(e->snappedLeftTime  - e->time)) {
                time = e->snappedRightTime;
            }
            else {
                time = e->snappedLeftTime;
            }
        }

        MatrixPercussionInsertionCommand *command =
            new MatrixPercussionInsertionCommand(m_currentViewSegment->
                                                   getSegment(),
                                                 time,
                                                 m_currentElement->event());
        CommandHistory::getInstance()->addCommand(command);

        ev = m_currentElement->event();
        delete m_currentElement;
        delete ev;

        ev = command->getLastInsertedEvent();
        if (ev) {
            m_scene->setSingleSelectedEvent
                (&m_currentViewSegment->getSegment(), ev, false);
        }
    } else {  // Normal pitched segment/notes.
        timeT endTime = e->snappedRightTime;
        if (endTime <= time && e->snappedLeftTime < time) {
            endTime = e->snappedLeftTime;
        }
        if (endTime == time) endTime = time + e->snapUnit;
        if (time > endTime) std::swap(time, endTime);

        SegmentMatrixHelper helper(m_currentViewSegment->getSegment());

        MatrixInsertionCommand* command =
            new MatrixInsertionCommand(m_currentViewSegment->getSegment(),
                                       time,
                                       endTime,
                                       m_currentElement->event());

        CommandHistory::getInstance()->addCommand(command);

        ev = m_currentElement->event();
        delete m_currentElement;
        delete ev;

        ev = command->getLastInsertedEvent();
        if (ev) {
            m_scene->setSingleSelectedEvent
                (&m_currentViewSegment->getSegment(), ev, false);
        }
    }

    // Add newly created note to m_doubleClickSelection which
    // contains previously selected notes (if any) which are now not
    // as newly created note is now sole member of selection.
    if (ev) m_doubleClickSelection->addEvent(ev);

    m_currentElement = nullptr;
    m_currentViewSegment = nullptr;
}

bool
MatrixPainter::handleKeyPress(const MatrixMouseEvent *e, const int key)
{
    if (!e) {
        setContextHelpForPos(e);
        return true;
    }

    if (key == Qt::Key_Control && overlaySelectOrSegment(e)) {
        m_toolbox->getActiveTool()->readyAtPos(e);
        return true;
    }

    if (!m_isFinished || key != ALTERNATE_TOOL_QT_KEY) return false;
    MatrixTool *multiTool = dynamic_cast<MatrixTool*>(
                            m_toolbox->getTool(MatrixMultiTool::ToolName()));
    if (!multiTool) return false;    // shouldn't ever happen
    multiTool->setRole(Role::PERSISTENT);
    m_toolbox->appendDispatchTool(multiTool, e);
    return true;
}

bool
MatrixPainter::handleKeyRelease(const MatrixMouseEvent *e, const int key)
{
    if (key == Qt::Key_Control) {
        m_toolbox->popDispatchTool();   // MatrixSelect or MatrixSegment
        readyAtPos(e);
        return true;
    }
    else if (key != ALTERNATE_TOOL_QT_KEY)
        return false;

    if (m_isFinished) {
        m_toolbox->popDispatchTool();
        return true;
    }
    else
        return false;
}

void MatrixPainter::readyAtPos(const MatrixMouseEvent *e)
{
    setCursor();
    m_isFinished = true;
    if (e && (e->modifiers & Qt::ControlModifier) && overlaySelectOrSegment(e))
        return;
    setContextHelpForPos(e);
}

void
MatrixPainter::setCursor()
{
    if (m_widget) {
        if (m_cursor) m_widget->setCanvasCursor(m_cursor);
        else          m_widget->setCanvasCursor(Qt::CrossCursor);
    }
}

void
MatrixPainter::setSelectCursor()
{
    if (m_widget) {
        if (m_selectCursor) m_widget->setCanvasCursor(m_selectCursor);
        else          m_widget->setCanvasCursor(Qt::CrossCursor);
    }
}

QString
MatrixPainter::altToolHelpString()
const
{
    return altToolHelpStringTools(tr("draw"), tr("multi"));
}

void
MatrixPainter::setContextHelpForPos(const MatrixMouseEvent *e)
{
    if (!e) {
        setContextHelp(tr("Draw notes tool: Move mouse into matrix "
                          "grid for help."));
        return;
    }

    if (e->buttons != Qt::NoButton) {
        setContextHelp(tr("Drag to move or resize note (add Shift to disable "
                          "grid snap), release button to finish"));
        return;
    }

    ContextInfo contextInfo(this, e);
    QString help = tr("Click to create note, release to finish "
                      "or drag to move/resize");

    if (e->element) {
        const Segment *segment = e->element->getSegment();

        if (segment == m_scene->getCurrentSegment())
            help += tr(", or double-click to delete") +
                    (contextInfo.inSelection ? tr(" selected notes")
                                             : tr(" note")) +
                    tr(", or right-click (optional add Shift) to "
                       "edit note properties");
        else
            help += tr(", or Ctrl-click to switch to ") +
                    m_widget->segmentTrackInstrumentLabel(
                        " %5 (Track %1, %2, %3, %4)", segment) +
                    tr(", or right-click for tool/undo/segment menu");
    }
    else
        help += tr(", or right-click for tool/undo/segment menu");

    help += m_toolbox->getCurrentTool()->altToolHelpString();

    setContextHelp(help);
}

QString MatrixPainter::ToolName() { return "painter"; }

}
