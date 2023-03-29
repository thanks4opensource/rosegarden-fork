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
                                     const bool drumMode,
                                     const unsigned segmentIndex) :
    ViewSegment(*segment),
    m_scene(scene),
    m_drum(drumMode),
    m_segmentIndex(segmentIndex),
    m_refreshStatusId(segment->getNewRefreshStatusId())
{
}

MatrixViewSegment::~MatrixViewSegment()
{
    m_segment.deleteRefreshStatusId(m_refreshStatusId);
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

// This is invoked via SegmentObserver notifications.
void
MatrixViewSegment::eventAdded(const Segment *segment,
                              Event *event)
{
    ViewSegment::eventAdded(segment, event);
}

// This is invoked via SegmentObserver notifications.
void
MatrixViewSegment::eventRemoved(const Segment *segment,
                                Event *event)
{
    // !!! This deletes the associated MatrixElement.
    ViewSegment::eventRemoved(segment, event);

    // At this point, the MatrixElement is gone.  See ViewElement::erase().
    // Any clients that respond to the following and any handlers of
    // signals emitted by the following must not try to use the MatrixElement.
}

ViewElement *
MatrixViewSegment::makeViewElement(Event* e)
{
    //RG_DEBUG << "makeViewElement(): event at " << e->getAbsoluteTime();

    // transpose bits
    long pitchOffset = getSegment().getTranspose();

    return new MatrixElement(m_scene,
                             e,
                             m_drum,
                             pitchOffset, &getSegment(),
                             m_segmentIndex);
}

#if 0  // Respond to finer-grained colourChanged() and labelChanged() instead
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
#endif

// This is invoked via SegmentObserver notifications.
void
MatrixViewSegment::colourChanged(const Segment *segment)
{
#ifdef CHORD_NAME_RULER_AND_CONFLICTING_KEY_CHANGES_DEBUG
    qDebug() << "\nMatrixViewSegment::colourChanged():"
             << "\n  givn:"
             << (segment ? segment->getLabel() : "<nullptr>")
             << "\n  self:"
             << getSegment().getLabel();
#endif

    updateNoteColors();

    if (m_scene->getCurrentSegment() == segment) {
        m_scene->getMatrixWidget()->setSegmentLabelAndChangerColor(segment);
    }

    m_scene->getMatrixWidget()->getToolBox()->updateToolsMenus(segment);
}

// This is invoked via SegmentObserver notifications.
void
MatrixViewSegment::labelChanged(const Segment *segment)
{
#ifdef CHORD_NAME_RULER_AND_CONFLICTING_KEY_CHANGES_DEBUG
    qDebug() << "\nMatrixViewSegment::labelChanged():"
             << "\n  givn:"
             << (segment ? segment->getLabel() : "<nullptr>")
             << "\n  self:"
             << getSegment().getLabel();
#endif

    if (m_scene->getCurrentSegment() == segment) {
        m_scene->getMatrixWidget()->setSegmentTrackInstrumentLabel(segment);
    }
    m_scene->getMatrixWidget()->getToolBox()->updateToolsMenus(segment);
}

// This is invoked via SegmentObserver notifications.
void
MatrixViewSegment::startChanged(const Segment *segment, timeT time)
{
#ifdef CHORD_NAME_RULER_AND_CONFLICTING_KEY_CHANGES_DEBUG
    qDebug() << "\nMatrixViewSegment::startChanged():"
             << "\n  givn:"
             << (segment ? segment->getLabel() : "<nullptr>")
             << "\n  self:"
             << getSegment().getLabel();
#endif

    ViewSegment::endMarkerTimeChanged(segment, time);
    if (m_scene) m_scene->handleSegmentStartChanged(segment, time);
}

// This is invoked via SegmentObserver notifications.
void
MatrixViewSegment::endMarkerTimeChanged(const Segment *segment, bool shorten)
{
#ifdef CHORD_NAME_RULER_AND_CONFLICTING_KEY_CHANGES_DEBUG
    qDebug() << "\nMatrixViewSegment::endMarkerTimeChanged():"
             << "\n  givn:"
             << (segment ? segment->getLabel() : "<nullptr>")
             << "\n  self:"
             << getSegment().getLabel();
#endif

    ViewSegment::endMarkerTimeChanged(segment, shorten);
    if (m_scene) m_scene->handleSegmentEndMarkerTimeChanged(segment, shorten);
}

void
MatrixViewSegment::updateElements(timeT from, timeT to)
{
#ifndef CHORD_NAME_RULER_AND_CONFLICTING_KEY_CHANGES_DEBUG
    if (!m_viewElementList || m_viewElementList->empty())
        return;
#endif

    const Segment   *currentSegment = m_scene->getCurrentSegment();
    bool             keyDependent(m_scene->getMatrixWidget()
                                         ->notesAreKeyDependent());
#ifdef CHORD_NAME_RULER_AND_CONFLICTING_KEY_CHANGES_DEBUG
    qDebug() << "MatrixViewSegment:::updateElements() in "
             << getSegment().getLabel()
             << "\n "
             << from
             << "->"
             << to
             << (keyDependent ? "key-dependent" : "key-independent")
             << "\n  curseg:"
             << currentSegment->getLabel();
    if (!m_viewElementList || m_viewElementList->empty())
        return;
#endif

    ViewElementList::iterator viewElem = m_viewElementList->findTime(from),
                              endElem  = m_viewElementList->findTime(to  );

    if (!keyDependent) {
        for ( ; viewElem != m_viewElementList->end() ; ++viewElem) {
            MatrixElement *e = static_cast<MatrixElement *>(*viewElem);
            e->reconfigure(false, nullptr);
        }
        return;
    }

    if (!currentSegment) return;

    static const Key defaultKey;
    auto             keyIter(  currentSegment->getKeySignatureIterAtTime(from));
    const Key       *key    (  currentSegment->keySignatureIterIsValid(keyIter)
                             ? &(keyIter++)->second  // will change at next one
                             : &defaultKey);

    // For each ViewElement in the from-to range...
    while (viewElem != m_viewElementList->end()) {
        MatrixElement *e = static_cast<MatrixElement *>(*viewElem);

        if (   currentSegment->keySignatureIterIsValid(keyIter)
            && e->getTime() >= keyIter->first)
            key = &(keyIter++)->second;
        // else didn't pass next key change time, or are past end of key changes

        // Update the item on the display.
        e->reconfigure(true, key);  // true == key dependent

        if (viewElem++ == endElem)break;
    }
}  // updateElements()

void
MatrixViewSegment::reconfigureNotes()
{
    qDebug() << "MatrixViewSegment::reconfigureNotes() for"   // trosDEBUG
             << getSegment().getLabel();

    if (!m_viewElementList) return;

    for(ViewElementList::iterator i = m_viewElementList->begin();
        i != m_viewElementList->end();
        ++i) {
        MatrixElement *e = static_cast<MatrixElement *>(*i);
        e->reconfigure();
    }
}

void
MatrixViewSegment::updateNoteLabels()
{
#ifndef CHORD_NAME_RULER_AND_CONFLICTING_KEY_CHANGES_DEBUG
    if (!m_viewElementList || m_viewElementList->empty())
        return;
#endif

    bool             keyDependent(m_scene->getMatrixWidget()
                                         ->notesAreKeyDependent());
    const Segment   *currentSegment = m_scene->getCurrentSegment();

#ifdef CHORD_NAME_RULER_AND_CONFLICTING_KEY_CHANGES_DEBUG
    qDebug() << "MatrixViewSegment::updateNoteLabels()"
             << (keyDependent ? "key-dependent" : "key-independent")
             << "\n  this:"
             << getSegment().getLabel()
             << '@'
             << static_cast<const void*>(&getSegment())
             << "\n  crnt:"
             << (currentSegment ? currentSegment->getLabel() : "<nullptr>")
             << '@'
             << static_cast<const void*>(currentSegment);
    if (!m_viewElementList || m_viewElementList->empty())
        return;
#endif


    if (!keyDependent) {
        for (ViewElement* const element : *m_viewElementList)
            static_cast<MatrixElement*>(element)->setLabel(false, nullptr);
        return;
    }

    if (!currentSegment) return;

    static const Key defaultKey;
    auto             keyIter(  currentSegment->keySignatureMap().begin());
    const Key       *key    (  currentSegment->keySignatureIterIsValid(keyIter)
                             ? &(keyIter++)->second  // will change at next one
                             : &defaultKey);

    for (ViewElement* const viewElement : *m_viewElementList) {
        MatrixElement *matrixElement = static_cast<MatrixElement*>(viewElement);

        if (   currentSegment->keySignatureIterIsValid(keyIter)
            && matrixElement->getTime() >= keyIter->first)
            key = &(keyIter++)->second;
        // else didn't pass next key change time, or are past end of key changes

        matrixElement->setLabel(true, key);  // true == key dependent
    }
}  // updateNoteLabels()

void
MatrixViewSegment::updateNoteColors()
{
#if 1  // #ifndef CHORD_NAME_RULER_AND_CONFLICTING_KEY_CHANGES_DEBUG
    if (!m_viewElementList || m_viewElementList->empty())
        return;
#endif

    MatrixWidget    *matrixWidget  (m_scene->getMatrixWidget());
    bool             keyDependent  (matrixWidget->notesAreKeyDependent()),
                     colorDependent(matrixWidget->notesAreColorDependent());
    const Segment   *currentSegment = m_scene->getCurrentSegment();

#if 0  // #ifdef CHORD_NAME_RULER_AND_CONFLICTING_KEY_CHANGES_DEBUG
    qDebug() << "MatrixViewSegment::updateNoteColors()"
             << (keyDependent ? "key-dependent" : "key-independent")
             << (colorDependent ? "color-dependent" : "color-independent")
             << (  m_scene->getMatrixWidget()->getNoteColorAllSegments()
                 ? "color-all" : "only-active")
             << "\n  this:"
             << getSegment().getLabel()
             << '@'
             << static_cast<const void*>(&getSegment())
             << "\n  crnt:"
             << (currentSegment ? currentSegment->getLabel() : "<nullptr>")
             << '@'
             << static_cast<const void*>(currentSegment);
    if (!m_viewElementList || m_viewElementList->empty())
        return;
#endif


    if (!keyDependent && !colorDependent) {
        for (ViewElement* const element : *m_viewElementList)
            static_cast<MatrixElement*>(element)->setColor(keyDependent,
                                                           nullptr);
        return;
    }

    if (!currentSegment) return;

    static const Key defaultKey;
    auto             keyIter(  currentSegment->keySignatureMap().begin());
    const Key       *key    (  currentSegment->keySignatureIterIsValid(keyIter)
                             ? &(keyIter++)->second  // will change at next one
                             : &defaultKey);

    for (ViewElement* const viewElement : *m_viewElementList) {
        MatrixElement *matrixElement = static_cast<MatrixElement*>(viewElement);

        if (   currentSegment->keySignatureIterIsValid(keyIter)
            && matrixElement->getTime() >= keyIter->first)
            key = &(keyIter++)->second;
        // else didn't pass next key change time, or are past end of key changes

        matrixElement->setColor(true, key);  // true == key dependent
    }
}  // updateNoteColors()

void
MatrixViewSegment::setNotesCurrentSegment(
bool isCurrent)
{
#ifdef CHORD_NAME_RULER_AND_CONFLICTING_KEY_CHANGES_DEBUG
    qDebug() << "MatrixViewSegment::setNotesCurrentSegment() for"
             << getSegment().getLabel();
#endif

    for (ViewElement* const element : *m_viewElementList)
        static_cast<MatrixElement*>(element)->setCurrent(isCurrent);
}

void
MatrixViewSegment::updateTiedUntied(const EventContainer &notes)
{
#ifdef CHORD_NAME_RULER_AND_CONFLICTING_KEY_CHANGES_DEBUG
    qDebug() << "MatrixViewSegment::updateTiedUntied() for"
             << getSegment().getLabel();
#endif

    for (ViewElement* const element : *m_viewElementList) {
        MatrixElement *e = static_cast<MatrixElement*>(element);
        if (notes.find(e->event()) != notes.end())
            e->reconfigure();
    }
}

void
MatrixViewSegment::clearAllSelected()
{
#ifdef CHORD_NAME_RULER_AND_CONFLICTING_KEY_CHANGES_DEBUG
    qDebug() << "MatrixViewSegment::clearAllSelected() for"
             << getSegment().getLabel();
#endif

    for (ViewElement* const element : *m_viewElementList) {
        MatrixElement *e = static_cast<MatrixElement*>(element);
        e->setSelected(false);
        e->reconfigure();
    }
}

void
MatrixViewSegment::setDrumMode(
bool drumMode)
{
#ifdef CHORD_NAME_RULER_AND_CONFLICTING_KEY_CHANGES_DEBUG
    qDebug() << "MatrixViewSegment::setDrumMode() for"
             << getSegment().getLabel();
#endif

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
#ifdef CHORD_NAME_RULER_AND_CONFLICTING_KEY_CHANGES_DEBUG
    qDebug() << "MatrixViewSegment::getSelectedElements() in"
             << getSegment().getLabel();
#endif

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
