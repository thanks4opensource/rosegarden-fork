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

#define RG_MODULE_STRING "[MatrixElement]"

#include "MatrixElement.h"
#include "MatrixScene.h"
#include "MatrixWidget.h"

#include "IsotropicDiamondItem.h"
#include "IsotropicRectItem.h"
#include "IsotropicTextItem.h"

#include "base/BaseProperties.h"
#include "base/Event.h"
#include "base/NotationTypes.h"
#include "base/RulerScale.h"
#include "document/RosegardenDocument.h"
#include "gui/general/GUIPalette.h"
#include "gui/general/MidiPitchLabel.h"
#include "gui/rulers/ChordNameRuler.h"
#include "gui/rulers/DefaultVelocityColour.h"
#include "misc/ConfigGroups.h"
#include "misc/Debug.h"
#include "misc/Preferences.h"

#include <QGraphicsRectItem>
#include <QBrush>
#include <QColor>
#include <QPixmap>
#include <QSettings>

#include <limits>


namespace
{
QBrush naturalColors[] = {
    QBrush(QColor(  0, 255,   0)),  // 0  1  green
    QBrush(QColor(112, 112, 255)),  // 1  2  blue
    QBrush(QColor(  0, 220, 232)),  // 2  3  cyan
    QBrush(QColor(255, 144,   0)),  // 3  4  orange
    QBrush(QColor(232, 232,   0)),  // 4  5  yellow
    QBrush(QColor(240,  64,  64)),  // 5  6  red
    QBrush(QColor(216,   8, 232)),  // 6  7  magenta (needs g=8 for black text)
};

// Note fortuitous side effect of Qt failing: Pixmap QBrushes always
//   have color() of black, so textColor() returns white which is what's
//   desired anyway.
const char *pixmap1to2Data[] = {"8 8 2 1",
                               "  c #00c000",   // green
                               ". c #0000a0",   // blue
                               "..  ..  ",
                               "..  ..  ",
                               "  ..  ..",
                               "  ..  ..",
                               "..  ..  ",
                               "..  ..  ",
                               "  ..  ..",
                               "  ..  .."},
           *pixmap2to3Data[] = {"8 8 2 1",
                               "  c #0000a0",   // blue
                               ". c #0090b0",   // cyan
                               "..  ..  ",
                               "..  ..  ",
                               "  ..  ..",
                               "  ..  ..",
                               "..  ..  ",
                               "..  ..  ",
                               "  ..  ..",
                               "  ..  .."},
           *pixmap3to4Data[] = {"8 8 2 1",
                               "  c #006080",   // cyan
                               ". c #f08000",   // orange
                               "..  ..  ",
                               "..  ..  ",
                               "  ..  ..",
                               "  ..  ..",
                               "..  ..  ",
                               "..  ..  ",
                               "  ..  ..",
                               "  ..  .."},
           *pixmap4to5Data[] = {"8 8 2 1",
                               ". c #806000",   // orange
                               ". c #c0c000",   // yellow
                               "..  ..  ",
                               "..  ..  ",
                               "  ..  ..",
                               "  ..  ..",
                               "..  ..  ",
                               "..  ..  ",
                               "  ..  ..",
                               "  ..  .."},
           *pixmap5to6Data[] = {"8 8 2 1",
                               "  c #c08000",   // yellow
                               ". c #a00000",   // red
                               "..  ..  ",
                               "..  ..  ",
                               "  ..  ..",
                               "  ..  ..",
                               "..  ..  ",
                               "..  ..  ",
                               "  ..  ..",
                               "  ..  .."},
           *pixmap6to7Data[] = {"8 8 2 1",
                               ". c #e00000",   // red
                               ". c #5000a0",   // magenta
                               "..  ..  ",
                               "..  ..  ",
                               "  ..  ..",
                               "  ..  ..",
                               "..  ..  ",
                               "..  ..  ",
                               "  ..  ..",
                               "  ..  .."},
           *pixmap7to1Data[] = {"8 8 2 1",
                               "  c #400080",   // magenta
                               ". c #00a000",   // green
                               "..  ..  ",
                               "..  ..  ",
                               "  ..  ..",
                               "  ..  ..",
                               "..  ..  ",
                               "..  ..  ",
                               "  ..  ..",
                               "  ..  .."};

const char *pixmapTriggerNote[] = {"8 8 2 1",
                                   "  c #000000",   // black
                                   ". c #c0c0c0",   // light gray
                                   "..  ..  ",
                                   "..  ..  ",
                                   "  ..  ..",
                                   "  ..  ..",
                                   "..  ..  ",
                                   "..  ..  ",
                                   "  ..  ..",
                                   "  ..  .."};

QBrush* accidentalPixmaps[7] = {nullptr};

// Can't be const, and can't really init pointers to accidentalPixmaps
//   (kept as placeholders for documenation's sake) here because
//   accidentalPixmaps not created until initPixmaps().
QBrush *majorScaleBrushes[] = {
    &naturalColors    [0],  // 0   (1)
     accidentalPixmaps[0],  // 1   1->2
    &naturalColors    [1],  // 2   (2)
     accidentalPixmaps[1],  // 3   2->3
    &naturalColors    [2],  // 4   (3)
    &naturalColors    [3],  // 5   (4)
     accidentalPixmaps[3],  // 6   4->5
    &naturalColors    [4],  // 7   (5)
     accidentalPixmaps[4],  // 8   5->6
    &naturalColors    [5],  // 9   (6)
     accidentalPixmaps[5],  // 10  6->7
    &naturalColors    [6],  // 11  (7)
};

// Ibid majorScaleBrushes directly above
QBrush *minorScaleBrushes[] = {
    &naturalColors    [0],  // 0   (1)
     accidentalPixmaps[0],  // 1   1->2
    &naturalColors    [1],  // 2   (2)
    &naturalColors    [2],  // 3   (3)
     accidentalPixmaps[2],  // 4   3->4
    &naturalColors    [3],  // 5   (4)
     accidentalPixmaps[3],  // 6   4->5
    &naturalColors    [4],  // 7   (5)
    &naturalColors    [5],  // 8   (6)
     accidentalPixmaps[5],  // 9   6->7
    &naturalColors    [6],  // 10  (7)
     accidentalPixmaps[6],  // 11  7->1
};

QBrush *triggerNoteBrush;

// For mapping QAbstractItemModels to MatrixElemens via
// QAbstractItemModel::setData() and data().
// Set in reconfigure() and retrieved by getMatrixElement().
static const int MatrixElementData = 2;

}  // namespace


namespace Rosegarden
{

// Workaround because can't statically init nonDiatonicPitchPixmaps[]
// before Qt is up and running.
// Done only once per program execution.
void MatrixElement::initPixmaps()
{
    static bool pixmapsInitialized = false;

    if (pixmapsInitialized)
        return;

    accidentalPixmaps[0] = new QBrush(QPixmap(pixmap1to2Data));
    accidentalPixmaps[1] = new QBrush(QPixmap(pixmap2to3Data));
    accidentalPixmaps[2] = new QBrush(QPixmap(pixmap3to4Data));
    accidentalPixmaps[3] = new QBrush(QPixmap(pixmap4to5Data));
    accidentalPixmaps[4] = new QBrush(QPixmap(pixmap5to6Data));
    accidentalPixmaps[5] = new QBrush(QPixmap(pixmap6to7Data));
    accidentalPixmaps[6] = new QBrush(QPixmap(pixmap7to1Data));

    majorScaleBrushes[1]  = accidentalPixmaps[0];  // 1   1->2
    majorScaleBrushes[3]  = accidentalPixmaps[1];  // 3   2->3
    majorScaleBrushes[6]  = accidentalPixmaps[3];  // 6   4->5
    majorScaleBrushes[8]  = accidentalPixmaps[4];  // 8   5->6
    majorScaleBrushes[10] = accidentalPixmaps[5];  // 10  6->7

    minorScaleBrushes[1]  = accidentalPixmaps[0];  // 1   1->2
    minorScaleBrushes[4]  = accidentalPixmaps[2];  // 4   3->4
    minorScaleBrushes[6]  = accidentalPixmaps[3];  // 6   4->5
    minorScaleBrushes[9]  = accidentalPixmaps[5];  // 9   6->7
    minorScaleBrushes[11] = accidentalPixmaps[6];  // 11  7->1

    triggerNoteBrush = new QBrush(QPixmap(pixmapTriggerNote));

    pixmapsInitialized = true;
}

MatrixElement::MatrixElement(MatrixScene *scene, Event *event,
                             bool drum, long pitchOffset,
                             const Segment *segment,
                             const unsigned segmentIndex) :
    ViewElement(event),
    m_scene(scene),
    m_event(event),
    m_drumMode(drum),
    m_drumDisplay(drum),
    m_current(false),
    m_selected(false),
    m_showName(true),
    m_time(std::numeric_limits<timeT>::min()),
    m_pitch(-1),
    m_tonic(0),
    m_degree(0),
    m_keyRelative(false),
    m_sharps(true),
    m_minor(false),
    m_offsetMinors(false),
    m_alternateMinorNotes(false),
    m_noteItem(nullptr),
    m_drumItem(nullptr),
    m_textItem(nullptr),
    m_noteSelectItem(nullptr),
    m_drumSelectItem(nullptr),
    m_tiedNoteItem(nullptr),
    m_pitchOffset(pitchOffset),
    m_segment(segment),
    m_segmentIndex        (segmentIndex),
    m_noteZ               (segmentNoteZ(segmentIndex)),
    m_textZ               (segmentTextZ(segmentIndex)),
    m_activeSegmentNoteZ  (segmentNoteZ(m_scene->segmentIndexEnd())),
    m_activeSegmentTextZ  (m_activeSegmentNoteZ   + 1.0),
    m_selectionBorderZ    (m_activeSegmentTextZ   + 1.0),
    m_selectedSegmentNoteZ(m_activeSegmentTextZ   + SEGMENT_Z_INCREMENT),
    m_selectedSegmentTextZ(m_selectedSegmentNoteZ + 1.0)
{
    if (segment && scene && segment == scene->getCurrentSegment()) {
        m_current = true;
    }

    getKeyInfo(nullptr, m_showName, m_time, 0);
    reconfigure(false);
}

MatrixElement::~MatrixElement()
{
    m_scene->graphicsRectPool.putBack(m_noteItem);
    m_scene->graphicsIsotropicDiamondPool.putBack(m_drumItem);
    m_scene->graphicsIsotropicRectPool.putBack(m_noteSelectItem);
    m_scene->graphicsIsotropicDiamondPool.putBack(m_drumSelectItem);
    m_scene->graphicsEllipsePool.putBack(m_tiedNoteItem);
    m_scene->graphicsTextPool.putBack(m_textItem);
}

void
MatrixElement::reconfigure(
const bool          needKey,
const Key* const    key)
{
    timeT time = event()->getAbsoluteTime();
    timeT duration = event()->getDuration();
    reconfigure(time, duration, needKey, key);
}

void
MatrixElement::reconfigure(
const int           velocity,
const bool          needKey,
const Key* const    key)
{
    timeT time = event()->getAbsoluteTime();
    timeT duration = event()->getDuration();
    long pitch = 60;
    event()->get<Int>(BaseProperties::PITCH, pitch);
    reconfigure(time, duration, pitch, velocity, needKey, key);
}

void
MatrixElement::reconfigure(
const timeT         time,
const timeT         duration,
const bool          needKey,
const Key* const    key)
{
    long pitch = 60;
    event()->get<Int>(BaseProperties::PITCH, pitch);

    reconfigure(time, duration, pitch, needKey, key);
}

void
MatrixElement::reconfigure(
const timeT         time,
const timeT         duration,
const int           pitch,
const bool          needKey,
const Key* const    key)
{
    long velocity = 100;
    event()->get<Int>(BaseProperties::VELOCITY, velocity);

    reconfigure(time, duration, pitch, velocity, needKey, key);
}

void
MatrixElement::reconfigure(
const timeT         time,
const timeT         duration,
const int           pitch,
const int           velocity,
const bool          needKey,
const Key* const    key)
{
    const RulerScale *scale = m_scene->getRulerScale();
    int resolution = m_scene->getYResolution();

    double x0 = scale->getXForTime(time);
    double x1 = scale->getXForTime(time + duration);
    m_width = x1 - x0;

    m_velocity = velocity;

    bool tiedNote = (event()->has(BaseProperties::TIED_FORWARD) ||
                     event()->has(BaseProperties::TIED_BACKWARD));

    // Have to do this before noteColorBrush() instead of later (and/or
    //   only if showName)
    // Optional note name.
    // But no note names on diamond-shaped percussion notes.
    m_drumDisplay =     m_drumMode
                    && !m_scene->getMatrixWidget()
                               ->getShowPercussionDurations();
    bool showName =    m_scene->getMatrixWidget()->getShowNoteNames()
                    && !m_drumDisplay;

    const MatrixWidget::NoteColorType   noteColorType
                                      =   m_scene
                                        ->getMatrixWidget()
                                        ->getNoteColorType();

    if (showName || noteColorType == MatrixWidget::NoteColorType::PITCH) {
        if (   needKey
            || showName != m_showName
            || time     != m_time
            || pitch    != m_pitch)     // really only need in pitch "modes"
             getKeyInfo(key, showName, time, pitch);
        else {
            m_showName = showName;
            m_time     = time;
            m_pitch    = pitch;
        }
    }

    const QBrush brush(noteBrush());

    // Turned off because adds little or no information to user (another
    // segment's notes are underneath?) and generally just confusingly
    // changes velocity color of notes.
    // colour.setAlpha(160);

    // set the Y position taking m_pitchOffset into account, subtracting the
    // opposite of whatever the originating segment transpose was
    double pitchy = (127 - pitch - m_pitchOffset) * (resolution + 1);

    // Only one of m_noteItem() or m_drumItem() active, depending
    // on various modes and settings. Is more efficient to have  both
    // with inactive one set to hide() vs dynamically creating/deleting
    // and adding/removing from scene.
    // The various MatrixScene::graphicsXxxPools create and cache
    // QGraphicsItems for reuse as needed.
    qreal fres(resolution + 1);
    float safeWidth =  m_width;
    if (m_drumDisplay) {
        if (m_noteItem) {
            // Very rare case of changing segment from pitched to percussion.
            // Could just m_noteItem->hide(), but even if changed back to
            // pitched will be fully re-initialized and getFrom() is no-cost,
            // so might as well putBack() for possible reuse by a different
            // segment.
            m_scene->graphicsRectPool.putBack(m_noteItem);
            m_noteItem = nullptr;
            if (m_noteSelectItem) {
                // Even rarer case of changing from pitched to percussion
                // while note is selected.
                m_scene->graphicsIsotropicRectPool.putBack(m_noteSelectItem);
                m_noteSelectItem = nullptr;
            }
        }
        if (!m_drumItem)
            m_drumItem = m_scene->graphicsIsotropicDiamondPool.getFrom();
        m_drumItem->setSize(0.5 * fres, fres);
        m_drumItem->setPen(outlinePen());
        m_drumItem->setBrush(brush);
        m_drumItem->setPos(x0, pitchy);
        if (m_selected) {
            m_drumItem->setZValue(m_selectedSegmentNoteZ);
            // First true is selected, second to force update if already
            // selected (extreme edge case of changing segment's track
            // from notes to drum while element is selected)
            setSelected(true, true);
        }
        else
            m_drumItem->setZValue(m_current ? m_activeSegmentNoteZ : m_noteZ);
        m_drumItem->setData(MatrixElementData,
                            QVariant::fromValue((void *)this));
    }
    else {  // !m_drumDisplay, i.e. is pitched note segment
        if (m_drumItem) {
            // Very rare case of changing segment from percussion to pitched.
            // Could just m_drumItem->hide(), but even if changed back to
            // percussion will be fully re-initialized, and getFrom() is
            // no-cost, so might as well putBack() for possible reuse by a
            // different segment.
            m_scene->graphicsIsotropicDiamondPool.putBack(m_drumItem);
            m_drumItem = nullptr;
            if (m_drumSelectItem) {
                // Even rarer case of changing from percussion to pitched
                // while drumbeat is selected.
                m_scene->graphicsIsotropicDiamondPool.putBack(
                                                        m_drumSelectItem);
                m_drumSelectItem = nullptr;
            }
        }

        if (!m_noteItem) m_noteItem = m_scene->graphicsRectPool.getFrom();

        if (safeWidth < 1) {
            x0 = std::max(0.0, x1 - 1);
            safeWidth = 1;
        }

        QRectF rect(0, 0, safeWidth, fres);
        m_noteItem->setRect(rect);
        m_noteItem->setPen(outlinePen());
        m_noteItem->setBrush(brush);
        m_noteItem->setPos(x0, pitchy);

        if (m_selected) {
            m_noteItem->setZValue(m_selectedSegmentNoteZ);
            // First true is selected, second to force update if already
            // selected (extreme edge case of changing segment's track
            // from notes to drum while element is selected)
            setSelected(true, true);
        }
        else
            m_noteItem->setZValue(m_current ? m_activeSegmentNoteZ : m_noteZ);
        m_noteItem->setData(MatrixElementData,
                            QVariant::fromValue((void *)this));
    }

    if (showName && !m_drumMode)  // check is for "show percussion durations"
        setLabel(time, pitch, x0, brush);
    else if (m_textItem) {
        // Could just m_textItem->hide() and then only ->show() above if
        // not doing getFrom(). But only additional optimization would be
        // not doing setFont (because resolution can't change, is dependent
        // on whether any percussion segments) (and isn't recomputed  even
        // if had drum segemnt but that then deleted in main window). So
        // even though unlikely, might as well putBack() in case segment's
        // number of notes decreases before next "View -> Notes -> Show
        // note names" and a different segment can reuse from pool. No
        // large impact either way.
        m_scene->graphicsTextPool.putBack(m_textItem);
        m_textItem = nullptr;
    }

    // Leaving this in for now, but worried about it.
    // No Qt removeToolTip(). Is setting to empty QString the same,
    //   or does that leave empty QString tooltips on all non-tied
    //   notes (99.99% of them) that otherwise would have no tooltip?
    // Adding expensive m_onceHadToolTips hack to avoid.
    // Can't have an m_hadToolTip flag because MatrixElements reuse
    //   existing m_noteItems from pool so not are not associated
    //   one-to-one MatrixElement-to-QGraphicsRectItem.
    if (m_noteItem) {
        if (event()->has(BaseProperties::TRIGGER_SEGMENT_ID)) {
            // Takes precedence over also being tied note
            m_noteItem->setToolTip(QObject::tr("This note triggers an "
                                               "ornamentation segment."));
            m_onceHadToolTips.insert(m_noteItem);
        }
        else if (tiedNote) {
            if (event()->has(BaseProperties::TIED_FORWARD)) {
                if (!m_tiedNoteItem)
                    m_tiedNoteItem = m_scene->graphicsEllipsePool.getFrom();

                if (m_selected)
                    m_tiedNoteItem->setZValue(m_selectedSegmentTextZ);
                else
                    m_tiedNoteItem->setZValue(  m_current
                                              ? m_activeSegmentTextZ
                                              : m_textZ);

                m_tiedNoteItem->setBrush(textColor(brush.color()));
                m_tiedNoteItem->setPen(Qt::NoPen);
                qreal oneThird = fres / 3.0;
                m_tiedNoteItem->setRect(0, 0, oneThird, oneThird);
                m_tiedNoteItem->setPos(x0 + safeWidth - oneThird * 0.5,
                                       pitchy + oneThird);
            }

            m_noteItem->setToolTip(QObject::tr("This note is tied to "
                                               "another note."));
            m_onceHadToolTips.insert(m_noteItem);
        }
        else {
            if (m_tiedNoteItem) {
                m_scene->graphicsEllipsePool.putBack(m_tiedNoteItem);
                m_tiedNoteItem = nullptr;
            }

            if (m_onceHadToolTips.find(m_noteItem) != m_onceHadToolTips.end())
                m_noteItem->setToolTip(QString());
        }
    }

    setLayoutX(x0);
    setLayoutY(pitchy);
}  // reconfigure(timeT, timeT, int, int, bool, const Key* const)

// See documentation in .h file
void
MatrixElement::getKeyInfo(
const Key*          key,
bool                showName,
const timeT         time,
const int           pitch)
{
    static const Key    defaultKey;
    if (!key) {
        const Segment *segment = m_scene->getCurrentSegment();
        if (!segment) return;  // Impossible, but check anyway.
        auto keyIter = segment->getKeySignatureIterAtTime(time);
        if (segment->keySignatureIterIsValid(keyIter))
            key =  &keyIter->second;
        else  // should never happen, but in case ..
            key = &defaultKey;
    }

    m_showName = showName;
    m_time     = time;
    m_pitch    = pitch;

    const MatrixWidget *matrixWidget(m_scene->getMatrixWidget());
    m_offsetMinors        = matrixWidget->getNoteNamesOffsetMinors   ();
    m_alternateMinorNotes = matrixWidget->getNoteNamesAlternateMinors();
    m_keyRelative         = matrixWidget->notesAreKeyDependent       ();

    auto setDegree = [this, pitch]()
    {
        this->m_degree =   (  pitch
                            + (   m_minor && m_offsetMinors
                               ? 21   // C->A, +9 but make sure always positive
                               : 12)  // make sure always positive
                            - m_tonic)
                         % 12;
    };

    if (key->getName() == m_keyName) {
        setDegree();
        return;  // no change
    }
    else
        m_keyName = key->getName();

    if (key->getAccidentalCount() == 0)
        m_sharps = !m_scene->getMatrixWidget()->getNoteNamesCmajFlats();
    else
        m_sharps = key->isSharp();

    m_tonic = key->getTonicPitch();
    m_minor = key->isMinor();

    setDegree();

}  // getKeyInfo()


QAbstractGraphicsShapeItem*
MatrixElement::getActiveItem()
const
{
    if (!m_drumDisplay && m_noteItem)
        return dynamic_cast<QAbstractGraphicsShapeItem *>(m_noteItem);
    else if (m_drumItem)  // must be true
        return dynamic_cast<QAbstractGraphicsShapeItem *>(m_drumItem);
    else  // safety check, can't happen
        return nullptr;
}

void
MatrixElement::setSelected(bool selected, bool force)
{
    // force==true is for use by reconfigure() in extreme edge
    // case of changing segment from drums to notes or vice-versa
    // while element is selected
    if (!force && selected == m_selected) return;

    QAbstractGraphicsShapeItem *item = getActiveItem();
    if (!item) return;

    if (selected) {
        QColor selectionColor = selectionBorderColor(item);
        float res = m_scene->getYResolution() + 1;
        item->setZValue(m_selectedSegmentNoteZ);
        if (item == m_noteItem) {
            if (m_textItem)
                m_textItem->setZValue(m_selectedSegmentTextZ);
            if (m_tiedNoteItem)
                m_tiedNoteItem->setZValue(m_selectedSegmentTextZ);
            if (!m_noteSelectItem) {
                m_noteSelectItem = m_scene->graphicsIsotropicRectPool.getFrom();
                m_noteSelectItem->setZValue(m_selectionBorderZ);
            }
            m_noteSelectItem->setRect(m_noteItem->rect());
            m_noteSelectItem->setPos(m_noteItem->pos());
            m_noteSelectItem->setBrush(selectionColor);
            m_noteSelectItem->setData(MatrixElementData,
                                QVariant::fromValue((void *)this));
        } else {   // item == m_drumItem
            m_drumItem->setPen(Qt::NoPen);
            if (!m_drumSelectItem) {
                m_drumSelectItem = m_scene->graphicsIsotropicDiamondPool.
                                   getFrom();
                m_drumSelectItem->setZValue(m_selectionBorderZ);
                m_drumSelectItem->setPen(Qt::NoPen);
            }
            m_drumSelectItem->setSize(res * 0.5 + DRUM_SELECT_BORDER_WIDTH,
                                      res       + DRUM_SELECT_BORDER_WIDTH * 2);
            m_drumSelectItem->setPos(m_drumItem->pos().x(),
                                     m_drumItem->pos().y() -
                                       DRUM_SELECT_BORDER_WIDTH);
            m_drumSelectItem->setBrush(QBrush(selectionColor));
            m_drumSelectItem->setData(MatrixElementData,
                                QVariant::fromValue((void *)this));
        }
    } else {  // not selected
        if (item == m_noteItem) {
            m_noteItem->setZValue(m_activeSegmentNoteZ);
            if (m_textItem)
                m_textItem->setZValue(m_activeSegmentTextZ);
            if (m_tiedNoteItem)
                m_tiedNoteItem->setZValue(m_activeSegmentTextZ);
            if (m_noteSelectItem) {
                // Definitely putBack() instead of hide(). See comments
                // in reconfigure(timeT,timeT,int,int) above. If just
                // hide() pool would continuously grow each time a new
                // element was selected until all had selectItems, even
                // though very few are likely be active at any given time.
                // Plus, almost all initialization above needs to be done
                // if was show() instead of getFrom() (and getFrom() is
                // cheap) so no good reason to do hide()/show() instead.
                m_scene->graphicsIsotropicRectPool.putBack(m_noteSelectItem);
                m_noteSelectItem = nullptr;
            }
        } else {   // item == m_drumItem
            m_drumItem->setZValue(m_activeSegmentNoteZ);
            m_drumItem->setPen(outlinePen());
            if (m_drumSelectItem) {
                // See comment above
                m_scene->graphicsIsotropicDiamondPool.putBack(m_drumSelectItem);
                m_drumSelectItem = nullptr;
            }
        }
    }
    m_selected = selected;
}

void
MatrixElement::setCurrent(bool current)
{
    if (current == m_current ) return;

    m_current = current;  // must be done before call to noteColor()

    QAbstractGraphicsShapeItem *item = getActiveItem();
    if (!item) return;

    const QBrush brush(noteBrush());
    item->setBrush(brush);
    item->setPen(outlinePen());

    item->setZValue(current ? m_activeSegmentNoteZ : m_noteZ);

    if (m_textItem) {
        m_textItem->setBrush(textColor(brush.color()));
        m_textItem->setZValue(current ? m_activeSegmentTextZ : m_textZ);
    }
}

MatrixElement *
MatrixElement::getMatrixElement(QGraphicsItem *item)
{
    QVariant v = item->data(MatrixElementData);
    if (v.isNull()) return nullptr;
    return static_cast<MatrixElement *>(v.value<void *>());
}

// See comment at setColor(), below. Always have current key info
// when this being called.
void MatrixElement::setLabel(
const bool          keyDependent,
const Key* const    key)
{
    if (m_drumMode) return;

    if (!m_scene->getMatrixWidget()->getShowNoteNames()) {
        if (m_textItem) {
            m_scene->graphicsTextPool.putBack(m_textItem);
            m_textItem = nullptr;
        }
        return;
    }

    if (keyDependent)
        getKeyInfo(key, m_showName, m_time, m_pitch);

    setLabel(m_time,
             m_pitch,
             m_scene->getRulerScale()->getXForTime(m_time),
             noteBrush());
}

void MatrixElement::setLabel(
const timeT      time,
const int        pitch,
const double     x0,
const QBrush    &brush)
{
    int               resolution     = m_scene->getYResolution();
    ChordNameRuler   *chordNameRuler = m_scene->getMatrixWidget()
                                               ->chordNameRuler();

    qreal   fres     (resolution + 1);
    double  pitchy = (127 - pitch - m_pitchOffset) * (resolution + 1);

    if (!m_textItem) m_textItem = m_scene->graphicsTextPool.getFrom();

    // Draw text on top of note, see constants in .h file
    if (m_selected)
        m_textItem->setZValue(m_selectedSegmentTextZ);
    else
        m_textItem->setZValue(m_current ? m_activeSegmentTextZ : m_textZ);
    m_textItem->setBrush(textColor(brush.color()));

    const MatrixWidget *matrixWidget = m_scene->getMatrixWidget();

    QString noteName;
    switch (matrixWidget->getNoteNameType()) {
        case MatrixWidget::NoteNameType::OFF:
            noteName = "";
            break;

        case MatrixWidget::NoteNameType::CONCERT:
            noteName = MidiPitchLabel(pitch, "", m_sharps).getQString();
        break;

        case MatrixWidget::NoteNameType::FIXED_DO:
            noteName = MidiPitchLabel::fixedSolfege(pitch, m_sharps);
        break;

        case MatrixWidget::NoteNameType::INTEGER_ABSOLUTE:
        noteName =  QString("%1:%2")
                   .arg(pitch % 12)
                   .arg(  pitch / 12
                        + Preferences::midiOctaveNumberOffset.get());
        break;

        case MatrixWidget::NoteNameType::VELOCITY:
            noteName = QString("%1").arg(m_velocity);
        break;

        case MatrixWidget::NoteNameType::RAW_MIDI:
            noteName = QString("%1").arg(pitch);
        break;

        case MatrixWidget::NoteNameType::DEGREE:
            if (m_minor && !m_offsetMinors && m_alternateMinorNotes)
                noteName =   MidiPitchLabel
                           ::scaleDegreeMinorAlt(m_degree, m_sharps);
            else
                noteName = MidiPitchLabel::scaleDegreeMajor(m_degree, m_sharps);
        break;

        case MatrixWidget::NoteNameType::MOVABLE_DO:
            noteName = MidiPitchLabel::movableSolfege(m_degree, m_sharps);
        break;

        case MatrixWidget::NoteNameType::INTEGER_KEY:
            noteName = QString("%1").arg(m_degree);
        break;
    }

    if (      matrixWidget->getChordSpellingType()
           != MatrixWidget::ChordSpellingType::OFF
        && chordNameRuler
        && chordNameRuler->haveChords()) {
        int rootDegree = chordNameRuler->chordRootPitchAtTime(time);
        if (rootDegree != -1) {
            unsigned chordDegree = (pitch + 12 - rootDegree) % 12;
            if (   m_scene->getMatrixWidget()->getChordSpellingType()
                == MatrixWidget::ChordSpellingType::DEGREE)
                noteName =  QString("%1(%2)")
                           .arg(noteName)
                           .arg(  MidiPitchLabel
                                ::scaleDegreeMajor(chordDegree, m_sharps));
            else
                noteName = QString("%1(%2)").arg(noteName)
                                            .arg(chordDegree);
        }
    }

    m_textItem->setText(noteName, m_noteItem->rect());

    QFont font;
    font.setPixelSize(resolution);
    m_textItem->setFont(font);
    m_textItem->setData(MatrixElementData,
                        QVariant::fromValue((void *)this));
    m_textItem->setPos(x0, pitchy - fres * 0.1);
}  // setLabel(const int, const bool, const QBrush&)

void MatrixElement::setColor(
const bool          keyDependent,
const Key* const    key)
{
    QAbstractGraphicsShapeItem *item = getActiveItem();
    if (!item) return;

    if (keyDependent)
        getKeyInfo(key, m_showName, m_time, m_pitch);

    const QBrush brush(noteBrush());
    item->setBrush(brush);
    item->update();
    if (m_textItem) m_textItem->setBrush(textColor(brush.color()));

    if (item == m_noteItem && m_noteSelectItem)
        m_noteSelectItem->setBrush(selectionBorderColor(m_noteItem));
    else if (item == m_drumItem && m_drumSelectItem)
        m_drumSelectItem->setBrush(selectionBorderColor(m_drumItem));
}

// See comment at setColor(), above. Always have current key info
// when this being called.
QBrush MatrixElement::noteBrush()
const
{
    const MatrixWidget* const   widget = m_scene->getMatrixWidget();
    MatrixWidget::NoteColorType noteColorType = widget->getNoteColorType();
    bool colorAllSegments = widget->getNoteColorAllSegments();
    QColor colour;

    if (!m_current && !colorAllSegments) {
        colour = QColor(GRAY_RED_COMPONENT,
                        GRAY_GREEN_COMPONENT,
                        GRAY_BLUE_COMPONENT);
        return QBrush(colour);
    }

    // Never pitch-color drum "notes".
    // Velocity as best choice because likely only on percussion
    //   track (segments) so SEGMENT not useful.
    // Velocity should likely stand out against PITCH notes.
    // Full solution would be to keep "previousNoteColorType" updated
    //   to anything but PITCH and use that, but more trouble than worth.
    if (m_drumMode && noteColorType == MatrixWidget::NoteColorType::PITCH)
        noteColorType = MatrixWidget::NoteColorType::VELOCITY;

    if (event()->has(BaseProperties::TRIGGER_SEGMENT_ID)) {
        // Was historically solid Qt::cyan as contrasting color to
        // green->red spectrum of velocity colors. That now potentially
        // conflicting with segment and/or pitch colors, so using
        // black/gray pixmap.
        return *triggerNoteBrush;
    }

#if 0   // Unused, see below
    // For darkening non-current segment notes
    static const int DARKEN  = 133, // >100 is darken  for QColor::darker()
                     LIGHTEN = 133; // >100 is lighten for QColor::lighter()
#endif

    if (noteColorType == MatrixWidget::NoteColorType::VELOCITY) {
        colour = DefaultVelocityColour::getInstance()->getColour(m_velocity);
#if 0   // t4os: Removing this classic RG hack.
        //       Is confusing (why is same velocity sometimes different color?)
        //       Use "Only active" (color) if want to differentiate current
        //         segment vs others.
        if (colorAllSegments) {
            // Change brightness to differentiate active segment vs others
            if (m_current) colour = colour.lighter(LIGHTEN);
            else           colour = colour.darker(DARKEN);
        }
        return QBrush(colour);
#endif
        return QBrush(colour.lighter());  // match what velocity ruler does
    }
    else if (noteColorType != MatrixWidget::NoteColorType::PITCH) {
        const RosegardenDocument* document = m_scene->getDocument();
        const Composition &composition = document->getComposition();
        colour = composition.getSegmentColourMap().
                 getColour(m_segment->getColourIndex());
        if (noteColorType == MatrixWidget::NoteColorType::SEGMENT_AND_VELOCITY){
            // Lighten or darken segment color by velocity
            const int MIDDLE_VELOCITY = 63;   // MIDI (are we always MIDI??)
            if (m_velocity >= MIDDLE_VELOCITY) {
                colour = colour.lighter(100 + m_velocity - MIDDLE_VELOCITY);
            } else {
                colour = colour.darker(100 + MIDDLE_VELOCITY - m_velocity);
            }
        }
        return QBrush(colour);
    }
    else {  // noteColorType == MatrixWidget::NoteColorType::PITCH)
        if (m_keyRelative) {
            if (m_minor) {
                if (m_offsetMinors)
                    return *majorScaleBrushes[m_degree % 12];
                else
                    return *minorScaleBrushes[m_degree];
            }
            else
                return *majorScaleBrushes[m_degree];
        }
        else
            return *majorScaleBrushes[m_pitch % 12];
    }
}  // MatrixElement::noteBrush()

QColor
MatrixElement::textColor(
const QColor noteColor)
const
{
    // Note pixmap QBrushes (accidental/out-of-key notes in
    // MatrixWidget::NoteColorType::PITCH mode) return black for
    // color(), so this returns white which is desired.
    int intensity = qGray(noteColor.rgb());
    if (intensity > 112) return Qt::black;
    else                 return Qt::white;
}

QColor
MatrixElement::selectionBorderColor(
const QAbstractGraphicsShapeItem *item)
const
{
    // Outline in default blue for velocity color mode, otherwise
    // choose contrasting color.

    if (m_scene->getMatrixWidget()->getNoteColorType() ==
        MatrixWidget::NoteColorType::VELOCITY)
    {
        return GUIPalette::getColour(GUIPalette::SelectedElement);
    } else {
        const QColor noteColor = item->brush().color();

        // map 0,63,64,127,128,191,192,255 to 160,191,192,223,224,255,128,159
        auto mapTo128_255 = [](unsigned satVal) {
            unsigned rot_64   = (satVal + 64) % 256,
                     map_0_to_127  = static_cast<unsigned>(rot_64 * 127.0 /
                                                           255.0 + 0.5);
            return map_0_to_127 + 128;
            };

        const QColor result =
               QColor::fromHsv((noteColor.hue() + 240) % 360,
                               mapTo128_255(noteColor.saturation()),
                               mapTo128_255(noteColor.value()),
                               255);// alpha, was 128 to show non-active-
                                    // segment notes, but reduces visibility
                                    // and causes artifacts with contiguous
                                    // and/or tied notes.

        return result;
    }
}

}  // namespace Rosegarden
