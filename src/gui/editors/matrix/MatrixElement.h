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

#ifndef RG_MATRIXELEMENT_H
#define RG_MATRIXELEMENT_H

#include <QPen>  // for inline outlinePen()

#include "base/ViewElement.h"

class QAbstractGraphicsShapeItem;
class QBrush;
class QColor;
class QGraphicsEllipseItem;
class QGraphicsItem;
class QGraphicsRectItem;

#include <set>


namespace Rosegarden
{

class MatrixScene;
class Event;
class Segment;
class IsotropicRectItem;
class IsotropicTextItem;
class IsotropicDiamondItem;
class ChordNameRuler;

/// A note on the matrix editor.
/**
 * This is more of a "MatrixNoteView" in pattern-speak or a "MatrixNoteItem"
 * in Qt-speak.
 *
 * m_item is the QGraphicsRectItem (or QGraphicsPolygonItem) that is
 * displayed for a note on the matrix.  reconfigure() creates m_item and
 * adds it to the scene.  m_item is owned by this class.
 *
 * MatrixElements (and ViewElements in general) are stored in
 * ViewSegment::m_viewElementList.  They are created in
 * MatrixViewSegment::makeViewElement().
 *
 * See MatrixViewSegment.
 */
class MatrixElement : public ViewElement
{
public:
    MatrixElement(MatrixScene *scene,
                  Event *event,
                  bool drum,
                  long pitchOffset,
                  const Segment *segment);
    ~MatrixElement() override;

    // Workaround because can't statically init QPixmaps before
    // Qt is up and running.
    static void initPixmaps();

    /// Returns true if the wrapped event is a note
    bool isNote() const;

    // return at least 6.0 for width, addresses #1502, avoids drawing velocity
    // bars that are too small to see and manipulate for extremely short notes
    // (we have encountered MIDI gear in the field that generates durations as
    // short as 0, and durations less than about 60 generate too narrow a width
    // for human manipulation)
    double getWidth() const { return (m_width >= 6.0f ? m_width : 6.0f); }
    // For e.g. MatrixTool::moveResizeVelocity() which need accurate
    // value at large horizontal expansion ratios.
    double getAccurateWidth() const { return m_width; }

    double getElementVelocity() { return m_velocity; }

    void setSelected(bool selected, bool force = false);

    void setCurrent(bool current);

    /// Adjust the item to reflect the values of our event
    void reconfigure();

    /// Adjust the item to reflect the given values, not those of our event
    void reconfigure(int velocity);

    /// Adjust the item to reflect the given values, not those of our event
    void reconfigure(timeT time, timeT duration);

    /// Adjust the item to reflect the given values, not those of our event
    void reconfigure(timeT time, timeT duration, int pitch);

    // See comment at m_segment
    const Segment *getSegment() const  { return m_segment; }

    MatrixScene *getScene() { return m_scene; }

    static MatrixElement *getMatrixElement(QGraphicsItem *);

    void setColor();

    void setDrumMode(bool isDrum) { m_drumMode = isDrum;}

    bool isSelected() const { return m_selected; }

    // Z values for occlusion/layering of object in graph display.
    // Diffence between NORMAL_ and ACTIVE_  needed when notes from
    // different segments overlay each other at same pitch and time
    // to ensure that mouse click will get active segment's note
    // regardless if clicked on note or text (latter in case
    // "View -> Show note names" is in effect).
    static constexpr float SELECTED_SEGMENT_TEXT_Z =   6.0,
                           SELECTED_SEGMENT_NOTE_Z =   5.0,
                           SELECTION_BORDER_Z      =   4.0,
                           ACTIVE_SEGMENT_TEXT_Z   =   3.0,
                           ACTIVE_SEGMENT_NOTE_Z   =   2.0,
                           NORMAL_SEGMENT_TEXT_Z   =   1.0,
                           NORMAL_SEGMENT_NOTE_Z   =   0.0,
                           VERTICAL_BAR_LINE_Z     =  -8.0,
                           HORIZONTAL_LINE_Z       =  -9.0,
                           VERTICAL_BEAT_LINE_Z    = -10.0,
                           HIGHLIGHT_Z             = -11.0;


protected:
    // Current vs m_prevShowName decison if expensive getKeyInfo() is necessary
    enum class ShowName {
        SKIP  = 0,  // 0 vs non-zero in case have to cast to bool
        SHOW  = 1,  // "     "   "   "   "    " "  "  "   "   "
        UNSET = 2,  // "     "   "   "   "    " "  "  "   "   "
    };

    static const unsigned GRAY_RED_COMPONENT   = 200,
                          GRAY_GREEN_COMPONENT = 200,
                          GRAY_BLUE_COMPONENT  = 200;

    static const unsigned DRUM_SELECT_BORDER_WIDTH = 4;

    MatrixScene *m_scene;
    Event* const m_event;
    bool m_drumMode;
    bool m_drumDisplay;
    bool m_current;
    bool m_selected;
    ShowName m_prevShowName;
    timeT m_prevTime;
    unsigned m_tonic;
    unsigned m_degree;
    bool m_sharps;
    bool m_minor;
    bool m_offsetMinors;
    bool m_alternateMinorNotes;
    QGraphicsRectItem *m_noteItem;
    IsotropicDiamondItem *m_drumItem;
    IsotropicTextItem *m_textItem;
    IsotropicRectItem *m_noteSelectItem;
    IsotropicDiamondItem *m_drumSelectItem;
    QGraphicsEllipseItem *m_tiedNoteItem;

    std::set<QGraphicsRectItem*> m_onceHadToolTips;

    double m_width;
    double m_velocity;

    /** Events don't know anything about what segment owns them, so neither do
     * MatrixElements.  In order to handle transposing segments properly, we
     * have to adjust the pitch relative to the segment transpose, and this can
     * only be known from the outside.  We have to take it as a construction
     * parameter, and use it appropriately when doing height calculations
     */
    long m_pitchOffset;

    // Bug #1624: Allows MatrixPainter::handleLeftButtonPress() and
    // other MatrixSomeClass::handleSomeButtonSomeAction() methods to tell
    // if existing note at same pitch/time as new one being created is
    // in current active segment or not. Needed because Events lack
    // back-pointers to their parent Segment, and constructor is called
    // from generic signal-handling function with a base Event (not
    // a MatrixMouseEvent) so Segment member must be here instead.
    // Future work: Might also allow elimination of m_pitchOffset, above.
    const Segment *m_segment;


    // Get key info at time, and set m_prevTime and m_prevShowName
    // Checking of latter two in reconfigure(...) avoids expensive
    //   Segment::getKeyAtTime() unless necessary.
    // Returns false if no chord name ruler and time is outside
    //   of MatrixScene::getCurrentSegment() span (so as not to
    //   display any key-relative note names in that case).
    bool getKeyInfo(const ChordNameRuler*,
                    const bool chordNameRulerIsVisible,
                    const ShowName showName,
                    const timeT time,
                    const unsigned pitch = 0);

    // Common code used by reconfigure(timeT, timeT, int, int),
    // setCurrent(), and setColor().
    QColor textColor(const QColor noteColor) const;

    // Common code used by setSelected() and setColor()
    QColor selectionBorderColor(const QAbstractGraphicsShapeItem*) const;

    // Common code used by reconfigure(timeT, timeT, int, int)
    // and setCurrent(bool).
    QBrush noteBrush() const;

    QPen outlinePen() const { return QPen(Qt::black, 0); };

    // Common code used by setSelected() and setCurrent()
    QAbstractGraphicsShapeItem *getActiveItem() const;

private:
    /// Adjust the item to reflect the given values, not those of our event
    void reconfigure(timeT time, timeT duration, int pitch, int velocity);
};


typedef ViewElementList MatrixElementList;


}

#endif
