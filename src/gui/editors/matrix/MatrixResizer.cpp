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

#include "MatrixResizer.h"

#include "base/BaseProperties.h"
#include "base/Event.h"
#include "base/Segment.h"
#include "base/Selection.h"
#include "base/SnapGrid.h"
#include "base/ViewElement.h"
#include "commands/matrix/MatrixModifyCommand.h"
#include "commands/notation/NormalizeRestsCommand.h"
#include "document/CommandHistory.h"
#include "MatrixElement.h"
#include "MatrixMover.h"
#include "MatrixScene.h"
#include "MatrixTool.h"
#include "MatrixToolBox.h"
#include "MatrixWidget.h"
#include "MatrixViewSegment.h"
#include "MatrixMouseEvent.h"
#include "misc/Debug.h"

#include <Qt>


namespace Rosegarden
{

QCursor *MatrixResizer::m_cursor(nullptr);
QCursor *MatrixResizer::m_selectCursor(nullptr);

MatrixResizer::MatrixResizer(MatrixWidget *parent, MatrixToolBox *toolbox) :
    MatrixTool("MatrixResizer", parent, toolbox),
    m_currentElement(nullptr),
    m_event(nullptr),
    m_currentViewSegment(nullptr),
    m_mouseMoved(false)
{
    createMenu();

    if (!m_cursor      ) m_cursor       = makeCursor("resize_cursor", 11, 11);
    if (!m_selectCursor) m_selectCursor = makeCursor("resize_select_cursor",
                                                     16, 16);
}

void
MatrixResizer::handleEventRemoved(Event *event)
{
    // NOTE: Do not use m_currentElement in here as it was freed by
    //       ViewSegment::eventRemoved() before we get here.

    // Is it the event we are holding on to?
    if (m_event == event) {
        m_currentElement = nullptr;
        m_event = nullptr;
    }
}

void
MatrixResizer::handleLeftButtonPress(const MatrixMouseEvent *e)
{
    MATRIX_DEBUG << "MatrixResizer::handleLeftButtonPress() : el = "
                 << e->element;

    m_isFinished = false;
    m_mouseMoved = false;

    if (!e->element) return;

    // Only resize normal segment notes, or percussion segment notes
    // if in show percussion durations
    if (m_widget->isDrumMode() && !m_widget->getShowPercussionDurations())
        return;

    // Only resize active segment's notes
    if (e->element->getSegment() !=
        e->element->getScene()->getCurrentSegment()) {
        RG_WARNING << "handleLeftButtonPress(): Will only resize notes "
                      "in active segment.";
        return;
    }

    m_currentViewSegment = e->viewSegment;
    m_currentElement = e->element;
    m_event = m_currentElement->event();

    // Add this element and allow movement
    //
    EventSelection* selection = m_scene->getSelection();

    if (selection) {
        EventSelection *newSelection;

        if ((e->modifiers & Qt::ShiftModifier) ||
            selection->contains(m_event)) {
            newSelection = new EventSelection(*selection);
        } else {
            newSelection = new EventSelection(m_currentViewSegment->getSegment());
        }

        newSelection->addEvent(m_event);
        m_scene->setSelection(newSelection, true);
//        m_mParentView->canvas()->update();
    } else {
        m_scene->setSingleSelectedEvent(m_currentViewSegment,
                                        m_currentElement,
                                        true);
//            m_mParentView->canvas()->update();
    }
}

void MatrixResizer::handleMidButtonPress(const MatrixMouseEvent *event)
{
    MatrixTool *mover = dynamic_cast<MatrixTool*>(
                            m_toolbox->getTool(MatrixMover::ToolName()));
    if (!mover) return;  // shouldn't ever happen
    mover->setRole(Role::SINGLE_ACTION);
    m_toolbox->appendDispatchTool(mover, event);
    // Mover might have done appendDispatchTool() of Select or Segment.
    m_toolbox->getActiveTool()->handleLeftButtonPress(event);
}

bool
MatrixResizer::handleKeyPress(const MatrixMouseEvent *e, const int key)
{
    if (!m_isFinished || key != ALTERNATE_TOOL_QT_KEY)
        return false;

    if (m_role == Role::OVERLAY) {  // from MultiTool
        m_toolbox->popDispatchTool();
        return m_toolbox->getActiveTool()->handleKeyPress(e, key);
    }

    MatrixTool *mover = dynamic_cast<MatrixTool*>(
                            m_toolbox->getTool(MatrixMover::ToolName()));
    if (!mover) return false;    // shouldn't ever happen
    mover->setRole(Role::PERSISTENT);
    m_toolbox->appendDispatchTool(mover, e);
    return true;
}

FollowMode
MatrixResizer::handleMouseMove(const MatrixMouseEvent *e)
{
    if (!e || (e->buttons == Qt::NoButton && overlaySelectOrSegment(e)))
        return NO_FOLLOW;

    m_mouseMoved = true;

    setContextHelpForPos(e);

    if (!m_currentElement || !m_currentViewSegment) return NO_FOLLOW;

    if (getSnapGrid()->getSnapSetting() != SnapGrid::NoSnap) {
        setContextHelp(tr("Hold Shift to avoid snapping to beat grid"));
    }

    EventSelection* selection = m_scene->getSelection();

    // Can't happen, wouldn't be here if so.
    if (!selection || selection->getAddedEvents() == 0)
        return NO_FOLLOW;

    timeT durationDiff = calculateDurationDiff(e);

    if (durationDiff == 0) return FOLLOW_HORIZONTAL;

    for (MatrixElement *element : m_currentViewSegment->getSelectedElements()) {
        // Don't resize notes that are tied to subsequent ones.
        if (element->event()->has(BaseProperties::TIED_FORWARD)) continue;

        timeT t = element->getViewAbsoluteTime();
        timeT d = element->getViewDuration();

        adjustTimeAndDuration(t, d, element->event(), durationDiff);

        element->reconfigure(t, d);
    }

//    m_mParentView->canvas()->update();
    return FOLLOW_HORIZONTAL;
}

void
MatrixResizer::handleMouseRelease(const MatrixMouseEvent *e)
{
    m_isFinished = true;

    if (!e || !m_currentElement || !m_currentViewSegment || !m_mouseMoved)
        return;
    m_mouseMoved = false;

    timeT durationDiff = calculateDurationDiff(e);

    EventSelection *selection = m_scene->getSelection();

    // Can't happen, wouldn't be here if so.
    if (!selection || selection->getAddedEvents() == 0)
        return;

    QString commandLabel = tr("Resize Event");
    if (selection->getAddedEvents() > 1) commandLabel = tr("Resize Events");

    MacroCommand *macro = new MacroCommand(commandLabel);

    EventContainer::iterator it =
        selection->getSegmentEvents().begin();

    Segment &segment = m_currentViewSegment->getSegment();

    EventSelection *newSelection = new EventSelection(segment);

    // Save forward tied notes because not resizing them and
    // when command is executed they get deleted from the selection
    // along with the original, pre-resize, note they're tied to.
    // Can't be an EventSelection because those obey tied note
    // semantics and notes get deleted from them when their
    // tied note is removed from newSelection.
    EventContainer *forwardTiedNotes = new EventContainer();

    timeT normalizeStart = selection->getStartTime();
    timeT normalizeEnd = selection->getEndTime();

    for (; it != selection->getSegmentEvents().end(); ++it) {
        // Don't resize notes that are tied to subsequent ones.
        if ((*it)->has(BaseProperties::TIED_FORWARD)) {
            forwardTiedNotes->insert(*it);
            continue;
        }

        timeT t = (*it)->getAbsoluteTime();
        timeT d = (*it)->getDuration();

        adjustTimeAndDuration(t, d, *it, durationDiff);

        Event *newEvent = new Event(**it, t, d);

        macro->addCommand(new MatrixModifyCommand(segment,
                                                  *it,
                                                  newEvent,
                                                  false,
                                                  false));

        newSelection->addEvent(newEvent);
    }

    normalizeStart = std::min(normalizeStart, newSelection->getStartTime());
    normalizeEnd = std::max(normalizeEnd, newSelection->getEndTime());

    macro->addCommand(new NormalizeRestsCommand(segment,
                                                normalizeStart,
                                                normalizeEnd));
    m_scene->setSelection(nullptr, false);
    CommandHistory::getInstance()->addCommand(macro);

    // Tied forward notes were removed from newSelection during execution
    // of command (something to do with tied notes being selected together)
    // so add them back in.
    for (auto it = forwardTiedNotes->begin() ;
              it != forwardTiedNotes->end() ;
            ++it)
        newSelection->addEvent(*it, false, false);

    m_scene->setSelection(newSelection, false);

    delete forwardTiedNotes;

//    m_mParentView->update();
    m_currentElement = nullptr;
    m_event = nullptr;

    MatrixMouseEvent ePrime(*e);
    m_scene->setMouseEventElement(ePrime);  // might have changed
    setContextHelpForPos(&ePrime);
}

timeT
MatrixResizer::calculateDurationDiff(
const MatrixMouseEvent *e)
const
{
    timeT snapTime = e->snappedLeftTime;
    if (e->snappedRightTime - e->time < e->time - e->snappedLeftTime) {
        snapTime = e->snappedRightTime;
    }

    timeT newDuration  = snapTime - m_currentElement->getViewAbsoluteTime();
    timeT durationDiff = newDuration - m_currentElement->getViewDuration();

    return durationDiff;
}

void
MatrixResizer::adjustTimeAndDuration(timeT &time,
                                     timeT &duration,
                                     const Event *event,
                                     const timeT durationDiff)
const
{
    duration = duration + durationDiff;
    if (duration < 0) {
        if (event->has(BaseProperties::TIED_BACKWARD))
            // Don't resize last of tied note backwards
            duration = Note(Note::Shortest).getDuration();
        else {
            time = time + duration;
            duration = -duration;
        }
    } else if (duration == 0) {
        duration = getSnapGrid()->getSnapTime(time);
    }

    Segment &segment = m_currentViewSegment->getSegment();

    if (time + duration > segment.getEndMarkerTime()) {
        duration = segment.getEndMarkerTime() - time;
        if (duration <= 0) {
            duration = segment.getEndMarkerTime();
            time = duration - getSnapGrid()->getSnapTime(time);
        }
    }
}

void
MatrixResizer::readyAtPos(const MatrixMouseEvent *event)
{
    setCursor();
    m_isFinished = true;
    if (!overlaySelectOrSegment(event)) setContextHelpForPos(event);
}

void
MatrixResizer::setCursor()
{
    if (m_widget) {
        if (m_cursor) m_widget->setCanvasCursor(m_cursor);
        else          m_widget->setCanvasCursor(Qt::SizeHorCursor);
    }
}
void
MatrixResizer::setSelectCursor()
{
    if (m_widget) {
        if (m_selectCursor) m_widget->setCanvasCursor(m_selectCursor);
        else                m_widget->setCanvasCursor(Qt::SizeHorCursor);
    }
}

QString
MatrixResizer::altToolHelpString()
const
{
    return altToolHelpStringTools(tr("resize"), tr("move"));
}

void MatrixResizer::setContextHelpForPos(const MatrixMouseEvent *e)
{
    if (!e) {
        setContextHelp(tr("Resize tool: Move mouse over note or empty "
                          "area for help."));
        return;
    }

    if (m_widget->isDrumMode() && !m_widget->getShowPercussionDurations()) {
        setContextHelp(tr("Turn on \"View -> Notes -> Show percussion "
                          "durations\" to modify percussion segment notes, "
                          "or left-click for tool/undo/segment menu."));
        return;
    }

    moveResizeVelocityContextHelp(e, tr("resize"), tr("resize"), "");
}

QString MatrixResizer::ToolName() { return "resizer"; }

}
