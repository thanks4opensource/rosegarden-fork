// -*- c-basic-offset: 4 -*-

/*
    Rosegarden-4
    A sequencer and musical notation editor.

    This program is Copyright 2000-2004
        Guillaume Laurent   <glaurent@telegraph-road.org>,
        Chris Cannam        <cannam@all-day-breakfast.com>,
        Richard Bown        <bownie@bownie.com>

    The moral right of the authors to claim authorship of this work
    has been asserted.

    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License as
    published by the Free Software Foundation; either version 2 of the
    License, or (at your option) any later version.  See the file
    COPYING included with this distribution for more information.
*/


#include "colours.h"


namespace RosegardenGUIColours
{
    const QColor ActiveRecordTrack = QColor(255, 0, 0);

    const QColor SegmentCanvas = QColor(230, 230, 230);
    const QColor SegmentBorder = QColor(0, 0, 0);
    const QColor RecordingSegmentBlock = QColor(255, 182, 193);
    const QColor RecordingSegmentBorder = QColor(0, 0, 0);

    const QColor RepeatSegmentBorder = QColor(130, 133, 170);

    const QColor SegmentAudioPreview = QColor(39, 71, 22);
    const QColor SegmentInternalPreview = QColor(255, 255, 255);
    const QColor SegmentLabel = QColor(0, 0, 0);
    const QColor SegmentSplitLine = QColor(0, 0, 0);

    const QColor MatrixElementBorder = QColor(0, 0, 0);
    const QColor MatrixElementBlock = QColor(98, 128, 232);

    const QColor LoopRulerBackground = QColor(120, 120, 120);
    const QColor LoopRulerForeground = QColor(255, 255, 255);
    const QColor LoopHighlight = QColor(255, 255, 255);

    const QColor TempoBase = QColor(197, 211, 125);

    //const QColor TextRulerBackground = QColor(60, 205, 230, QColor::Hsv);
//    const QColor TextRulerBackground = QColor(120, 90, 238, QColor::Hsv);
//    const QColor TextRulerBackground = QColor(210, 220, 140);
    const QColor TextRulerBackground = QColor(226, 232, 187);
    const QColor TextRulerForeground = QColor(255, 255, 255);

    const QColor ChordNameRulerBackground = QColor(230, 230, 230);
    const QColor ChordNameRulerForeground = QColor(0, 0, 0);

    const QColor RawNoteRulerBackground = QColor(240, 240, 240);
    const QColor RawNoteRulerForeground = QColor(0, 0, 0);

    const QColor LevelMeterGreen = QColor(0, 200, 0);
    const QColor LevelMeterOrange = QColor(255, 165, 0);
    const QColor LevelMeterRed = QColor(200, 0, 0);

    const QColor LevelMeterSolidGreen = QColor(0, 140, 0);
    const QColor LevelMeterSolidOrange = QColor(220, 120, 0);
    const QColor LevelMeterSolidRed = QColor(255, 50, 50);

    const QColor BarLine = QColor(0, 0, 0);
    const QColor BarLineIncorrect = QColor(211, 0, 31);
    const QColor BeatLine = QColor(100, 100, 100);
    const QColor SubBeatLine = QColor(212, 212, 212);
    const QColor StaffConnectingLine = QColor(192, 192, 192);
    const QColor StaffConnectingTerminatingLine = QColor(128, 128, 128);

    const QColor Pointer = QColor(0, 0, 128);
    const QColor PointerRuler = QColor(100, 100, 100);

    const QColor InsertCursor = QColor(160, 104, 186);
    const QColor InsertCursorRuler = QColor(160, 136, 170);

    const QColor MovementGuide = QColor(172, 230, 139);
    //const QColor MovementGuide = QColor(62, 161, 194);
    //const QColor MovementGuide = QColor(255, 189, 89);
    const QColor SelectionRectangle = QColor(103, 128, 211);
    const QColor SelectedElement = QColor(0, 54, 232);

    const int SelectedElementHue = 225;
    const int SelectedElementMinValue = 220;
    const int HighlightedElementHue = 25;
    const int HighlightedElementMinValue = 220;
    const int QuantizedNoteHue = 69;
    const int QuantizedNoteMinValue = 140;
    const int TriggerNoteHue = 4;
    const int TriggerNoteMinValue = 140;

    const QColor TextAnnotationBackground = QColor(255, 255, 180);

    const QColor AudioCountdownBackground = QColor(128, 128, 128);
    const QColor AudioCountdownForeground = QColor(255, 0, 0);

    const QColor RotaryFloatBackground = QColor(0, 255, 255);
    const QColor RotaryFloatForeground = QColor(0, 0, 0);

    const QColor RotaryPastelBlue = QColor(205, 212, 255);
    const QColor RotaryPastelRed = QColor(255, 168, 169);
    const QColor RotaryPastelGreen = QColor(231, 255, 223);
    const QColor RotaryPastelOrange = QColor(255, 233, 208);
    const QColor RotaryPastelYellow = QColor(249, 255, 208);

    const QColor MatrixKeyboardFocus = QColor(224, 112, 8);

//    const QColor RotaryPlugin = QColor(185, 255, 248);
    const QColor RotaryPlugin = QColor(185, 200, 248);
//    const QColor RotaryPlugin = QColor(185, 185, 185);

    const QColor RotaryMeter = QColor(255, 100, 0);

    const QColor MarkerBackground = QColor(185, 255, 248);

//    const QColor MuteTrackLED = QColor(218, 190, 230, QColor::Hsv);
    const QColor MuteTrackLED = QColor(211, 194, 238, QColor::Hsv);
    const QColor RecordTrackLED = QColor(0, 250, 225, QColor::Hsv);

    const QColor PlaybackFaderOutline = QColor(211, 194, 238, QColor::Hsv);
    const QColor RecordFaderOutline = QColor(0, 250, 225, QColor::Hsv);

Rosegarden::Colour
convertColour (const QColor &input)
{
	int r,g,b;
	input.rgb(&r, &g, &b);
	return Rosegarden::Colour(r,g,b);
}

QColor
convertColour(const Rosegarden::Colour& input)
{
	return QColor(input.getRed(), input.getGreen(), input.getBlue());
}

}


