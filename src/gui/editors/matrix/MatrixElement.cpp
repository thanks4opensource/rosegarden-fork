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

#define RG_MODULE_STRING "[MatrixElement]"

#include "MatrixElement.h"
#include "MatrixScene.h"
#include "MatrixWidget.h"

#include "IsotropicDiamondItem.h"
#include "IsotropicRectItem.h"
#include "IsotropicTextItem.h"

#include "misc/Debug.h"
#include "base/RulerScale.h"
#include "misc/ConfigGroups.h"

#include <QGraphicsRectItem>
#include <QBrush>
#include <QColor>
#include <QSettings>

#include "base/Event.h"
#include "base/NotationTypes.h"
#include "base/BaseProperties.h"
#include "document/RosegardenDocument.h"
#include "gui/general/GUIPalette.h"
#include "gui/rulers/DefaultVelocityColour.h"
#include "gui/general/MidiPitchLabel.h"

namespace Rosegarden
{


static const int MatrixElementData = 2;

MatrixElement::MatrixElement(MatrixScene *scene, Event *event,
                             bool drum, long pitchOffset,
                             const Segment *segment) :
    ViewElement(event),
    m_scene(scene),
    m_event(event),
    m_drumMode(drum),
    m_drumDisplay(drum),
    m_current(true),
    m_selected(false),
    m_noteItem(nullptr),
    m_drumItem(nullptr),
    m_textItem(nullptr),
    m_noteSelectItem(nullptr),
    m_drumSelectItem(nullptr),
    m_pitchOffset(pitchOffset),
    m_segment(segment)
{
    if (segment && scene && segment != scene->getCurrentSegment()) {
        m_current = false;
    }
    reconfigure();
}

MatrixElement::~MatrixElement()
{
    m_scene->graphicsRectPool.putBack(m_noteItem);
    m_scene->graphicsIsotropicDiamondPool.putBack(m_drumItem);
    m_scene->graphicsIsotropicRectPool.putBack(m_noteSelectItem);
    m_scene->graphicsIsotropicDiamondPool.putBack(m_drumSelectItem);
    m_scene->graphicsTextPool.putBack(m_textItem);
}

void
MatrixElement::reconfigure()
{
    timeT time = event()->getAbsoluteTime();
    timeT duration = event()->getDuration();
    reconfigure(time, duration);
}

void
MatrixElement::reconfigure(int velocity)
{
    timeT time = event()->getAbsoluteTime();
    timeT duration = event()->getDuration();
    long pitch = 60;
    event()->get<Int>(BaseProperties::PITCH, pitch);
    reconfigure(time, duration, pitch, velocity);
}

void
MatrixElement::reconfigure(timeT time, timeT duration)
{
    long pitch = 60;
    event()->get<Int>(BaseProperties::PITCH, pitch);

    reconfigure(time, duration, pitch);
}

void
MatrixElement::reconfigure(timeT time, timeT duration, int pitch)
{
    long velocity = 100;
    event()->get<Int>(BaseProperties::VELOCITY, velocity);

    reconfigure(time, duration, pitch, velocity);
}

void
MatrixElement::reconfigure(timeT time, timeT duration, int pitch, int velocity)
{
    const RulerScale *scale = m_scene->getRulerScale();
    int resolution = m_scene->getYResolution();

    double x0 = scale->getXForTime(time);
    double x1 = scale->getXForTime(time + duration);
    m_width = x1 - x0;

    m_velocity = velocity;

    // if the note has TIED_FORWARD or TIED_BACK properties, draw it with a
    // different fill pattern
    bool tiedNote = (event()->has(BaseProperties::TIED_FORWARD) ||
                     event()->has(BaseProperties::TIED_BACKWARD));
    Qt::BrushStyle brushPattern = (tiedNote ? Qt::Dense2Pattern : Qt::SolidPattern);

    const QColor colour(noteColor());

    // Turned off because adds little or no information to user (another
    // segment's notes are underneath?) and generally just confusingly
    // changes velocity color of notes.
    // colour.setAlpha(160);

    // set the Y position taking m_pitchOffset into account, subtracting the
    // opposite of whatever the originating segment transpose was
    double pitchy = (127 - pitch - m_pitchOffset) * (resolution + 1);

    m_drumDisplay = m_drumMode &&
        !m_scene->getMatrixWidget()->getShowPercussionDurations();

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
        if (!m_drumItem) {
            m_drumItem = m_scene->graphicsIsotropicDiamondPool.getFrom();
        }
        m_drumItem->setSize(0.5 * fres, fres);
        m_drumItem->setPen(outlinePen());
        m_drumItem->setBrush(QBrush(colour, brushPattern));
        m_drumItem->setPos(x0, pitchy);
        if (m_selected) {
            m_drumItem->setZValue(SELECTED_SEGMENT_NOTE_Z);
            // First true is selected, second to force update if already
            // selected (extreme edge case of changing segment's track
            // from notes to drum while element is selected)
            setSelected(true, true);
        } else {
            m_drumItem->setZValue(m_current ? ACTIVE_SEGMENT_NOTE_Z
                                            : NORMAL_SEGMENT_NOTE_Z);
        }
        m_drumItem->setData(MatrixElementData,
                            QVariant::fromValue((void *)this));
    } else {  // !m_drumDisplay, i.e. is pitched note segment
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
        safeWidth = m_width;
        if (safeWidth < 1) {
            x0 = std::max(0.0, x1 - 1);
            safeWidth = 1;
        }
        QRectF rect(0, 0, safeWidth, fres);
        m_noteItem->setRect(rect);
        m_noteItem->setPen(outlinePen());
        m_noteItem->setBrush(QBrush(colour, brushPattern));
        m_noteItem->setPos(x0, pitchy);

        if (m_selected) {
            m_noteItem->setZValue(SELECTED_SEGMENT_NOTE_Z);
            // First true is selected, second to force update if already
            // selected (extreme edge case of changing segment's track
            // from notes to drum while element is selected)
            setSelected(true, true);
        } else {
            m_noteItem->setZValue(m_current ? ACTIVE_SEGMENT_NOTE_Z
                                            : NORMAL_SEGMENT_NOTE_Z);
        }
        m_noteItem->setData(MatrixElementData,
                            QVariant::fromValue((void *)this));
    }

    // Optional note name.
    // But no note names on diamond-shaped percussion notes.
    bool showName = m_scene->getMatrixWidget()->getShowNoteNames() &&
                    !m_drumDisplay;
    if (showName) {
        if (!m_textItem) m_textItem = m_scene->graphicsTextPool.getFrom();

        // Draw text on top of note, see constants in .h file
        if (m_selected)
            m_textItem->setZValue(SELECTED_SEGMENT_TEXT_Z);
        else
            m_textItem->setZValue(m_current ? ACTIVE_SEGMENT_TEXT_Z
                                            : NORMAL_SEGMENT_TEXT_Z);

        m_textItem->setBrush(textColor(colour));

        QString noteName = MidiPitchLabel(pitch, "").getQString();
        m_textItem->setText(noteName, m_noteItem->rect());
        QFont font;
        font.setPixelSize(resolution);
        m_textItem->setFont(font);
        m_textItem->setData(MatrixElementData,
                            QVariant::fromValue((void *)this));
        m_textItem->setPos(x0, pitchy - fres * 0.1);
    }
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

    setLayoutX(x0);

    // set a tooltip explaining why this event is drawn in a different pattern
    if (tiedNote && m_noteItem) {
        m_noteItem->setToolTip(QObject::tr("This event is tied to another event."));
    }
}

bool
MatrixElement::isNote() const
{
    return event()->isa(Note::EventType);
}

QAbstractGraphicsShapeItem*
MatrixElement::getActiveItem()
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
        item->setZValue(SELECTED_SEGMENT_NOTE_Z);
        if (item == m_noteItem) {
            if (m_textItem) m_textItem->setZValue(SELECTED_SEGMENT_TEXT_Z);
            if (!m_noteSelectItem) {
                m_noteSelectItem = m_scene->graphicsIsotropicRectPool.getFrom();
                m_noteSelectItem->setZValue(SELECTION_BORDER_Z);
            }
            m_noteSelectItem->setRect(m_noteItem->rect());
            m_noteSelectItem->setPos(m_noteItem->pos());
            m_noteSelectItem->setBrush(QBrush(selectionColor));
            m_noteSelectItem->setData(MatrixElementData,
                                QVariant::fromValue((void *)this));
        } else {   // item == m_drumItem
            m_drumItem->setPen(Qt::NoPen);
            if (!m_drumSelectItem) {
                m_drumSelectItem = m_scene->graphicsIsotropicDiamondPool.
                                   getFrom();
                m_drumSelectItem->setZValue(SELECTION_BORDER_Z);
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
            m_noteItem->setZValue(ACTIVE_SEGMENT_NOTE_Z);
            if (m_textItem) m_textItem->setZValue(ACTIVE_SEGMENT_TEXT_Z);
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
            m_drumItem->setZValue(ACTIVE_SEGMENT_NOTE_Z);
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
    if (m_current == current) return;

    m_current = current;  // must be done before call to noteColor()

    QAbstractGraphicsShapeItem *item = getActiveItem();
    if (!item) return;

    QColor  color = noteColor();

    item->setBrush(color);
    item->setPen(outlinePen());

    item->setZValue(current ? ACTIVE_SEGMENT_NOTE_Z : NORMAL_SEGMENT_NOTE_Z);

    if (m_textItem) {
        m_textItem->setBrush(textColor(color));
        m_textItem->setZValue(current ? ACTIVE_SEGMENT_TEXT_Z
                                      : NORMAL_SEGMENT_TEXT_Z);
    }
}

MatrixElement *
MatrixElement::getMatrixElement(QGraphicsItem *item)
{
    QVariant v = item->data(MatrixElementData);
    if (v.isNull()) return nullptr;
    return (MatrixElement *)v.value<void *>();
}

void MatrixElement::setColor()
{
    QColor  color = noteColor();

    QAbstractGraphicsShapeItem *item = getActiveItem();
    if (!item) return;

    item->setBrush(color);
    item->update();

    if (m_textItem) m_textItem->setBrush(textColor(color));

    if (item == m_noteItem && m_noteSelectItem)
        m_noteSelectItem->setBrush(selectionBorderColor(m_noteItem));
    else if (item == m_drumItem && m_drumSelectItem)
        m_drumSelectItem->setBrush(selectionBorderColor(m_drumItem));
}

QPen MatrixElement::outlinePen() const
{
    const QColor outlineColor =
        m_current ?
        GUIPalette::getColour(GUIPalette::MatrixElementBorder) :
        GUIPalette::getColour(GUIPalette::MatrixElementLightBorder);
    return QPen(outlineColor, 0);
}

QColor
MatrixElement::noteColor()
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
        return colour;
    }

    // For darkening non-current segment notes
    static const int DARKEN  = 133, // >100 is darken  for QColor::darker()
                     LIGHTEN = 133; // >100 is lighten for QColor::lighter()

    if (event()->has(BaseProperties::TRIGGER_SEGMENT_ID)) {
        //!!! Using gray for trigger events and events from other,
        // non-active segments won't work. This should be handled some
        // other way, with a color outside the range of possible velocity
        // choices, which probably leaves some kind of curious light
        // blue or something.
        //
        // Was colour = Qt:gray in setCurrent() before refactor into
        // noteColor()
        colour = Qt::cyan;
        if (colorAllSegments && !m_current) {
            // Make dimmer to differentiate vs current segment
            colour = colour.darker(DARKEN);
        }
        return colour;
    }

    if (noteColorType == MatrixWidget::NoteColorType::VELOCITY) {
        colour = DefaultVelocityColour::getInstance()->getColour(m_velocity);
        if (colorAllSegments) {
            // Change brightness to differentiate active segment vs others
            if (m_current) colour = colour.lighter(LIGHTEN);
            else           colour = colour.darker(DARKEN);
        }
    }
    else {  // MatrixWidget::NoteColorType::SEGMENT || SEGMENT_AND_VELOCITY
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
    }

    return colour;
}

QColor
MatrixElement::textColor(
const QColor noteColor)
const
{
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
                               mapTo128_255(noteColor.value()));

        return result;
    }
}

}  // namespace
