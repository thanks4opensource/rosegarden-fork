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
#define RG_NO_DEBUG_PRINT 1

#include "MatrixElement.h"
#include "MatrixScene.h"
#include "MatrixWidget.h"

#include "misc/Debug.h"
#include "base/RulerScale.h"
#include "misc/ConfigGroups.h"

#include <QGraphicsRectItem>
#include <QGraphicsPolygonItem>
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
    m_pitchOffset(pitchOffset),
    m_pitch(255),
    m_segment(segment)
{
    RG_DEBUG << "MatrixElement()";

#if 1   // t4osDEBUG
 // long pitch = 60;
    event->get<Int>(BaseProperties::PITCH, m_pitch);
    RG_WARNING << "MatrixElement"
                << this
                << m_pitch
                << event->getAbsoluteTime()
                << event->getDuration();
#endif

    if (segment && scene && segment != scene->getCurrentSegment()) {
        m_current = false;
    }
    reconfigure();
}

MatrixElement::~MatrixElement()
{
    m_scene->graphicsRectPool.putBack(m_noteItem);
    m_scene->graphicsPolyPool.putBack(m_drumItem);
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
MatrixElement::reconfigure(timeT time, timeT duration, int pitch, bool setPen)
{
    long velocity = 100;
    event()->get<Int>(BaseProperties::VELOCITY, velocity);

    reconfigure(time, duration, pitch, velocity, setPen);
}

void
MatrixElement::reconfigure(timeT time, timeT duration, int pitch, int velocity,
                           bool setPen)
{
    RG_DEBUG << "reconfigure" << time << duration <<
        pitch << velocity << m_current;

#if 0   // t4osDEBUG
    RG_WARNING << "reconfigure()"   // t4osDEBUG
               << this
               << m_segment->getLabel()
               << m_pitch
               << "->"
               << pitch
               << "@"
               << time
               << "+"
               << duration
               << "v"
               << velocity;
    m_pitch = pitch;
#endif

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

    QColor colour = noteColor();

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
    // on various modes and settings. Is cheaper to keep both around
    // with inactive one set to hide() vs dynamically creating/deleting
    // and/or adding/removing from scene.
    /*double*/ qreal fres(resolution + 1);
    if (m_drumDisplay) {
        if (m_noteItem) m_noteItem->hide();
        if (m_drumItem) m_drumItem->show();
        else            m_drumItem = m_scene->graphicsPolyPool.getFrom();
        QPolygonF polygon = drumPolygon(fres, 0.0);
        m_drumItem->setPolygon(polygon);
        if (setPen) {
            m_drumItem->setPen
                (QPen(GUIPalette::getColour(GUIPalette::MatrixElementBorder),
                0));
        }
        m_drumItem->setBrush(QBrush(colour, brushPattern));
        m_drumItem->setPos(x0, pitchy);
        m_drumItem->setZValue(m_current ? ACTIVE_SEGMENT_NOTE_Z
                                        : NORMAL_SEGMENT_NOTE_Z);
        m_drumItem->setData(MatrixElementData,
                            QVariant::fromValue((void *)this));
    } else {
        if (m_drumItem) m_drumItem->hide();
        if (m_noteItem) m_noteItem->show();
        else            m_noteItem = m_scene->graphicsRectPool.getFrom();
        float width = m_width;
        if (width < 1) {
            x0 = std::max(0.0, x1 - 1);
            width = 1;
        }
        QRectF rect(0, 0, width, fres);
        m_noteItem->setRect(rect);
        if (setPen) {
            m_noteItem->setPen
                (QPen(GUIPalette::getColour(GUIPalette::MatrixElementBorder),
                0));
        }
        m_noteItem->setBrush(QBrush(colour, brushPattern));
        m_noteItem->setPos(x0, pitchy);
        m_noteItem->setZValue(m_current ? ACTIVE_SEGMENT_NOTE_Z
                                        : NORMAL_SEGMENT_NOTE_Z);
        m_noteItem->setData(MatrixElementData,
                            QVariant::fromValue((void *)this));
    }

    // Optional note name.
    // But don't show on normal diamond percussion notes
    bool showName = m_scene->getMatrixWidget()->getShowNoteNames() &&
                    !m_drumDisplay;
    if (showName) {
        if (!m_textItem) m_textItem = m_scene->graphicsTextPool.getFrom();

        // Draw text on top of note, see constants in .h file
        m_textItem->setZValue(m_current ? ACTIVE_SEGMENT_TEXT_Z
                                        : NORMAL_SEGMENT_TEXT_Z);
        m_textItem->setBrush(textColor(colour));

        QString noteName = MidiPitchLabel(pitch).getQString();
        m_textItem->setText(noteName);
        QFont font;
        font.setPixelSize(8);
        m_textItem->setFont(font);
        m_textItem->setData(MatrixElementData,
                            QVariant::fromValue((void *)this));
        m_textItem->show();
    }
    else if (m_textItem) {
        m_textItem->hide();
    }

    if (m_textItem && showName) {
        m_textItem->setPos(x0 + 1, pitchy - 1);
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

QPolygonF MatrixElement::drumPolygon(qreal resolution, qreal outline)
{
    QPolygonF polygon;
    polygon << QPointF(-outline, -outline)
            << QPointF(resolution * 0.5 + outline, resolution * 0.5 + outline)
            << QPointF(-outline, resolution + outline)
            << QPointF(-resolution * 0.5 + outline, resolution * 0.5 + outline)
            << QPointF(-outline, -outline);
    return polygon;
}

QAbstractGraphicsShapeItem*
MatrixElement::getActiveItem()
{
    if (m_drumDisplay) {
        return dynamic_cast<QAbstractGraphicsShapeItem *>(m_drumItem);
    }
    else {
        return dynamic_cast<QAbstractGraphicsShapeItem *>(m_noteItem);
    }
}

void
MatrixElement::setSelected(bool selected)
{
    RG_DEBUG << "setSelected" << event()->getAbsoluteTime() << selected;
    if (selected == m_selected) return;

    QAbstractGraphicsShapeItem *item = getActiveItem();
    if (!item) return;

#if 0   // t4osDEBUG
    RG_WARNING << "setSelected()"   // t4osDEBUG
               << m_segment->getLabel()
               << item->pos()
               << m_selected
               << "->"
               << selected;
#endif

    static const qreal OUTLINE = 1.0,
                       DOUBLE = 2.0;

    if (selected) {
        // Outline in default blue for velocity color mode, otherwise
        // choose contrasting color.
        QColor selectionColor;
        if (m_scene->getMatrixWidget()->getNoteColorType() ==
            MatrixWidget::NoteColorType::VELOCITY) {
            selectionColor = GUIPalette::getColour(GUIPalette::
                                                   SelectedElement);
        }
        else {
            selectionColor = QColor::fromHsv(
                                    (item->brush().color().hue() + 180) % 360,
                                    255, 224);
        }

        float res = m_scene->getYResolution() + 1;
        if (item == m_noteItem) {
            QRectF rect = m_noteItem->rect();
         // RG_WARNING << rect;  // t4osDEBUG
            rect.adjust(OUTLINE, OUTLINE, -OUTLINE, -OUTLINE);
            m_noteItem->setRect(rect);
         // RG_WARNING  << rect << '\n';     // t4osDEBUG
        } else {
            QPolygonF polygon = drumPolygon(res, OUTLINE);
            m_drumItem->setPolygon(polygon);
        }

     // pen.setCosmetic(!m_drumDisplay);
        QPen pen(selectionColor, DOUBLE, Qt::SolidLine, Qt::SquareCap,
                 Qt::MiterJoin);
        item->setPen(pen);
    } else {
        if (item == m_noteItem) {
            QRectF rect = m_noteItem->rect();
         // RG_WARNING << rect;  // t4osDEBUG
            rect.adjust(-OUTLINE, -OUTLINE, OUTLINE, OUTLINE);
         // RG_WARNING  << rect << '\n';     // t4osDEBUG
            m_noteItem->setRect(rect);
        }
        item->setPen
            (QPen(GUIPalette::getColour(GUIPalette::MatrixElementBorder), 0));
    }
    m_selected = selected;
}

void
MatrixElement::setCurrent(bool current)
{
    RG_DEBUG << "setCurrent" << event()->getAbsoluteTime() <<
        current << m_current;
    if (m_current == current) return;

    m_current = current;  // must be done before call to noteColor()

    QAbstractGraphicsShapeItem *item = getActiveItem();
    if (!item) return;

    QColor  color = noteColor();

    item->setBrush(color);

    item->setZValue(current ? ACTIVE_SEGMENT_NOTE_Z : NORMAL_SEGMENT_NOTE_Z);

    if (m_textItem) {
        m_textItem->setBrush(textColor(color));
        m_textItem->setZValue(current ? ACTIVE_SEGMENT_TEXT_Z
                                      : NORMAL_SEGMENT_TEXT_Z);
    }

    if (current) {
        item->setPen
            (QPen(GUIPalette::getColour(GUIPalette::MatrixElementBorder), 0));
    } else {
        item->setPen
            (QPen(GUIPalette::getColour(GUIPalette::MatrixElementLightBorder), 0));
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

}  // namespace
