/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*- vi:set ts=8 sts=4 sw=4: */

/*
    Rosegarden
    A MIDI and audio sequencer and musical notation editor.
    Copyright 2000-2023 the Rosegarden development team.
    Modifications and additions Copyright (c) 2023 Mark R. Rubin aka "thanks4opensource" aka "thanks4opensrc"

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
class Key;

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
                  const Segment *segment,
                  const unsigned segmentIndex);
    ~MatrixElement() override;

    // Workaround because can't statically init QPixmaps before
    // Qt is up and running.
    static void initPixmaps();

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
    void reconfigure(const bool       needKey = true,
                     const Key* const key     = nullptr);

    /// Adjust the item to reflect the given values, not those of our event
    void reconfigure(const int          velocity,
                     const bool         needKey = true,
                     const Key* const   key     = nullptr);

    /// Adjust the item to reflect the given values, not those of our event
    void reconfigure(const timeT        time,
                     const timeT        duration,
                     const bool         needKey = true,
                     const Key* const   key     = nullptr);

    /// Adjust the item to reflect the given values, not those of our event
    void reconfigure(const timeT        time,
                     const timeT        duration,
                     const int          pitch,
                     const bool         needKey = true,
                     const Key* const   key     = nullptr);

    // See comment at m_segment
    const Segment *getSegment() const  { return m_segment; }

    MatrixScene *getScene() { return m_scene; }

    static MatrixElement *getMatrixElement(QGraphicsItem *);

    void setLabel(const bool keyDependent, const Key* const);
    void setColor(const bool keyDependent, const Key* const);

    void setDrumMode(bool isDrum) { m_drumMode = isDrum;}

    bool isSelected() const { return m_selected; }

    timeT getTime() const { return m_time; }

    // Z values for occlusion/layering of object in graph display.
    // Diffence between levels needed when notes from
    // different segments overlay each other at same pitch and time
    // to ensure that mouse click will get active segment's note
    // regardless if clicked on note or text (latter in case
    // "View -> Show note names" is in effect).
static constexpr float     SEGMENT_Z_INCREMENT     =   2.0,
                           LOWEST_SEGMENT_Z        =   0.0,
                           VERTICAL_BAR_LINE_Z     =  -8.0,
                           HORIZONTAL_LINE_Z       =  -9.0,
                           VERTICAL_BEAT_LINE_Z    = -10.0,
                           HIGHLIGHT_Z             = -11.0;


protected:
    float segmentNoteZ(const unsigned segmentIndex) const
    {
        return LOWEST_SEGMENT_Z + SEGMENT_Z_INCREMENT * segmentIndex;
    }
    float segmentTextZ(const unsigned segmentIndex) const
    {
        return segmentNoteZ(segmentIndex) + 1;
    }

    void setLabel(const timeT   time,
                  const int     pitch,
                  const double  x0,
                  const QBrush  &brush);

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
    bool m_showName;
    std::string m_keyName;
    timeT m_time;
    int m_pitch;
    unsigned m_tonic;
    unsigned m_degree;
    bool m_keyRelative;
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

    // For setting Z values of QGraphicsItems.
    const unsigned  m_segmentIndex;
    const float     m_noteZ,
                    m_textZ,
                    m_activeSegmentNoteZ,     // must be in this order to init
                    m_activeSegmentTextZ,     //  "   "  "   "     "   "   "
                    m_selectionBorderZ,       //  "   "  "   "     "   "   "
                    m_selectedSegmentNoteZ,   //  "   "  "   "     "   "   "
                    m_selectedSegmentTextZ;   //  "   "  "   "     "   "   "

    // Get key info at time, and set m_showName, m_time, and m_pitch.
    // Checking of latter two in reconfigure(...) avoids expensive
    //   Segment::getKeyAtTime() unless necessary.
    void getKeyInfo(const Key*          key,
                    const bool          showName,
                    const timeT         time,
                    const int           pitch);

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
    void reconfigure(const timeT        time,
                     const timeT        duration,
                     const int          pitch,
                     const int          velocity,
                     const bool         needKey,
                     const Key* const   key);

};


typedef ViewElementList MatrixElementList;


}

#endif
