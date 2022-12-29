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

#define RG_MODULE_STRING "[MatrixMover]"
#define RG_NO_DEBUG_PRINT 1

#include "MatrixMover.h"

#include "base/BaseProperties.h"
#include "base/Event.h"
#include "base/Segment.h"
#include "base/Selection.h"
#include "base/SnapGrid.h"
#include "base/ViewElement.h"
#include "commands/matrix/MatrixModifyCommand.h"
#include "commands/matrix/MatrixInsertionCommand.h"
#include "commands/notation/NormalizeRestsCommand.h"
#include "document/CommandHistory.h"
#include "misc/Debug.h"

#include "MatrixElement.h"
#include "MatrixMouseEvent.h"
#include "MatrixResizer.h"
#include "MatrixScene.h"
#include "MatrixTool.h"
#include "MatrixToolBox.h"
#include "MatrixViewSegment.h"
#include "MatrixWidget.h"


#include <Qt>


namespace Rosegarden
{

QCursor *MatrixMover::m_cursor      (nullptr);
QCursor *MatrixMover::m_selectCursor(nullptr);


MatrixMover::MatrixMover(MatrixWidget *parent, MatrixToolBox *toolbox) :
    MatrixTool("MatrixMover", parent, toolbox),
    m_currentElement(nullptr),
    m_event(nullptr),
    m_currentViewSegment(nullptr),
    m_clickSnappedLeftDeltaTime(0),
    m_duplicateElements(),
    m_quickCopy(false),
    m_lastPlayedPitch(-1),
    m_crntElementCrntTime(-1),
    m_prevDiffPitch(IMPOSSIBLE_DIFF_PITCH),
    m_mouseMoved(false)
{
    createMenu();
    if (!m_cursor      ) m_cursor       = makeCursor("move_cursor", 11, 11);
    if (!m_selectCursor) m_selectCursor = makeCursor("move_select_cursor",
                                                     16, 16);
}

void
MatrixMover::handleEventRemoved(Event *event)
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
MatrixMover::handleLeftButtonPress(const MatrixMouseEvent *e)
{
    RG_DEBUG << "handleLeftButtonPress() : snapped time = " << e->snappedLeftTime << ", el = " << e->element;

    m_isFinished = false;
    m_mouseMoved = false;

    if (!e->element) return;

    Segment *segment = m_scene->getCurrentSegment();
    if (!segment) return;

    m_currentViewSegment = e->viewSegment;
    m_currentElement = e->element;
    m_event = m_currentElement->event();

    timeT snappedAbsoluteLeftTime =
        getSnapGrid()->snapTime(m_currentElement->getViewAbsoluteTime());
    m_clickSnappedLeftDeltaTime = e->snappedLeftTime - snappedAbsoluteLeftTime;

    m_quickCopy = (e->modifiers & Qt::ControlModifier);

    if (!m_duplicateElements.empty()) {
        for (size_t i = 0; i < m_duplicateElements.size(); ++i) {
            delete m_duplicateElements[i]->event();
            delete m_duplicateElements[i];
        }
        m_duplicateElements.clear();
    }

    // Add this element and allow movement
    //
    EventSelection *selection = m_scene->getSelection();

    if (selection) {
        EventSelection *newSelection;
        bool didShiftRemove = false;

        if ((e->modifiers & Qt::ShiftModifier) ||
            selection->contains(m_event)) {
            newSelection = new EventSelection(*selection);
        } else {
            newSelection = new EventSelection(m_currentViewSegment->
                                              getSegment());
        }

        // if the selection already contains the event, remove it from the
        // selection if shift is pressed
        if (selection->contains(m_event)) {
            if (e->modifiers & Qt::ShiftModifier) {
                newSelection->removeEvent(m_event);
                didShiftRemove = true;
            }
        } else {
            newSelection->addEvent(m_event);
        }

        m_scene->setSelection(newSelection, true);

        if (didShiftRemove) {
            m_currentElement = nullptr;
            m_event = nullptr;
            setContextHelpForPos(e);
            return;
        }

        selection = newSelection;
    } else {
        m_scene->setSingleSelectedEvent(m_currentViewSegment,
                                        m_currentElement, true);
    }

    long velocity = m_widget->getCurrentVelocity();
    m_event->get<Int>(BaseProperties::VELOCITY, velocity);

    long pitchOffset = m_currentViewSegment->getSegment().getTranspose();

    long pitch = 60;
    m_event->get<Int>(BaseProperties::PITCH, pitch);

    // We used to m_scene->playNote() here, but the new concert pitch matrix was
    // playing chords the first time I clicked a note.  Investigation with
    // KMidiMon revealed two notes firing nearly simultaneously, and with
    // segments of 0 transpose, they were simply identical to each other.  One
    // of them came from here, and this was the one sounding at the wrong pitch
    // in transposed segments.  I've simply removed it with no apparent ill side
    // effects, and a problem solved super cheap.

    m_lastPlayedPitch = pitch;

    if (m_quickCopy && selection) {
        for (EventContainer::iterator i =
                 selection->getSegmentEvents().begin();
             i != selection->getSegmentEvents().end(); ++i) {
            MatrixElement *duplicate = new MatrixElement
                (m_scene, new Event(**i),
                 m_widget->isDrumMode(), pitchOffset,
                 m_scene->getCurrentSegment());

            m_duplicateElements.push_back(duplicate);
        }
    }

    m_crntElementCrntTime = e->snappedLeftTime - m_clickSnappedLeftDeltaTime;
    m_prevDiffPitch = IMPOSSIBLE_DIFF_PITCH;

    setContextHelpForPos(e);
}

void MatrixMover::handleMidButtonPress(const MatrixMouseEvent *event)
{
    MatrixTool *resizer = dynamic_cast<MatrixTool*>(
                            m_toolbox->getTool(MatrixResizer::ToolName()));
    if (!resizer) return;    // shouldn't ever happen
    resizer->setRole(Role::SINGLE_ACTION);
    m_toolbox->appendDispatchTool(resizer, event);

    // Resizer might have done appendDispatchTool() of Select or Segment.
    m_toolbox->getActiveTool()->handleLeftButtonPress(event);
}

bool
MatrixMover::handleKeyPress(const MatrixMouseEvent *e, const int key)
{
    if (!m_isFinished || key != ALTERNATE_TOOL_QT_KEY)
        return false;

    if (m_role == Role::OVERLAY) {  // from MultiTool
        m_toolbox->popDispatchTool();
        return m_toolbox->getActiveTool()->handleKeyPress(e, key);
    }

    MatrixTool *resizer = dynamic_cast<MatrixTool*>(
                            m_toolbox->getTool(MatrixResizer::ToolName()));
    if (!resizer) return false;  // shouldn't ever happen
    resizer->setRole(Role::PERSISTENT);
    m_toolbox->appendDispatchTool(resizer, e);
    return true;
}

FollowMode
MatrixMover::handleMouseMove(const MatrixMouseEvent *e)
{
    if (!e || (e->buttons == Qt::NoButton && overlaySelectOrSegment(e)))
        return NO_FOLLOW;

    m_mouseMoved = true;

    setContextHelpForPos(e);

    if (!m_currentElement || !m_currentViewSegment) return NO_FOLLOW;

    timeT newTime = e->snappedLeftTime - m_clickSnappedLeftDeltaTime;
    int newPitch = e->pitch;

    emit hoveredOverNoteChanged(newPitch, true, newTime);

    // get a basic pitch difference calculation comparing the current element's
    // pitch to the clicked pitch (this does not take the transpose factor into
    // account, so in a -9 segment, the initial result winds up being 9
    // semitones too low)
    using BaseProperties::PITCH;
    int diffPitch = 0;
    if (m_event->has(PITCH)) {
        diffPitch = newPitch - m_event->get<Int>(PITCH);
    }

    long pitchOffset;
    if (m_scene && m_scene->getSelection())
        pitchOffset = m_scene->getSelection()->getSegment().getTranspose();
    else if (e->element && e->element->getSegment())
        pitchOffset = e->element->getSegment()->getTranspose();
    else
        return NO_FOLLOW;

    diffPitch += (pitchOffset * -1);
    if (m_prevDiffPitch == IMPOSSIBLE_DIFF_PITCH) m_prevDiffPitch = diffPitch;

    if (diffPitch != m_prevDiffPitch || newTime != m_crntElementCrntTime) {
        m_prevDiffPitch = diffPitch;
        m_crntElementCrntTime = newTime;
        for (MatrixElement *element : m_currentViewSegment->
                                        getSelectedElements()) {
            timeT diffTime = element->getViewAbsoluteTime() -
                m_currentElement->getViewAbsoluteTime();

            int epitch = 0;
            if (element->event()->has(PITCH)) {
                epitch = element->event()->get<Int>(PITCH);
            }

            element->reconfigure(newTime + diffTime,
                                 element->getViewDuration(),
                                 epitch + diffPitch);
        }
    }

    if (newPitch != m_lastPlayedPitch) {
        long velocity = m_widget->getCurrentVelocity();
        m_event->get<Int>(BaseProperties::VELOCITY, velocity);
        m_scene->playNote(m_currentViewSegment->getSegment(), newPitch + (pitchOffset * -1), velocity);
        m_lastPlayedPitch = newPitch;
    }

    return (FOLLOW_HORIZONTAL | FOLLOW_VERTICAL);
}

void
MatrixMover::handleMouseRelease(const MatrixMouseEvent *e)
{
    m_isFinished = true;

    if (!e || !m_mouseMoved) return;
    m_mouseMoved = false;

    RG_DEBUG << "handleMouseRelease() - newPitch = " << e->pitch;

    if (!m_currentElement || !m_currentViewSegment) return;

    timeT newTime = e->snappedLeftTime - m_clickSnappedLeftDeltaTime;
    int newPitch = e->pitch;

    if (newPitch > 127) newPitch = 127;
    if (newPitch < 0) newPitch = 0;

    // get a basic pitch difference calculation comparing the current element's
    // pitch to the pitch the mouse was released at (see note in
    // handleMouseMove)
    using BaseProperties::PITCH;
    timeT diffTime = newTime - m_currentElement->getViewAbsoluteTime();
    int diffPitch = 0;
    if (m_event && m_event->has(PITCH)) {
        diffPitch = newPitch - m_event->get<Int>(PITCH);
    }

    EventSelection* selection = m_scene->getSelection();
    // factor in transpose to adjust the height calculation
    bool havePitchOffset = true;
    long pitchOffset = 0;
    if (selection)
        pitchOffset = m_scene->getSelection()->getSegment().getTranspose();
    else if (e->element && e->element->getSegment())
        pitchOffset = e->element->getSegment()->getTranspose();
    else
        havePitchOffset = false;

    diffPitch += (pitchOffset * -1);

    if ((havePitchOffset && diffTime == 0 && diffPitch == 0) ||
        (selection && selection->getAddedEvents() == 0)) {
        for (size_t i = 0; i < m_duplicateElements.size(); ++i) {
            delete m_duplicateElements[i]->event();
            delete m_duplicateElements[i];
        }
        m_duplicateElements.clear();
        m_currentElement = nullptr;
        m_event = nullptr;
        return;
    }

    if (newPitch != m_lastPlayedPitch) {
        long velocity = m_widget->getCurrentVelocity();
        m_event->get<Int>(BaseProperties::VELOCITY, velocity);
        m_scene->playNote(m_currentViewSegment->getSegment(),
                          newPitch + (pitchOffset * -1),
                          velocity);
        m_lastPlayedPitch = newPitch;
    }

    QString commandLabel;
    if (m_quickCopy) {
        if (selection && selection->getAddedEvents() < 2) {
            commandLabel = tr("Copy and Move Event");
        } else {
            commandLabel = tr("Copy and Move Events");
        }
    } else {
        if (selection->getAddedEvents() < 2) {
            commandLabel = tr("Move Event");
        } else {
            commandLabel = tr("Move Events");
        }
    }

    MacroCommand *macro = new MacroCommand(commandLabel);

    EventSelection singleSelection(*const_cast<Segment*>(m_currentElement->
                                                         getSegment()));
    if (!selection) {
        if (m_event) singleSelection.addEvent(m_event);
        selection = &singleSelection;
    }

    EventContainer::iterator it =
        selection->getSegmentEvents().begin();

    Segment &segment = m_currentViewSegment->getSegment();

    EventSelection *newSelection = new EventSelection(segment);

    timeT normalizeStart = selection->getStartTime();
    timeT normalizeEnd = selection->getEndTime();

    if (m_quickCopy) {
        for (size_t i = 0; i < m_duplicateElements.size(); ++i) {
            timeT time = m_duplicateElements[i]->getViewAbsoluteTime();
            timeT endTime = time + m_duplicateElements[i]->getViewDuration();
            if (time < normalizeStart) normalizeStart = time;
            if (endTime > normalizeEnd) normalizeEnd = endTime;
            macro->addCommand(new MatrixInsertionCommand
                              (segment, time, endTime,
                               m_duplicateElements[i]->event()));
            delete m_duplicateElements[i]->event();
            delete m_duplicateElements[i];
        }
        m_duplicateElements.clear();
        m_quickCopy = false;
    }

    for (; it != selection->getSegmentEvents().end(); ++it) {

        timeT newTime = (*it)->getAbsoluteTime() + diffTime;

        int newPitch = 60;
        if ((*it)->has(PITCH)) {
            newPitch = (*it)->get<Int>(PITCH) + diffPitch;
        }

        Event *newEvent = nullptr;

        if (newTime < segment.getStartTime()) {
            newTime = segment.getStartTime();
        }

        if (newTime + (*it)->getDuration() >= segment.getEndMarkerTime()) {
            timeT limit = getSnapGrid()->snapTime
                (segment.getEndMarkerTime() - 1, SnapGrid::SnapLeft);
            if (newTime > limit) newTime = limit;
            timeT newDuration = std::min
                ((*it)->getDuration(), segment.getEndMarkerTime() - newTime);
            newEvent = new Event(**it, newTime, newDuration);
        } else {
            newEvent = new Event(**it, newTime);
        }

        newEvent->set<Int>(BaseProperties::PITCH, newPitch);

        macro->addCommand(new MatrixModifyCommand(segment,
                                                  (*it),
                                                  newEvent,
                                                  true,
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
    m_scene->setSelection(newSelection, false);

//    m_mParentView->canvas()->update();
    m_currentElement = nullptr;
    m_event = nullptr;

    MatrixMouseEvent ePrime(*e);
    m_scene->setMouseEventElement(ePrime);  // might have changed
    setContextHelpForPos(&ePrime);
}

void MatrixMover::readyAtPos(const MatrixMouseEvent *e)
{
    setCursor();
    m_isFinished = true;
    if (!overlaySelectOrSegment(e)) setContextHelpForPos(e);
}

void
MatrixMover::setCursor()
{
    if (m_widget) {
        if (m_cursor) m_widget->setCanvasCursor(m_cursor);
        else          m_widget->setCanvasCursor(Qt::SizeAllCursor);
    }
}
void
MatrixMover::setSelectCursor()
{
    if (m_widget) {
        if (m_selectCursor) m_widget->setCanvasCursor(m_selectCursor);
        else                m_widget->setCanvasCursor(Qt::SizeAllCursor);
    }
}

QString
MatrixMover::altToolHelpString()
const
{
    return altToolHelpStringTools(tr("move"), tr("resize"));
}

void MatrixMover::setContextHelpForPos(const MatrixMouseEvent *e)
{
    if (!e) {
        setContextHelp(tr("Move tool: Move mouse over note or empty "
                          "area for help."));
        return;
    }

    moveResizeVelocityContextHelp(e,
                                  tr("move (Ctrl to copy)"),
                                  tr("move/copy"),
                                  tr(", hold Shift to disable grid snap"));
}

QString MatrixMover::ToolName() { return "mover"; }

}
