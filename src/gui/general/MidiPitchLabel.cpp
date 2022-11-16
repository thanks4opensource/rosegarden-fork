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


#include "MidiPitchLabel.h"
#include "misc/ConfigGroups.h"
#include "misc/Preferences.h"

#include <QApplication>
#include <QSettings>
#include <QString>


namespace Rosegarden
{

// Unicode:  flat==u8"\u266d"  sharp== u8"\u266f"

MidiPitchLabel::MidiPitchLabel(int pitch, const char *separator, bool sharps)
{
    // this was refactored to take advantage of these translations being
    // available in other contexts, and to avoid extra work for translators
#define NN(NOTE) QObject::tr(NOTE, "note name")
    static QString sharpNotes[] = {
        NN("C"),            NN(u8"C\u266f"),
        NN("D"),            NN(u8"D\u266f"),
        NN("E"),            NN("F"),
        NN(u8"F\u266f"),    NN("G"),
        NN(u8"G\u266f"),    NN("A"),
        NN(u8"A\u266f"),    NN("B")
    };
    static QString flatNotes[] = {
        NN("C"),            NN(u8"D\u266d"),
        NN("D"),            NN(u8"E\u266d"),
        NN("E"),            NN("F"),
        NN(u8"G\u266d"),    NN("G"),
        NN(u8"A\u266d"),    NN("A"),
        NN(u8"B\u266d"),    NN("B")
    };
#undef NN

    if (pitch < 0 || pitch > 127)
        m_midiNote = "";
    else {
#if 0   // t4os
        // Very expensive per-note in MatrixElement::configure(...)
        QSettings settings;
        settings.beginGroup(GeneralOptionsConfigGroup);
        int baseOctave = settings.value("midipitchoctave", -2).toInt() ;
#elif 0   // t4os -- use even faster way, below
        int baseOctave = Preferences::getPreference(GeneralOptionsConfigGroup,
                                                    "midipitchoctave",
                                                    -2);
#else
        const int baseOctave = Preferences::midiOctaveNumberOffset.get();
#endif

        QString *notes = sharps ? sharpNotes : flatNotes;

        int octave = (int)(((float)pitch) / 12.0) + baseOctave;
        m_midiNote = QString("%1%2%3").arg(notes[pitch % 12])
                                      .arg(separator)
                                      .arg(octave);

#if 0   // t4os
        settings.endGroup();
#endif
    }
}

QString MidiPitchLabel::scaleDegreeMinorAlt(int pitch, bool isSharpKey)
{
#define NN(NOTE) QObject::tr(NOTE, "note name")
    static QString sharpDegrees[] = {
        NN("1"),            NN(u8"\u266f1"),
        NN("2"),            NN("3"),
        NN(u8"\u266f3"),    NN("4"),
        NN(u8"\u266f4"),    NN("5"),
        NN("6"),            NN(u8"\u266f6"),
        NN("7"),            NN(u8"\u266f7")
    };
    static QString flatDegrees[] = {
        NN("1"),            NN(u8"\u266d2"),
        NN("2"),            NN("3"),
        NN(u8"\u266d4"),    NN("4"),
        NN(u8"\u266d5"),    NN("5"),
        NN("6"),            NN(u8"\u266d7"),
        NN("7"),            NN(u8"\u266d1")
    };
#undef NN

    if (isSharpKey) return sharpDegrees[pitch % 12];
    else            return  flatDegrees[pitch % 12];
}

QString MidiPitchLabel::scaleDegreeMajor(int pitch, bool isSharpKey)
{
#define NN(NOTE) QObject::tr(NOTE, "note name")
    static QString sharpDegrees[] = {
        NN("1"),            NN(u8"\u266f1"),
        NN("2"),            NN(u8"\u266f2"),
        NN("3"),            NN("4"),
        NN(u8"\u266f4"),    NN("5"),
        NN(u8"\u266f5"),    NN("6"),
        NN(u8"\u266f6"),    NN("7")
    };
    static QString flatDegrees[] = {
        NN("1"),            NN(u8"\u266d2"),
        NN("2"),            NN(u8"\u266d3"),
        NN("3"),            NN("4"),
        NN(u8"\u266d5"),    NN("5"),
        NN(u8"\u266d6"),    NN("6"),
        NN(u8"\u266d7"),    NN("7")
    };
#undef NN

    if (isSharpKey) return sharpDegrees[pitch % 12];
    else            return  flatDegrees[pitch % 12];

}

QString MidiPitchLabel::fixedSolfege(int pitch, bool isSharpKey)
{
#define NN(NOTE) QObject::tr(NOTE, "note name")
    static QString sharpDegrees[] = {
        NN("Do"),          NN(u8"Do\u266f"),
        NN("Re"),          NN(u8"Re\u266f"),
        NN("Mi"),          NN("Fa"),
        NN(u8"Fa\u266f"),  NN("Sol"),
        NN(u8"Sol\u266f"), NN("La"),
        NN(u8"La\u266f"),  NN("Ti")
    };
    static QString flatDegrees[] = {
        NN("Do"),          NN(u8"Re\u266d"),
        NN("Re"),          NN(u8"Mi\u266d"),
        NN("Mi"),          NN("Fa"),
        NN(u8"Sol\u266d"), NN("Sol"),
        NN(u8"La\u266d"),  NN("La"),
        NN(u8"Ti\u266d"),  NN("Ti")
    };
#undef NN

        const int baseOctave = Preferences::midiOctaveNumberOffset.get();

    QString octave = QString("%1").arg(pitch / 12 + baseOctave);

    if (isSharpKey) return sharpDegrees[pitch % 12] + octave;
    else            return  flatDegrees[pitch % 12] + octave;
}

QString MidiPitchLabel::movableSolfege(int degree, bool isSharpKey)
{
#define NN(NOTE) QObject::tr(NOTE, "note name")
    static QString sharpDegrees[] = {
        NN("Do"), NN("Di"),
        NN("Re"), NN("Ri"),
        NN("Mi"), NN("Fa"),
        NN("Fi"), NN("Sol"),
        NN("Si"), NN("La"),
        NN("Li"), NN("Ti")
    };
    static QString flatDegrees[] = {
        NN("Do"), NN("Ra"),
        NN("Re"), NN("Ma"),
        NN("Mi"), NN("Fa"),
        NN("Se"), NN("Sol"),
        NN("Lo"), NN("La"),
        NN("Ta"), NN("Ti")
    };
#undef NN

    if (isSharpKey) return sharpDegrees[degree % 12];
    else            return  flatDegrees[degree % 12];
}

}
