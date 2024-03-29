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

#define RG_MODULE_STRING "[MatrixScene]"
#define RG_NO_DEBUG_PRINT 1

#include "MatrixScene.h"

#include "MatrixElement.h"
#include "MatrixTool.h"
#include "MatrixToolBox.h"
#include "MatrixMouseEvent.h"
#include "MatrixViewSegment.h"
#include "MatrixWidget.h"

#include "IsotropicDiamondItem.h"
#include "IsotropicRectItem.h"
#include "IsotropicTextItem.h"

#include "base/BaseProperties.h"
#include "base/ConflictingKeyChanges.h"
#include "base/NotationRules.h"
#include "base/RulerScale.h"
#include "base/Selection.h"
#include "base/SnapGrid.h"

#include "commands/edit/EraseCommand.h"

#include "document/Command.h"
#include "document/CommandHistory.h"
#include "document/RosegardenDocument.h"

#include "gui/application/RosegardenMainWindow.h"
#include "gui/dialogs/CheckableListDialog.h"
#include "gui/general/GUIPalette.h"
#include "gui/rulers/ChordNameRuler.h"
#include "gui/studio/StudioControl.h"
#include "gui/widgets/Panned.h"

#include "misc/ConfigGroups.h"
#include "misc/Debug.h"

#include "sequencer/RosegardenSequencer.h"

#include <QGraphicsLineItem>
#include <QGraphicsSceneMouseEvent>
#include <QGuiApplication>
#include <QKeyEvent>
#include <QPointF>
#include <QRectF>
#include <QSettings>


#if 0  // Failed experiments to use Alt or Tab for alternate tool switching
// Not defined in any Qt header file.
void qt_set_sequence_auto_mnemonic(bool b);
#endif

#include <algorithm>  // for std::sort
#include <numeric>    // for std::iota


//#define DEBUG_MOUSE

namespace Rosegarden
{


using namespace BaseProperties;

MatrixScene::MatrixScene() :
    graphicsRectPool(this),
    graphicsTextPool(this),
    graphicsIsotropicRectPool(this),
    graphicsIsotropicDiamondPool(this),
    graphicsEllipsePool(this),
    m_widget(nullptr),
    m_document(nullptr),
    m_observerAdded(false),
    m_currentSegmentIndex(0),
    m_scale(nullptr),
    m_referenceScale(nullptr),
    m_snapGrid(nullptr),
    m_resolution(8),
    m_selection(nullptr),
    m_highlightType(HT_BlackKeys),
    m_notesChanged(false),
    m_keysChanged(false),
    m_currentSegmentKeysChanged(false),
    m_segmentChanged(false),
    m_timeSignatureChanged(false)
{
    connect(CommandHistory::getInstance(),
            &CommandHistory::commandExecuted,
            this,
            &MatrixScene::slotCommandExecuted);
    connect(RosegardenDocument::currentDocument,
            &RosegardenDocument::notesTied,
            this,
            &MatrixScene::slotNotesTiedUntied);
    connect(RosegardenDocument::currentDocument,
            &RosegardenDocument::notesUntied,
            this,
            &MatrixScene::slotNotesTiedUntied);
    connect(RosegardenMainWindow::self(),
            &RosegardenMainWindow::midiOctaveOffsetChanged,
            this,
            [this](){updateNotes(UpdateNotes::FORCE,      // update labels
                                 UpdateNotes::FORCE);});  // update colors

#if 0  // Failed experiments to use Alt or Tab for alternate tool switching
    qt_set_sequence_auto_mnemonic(false);
#endif
#if 0  // Failed experiments to use Alt or Tab for alternate tool switching
    installEventFilter(this);
#endif
}

MatrixScene::~MatrixScene()
{
    RG_DEBUG << "MatrixScene::~MatrixScene() - start";

    if (m_document) {
        if (!isCompositionDeleted()) { // implemented in CompositionObserver
            m_document->getComposition().removeObserver(this);
        }
    }
    for (unsigned int i = 0; i < m_viewSegments.size(); ++i) {
        delete m_viewSegments[i];
    }

    delete m_snapGrid;
    delete m_referenceScale;
    delete m_scale;
    delete m_selection;

    RG_DEBUG << "MatrixScene::~MatrixScene() - end";
}

namespace
{
    // Functor for std::sort that compares the track positions of two Segments.
    struct TrackPositionLess {
        TrackPositionLess() :
            m_composition(RosegardenDocument::currentDocument->
                              getComposition())
        {
        }

        bool operator()(const Segment *lhs, const Segment *rhs)
        {
            // ??? Could also sort by Segment name and Segment start time.
            const int lPos =
                    m_composition.getTrackById(lhs->getTrack())->getPosition();
            const int rPos =
                    m_composition.getTrackById(rhs->getTrack())->getPosition();
            return (lPos < rPos);
        }

    private:
        const Composition &m_composition;
    };
}

void
MatrixScene::setSegments(RosegardenDocument *document,
                         std::vector<Segment *> segments)
{
    if (m_document && document != m_document) {
        m_document->getComposition().removeObserver(this);
        m_observerAdded = false;
    }

    for (Segment *segment : segments) {
        // See m_isPercussion in Segment.h
        // Need this crap because can't do true initialization.
        // Calls non-const version which does the initialization and
        //   allows all subsequent uses to use const version.
        segment->isPercussion();

#if 0   // t4os: No longer needed.
        //
        // Ensure that all segments have a key change at their starting time.
        // Simplifies and makes safer various ChordNameRuler and
        // ConflictingKeyChanges associated processes.

        bool needDefaultKeyAtSegmentStart = true;
        for (auto event : *segment) {
            if (event->getAbsoluteTime() > segment->getStartTime())
                break;
            else if (event->isa(Key::EventType)) {
                needDefaultKeyAtSegmentStart = false;
                break;
            }
        }

        if (needDefaultKeyAtSegmentStart) {
            Key cMajor; // default is C major
            segment->insert(cMajor.getAsEvent(segment->getStartTime()));
        }
#endif
        connect(segment, &Segment    ::noteAddedOrRemoved,
                this,    &MatrixScene::slotNoteAddedOrRemoved);
        connect(segment, &Segment    ::noteModified,
                this,    &MatrixScene::slotNoteModified);
        connect(segment, &Segment    ::keyAddedOrRemoved,
                this,    &MatrixScene::slotKeyAddedOrRemoved);
    }

    m_document = document;
    m_segments = segments;

    // Sort the Segments into TrackPosition order.  This makes the
    // Segment changer wheel in the Matrix editor
    // (MatrixWidget::m_segmentChanger) easier to understand.
    std::sort(m_segments.begin(), m_segments.end(), TrackPositionLess());

    // Check before adding self as an observer because there is no guarantee
    // that this method will not be invoked multiple times, and
    // Composition::addObserver() doesn't check for duplicates which
    // in turn would be invoked multiple times -- bad not only for efficiency
    // but could cause worse problems (multiple instantiations of objects
    // intended to be done only once, etc).
    if (!m_observerAdded) {
        m_document->getComposition().addObserver(this);
        m_observerAdded = true;
    }

    SegmentSelection selection;
    selection.insert(segments.begin(), segments.end());

    delete m_snapGrid;
    delete m_scale;
    delete m_referenceScale;
    m_scale = new SegmentsRulerScale(&m_document->getComposition(),
                                     selection,
                                     0,
                                     Note(Note::Shortest).getDuration() / 2.0);

    m_referenceScale = new ZoomableRulerScale(m_scale);
    m_snapGrid = new SnapGrid(m_referenceScale);

    for (unsigned int i = 0; i < m_viewSegments.size(); ++i) {
        delete m_viewSegments[i];
    }
    m_viewSegments.clear();

    // We should show diamonds instead of bars whenever we are in
    // "drum mode" (i.e. whenever we were invoked using the percussion
    // matrix menu option instead of the normal matrix one).

    // The question of whether to show the key names instead of the
    // piano keyboard is a separate one, handled in MatrixWidget, and
    // it depends solely on whether a key mapping exists for the
    // instrument (it is independent of whether this is a percussion
    // matrix or not).

    // Nevertheless, if the key names are shown, we need a little more space
    // between horizontal lines. That's why m_resolution depends from
    // keyMapping.

    // But since several segments may be edited in the same matrix, we
    // have to deal with simultaneous display of segments using piano keyboard
    // and segments using key mapping.
    // Key mapping may be displayed with piano keyboard resolution (even if
    // space is a bit short for the text) but the opposite is not possible.
    // So the only (easy) way I found is to use the resolution fitting with
    // piano keyboard when at least one segment needs it.

    m_resolution = (m_widget && m_widget->hasPercussionSegments()) ? 11 : 8;

    bool haveSetSnap = false;
    Composition &composition = m_document->getComposition();

    for (unsigned int i = 0; i < m_segments.size(); ++i) {

        int snapGridSize = m_segments[i]->getSnapGridSize();

        if (snapGridSize != -1) {
            m_snapGrid->setSnapTime(snapGridSize);
            haveSetSnap = true;
        }

        // Feature Request #501. Percussion vs normal mode is
        // now dynamic per segment depending on the segment's
        // track's instrument.
        TrackId trackId = m_segments[i]->getTrack();
        Track *track = composition.getTrackById(trackId);
        Instrument *instr = m_document->getStudio().getInstrumentById(
                                                  track->getInstrument());
        bool drumMode = instr->getKeyMapping();

        MatrixViewSegment *vs = new MatrixViewSegment(this,
                                                      m_segments[i],
                                                      drumMode,
                                                      i);

        (void)vs->getViewElementList(); // make sure it has been created
        m_viewSegments.push_back(vs);
    }

    if (!haveSetSnap) {
        QSettings settings;
        settings.beginGroup(MatrixViewConfigGroup);
        timeT snap = settings.value("Snap Grid Size",
                                    (int)SnapGrid::SnapToBeat).toInt();
        m_snapGrid->setSnapTime(snap);
        settings.endGroup();
        for (unsigned int i = 0; i < m_segments.size(); ++i) {
            m_segments[i]->setSnapGridSize(snap);
        }
    }

    recreateLines();
}  // setSegments();

Segment *
MatrixScene::getCurrentSegment()
{
    if (m_segments.empty()) return nullptr;
    if (m_currentSegmentIndex >= m_segments.size()) {
        m_currentSegmentIndex = int(m_segments.size()) - 1;
    }
    return m_segments[m_currentSegmentIndex];
}

void
MatrixScene::setCurrentSegment(const Segment* const segment)
{
    // Abort if same as current segment.
    const Segment* const previousSegment = getCurrentSegment();
#ifndef CHORD_NAME_RULER_AND_CONFLICTING_KEY_CHANGES_DEBUG
    if (previousSegment == segment) return;
#endif

    int segmentIndex = findSegmentIndex(segment);
#ifndef CHORD_NAME_RULER_AND_CONFLICTING_KEY_CHANGES_DEBUG
    if (segmentIndex == -1) return;
#endif

#ifdef CHORD_NAME_RULER_AND_CONFLICTING_KEY_CHANGES_DEBUG
    qDebug() << "MatrixScene::setCurrentSegment()  segs/conflicts:"
             << m_widget->conflictingKeyChanges()->numSegments()
             << m_widget->conflictingKeyChanges()->numConflicts()
             << "\n  was:"
             << static_cast<const void*>(previousSegment)
             << (previousSegment ? previousSegment->getLabel() : "<nullptr>")
             << "\n  new:"
             << static_cast<const void*>(segment)
             << (segment ? segment->getLabel() : "<nullptr>");

    if (previousSegment == segment || segmentIndex == -1) return;
#endif

    // Unset old current segment -- unsets "current" on segment's
    //   MatrixElement notes to update (change note color, selected/unselected).
    // Must do this before m_currentSegmentIndex = segmentIndex
    updateCurrentSegment(false);     // false == is not current segment

    // This is how the current segment is stored.
    // Replace with a new Segment *m_currentSegment member and when
    //   and index is needed (e.g. for accessing correct
    //   m_viewSegments[ndx]) use findSegmentIndex().
    // Accessing current segment far more frequent than above, so
    //   makes sense to have inline getCurrentSegment() simply
    //   returning m_currentSegment instead of accessing
    //   m_segments[m_currentSegmentIndex] along with check
    //   for validity.
    // Validity check can be done at entrance here with std::find()
    //   in m_segments[]. Should never fail, but even doing check
    //   is less frequent here than in getCurrentSegment().
    m_currentSegmentIndex = segmentIndex;

    updateCurrentSegment(true);  // true == is current segment

    if (!m_widget) return;  // Safety check, can't happen.
    m_widget->clearSelection();

    if (segment->isPercussion() != previousSegment->isPercussion())
        m_widget->generatePitchRuler();

    m_widget->updateToCurrentSegment(segment);

    ChordNameRuler *chordNameRuler = m_widget->chordNameRuler();
    const ConflictingKeyChanges   *conflictingKeyChanges
                                = m_widget->conflictingKeyChanges();

    if (chordNameRuler)  // safety check, should never fail
        chordNameRuler->setCurrentSegment(segment);

    if (   conflictingKeyChanges  // safety check, should never fail
        && conflictingKeyChanges->areConflicting(segment, previousSegment)) {
        // Knows whether analysis is required by current mode.
        chordNameRuler->analyzeChords();
        // UpdateNotes::CHECK for note labels uses
        // MatrixWidget::needUpdateNoteLabels() which
        // returns true if not MatrixWidget::ChordSpellingType::OFF
        // (if showing note names) which isn't applicable in this case,
        // so do own decision here.
        updateNotes(    m_widget->getShowNoteNames()
                     && m_widget->notesAreKeyDependent() ? UpdateNotes::FORCE
                                                         : UpdateNotes::NEVER,
                    UpdateNotes::CHECK);
    }
}  // setCurrentSegment()

Segment *
MatrixScene::getPriorSegment()
{
    if (m_currentSegmentIndex == 0) return nullptr;
    return m_segments[m_currentSegmentIndex-1];
}

Segment *
MatrixScene::getNextSegment()
{
    if ((unsigned int) m_currentSegmentIndex + 1 >= m_segments.size()) return nullptr;
    return m_segments[m_currentSegmentIndex+1];
}

MatrixViewSegment *
MatrixScene::getCurrentViewSegment()
{
    if (m_viewSegments.empty())
        return nullptr;

    // ??? Why doesn't this use m_currentSegmentIndex?
    // return m_viewSegments[0];

    // It should. Otherwise these callers work incorrectly
    //      MatrixView::slotExtendSelectionBackward(bool)
    //      MatrixView::slotExtendSelectionForward(bool)
    //      MatrixWidget::slotKeyPressed(unsigned, bool)
    //      MatrixWidget::slotKeySelected(unsigned, bool)
    //      MatrixWidget::slotKeyReleased(unsigned, bool)
    return m_viewSegments[m_currentSegmentIndex];
}

bool
MatrixScene::segmentsContainNotes() const
{
    for (unsigned int i = 0; i < m_segments.size(); ++i) {

        const Segment *segment = m_segments[i];

        for (Segment::const_iterator i = segment->begin();
             segment->isBeforeEndMarker(i); ++i) {

            if (((*i)->getType() == Note::EventType)) {
                return true;
            }
        }
    }

    return false;
}

void
MatrixScene::recreateLines()
{
    // Why is this here instead of in MatrixWidget?

    timeT start = 0, end = 0;

    // Determine total distance that requires lines (considering multiple segments
    for (unsigned int i = 0; i < m_segments.size(); ++i) {
        if (i == 0 || m_segments[i]->getClippedStartTime() < start) {
            start = m_segments[i]->getClippedStartTime();
        }
        if (i == 0 || m_segments[i]->getEndMarkerTime() > end) {
            end = m_segments[i]->getEndMarkerTime();
        }
    }

    // Pen Width?
    double pw = 0;

    double startPos = m_scale->getXForTime(start);
    double endPos = m_scale->getXForTime(end);

    // Draw horizontal lines
    int i = 0;
    while (i < 127) {
         int y = (i + 1) * (m_resolution + 1);
         QGraphicsLineItem *line;
         if (i < (int)m_horizontals.size()) {
             line = m_horizontals[i];
         } else {
             line = new QGraphicsLineItem;
             line->setZValue(MatrixElement::HORIZONTAL_LINE_Z);
             line->setPen(QPen(GUIPalette::getColour
                               (GUIPalette::MatrixHorizontalLine), pw));
             addItem(line);
             m_horizontals.push_back(line);
         }
         line->setLine(startPos, y, endPos, y);
         line->show();
         ++i;
     }


     // Hide the other lines, if there are any.  Just a double check.
     while (i < (int)m_horizontals.size()) {
         m_horizontals[i]->hide();
         ++i;
    }

    setSceneRect(QRectF(startPos, 0, endPos - startPos, 128 * (m_resolution + 1)));

    Composition *c = &m_document->getComposition();

    int firstbar = c->getBarNumber(start), lastbar = c->getBarNumber(end);

#if 1   // t4os
    // Differentiate between differnt vertical line semantics.
    // Really need Qt to have non-zero width "isCosmetic" pens (specific
    //   screen space pixel widths other than 1) but isn't supported.
    // Can't manually fake it because this isn't called when matrix grid
    //   is resized. Only way around would be something like
    //   IsotropicXyzItem classes (see respective .h files).
    QPen barPen (Qt::black,             0.0, Qt::SolidLine),
#if 0   // t4os
         beatPen(QColor(200, 200, 200), 0.0, Qt::SolidLine),
         gridPen(QColor(200, 200, 200), 0.0, Qt:: DashLine);
#else
         beatPen(QColor(160, 160, 160), 0.0, Qt::SolidLine),
         gridPen(QColor(160, 160, 160), 0.0, Qt:: DashLine);
#endif
#endif
    // Draw Vertical Lines
    i = 0;
    for (int bar = firstbar; bar <= lastbar; ++bar) {

        std::pair<timeT, timeT> range = c->getBarRange(bar);

        bool discard = false;  // was not initalied...hmmm...try false?
        TimeSignature timeSig = c->getTimeSignatureInBar(bar, discard);

        double x0 = m_scale->getXForTime(range.first);
        double x1 = m_scale->getXForTime(range.second);
        double width = x1 - x0;

        double gridLines; // number of grid lines per bar may be fractional

        // If the snap time is zero we default to beat markers
        //
        if (m_snapGrid && m_snapGrid->getSnapTime(x0)) {
            gridLines = double(timeSig.getBarDuration()) /
                        double(m_snapGrid->getSnapTime(x0));
        } else {
            gridLines = timeSig.getBeatsPerBar();
        }

        double dx = width / gridLines;
        double x = x0;

#if 1   // t4os
        unsigned beatModulo = (unsigned)gridLines / timeSig.getNumerator();
        if (beatModulo < 1) beatModulo = 1;
#endif

        for (int index = 0; index < gridLines; ++index) {

            // Check to see if we are withing the first segments start time.
            if (x < startPos) {
                x += dx;
                continue;
            }

            // Exit if we have passed the end of last segment end time.
            if (x > endPos) {
                break;
            }

            QGraphicsLineItem *line;

            if (i < (int)m_verticals.size()) {
                line = m_verticals[i];
            } else {
                line = new QGraphicsLineItem;
                addItem(line);
                m_verticals.push_back(line);
            }

#if 0   // t4os
            if (index == 0) {
              // index 0 is the bar line
                line->setPen(QPen(GUIPalette::getColour(GUIPalette::MatrixBarLine), pw));
            } else {
                line->setPen(QPen(GUIPalette::getColour(GUIPalette::BeatLine), pw));
            }
#else
            // Visual distinction between bars/beats/grid lines.
            if      (index == 0)              line->setPen( barPen);
            else if (index % beatModulo == 0) line->setPen(beatPen);
            else                              line->setPen(gridPen);
#endif

            line->setZValue(index > 0 ? MatrixElement::VERTICAL_BEAT_LINE_Z
                                      : MatrixElement::VERTICAL_BAR_LINE_Z);
            line->setLine(x, 0, x, 128 * (m_resolution + 1));

            line->show();
            x += dx;
            ++i;
        }
    }

    // Hide the other lines. We are not using them right now.
    while (i < (int)m_verticals.size()) {
        m_verticals[i]->hide();
        ++i;
    }

    recreatePitchHighlights();

    // Force update so all vertical lines are drawn correctly.
    // ??? Works fine without.  Seems like update() isn't needed in this
    //     class.  In fact it might be harmful.
    //update();
}

void
MatrixScene::recreatePercussionHighlights()
{
    Segment *segment = getCurrentSegment();
    if (!segment) return;

    m_widget->getPanned()->setBackgroundBrush(Qt::white);

    timeT k0 = segment->getClippedStartTime();
    timeT k1 = segment->getEndMarkerTime();

    int i = 0;

    double x0 = m_scale->getXForTime(k0);
    double x1 = m_scale->getXForTime(k1);

    for (int pitch = 0 ; pitch < 128 ; pitch += 2) {
        QGraphicsRectItem *rect;

        if (i < (int)m_highlights.size()) {
            rect = m_highlights[i];
        } else {
            rect = new QGraphicsRectItem;
            rect->setZValue(MatrixElement::HIGHLIGHT_Z);
            rect->setPen(Qt::NoPen);
            addItem(rect);
            m_highlights.push_back(rect);
        }
        ++i;

        rect->setBrush(GUIPalette::getColour
                       (GUIPalette::MatrixPitchHighlight));

        rect->setRect(0, 0, x1 - x0, m_resolution + 1);
        rect->setPos(x0, (127 - pitch) * (m_resolution + 1));
        rect->show();
    }

    while (i < (int)m_highlights.size()) {
        m_highlights[i]->hide();
        ++i;
    }
}

void
MatrixScene::recreateKeyHighlights()
{
    Segment *segment = getCurrentSegment();
    if (!segment) return;

    m_widget->getPanned()->setBackgroundBrush(QColor(128, 128, 128));

    timeT k0, k1, endTime;
    k0      = segment->getClippedStartTime();
    endTime = segment->getEndMarkerTime(true);

    auto &keysMap(segment->keySignatureMap());   // must be auto&
    auto  keyIter(segment->getKeySignatureIterAtTime(k0));

    int i = 0;
    while (k0 < endTime) {
        // offset the highlights according to how far this key's tonic pitch is
        // from C major (0)
        int offset = (  keyIter == keysMap.end()
                      ? 0
                      : keyIter->second.getTonicPitch());

        // correct for segment transposition, moving representation the opposite
        // of pitch to cancel it out (C (MIDI pitch 0) in Bb (-2) is concert
        // Bb (10), so 0 -1 == 11 -1 == 10, we have to go +1 == 11 +1 == 0)
        int correction = segment->getTranspose(); // eg. -2
        correction *= -1;                         // eg.  2

        // key is Bb for Bb instrument, getTonicPitch() returned 10, correction
        // is +2
        offset -= correction;
        offset += 12;
        offset %= 12;

        // calculate the highlights relative to C major, plus offset
        // (I think this enough to do the job.  It passes casual tests.)
        static const int HCOUNT = 7;
        static const int majorSteps[HCOUNT] = {0, 2, 4, 5, 7, 9, 11};
        static const int minorSteps[HCOUNT] = {0, 2, 3, 5, 7, 8, 10};
        static const QColor colors [HCOUNT] = {QColor(255, 255, 255),
                                               QColor(224, 224, 224),
                                               QColor(224, 224, 224),
                                               QColor(224, 224, 224),
                                               QColor(224, 224, 224),
                                               QColor(224, 224, 224),
                                               QColor(224, 224, 224)};

        const int *hsteps;

        if (keyIter == keysMap.end()) {
            hsteps = majorSteps;
            k1     = endTime;
        }
        else {
            hsteps = (keyIter->second.isMinor() ? minorSteps : majorSteps);

            if (++keyIter == keysMap.end()) k1 = endTime;
            else                            k1 = keyIter->first;
        }

        double x0 = m_scale->getXForTime(k0);
        double x1 = m_scale->getXForTime(k1);

        for (int j = 0; j < HCOUNT; ++j) {
            QColor color(colors[j]);

            // Need to fill in between 0 and offset (i.e. key)
            int pitch = -12 + hsteps[j] + offset;
            if (pitch < 0) pitch += 12;

            while (pitch < 128) {
                QGraphicsRectItem *rect;

                if (i < (int)m_highlights.size())
                    rect = m_highlights[i];
                else {
                    rect = new QGraphicsRectItem;
                    rect->setZValue(MatrixElement::HIGHLIGHT_Z);
                    rect->setPen(Qt::NoPen);
                    addItem(rect);
                    m_highlights.push_back(rect);
                }

                rect->setBrush(color);

             // rect->setRect(0.5, 0.5, x1 - x0, m_resolution + 1);
                rect->setRect(0, 0, x1 - x0, m_resolution + 1);
                rect->setPos(x0, (127 - pitch) * (m_resolution + 1));
                rect->show();

                pitch += 12;

                ++i;
            }
        }
        k0 = k1;
    }

    while (i < (int)m_highlights.size()) {
        m_highlights[i]->hide();
        ++i;
    }
}  // recreateKeyHighlights()

void
MatrixScene::recreateTriadHighlights()
{
    Segment *segment = getCurrentSegment();
    if (!segment) return;

    m_widget->getPanned()->setBackgroundBrush(Qt::white);

    timeT k0 = segment->getClippedStartTime();
    timeT k1 = segment->getClippedStartTime();

    int i = 0;

    while (k0 < segment->getEndMarkerTime()) {

        Rosegarden::Key key = segment->getKeyAtTime(k0);

        // offset the highlights according to how far this key's tonic pitch is
        // from C major (0)
        int offset = key.getTonicPitch();

        // correct for segment transposition, moving representation the opposite
        // of pitch to cancel it out (C (MIDI pitch 0) in Bb (-2) is concert
        // Bb (10), so 0 -1 == 11 -1 == 10, we have to go +1 == 11 +1 == 0)
        int correction = segment->getTranspose(); // eg. -2
        correction *= -1;                         // eg.  2

        // key is Bb for Bb instrument, getTonicPitch() returned 10, correction
        // is +2
        offset -= correction;
        offset += 12;
        offset %= 12;

        if (!segment->getNextKeyTime(k0, k1)) k1 = segment->getEndMarkerTime();

        if (k0 == k1) {
            RG_WARNING << "WARNING: MatrixScene::recreatePitchHighlights: k0 == k1 ==" << k0;
            break;
        }

        double x0 = m_scale->getXForTime(k0);
        double x1 = m_scale->getXForTime(k1);

        // calculate the highlights relative to C major, plus offset
        // (I think this enough to do the job.  It passes casual tests.)
        const int hcount = 3;
        int hsteps[hcount];
        hsteps[0] = scale_Cmajor[0] + offset; // tonic
        hsteps[2] = scale_Cmajor[4] + offset; // fifth

        if (key.isMinor()) {
            hsteps[1] = scale_Cminor[2] + offset; // minor third
        } else {
            hsteps[1] = scale_Cmajor[2] + offset; // major third
        }

        for (int j = 0; j < hcount; ++j) {

            int pitch = hsteps[j];
            while (pitch < 128) {

                QGraphicsRectItem *rect;

                if (i < (int)m_highlights.size()) {
                    rect = m_highlights[i];
                } else {
                    rect = new QGraphicsRectItem;
                    rect->setZValue(MatrixElement::HIGHLIGHT_Z);
                    rect->setPen(Qt::NoPen);
                    addItem(rect);
                    m_highlights.push_back(rect);
                }

                if (j == 0) {
                    rect->setBrush(GUIPalette::getColour
                                   (GUIPalette::MatrixTonicHighlight));
                } else {
                    rect->setBrush(GUIPalette::getColour
                                   (GUIPalette::MatrixPitchHighlight));
                }

//                rect->setRect(0.5, 0.5, x1 - x0, m_resolution + 1);
                rect->setRect(0, 0, x1 - x0, m_resolution + 1);
                rect->setPos(x0, (127 - pitch) * (m_resolution + 1));
                rect->show();

                pitch += 12;

                ++i;
            }
        }

        k0 = k1;
    }
    while (i < (int)m_highlights.size()) {
        m_highlights[i]->hide();
        ++i;
    }
}  // recreateTriadHighlights()

void
MatrixScene::recreateBlackkeyHighlights()
{
    Segment *segment = getCurrentSegment();
    if (!segment) return;

    m_widget->getPanned()->setBackgroundBrush(Qt::white);

    timeT k0 = segment->getClippedStartTime();
    timeT k1 = segment->getEndMarkerTime();

    int i = 0;

    double x0 = m_scale->getXForTime(k0);
    double x1 = m_scale->getXForTime(k1);

    int bkcount = 5;
    int bksteps[bkcount];
    bksteps[0] = 1;
    bksteps[1] = 3;
    bksteps[2] = 6;
    bksteps[3] = 8;
    bksteps[4] = 10;
    for (int j = 0; j < bkcount; ++j) {

        int pitch = bksteps[j];
        while (pitch < 128) {

            QGraphicsRectItem *rect;

            if (i < (int)m_highlights.size()) {
                rect = m_highlights[i];
            } else {
                rect = new QGraphicsRectItem;
                rect->setZValue(MatrixElement::HIGHLIGHT_Z);
                rect->setPen(Qt::NoPen);
                addItem(rect);
                m_highlights.push_back(rect);
            }

            rect->setBrush(GUIPalette::getColour
                           (GUIPalette::MatrixPitchHighlight));

            rect->setRect(0, 0, x1 - x0, m_resolution + 1);
            rect->setPos(x0, (127 - pitch) * (m_resolution + 1));
            rect->show();

            pitch += 12;

            ++i;
        }
    }

    while (i < (int)m_highlights.size()) {
        m_highlights[i]->hide();
        ++i;
    }
}  // recreateBlackkeyHighlights()

void
MatrixScene::recreatePitchHighlights()
{
    Segment *segment = getCurrentSegment();
    if (!segment) return;

    QSettings settings;
    settings.beginGroup(MatrixViewConfigGroup);
    HighlightType chosenHighlightType = static_cast<HighlightType>(
        settings.value("highlight_type", HT_BlackKeys).toInt());
    settings.endGroup();

#if 0   // t4os -- no longer needed??
    // Need to do this regardless if new highlight type is same
    // or different than old, because will be called when changing
    // active segment, and new vs previous active segments might
    // span different ranges of measures.
    for (unsigned i = 0 ; i < m_highlights.size() ; ++i) {
        // Need to repaint highlight area to background because
        // Qt can leave trash behind otherwise.
        m_highlights[i]->setBrush(GUIPalette::getColour
                                  (GUIPalette::SegmentCanvas));
    }
#endif

    // Force update to repaint before possibly hiding some highlights
    // that won't be reset to new type/time/pitch
    update();

    if (m_highlightType != chosenHighlightType) {
        // Hide all previous highlights.
        for (unsigned i = 0 ; i < m_highlights.size() ; ++i) {
            m_highlights[i]->hide();
        }
        m_highlightType = chosenHighlightType;
    }

    // t4os: pass segment to recreateXyzHighlights() and
    // remove checks there

    TrackId trackId = segment->getTrack();
    Composition &composition = m_document->getComposition();
    Track *track = composition.getTrackById(trackId);
    Instrument *instr = m_document->
                        getStudio()
                        .getInstrumentById(track->getInstrument());

    if (instr->getKeyMapping())
        recreatePercussionHighlights();
    else
        switch (chosenHighlightType) {
            case HT_BlackKeys: recreateBlackkeyHighlights(); break;
            case HT_Triads:    recreateTriadHighlights();    break;
            case HT_Key:       recreateKeyHighlights();      break;
        }

    update();
}  // recreatePitchHighlights()

void MatrixScene::setMouseEventElement(MatrixMouseEvent &mme)
const
{
    setMouseEventElement(QPointF(mme.sceneX, mme.sceneY), mme);
}

bool
MatrixScene::setupMouseEvent(MatrixMouseEvent &mme) const
{
    bool foundPos = false;
    QPoint screenPos = QCursor::pos();
    QPointF scenePos;
    for (QGraphicsView *view: views()) {
        QWidget *viewport = view->viewport();
        QRect viewportRect = viewport->rect();
        QPoint viewportPos = viewport->mapFromGlobal(screenPos);
        if (viewportRect.contains(viewportPos)) {
            scenePos = view->mapToScene(viewportPos);
            foundPos = true;
            break;
        }
    }

    if (!foundPos) return false;

    setupMouseEvent(screenPos,
                    scenePos,
                    QGuiApplication::mouseButtons(),
                    Qt::KeyboardModifiers(),
                    mme);
    return true;
}

void
MatrixScene::setupMouseEvent(const QGraphicsSceneMouseEvent *e,
                             MatrixMouseEvent &mme) const
{
    setupMouseEvent(e->screenPos(),
                    e->scenePos(),
                    e->buttons(),
                    e->modifiers(),
                    mme);
}

void MatrixScene::setMouseEventElement(const QPointF &scenePos,
                                       MatrixMouseEvent &mme)
const
{
    mme.element = nullptr;
    QList<QGraphicsItem *> l = items(scenePos);
    for (int i = 0; i < l.size(); ++i) {
        MatrixElement *element = MatrixElement::getMatrixElement(l[i]);
        if (element) {
            // items are in z-order from top, so this is most salient
            mme.element = element;
            break;
        }
    }
}

void
MatrixScene::setupMouseEvent(const QPoint &screenPos,
                             const QPointF &scenePos,
                             const Qt::MouseButtons mouseButtons,
                             const Qt::KeyboardModifiers keyboardModifiers,
                             MatrixMouseEvent &mme) const
{
    double sx = scenePos.x();
    int sy = lrint(scenePos.y());

    mme.viewpos = screenPos;

    mme.sceneX = sx;
    mme.sceneY = sy;

    mme.modifiers = keyboardModifiers;
    mme.buttons = mouseButtons;

    setMouseEventElement(scenePos, mme);

    mme.viewSegment = m_viewSegments[m_currentSegmentIndex];

    mme.time = m_scale->getTimeForX(sx);

    if (keyboardModifiers & Qt::ShiftModifier) {
        mme.snappedLeftTime = mme.time;
        mme.snappedRightTime = mme.time;
        mme.snapUnit = Note(Note::Shortest).getDuration();
    } else {
//        mme.snappedLeftTime = m_snapGrid->snapX(sx, SnapGrid::SnapLeft);
//        mme.snappedRightTime = m_snapGrid->snapX(sx, SnapGrid::SnapRight);
        mme.snappedLeftTime = m_snapGrid->snapTime(mme.time, SnapGrid::SnapLeft);
        mme.snappedRightTime = m_snapGrid->snapTime(mme.time, SnapGrid::SnapRight);
        mme.snapUnit = m_snapGrid->getSnapTime(sx);
    }

    if (mme.viewSegment) {
        timeT start = mme.viewSegment->getSegment().getClippedStartTime();
        timeT end = mme.viewSegment->getSegment().getEndMarkerTime();
        if (mme.snappedLeftTime < start) mme.snappedLeftTime = start;
        if (mme.snappedLeftTime + mme.snapUnit > end) {
            mme.snappedLeftTime = end - mme.snapUnit;
        }
        if (mme.snappedRightTime < start) mme.snappedRightTime = start;
        if (mme.snappedRightTime > end) mme.snappedRightTime = end;
    }

   mme.pitch = calculatePitchFromY(sy);

#ifdef DEBUG_MOUSE
    MATRIX_DEBUG << "MatrixScene::setupMouseEvent: sx = " << sx
                 << ", sy = " << sy
                 << ", modifiers = " << mme.modifiers
                 << ", buttons = " << mme.buttons
                 << ", element = " << mme.element
                 << ", viewSegment = " << mme.viewSegment
                 << ", time = " << mme.time
                 << ", pitch = " << mme.pitch
                 << endl;
#endif
}

int MatrixScene::calculatePitchFromY(int y) const {
    int pitch = 127 - (y / (m_resolution + 1));
    if (pitch < 0) pitch = 0;
    if (pitch > 127) pitch = 127;
    return pitch;
}

#if 0  // Failed experiments to use Alt or Tab for alternate tool switching
// Override/intercept to pass through Alt and/or Tab normally filtered
// out by QGraphicsScene base class.
bool
MatrixScene::event(QEvent *e)
{
    qDebug() << "\nMatrixScene::Event()"
             << e
             << e->type()
             << '\n';

    if (e->type() == QEvent::KeyPress)
        keyPressEvent(dynamic_cast<QKeyEvent*>(e));
    else if (e->type() == QEvent::KeyRelease)
        keyReleaseEvent(dynamic_cast<QKeyEvent*>(e));
    else
        return QGraphicsScene::event(e);

    return false;
}
#endif

#if 0  // Failed experiments to use Alt or Tab for alternate tool switching
// Alternative to event(), above. Also problematic.
bool MatrixScene::eventFilter(QObject *obj, QEvent *event)
{
#if 0
    if (obj == textEdit) {
        if (event->type() == QEvent::KeyPress) {
            QKeyEvent *keyEvent = static_cast<QKeyEvent*>(event);
            qDebug() << "Ate key press" << keyEvent->key();
            return true;
        } else {
            return false;
        }
    } else {
        // pass the event on to the parent class
        return QMainWindow::eventFilter(obj, event);
    }
#else
    qDebug() << "\nMatrixScene::eventFilter()"
             << obj
             << event
             << '\n';
    return QGraphicsScene::eventFilter(obj, event);
#endif
}
#endif

#if 1   // Now done here instead of in MatrixWidget::enterEvent() so
        // always happens when mouse enters anywhere in matrix editor
        // window (including window manager border), not just widget area.
void MatrixScene::focusInEvent(QFocusEvent*)
{
    m_widget->getToolBox()->checkKeysStateOnEntry();
    m_widget->getToolBox()->getActiveTool()->ready();

    // Set to play MIDI with current segment's instrument

    Composition &composition = m_document->getComposition();
    const Segment *segment = getCurrentSegment();
    if (!segment) return;

    const TrackId trackId = segment->getTrack();
    const Track *track = composition.getTrackById(trackId);
    if (!track) return;

    InstrumentId instrumentId = track->getInstrument();
    Instrument  *instrument = m_document->getStudio().
                                    getInstrumentById(instrumentId);
    if (instrument) {
        RosegardenSequencer::getInstance()
            ->setTrackInstrumentOverride(instrumentId,
                                          instrument->getNaturalChannel());
    }
}
#endif

void
MatrixScene::keyPressEvent(QKeyEvent *e)
{
#if 0  // Failed experiments to use Alt or Tab for alternate tool switching
    qDebug() << "MatrixScene::keyPressEvent()"
             << "\n  key:" << e->key()
             << "\n  modifiers:" << e->modifiers()
             << "\n  nativeModifiers:" << e->nativeModifiers()
             << "\n  text:" << e->text()
             << "\n  key alt?" << (e->key() == Qt::Key_Alt)
             << "\n  mod alt?" << bool(e->modifiers() & Qt::AltModifier)
             << "\n  key lock?" << (e->key() == Qt::Key_CapsLock)
             << "\n  mod shtf?" << bool(e->modifiers() & Qt::ShiftModifier)
             << "\n  accepted?" << e->isAccepted()
             << "\n  mouse:" << QGuiApplication::mouseButtons();
#endif

#if 0  // Failed experiments to use Alt or Tab for alternate tool switching
    // Escape works, but is ergonomically inconvenient.
    // Also requires ignoring autorepeat.
    if (e->key() != Qt::Key_Escape || e->isAutoRepeat())
#elif 0
    // Ibid.
    if (e->isAutoRepeat() ||
        (e->key() != Qt::Key_Escape && e->key() != Qt::Key_Control))
#elif 0
    if (e->key() != Qt::Key_Alt && e->key() != Qt::Key_Control)
#else
    if (e->key() != Qt::Key_CapsLock && e->key() != Qt::Key_Control)
#endif
    {
        QGraphicsScene::keyPressEvent(e);
        return;
    }

    MatrixMouseEvent mme;
    if (setupMouseEvent(mme)) {
        // In setupMouseEvent(), both Qt::KeyboardModifiers()
        // and alternately QApplication::keyboardModifiers() are
        // empty, so have to put this in manually.
        if (e->key() == Qt::Key_Control)
            mme.modifiers |= Qt::ControlModifier;

        m_widget->getToolBox()->handleKeyPress(&mme, e->key());
    }
    else
        m_widget->getToolBox()->handleKeyPress(nullptr, e->key());

    // Pass all/others for normal use (shortcuts, accelerators, etc)
    QGraphicsScene::keyPressEvent(e);
}

void
MatrixScene::keyReleaseEvent(QKeyEvent *e)
{
#if 0  // Failed experiments to use Alt or Tab for alternate tool switching
    qDebug() << "MatrixScene::keyReleaseEvent()"
             << "\n  key:" << e->key()
             << "\n  modifiers:" << e->modifiers()
             << "\n  nativeModifiers:" << e->nativeModifiers()
             << "\n  text:" << e->text()
             << "\n  key lock?" << (e->key() == Qt::Key_CapsLock)
             << "\n  mod shft?" << bool(e->modifiers() & Qt::ShiftModifier)
             << "\n  accepted?" << e->isAccepted();
#endif

#if 0  // Failed experiments to use Alt or Tab for alternate tool switching
    // See comments in keyPressEvent(), above.
    if (e->key() != Qt::Key_Escape || e->isAutoRepeat())
#elif 0
    if (e->isAutoRepeat() ||
        (e->key() != Qt::Key_Escape && e->key() != Qt::Key_Control))
#elif 0
    if (e->key() != Qt::Key_Alt && e->key() != Qt::Key_Control)
#else
    if (e->key() != Qt::Key_CapsLock && e->key() != Qt::Key_Control)
#endif
    {
        QGraphicsScene::keyPressEvent(e);
        return;
    }

    MatrixMouseEvent mme;
    if (setupMouseEvent(mme)) {
        // In setupMouseEvent(), both Qt::KeyboardModifiers()
        // nor alternately QApplication::keyboardModifiers() are
        // empty, so have to put this in manually.
        if (e->key() == Qt::Key_Control) mme.modifiers |= Qt::ControlModifier;

        m_widget->getToolBox()->handleKeyRelease(&mme, e->key());
    }
    else
        m_widget->getToolBox()->handleKeyRelease(nullptr, e->key());

    // Pass all/others for normal use (shortcuts, accelerators, etc)
    QGraphicsScene::keyReleaseEvent(e);
}

void
MatrixScene::mousePressEvent(QGraphicsSceneMouseEvent *e)
{
    MatrixMouseEvent nme;
    setupMouseEvent(e, nme);
#if 0   // SIGNAL_SLOT_ABUSE
    emit mousePressed(&nme);
#else
    m_widget->slotDispatchMousePress(&nme);
#endif
}

void
MatrixScene::mouseMoveEvent(QGraphicsSceneMouseEvent *e)
{
    MatrixMouseEvent nme;
    setupMouseEvent(e, nme);
#if 0   // SIGNAL_SLOT_ABUSE
    emit mouseMoved(&nme);
#else
    m_widget->slotDispatchMouseMove(&nme);
#endif
}

void
MatrixScene::mouseReleaseEvent(QGraphicsSceneMouseEvent *e)
{
    MatrixMouseEvent nme;
    setupMouseEvent(e, nme);
#if 0   // SIGNAL_SLOT_ABUSE
    emit mouseReleased(&nme);
#else
    m_widget->slotDispatchMouseRelease(&nme);
#endif
}

void
MatrixScene::mouseDoubleClickEvent(QGraphicsSceneMouseEvent *e)
{
    MatrixMouseEvent nme;
    setupMouseEvent(e, nme);
#if 0   // SIGNAL_SLOT_ABUSE
    emit mouseDoubleClicked(&nme);
#else
    m_widget->slotDispatchMouseDoubleClick(&nme);
#endif
}

void
MatrixScene::slotCommandExecuted()
{
#ifdef CHORD_NAME_RULER_AND_CONFLICTING_KEY_CHANGES_DEBUG
    qDebug() << "\nMatrixScene::slotCommandExecuted()"
             << CommandHistory::getInstance()->lastCommandExecuted()
             << "\n "
             << (  CommandHistory::getInstance()->commandIsExecuting()
                 ? "COMMAND" : "!command")
             << (  CommandHistory::getInstance()->macroCommandIsExecuting()
                 ? "MACRO" : "!macro")
             << (m_notesChanged ? "NOTES" : "!notes")
             << (m_keysChanged ? "KEYS" : "!keys")
             << (m_currentSegmentKeysChanged ? "CURKEYS" : "!curKeys")
             << (m_segmentChanged ? "SEGMENT" : "!segment")
             << (m_timeSignatureChanged ? "TIMESIG" : "!timesig");
#endif

    // Have to check this for when editor being closed.
    if (m_segments.empty())
        return;

    if (   m_notesChanged
        || m_keysChanged
        || m_segmentChanged
        || m_timeSignatureChanged) {

        if (m_timeSignatureChanged)
            m_widget->updateStandardRulers();

        // Doesn't do if not visible
        m_widget->chordNameRuler()->analyzeChords();

        checkUpdate();

        if (   (m_keysChanged && m_highlightType != HT_BlackKeys)
            || m_segmentChanged
            || m_timeSignatureChanged)
            recreateLines();  // also does recreatePitchHighlights()

        if (m_keysChanged || m_notesChanged)
            updateNotes(UpdateNotes::CHECK,   // labels
                        UpdateNotes::CHECK);  // colors

        if (m_currentSegmentKeysChanged)
            m_widget->setExtantKeyLabel();

          m_notesChanged
        = m_keysChanged
        = m_currentSegmentKeysChanged
        = m_segmentChanged
        = m_timeSignatureChanged
        = false;
    }
}  // commandExecuted()

void
MatrixScene::checkUpdate()
{
#ifdef CHORD_NAME_RULER_AND_CONFLICTING_KEY_CHANGES_DEBUG
    qDebug() << "MatrixScene::checkUpdate(): "
             << m_viewSegments.size()
             << "view segments";
#endif

    bool updateSelectionElementStatus = false;

    for (unsigned int i = 0; i < m_viewSegments.size(); ++i) {
        SegmentRefreshStatus &rs = m_viewSegments[i]->getRefreshStatus();

#if 0  // #ifdef CHORD_NAME_RULER_AND_CONFLICTING_KEY_CHANGES_DEBUG
        qDebug() << "MatrixScene::checkUpdate() refresh"
                 << (rs.needsRefresh() ? "needs" : "unchanged")
                 << rs.from()
                 << "->"
                 << rs.to();
#endif

        if (rs.needsRefresh()) {
            // Refresh the required range.
            // Note that updateElements() does not handle deleted
            // ViewElements.  See MatrixViewSegment::eventRemoved().
            m_viewSegments[i]->updateElements(rs.from(), rs.to());

            if (!updateSelectionElementStatus && m_selection) {
                  updateSelectionElementStatus
                = (   m_viewSegments[i]->getSegment()
                   == m_selection->getSegment());
            }
        }

        rs.setNeedsRefresh(false);
    }

    if (updateSelectionElementStatus) {
        setSelectionElementStatus(m_selection, true);
    }
}

// This is invoked from Composition::addObserver() notifications.
void
MatrixScene::segmentRemoved(const Composition *, Segment *removedSegment)
{
    if (!removedSegment)
        return;
    if (m_segments.empty())
        return;

    const int removedSegmentIndex = findSegmentIndex(removedSegment);

    // Not found?  Bail.
    if (removedSegmentIndex == -1)
        return;

    // If we're about to remove the one they are looking at and
    // there is another to switch to...
    if (removedSegmentIndex == static_cast<int>(m_currentSegmentIndex) &&
        m_segments.size() > 1) {

        // Switch to another Segment.

        // Try the next.
        size_t newSegmentIndex = m_currentSegmentIndex + 1;
        // No next, go with the previous.
        if (newSegmentIndex == m_segments.size())
            newSegmentIndex = m_currentSegmentIndex - 1;
        setCurrentSegment(m_segments[newSegmentIndex]);
    }

    // Must do after setCurrentSegment()
    if (m_widget) {  // all safety checks, shouldn't ever fail
        if (m_widget->chordNameRuler())
              m_widget
            ->chordNameRuler()
            ->removeSegment(const_cast<Segment*>(removedSegment));
    }

    emit segmentDeleted(const_cast<Segment*>(removedSegment));

    delete m_viewSegments[removedSegmentIndex];
    m_viewSegments.erase(m_viewSegments.cbegin() + removedSegmentIndex);

    m_segments.erase(m_segments.cbegin() + removedSegmentIndex);

    // Adjust m_currentSegmentIndex
    if (static_cast<int>(m_currentSegmentIndex) > removedSegmentIndex)
        --m_currentSegmentIndex;

    // No more Segments?
    if (m_segments.empty())
        emit sceneDeleted();
}

// A track's instrument might have changed from pitched to percussion
// or vice-versa, so display of the track's segments notes might need
// to be update with respect to pitched-vs-percussion and the
// current "View -> Notes -> Show percussion durations" setting.
// This method is invoked from Composition::addObserver() notifications.
void
MatrixScene::trackChanged(const Composition *composition, Track *track)
{
    Instrument *instr = m_document->getStudio().getInstrumentById(
                                                track->getInstrument());
    bool trackIsNowDrum = instr->getKeyMapping();
    bool trackDrumChanged = false;
    const Segment *currentSegment = getCurrentSegment();
    bool currentSegmentChanged = false;

    for (std::vector<MatrixViewSegment *>::iterator i = m_viewSegments.begin();
         i != m_viewSegments.end(); ++i) {

        TrackId trackId = (*i)->getSegment().getTrack();
        Track *segmentTrack = composition->getTrackById(trackId);

        if (segmentTrack == track) {
            if (!currentSegmentChanged && currentSegment &&
                &(*i)->getSegment() == currentSegment) {
                currentSegmentChanged = true;
            }

            if ((*i)->isDrumMode() != trackIsNowDrum) {
                (*i)->setDrumMode(trackIsNowDrum);
                trackDrumChanged = true;
            }
        }
    }

    if (currentSegmentChanged) {
        if (trackDrumChanged) {
            m_widget->generatePitchRuler();
            recreatePitchHighlights();
        }

        // Don't need to call MatrixWidget::updateToCurrent(true)
        // because that sets current instrument playback (with "true"
        // argument) in addition to setting track/instrument label,
        // but this method is called by main window which is already
        // doing normal (not override) setting of instrument playback.
        m_widget->setSegmentTrackInstrumentLabel(currentSegment);
    }

    if (trackDrumChanged)
        m_widget->chordNameRuler()->resetPercussionSegments();
}  // trackChanged()

// This method is invoked from Composition::addObserver() notifications
// when a segment is moved from one track to another.
void MatrixScene::segmentTrackChanged(
const Composition* /*composition*/,
Segment *segment,
TrackId /*trackId*/)
{
    // Find MatrixViewSegment* for this Segment* and check if
    // percussion has changed.
    for (auto viewSegment : m_viewSegments) {
        if (&viewSegment->getSegment() == segment)
            if (viewSegment->isDrumMode() != segment->isPercussion()) {
                // Changed, so update.
                viewSegment->setDrumMode(segment->isPercussion());
                m_widget->chordNameRuler()->resetPercussionSegments();
                m_widget->generatePitchRuler();
                recreatePitchHighlights();
                break;  // can only "== segment" once
            }
    }

    // Don't need to do anything else because being invoked from main
    // window, and any updates to current instrument playback, etc.
    // occur when matrix gets focus back.

}  // MatrixScene::segmentTrackChanged(()

// This method is invoked from Composition::addObserver() notifications
// when a time signature is added or removed.
void
MatrixScene::timeSignatureChanged(const Composition*)
{
    m_timeSignatureChanged = true;
}

void
MatrixScene::slotNoteAddedOrRemoved(
const Segment   *segment,
      Event     *event,
      bool       added)
{
    m_widget->chordNameRuler()->slotNoteAddedOrRemoved(segment, event, added);
    m_notesChanged = true;
}

void
MatrixScene::slotNoteModified(
const Segment   *segment,
const Event     *event)
{
    m_widget->chordNameRuler()->slotNoteModified(segment, event);
    m_notesChanged = true;
}

void
MatrixScene::slotKeyAddedOrRemoved(
const Segment   *segment,
      Event     *event,
      bool       added)
{
#ifdef CHORD_NAME_RULER_AND_CONFLICTING_KEY_CHANGES_DEBUG
    qDebug() << "MatrixScene::slotKeyAddedOrRemoved()"
             << "\n "
             << (segment ? segment->getLabel() : "<nullptr>")
             << (added ? "added" : "removed");
#endif

    m_widget->chordNameRuler()->slotKeyAddedOrRemoved(segment, event, added);

    // Don't unset once set (for if changes keys in multiple segments)
    m_currentSegmentKeysChanged =    m_currentSegmentKeysChanged
                                  || (segment == getCurrentSegment());
    m_keysChanged = true;

    if (!added) {
        if (m_selection && m_selection->contains(event))
            m_selection->removeEvent(event);
        emit eventRemoved(event);  // Notify MatrixToolBox.
    }
}

// From SegmentObserver notification, forwarded from MatrixViewSegment
// (Or from Composition::addObserver() notifications??)
void
MatrixScene::handleSegmentEndMarkerTimeChanged(
const Segment *segment,
bool           shortened)
{
    m_widget->chordNameRuler()->endMarkerTimeChanged(segment, shortened);
    m_segmentChanged = true;
}

// From SegmentObserver notification, forwarded from MatrixViewSegment
// (Or from Composition::addObserver() notifications??)
void
MatrixScene::handleSegmentStartChanged(
const Segment *segment,
timeT          time)
{
    m_widget->chordNameRuler()->startChanged(segment, time);
    m_segmentChanged = true;
}

void
MatrixScene::setSelection(EventSelection *s, bool preview)
{
    if (!m_selection && !s) return;
    if (m_selection == s) return;
    if (m_selection && s && *m_selection == *s) {
        // selections are identical, no need to update elements, but
        // still need to replace the old selection to avoid a leak
        // (can't just delete s in case caller still refers to it)
        EventSelection *oldSelection = m_selection;
        m_selection = s;
        delete oldSelection;
        return;
    }

    EventSelection *oldSelection = m_selection;
    m_selection = s;

    if (oldSelection) setSelectionElementStatus(oldSelection, false);

    if (m_selection) {
        setSelectionElementStatus(m_selection, true);
        // ??? But we are going to do this at the end of this routine.
        //     Is this needed?  Notation only does this at the end.
        emit QGraphicsScene::selectionChanged();
        emit selectionChangedES(m_selection);
    }

    if (preview) previewSelection(m_selection, oldSelection);
    delete oldSelection;
    emit QGraphicsScene::selectionChanged();
    emit selectionChangedES(m_selection);
}

void
MatrixScene::setSingleSelectedEvent(MatrixViewSegment *viewSegment,
                                    MatrixElement *e,
                                    bool preview)
{
    if (!viewSegment || !e) return;
    EventSelection *s = new EventSelection(viewSegment->getSegment());
    s->addEvent(e->event());
    setSelection(s, preview);
}

void
MatrixScene::setSingleSelectedEvent(Segment *segment,
                                    Event *e,
                                    bool preview)
{
    if (!segment || !e) return;
    EventSelection *s = new EventSelection(*segment);
    s->addEvent(e);
    setSelection(s, preview);
}

void
MatrixScene::selectAll()
{
    Segment *segment = getCurrentSegment();
    if (!segment) return;
    Segment::iterator it = segment->begin();
    EventSelection *selection = new EventSelection(*segment);

    for (; segment->isBeforeEndMarker(it); ++it) {
        if ((*it)->isa(Note::EventType)) {
            selection->addEvent(*it);
        }
    }

    setSelection(selection, false);
}

void
MatrixScene::setSelectionElementStatus(EventSelection *s, bool set)
{
    if (!s) return;

    MatrixViewSegment *vs = nullptr;

    // Find the MatrixViewSegment for the EventSelection's Segment.
    for (MatrixViewSegment *currentViewSegment : m_viewSegments) {

        if (&currentViewSegment->getSegment() == &s->getSegment()) {
            vs = currentViewSegment;
            break;
        }

    }

    if (!vs) return;

    for (const Event *e : s->getSegmentEvents()) {

        ViewElementList::iterator viewElementIter = vs->findEvent(e);
        // Not in the view?  Try the next.
        if (viewElementIter == vs->getViewElementList()->end())
            continue;

        MatrixElement *element =
                dynamic_cast<MatrixElement *>(*viewElementIter);
        if (element)
            element->setSelected(set);

    }
}

void
MatrixScene::previewSelection(EventSelection *s,
                              EventSelection *oldSelection)
{
    if (!s) return;
    if (!m_document->isSoundEnabled()) return;

    for (EventContainer::iterator i = s->getSegmentEvents().begin();
         i != s->getSegmentEvents().end(); ++i) {

        Event *e = *i;
        if (oldSelection && oldSelection->contains(e)) continue;

        long pitch;
        if (e->get<Int>(BaseProperties::PITCH, pitch)) {
            long velocity = -1;
            (void)(e->get<Int>(BaseProperties::VELOCITY, velocity));
            if (!(e->has(BaseProperties::TIED_BACKWARD) &&
                  e->get<Bool>(BaseProperties::TIED_BACKWARD))) {
                playNote(s->getSegment(), pitch, velocity);
            }
        }
    }
}

void
MatrixScene::updateCurrentSegment(bool isCurrent)
{
    if (isCurrent) {
        // Not that highlight type (triads, piano black keys, etc) has
        // changed but rather the  new current segment might span a
        // different range of measures than old one and highlights
        // are only shown with current segment's span.
        recreatePitchHighlights();
    }

    MatrixViewSegment *vs = getCurrentViewSegment();
    if (vs) {
        vs->setNotesCurrentSegment(isCurrent);
        emit currentViewSegmentChanged(m_viewSegments[m_currentSegmentIndex]);
    }

    // changing the current segment may have overridden selection border colours
    setSelectionElementStatus(m_selection, true);
}

void
MatrixScene::setSnap(timeT t)
{
    MATRIX_DEBUG << "MatrixScene::slotSetSnap: time is " << t;
    m_snapGrid->setSnapTime(t);

    for (size_t i = 0; i < m_segments.size(); ++i) {
        m_segments[i]->setSnapGridSize(t);
    }

    QSettings settings;
    settings.beginGroup(MatrixViewConfigGroup);
    settings.setValue("Snap Grid Size", (int)t);
    settings.endGroup();

    recreateLines();
}

#if 0   // unused
bool
MatrixScene::constrainToSegmentArea(QPointF &scenePos)
{
    bool ok = true;

    int pitch = 127 - (lrint(scenePos.y()) / (m_resolution + 1));
    if (pitch < 0) {
        ok = false;
        scenePos.setY(127 * (m_resolution + 1));
    } else if (pitch > 127) {
        ok = false;
        scenePos.setY(0);
    }

    timeT t = m_scale->getTimeForX(scenePos.x());
    timeT start = 0, end = 0;
    for (size_t i = 0; i < m_segments.size(); ++i) {
        timeT t0 = m_segments[i]->getClippedStartTime();
        timeT t1 = m_segments[i]->getEndMarkerTime();
        if (i == 0 || t0 < start) start = t0;
        if (i == 0 || t1 > end) end = t1;
    }
    if (t < start) {
        ok = false;
        scenePos.setX(m_scale->getXForTime(start));
    } else if (t > end) {
        ok = false;
        scenePos.setX(m_scale->getXForTime(end));
    }

    return ok;
}
#endif

bool
MatrixScene::getLowHighPitches(
      int   &lowestPitch,
      int   &highestPitch,
const int    minTime,
const int    maxTime,
const bool   ignorePercussion)
const
{
    bool found = false;  // Return value, false if all segments outside range

    // Outside MIDI limits
    lowestPitch  = 128;
    highestPitch = -1;

    int  segmentLowest, segmentHighest;

    for (const Segment *segment : m_segments) {
        if (ignorePercussion && segment->isPercussion())
            continue;

        if (segment->getLowHighPitches(segmentLowest,
                                       segmentHighest,
                                       minTime,
                                       maxTime)) {
            found = true;
            if (segmentLowest  < lowestPitch)  lowestPitch  = segmentLowest;
            if (segmentHighest > highestPitch) highestPitch = segmentHighest;
        }
    }

    return found;
}

void
MatrixScene::playNote(Segment &segment, int pitch, int velocity)
{
//    std::cout << "Scene is playing a note of pitch: " << pitch
//              << " + " <<  segment.getTranspose();
    if (!m_document) return;

    Instrument *instrument = m_document->getStudio().getInstrumentFor(&segment);

    StudioControl::playPreviewNote(instrument,
                                   pitch + segment.getTranspose(),
                                   velocity,
                                   RealTime(0, 250000000));
}

void
MatrixScene::setHorizontalZoomFactor(double factor)
{
    for (Segment *segment : m_segments) {
        segment->matrixHZoomFactor = factor;
    }
}

void
MatrixScene::setVerticalZoomFactor(double factor)
{
    for (Segment *segment : m_segments) {
        segment->matrixVZoomFactor = factor;
    }
}

void
MatrixScene::updateNotes(
const UpdateNotes checkIfLabelsNeeded,
const UpdateNotes checkIfColorsNeeded)
{
    // true if FORCE, false if NEVER, will get changed correctly if CHECK
    bool needLabelUpdate = (checkIfLabelsNeeded == UpdateNotes::FORCE),
         needColorUpdate = (checkIfColorsNeeded == UpdateNotes::FORCE);

    if (m_widget) {  // safety check, can't be nullptr
        if (checkIfLabelsNeeded == UpdateNotes::CHECK)
            needLabelUpdate = m_widget->needUpdateNoteLabels();
        if (checkIfColorsNeeded == UpdateNotes::CHECK)
            needColorUpdate = m_widget->needUpdateNoteColors();
    }

#ifdef CHORD_NAME_RULER_AND_CONFLICTING_KEY_CHANGES_DEBUG
    qDebug() << "MatrixScene::updateNotes() labels:"
             << (  checkIfLabelsNeeded == UpdateNotes::CHECK ? "CHECK"
                 : checkIfLabelsNeeded == UpdateNotes::FORCE ? "FORCE"
                 : checkIfLabelsNeeded == UpdateNotes::NEVER ? "NEVER"
                 :                                             "?????")
             << " colors:"
             << (  checkIfColorsNeeded == UpdateNotes::CHECK ? "CHECK"
                 : checkIfColorsNeeded == UpdateNotes::FORCE ? "FORCE"
                 : checkIfColorsNeeded == UpdateNotes::NEVER ? "NEVER"
                 :                                             "?????")
             << " need:"
             << (needLabelUpdate     ? "label" : "-")
             << (needColorUpdate     ? "color" : "-");
#endif

    if (needLabelUpdate)
        for (auto &viewSegment : m_viewSegments)
            viewSegment->updateNoteLabels();
    if (needColorUpdate)
        for (auto &viewSegment : m_viewSegments)
            viewSegment->updateNoteColors();
}  // MatrixScene::updateNotes()

void
MatrixScene::updatePercussionNotes()
{
    for (auto &viewSegment : m_viewSegments)
        if (viewSegment->isDrumMode())
            viewSegment->reconfigureNotes();
}

void
MatrixScene::slotNotesTiedUntied(
const EventContainer &notes)
{
    MatrixViewSegment *viewSegment = getCurrentViewSegment();
    if (viewSegment) viewSegment->updateTiedUntied(notes);
}

void
MatrixScene::updateNoteLabels()
{
    for (auto &viewSegment : m_viewSegments)
        viewSegment->updateNoteLabels();
}

void
MatrixScene::updateNoteColors()
{
    for (auto &viewSegment : m_viewSegments)
        viewSegment->updateNoteColors();
}


int
MatrixScene::findSegmentIndex(const Segment *segment) const
{
    auto iter = std::find(m_segments.cbegin(), m_segments.cend(), segment);
    return   iter == m_segments.cend()
           ? -1
           : iter - m_segments.cbegin();
}

}   // namespace Rosegarden
