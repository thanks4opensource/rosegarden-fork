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

#ifndef RG_PIANOKEYBOARD_H
#define RG_PIANOKEYBOARD_H

#include "gui/rulers/PitchRuler.h"

#include <QSize>

#include <array>
#include <vector>


class QWidget;
class QPaintEvent;
class QMouseEvent;
class QEvent;


namespace Rosegarden
{


class PianoKeyboard : public PitchRuler
{
    Q_OBJECT
public:
    PianoKeyboard(QWidget *parent, int pitchY, int keys = 88);

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

    /// Draw a highlight to indicate the pitch that the mouse is hovering over.
    /**
     * For the PianoKeyboard, this is a small, usually orange rectangle over
     * the key.
     */
    void showHighlight(int evPitch) override;
    void hideHighlight() override;

protected:
    static const int   WHITE_KEY_WIDTH;
    static const float BLACK_KEY_WIDTH,
                       BLACK_KEY_HEIGHT_FACTOR,
                       INITIAL_Y;

    enum class KeyColor {WHITE, BLACK};
    static const std::array<KeyColor, 12> KeyColors;
    static const std::array<bool    , 12> KeyGaps;
    static const std::array<float   , 12> KeyHeights;

    void paintEvent(QPaintEvent*) override;

    void mouseMoveEvent(QMouseEvent*) override;
    void mousePressEvent(QMouseEvent*) override;
    void mouseReleaseEvent(QMouseEvent*) override;
#if (QT_VERSION >= QT_VERSION_CHECK(6, 0, 0))
    void enterEvent(QEnterEvent *) override;
#else
    void enterEvent(QEvent *) override;
#endif
    void leaveEvent(QEvent *) override;

    // compute all key positions and store them
    //
    void computeKeyPos();

    //--------------- Data members ---------------------------------
    int m_pitchY;
    QSize m_keySize;
    QSize m_blackKeySize;
    unsigned int m_nbKeys;

    struct KeyGeom {  // Can't be a union because non-default constructors
        QLineF line;
        QRectF rect;
    } ;
    std::vector<KeyGeom> m_keyGeoms;

    std::vector<unsigned int> m_labelKeyPos;

    bool                      m_mouseDown;
    bool                      m_selecting;

    // highlight element on the keyboard
    QWidget                  *m_highlight;
    int                       m_lastHighlightPitch;
    int                       m_lastKeyPressed;
};


}

#endif
