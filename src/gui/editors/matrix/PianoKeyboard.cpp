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

#define RG_MODULE_STRING "[PianoKeyboard]"

#include "PianoKeyboard.h"
#include "misc/Debug.h"

#include "gui/general/GUIPalette.h"
#include "gui/general/MidiPitchLabel.h"
#include "gui/rulers/PitchRuler.h"

#include <QColor>
#include <QCursor>
#include <QEvent>
#include <QFont>
#include <QPainter>
#include <QSize>
#include <QWidget>
#include <QMouseEvent>


namespace Rosegarden
{

const int   PianoKeyboard::WHITE_KEY_WIDTH         = 48;
const float PianoKeyboard::BLACK_KEY_WIDTH         = 32.0;

const std::array<PianoKeyboard::KeyColor, 12>
PianoKeyboard::KeyColors = {PianoKeyboard::KeyColor::WHITE,  //  0  C
                            PianoKeyboard::KeyColor::BLACK,  //  1  C#/Db
                            PianoKeyboard::KeyColor::WHITE,  //  2  D
                            PianoKeyboard::KeyColor::BLACK,  //  3  D#/Eb
                            PianoKeyboard::KeyColor::WHITE,  //  4  E
                            PianoKeyboard::KeyColor::WHITE,  //  5  F
                            PianoKeyboard::KeyColor::BLACK,  //  6  F#/Gb
                            PianoKeyboard::KeyColor::WHITE,  //  7  G
                            PianoKeyboard::KeyColor::BLACK,  //  8  G#/Ab
                            PianoKeyboard::KeyColor::WHITE,  //  9  A
                            PianoKeyboard::KeyColor::BLACK,  // 10  A#/Bb
                            PianoKeyboard::KeyColor::WHITE}; // 11  B

const std::array<bool, 12>
PianoKeyboard::KeyGaps =    {true,    //  0  C
                             true,    //  1  C#/Db
                             false,   //  2  D
                             true,    //  3  D#/Eb
                             false,   //  4  E
                             true,    //  5  F
                             true,    //  6  F#/Gb
                             false,   //  7  G
                             true,    //  8  G#/Ab
                             false,   //  9  A
                             true,    // 10  A#/Bb
                             false};  // 11  B
const float PianoKeyboard::BLACK_KEY_HEIGHT_FACTOR =  0.667;
const float PianoKeyboard::INITIAL_Y = 128.0;
const std::array<float, 12>
PianoKeyboard::KeyHeights =  {1.5,   //  0  C
                              0.0,   //  1  C#/Db
                              2.0,   //  2  D
                              0.0,   //  3  D#/Eb
                              1.5,    //  4  E
                              1.5,    //  5  F
                              0.0,    //  6  F#/Gb
                              2.0,   //  7  G
                              0.0,   //  8  G#/Ab
                              2.0,   //  9  A
                              0.0,   // 10  A#/Bb
                              1.5};  // 11  B

PianoKeyboard::PianoKeyboard(QWidget *parent, int pitchY, int keys)
        : PitchRuler(parent),
        m_pitchY(pitchY + 1),
        m_keySize(48, m_pitchY * 2),
        m_nbKeys(keys),
        m_mouseDown(false),
        m_highlight(new QWidget(this)),
        m_lastHighlightPitch(-1),
        m_lastKeyPressed(0)
{
    m_highlight->hide();
    QPalette highlightPalette = m_highlight->palette();
    highlightPalette.setColor(QPalette::Window, GUIPalette::getColour(GUIPalette::MatrixKeyboardFocus));
    m_highlight->setPalette(highlightPalette);
    m_highlight->setAutoFillBackground(true);

    computeKeyPos();
    setMouseTracking(true);
}

QSize PianoKeyboard::sizeHint() const
{
    return QSize(m_keySize.width(),
                 m_keySize.height() * m_nbKeys);
}

QSize PianoKeyboard::minimumSizeHint() const
{
    return m_keySize;
}

void PianoKeyboard::computeKeyPos()
{
    float spacing = m_pitchY;
    float y                  = INITIAL_Y * spacing + 1.0,
          blackKeyHeight     = BLACK_KEY_HEIGHT_FACTOR * spacing,
          blackKeyHalfHeight = 0.5 * blackKeyHeight;

    for (unsigned ndx = 0 ; ndx < 128 ; ++ndx) {
        KeyGeom geom;
        unsigned note = ndx % 12;

		y -= KeyHeights[note] * spacing;

        if (KeyColors[note] == KeyColor::WHITE) 
			geom.line = QLineF(0.0, y, WHITE_KEY_WIDTH, y);
		else
            geom.rect = QRectF(0.0,
                               y - blackKeyHalfHeight,
                               BLACK_KEY_WIDTH,
                               blackKeyHeight);

        m_keyGeoms.push_back(geom);
    }
}

void PianoKeyboard::paintEvent(QPaintEvent*)
{
    static QFont *pFont = nullptr;
    if (!pFont) {
        pFont = new QFont();
        pFont->setPixelSize(m_pitchY * 7 / 8);
    }

    QPainter paint(this);

    // Fill the keyboard with ivory.
    paint.fillRect(rect(), QColor(255, 255, 240));

    // White keys
    paint.setPen(Qt::black);
	for (unsigned pitch = 0 ; pitch < 128 ; ++pitch)
        if (KeyColors[pitch % 12] == KeyColor::WHITE)
            paint.drawLine(m_keyGeoms[pitch].line);

	// White key labels: MIDI octave number, on "C" keys
    paint.setFont(*pFont);
	unsigned textXPos    = BLACK_KEY_WIDTH * 17 / 16,
			 textYOffset = m_pitchY 	   *  9 /  8;
	for (unsigned pitch = 0 ; pitch < 128 ; ++pitch)
		if (pitch % 12 == 0) {
	        MidiPitchLabel label(pitch);
	        paint.drawText(textXPos,
						   m_keyGeoms[pitch].line.y1() + textYOffset,
    	                   label.getQString());
		}

    // Black keys
    paint.setBrush(Qt::black);
    for (unsigned pitch = 0 ; pitch < 128 ; ++pitch)
        if (KeyColors[pitch % 12] == KeyColor::BLACK)
            paint.drawRect(m_keyGeoms[pitch].rect);
}

#if (QT_VERSION >= QT_VERSION_CHECK(6, 0, 0))
void PianoKeyboard::enterEvent(QEnterEvent *)
#else
void PianoKeyboard::enterEvent(QEvent *)
#endif
{
    //showHighlight(e->y());
}

void PianoKeyboard::leaveEvent(QEvent*)
{
    hideHighlight();

    int pos = mapFromGlobal( cursor().pos() ).x();
    if ( pos > m_keySize.width() - 5 || pos < 0 ) { // bit of a hack
        emit keyReleased(m_lastKeyPressed, false);
    }
}

void PianoKeyboard::showHighlight(int evPitch)
{
    if (m_lastHighlightPitch != evPitch) {
        m_lastHighlightPitch = evPitch;

		if (KeyColors[evPitch % 12] == KeyColor::WHITE)
			m_highlight->setFixedSize(WHITE_KEY_WIDTH - 8, 4);
		else
			m_highlight->setFixedSize(BLACK_KEY_WIDTH - 8, 4);

		m_highlight->move(0, (127.5  - evPitch) * m_pitchY - 2);
		m_highlight->show();
	}
}

void PianoKeyboard::hideHighlight()
{
    m_highlight->hide();
    m_lastHighlightPitch = -1;
}

void PianoKeyboard::mouseMoveEvent(QMouseEvent* e)
{
    // The routine to work out where this should appear doesn't coincide with the note
    // that we send to the sequencer - hence this is a bit pointless and crap at the moment.
    // My own fault it's so crap but there you go.
    //
    // RWB (20040220)
    //
    if (e->buttons() & Qt::LeftButton) {
        if (m_selecting)
            emit keySelected(e->pos().y(), true);
        else
            emit keyPressed(e->pos().y(), true); // we're swooshing

        emit keyReleased(m_lastKeyPressed, true);
        m_lastKeyPressed = e->pos().y();
    } else
        emit hoveredOverKeyChanged(e->pos().y());
}

void PianoKeyboard::mousePressEvent(QMouseEvent *e)
{
    if (e->button() == Qt::LeftButton) {
        m_mouseDown = true;
        m_selecting = (e->modifiers() & Qt::ShiftModifier);
        m_lastKeyPressed = e->pos().y();

        if (m_selecting)
            emit keySelected(e->pos().y(), false);
        else
            emit keyPressed(e->pos().y(), false);
    }
}

void PianoKeyboard::mouseReleaseEvent(QMouseEvent *e)
{
    if (e->button() == Qt::LeftButton) {
        m_mouseDown = false;
        m_selecting = false;
        emit keyReleased(e->pos().y(), false);
    }
}

}
