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
#include "MatrixElement.h"
#include "MatrixScene.h"
#include "MatrixWidget.h"
#include "MatrixTool.h"
#include "MatrixMouseEvent.h"
#include "MatrixViewSegment.h"
#include "misc/Debug.h"

#include <Qt>


namespace Rosegarden
{

MatrixMover::MatrixMover(MatrixWidget *parent) :
    MatrixTool("matrixmover.rc", "MatrixMover", parent),
    m_currentElement(nullptr),
    m_event(nullptr),
    m_currentViewSegment(nullptr),
    m_clickSnappedLeftDeltaTime(0),
    m_duplicateElements(),
    m_quickCopy(false),
    m_lastPlayedPitch(-1),
    m_crntElementCrntTime(-1),
    m_prevDiffPitch(IMPOSSIBLE_DIFF_PITCH)
{
    createAction("select", SLOT(slotSelectSelected()));
    createAction("draw", SLOT(slotDrawSelected()));
    createAction("erase", SLOT(slotEraseSelected()));
    createAction("resize", SLOT(slotResizeSelected()));

    createMenu();
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

    if (!e->element) return;

    Segment *segment = m_scene->getCurrentSegment();
    if (!segment) return;

    // Check the scene's current segment (apparently not necessarily the same
    // segment referred to by the scene's current view segment) for this event;
    // return if not found, indicating that this event is from some other,
    // non-active segment.
    //
    // I think notation just makes whatever segment active when you click an
    // event outside the active segment, and I think that's what this code
    // attempted to do too.  I couldn't get that to work at all.  This is better
    // than being able to click on non-active elements to create new events by
    // accident, and will probably fly.  Especially since the multi-segment
    // matrix is new, and we're defining the terms of how it works.
    if (e->element->getSegment() != segment) {
        // The above comment written and following warning/return code written
        // 2009-12-18 05:01:46 +0000. Now implementing switching active
        // segment on Alt+click.
        // RG_WARNING << "handleLeftButtonPress(): "
        //               "Clicked element not owned by active segment. "
        //               "Returning...";
        // return;

        // Switch active segemnt on alt-click
        if (e->modifiers & Qt::AltModifier) {
            // But don't do if also shift or ctrl. Gets too confusing
            // for both code and user to simultaneously switch segments
            // and move/copy notes and/or add/subtract selection.
            if (e->modifiers & (Qt::ControlModifier | Qt::ShiftModifier)) {
                return;
            }
            const Segment *newSegment = const_cast<Segment*>(
                                            e->element->getSegment());
            m_widget->getScene()->setCurrentSegment(newSegment);
            m_widget->clearSelection();
            m_widget->updateToCurrentSegment(true, // true == change instrument
                                             newSegment);
            m_widget->generatePitchRuler(); //in case normal->drum or vice-versa
            m_currentViewSegment = nullptr;
            m_currentElement = nullptr;
            m_event = nullptr;
            setBasicContextHelp(e);
            return;  // do nothing more -- wait for fresh click in new segment
        } else {
            // Not an error, no need for bogus stdout/stderr warning.
            return;
        }
    }

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
    EventSelection* selection = m_scene->getSelection();

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
            setBasicContextHelp(e);
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

    setBasicContextHelp(e);
}

FollowMode
MatrixMover::handleMouseMove(const MatrixMouseEvent *e)
{
    if (!e) return NO_FOLLOW;

    setBasicContextHelp(e);

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

    EventSelection* selection = m_scene->getSelection();

    // factor in transpose to adjust the height calculation
    long pitchOffset = selection->getSegment().getTranspose();
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
    if (!e) return;

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
    if (m_event->has(PITCH)) {
        diffPitch = newPitch - m_event->get<Int>(PITCH);
    }

    EventSelection* selection = m_scene->getSelection();

    // factor in transpose to adjust the height calculation
    long pitchOffset = selection->getSegment().getTranspose();
    diffPitch += (pitchOffset * -1);

    if ((diffTime == 0 && diffPitch == 0) || selection->getAddedEvents() == 0) {
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
        m_scene->playNote(m_currentViewSegment->getSegment(), newPitch + (pitchOffset * -1), velocity);
        m_lastPlayedPitch = newPitch;
    }

    QString commandLabel;
    if (m_quickCopy) {
        if (selection->getAddedEvents() < 2) {
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

    setBasicContextHelp(nullptr);
}

void MatrixMover::ready()
{
    m_widget->setCanvasCursor(Qt::SizeAllCursor);
    setBasicContextHelp(nullptr);
}

void MatrixMover::stow()
{
    // Nothing of this vestigial code remains in modern Qt Rosegarden.
}

void MatrixMover::setBasicContextHelp(const MatrixMouseEvent *e)
{
    bool mouseButton     = false;
    bool altPressed      = false;
    bool onNote          = false;
    bool inActiveSegment = false;
    const Segment *newSegment = nullptr;

    if (e) {
        mouseButton     = static_cast<bool>(e->buttons);
        altPressed      = e->modifiers & Qt::AltModifier;
        onNote          = e->element;
        if (e->element) {
            newSegment = e->element->getSegment();
            inActiveSegment = onNote &&
                              newSegment == m_scene->getCurrentSegment();
        }
    }

    const EventSelection *selection = m_scene->getSelection();
    unsigned selectionSize = selection ? selection->getAddedEvents() : 0;
    QString msg("");

    if (m_currentElement && mouseButton && !altPressed) {
        if (selectionSize  > 0) {
            msg = tr("Move");
            if (selectionSize == 1) msg += tr(" note");
            else                    msg += tr(" notes");
            msg += tr(", release mouse button to place.");
        }
    } else if (onNote) {
        if (inActiveSegment) {
            msg = ("Click/drag to move, Ctrl-click/drag to copy, "
                   "Shift-click to add or remove from selection.");
        } else {
            msg =   tr("Alt-click to switch to")
                  + m_widget->segmentTrackInstrumentLabel(
                    " %5 (Track %1, %2, %3, %4)", newSegment);
        }
    }
    else msg = tr("Hover over note to move/copy/select/deselect "
                  "or switch active segment.");

    setContextHelp(msg);
}

QString MatrixMover::ToolName() { return "mover"; }

}
