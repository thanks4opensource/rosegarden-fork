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

#define RG_MODULE_STRING "[MatrixViewSegment]"
#define NDEBUG 1

#include "MatrixViewSegment.h"

#include "MatrixElement.h"
#include "MatrixScene.h"
#include "MatrixToolBox.h"
#include "MatrixWidget.h"

#include "base/NotationTypes.h"
#include "base/SnapGrid.h"
#include "base/MidiProgram.h"
#include "base/SnapGrid.h"
#include "base/SnapGrid.h"

#include "misc/Debug.h"

namespace Rosegarden
{

MatrixViewSegment::MatrixViewSegment(MatrixScene *scene,
                                     Segment *segment,
                                     bool drumMode) :
    ViewSegment(*segment),
    m_scene(scene),
    m_drum(drumMode),
    m_refreshStatusId(segment->getNewRefreshStatusId())
{
}

MatrixViewSegment::~MatrixViewSegment()
{
}

SegmentRefreshStatus &
MatrixViewSegment::getRefreshStatus() const
{
    return m_segment.getRefreshStatus(m_refreshStatusId);
}

void
MatrixViewSegment::resetRefreshStatus()
{
    m_segment.getRefreshStatus(m_refreshStatusId).setNeedsRefresh(false);
}

bool
MatrixViewSegment::wrapEvent(Event* e)
{
    return e->isa(Note::EventType) && ViewSegment::wrapEvent(e);
}

void
MatrixViewSegment::eventAdded(const Segment *segment,
                              Event *event)
{
    ViewSegment::eventAdded(segment, event);
    m_scene->handleEventAdded(event);
}

void
MatrixViewSegment::eventRemoved(const Segment *segment,
                                Event *event)
{
    // !!! This deletes the associated MatrixElement.
    ViewSegment::eventRemoved(segment, event);

    // At this point, the MatrixElement is gone.  See ViewElement::erase().
    // Any clients that respond to the following and any handlers of
    // signals emitted by the following must not try to use the MatrixElement.
    m_scene->handleEventRemoved(event);
}

ViewElement *
MatrixViewSegment::makeViewElement(Event* e)
{
    //RG_DEBUG << "makeViewElement(): event at " << e->getAbsoluteTime();

    // transpose bits
    long pitchOffset = getSegment().getTranspose();

    return new MatrixElement(m_scene, e, m_drum, pitchOffset, &getSegment());
}

// This is invoked via SegmentObserver notifications.
void
MatrixViewSegment::appearanceChanged(const Segment* /*segment*/)
{
#if 0  // Respond to finer-grained colourChanged() and labelChanged() instead
    if (m_scene->getCurrentSegment() == segment) {
        m_scene->getMatrixWidget()->setSegmentTrackInstrumentLabel(segment);
    }
#endif
}

// This is invoked via SegmentObserver notifications.
void
MatrixViewSegment::colourChanged(const Segment *segment)
{
    for (auto element : *m_viewElementList)
        static_cast<MatrixElement*>(element)->setColor();

    if (m_scene->getCurrentSegment() == segment) {
        m_scene->getMatrixWidget()->setSegmentLabelAndChangerColor(segment);
    }

    m_scene->getMatrixWidget()->getToolBox()->updateToolsMenus(segment);
}

// This is invoked via SegmentObserver notifications.
void
MatrixViewSegment::labelChanged(const Segment *segment)
{
    if (m_scene->getCurrentSegment() == segment) {
        m_scene->getMatrixWidget()->setSegmentTrackInstrumentLabel(segment);
    }
    m_scene->getMatrixWidget()->getToolBox()->updateToolsMenus(segment);
}

// This is invoked via SegmentObserver notifications.
void
MatrixViewSegment::endMarkerTimeChanged(const Segment *segment, bool shorten)
{
    ViewSegment::endMarkerTimeChanged(segment, shorten);
    if (m_scene) m_scene->segmentEndMarkerTimeChanged(segment, shorten);
}

#if 0
// Is never(??) invoked because when segment is canonically removed
// in main interface (GDB reverse chronological):
//  #0  in Rosegarden::Segment::removeObserver(Rosegarden::SegmentObserver*) ()
//  #1 in non-virtual thunk to Rosegarden::CompositionModelImpl::segmentRemoved(Rosegarden::Composition const*, Rosegarden::Segment*) ()
//  #2 in Rosegarden::Composition::notifySegmentRemoved(Rosegarden::Segment*) const ()
//  #3 in Rosegarden::Composition::detachSegment(Rosegarden::Segment*) ()
//  #4 in Rosegarden::SegmentEraseCommand::execute() ()
//
// Therefor (debug output, forward chronological):
//  Composition::notifySegmentRemoved Rosegarden::Segment(0x32eed00)
//  [Segment] removeObserver Rosegarden::Segment(0x32eed00) 0x3548e10
//  [MatrixScene] segmentRemoved() Rosegarden::Segment(0x32eed00)
//  [Segment] removeObserver Rosegarden::Segment(0x32eed00) 0x303abe0
//  [MatrixWidget] slotDocumentModified()
//  [MatrixWidget] slotDocumentModified()
//  [Segment] dtor Rosegarden::Segment(0x34036f0)
//  [Segment] notifySourceDeletion Rosegarden::Segment(0x34036f0)
//
// So Segment::notifySourceDeletion() no longer has this object in
// notification set.
// Needs to be re-thought/redesigned/reimplmented, or
// Segment::segmentDeleted() removed
//
// This is invoked via SegmentObserver notifications.
void
MatrixViewSegment::segmentDeleted(const Segment *segment)
{
    RG_DEBUG << "segmentDeleted()"
             << segment
             << (segment ? segment->getLabel() : std::string("nullptr"));
}
#endif

void
MatrixViewSegment::updateElements(timeT from, timeT to)
{
    if (!m_viewElementList)
        return;

    ViewElementList::iterator viewElementIter =
            m_viewElementList->findTime(from);
    ViewElementList::iterator endIter = m_viewElementList->findTime(to);

    // For each ViewElement in the from-to range...
    while (viewElementIter != m_viewElementList->end()) {
        MatrixElement *e = static_cast<MatrixElement *>(*viewElementIter);
        // Update the item on the display.
        e->reconfigure();

        if (viewElementIter == endIter)
            break;
        ++viewElementIter;
    }
}

void
MatrixViewSegment::updateAll()
{
    for(ViewElementList::iterator i = m_viewElementList->begin();
        i != m_viewElementList->end();
        ++i) {
        MatrixElement *e = static_cast<MatrixElement *>(*i);
        e->reconfigure();
    }
}

void
MatrixViewSegment::updateAllColors()
{
    for(ViewElementList::iterator i = m_viewElementList->begin();
        i != m_viewElementList->end();
        ++i) {
        MatrixElement *e = static_cast<MatrixElement *>(*i);
        e->setColor();
    }
}

void
MatrixViewSegment::updateTiedUntied(const EventContainer &notes)
{
    for (auto elem : *m_viewElementList) {
        MatrixElement *e = static_cast<MatrixElement*>(elem);
        if (notes.find(e->event()) != notes.end())
            e->reconfigure();
    }
}

void
MatrixViewSegment::clearAllSelected()
{
    for (auto elem : *m_viewElementList) {
        MatrixElement *e = static_cast<MatrixElement*>(elem);
        e->setSelected(false);
        e->reconfigure();
    }
}

void
MatrixViewSegment::setDrumMode(
bool drumMode)
{
    m_drum = drumMode;
    for(ViewElementList::iterator i = m_viewElementList->begin();
        i != m_viewElementList->end();
        ++i) {
        MatrixElement *e = static_cast<MatrixElement *>(*i);
        e->setDrumMode(drumMode);
        e->reconfigure();
    }
}

std::vector<MatrixElement*> MatrixViewSegment::getSelectedElements()
{
    std::vector<MatrixElement*> selElems;

    for(ViewElementList::iterator i = m_viewElementList->begin();
        i != m_viewElementList->end();
        ++i) {
        MatrixElement *e = static_cast<MatrixElement *>(*i);
        if (e && e->isSelected()) selElems.push_back(e);
    }

    return selElems;
}

}
