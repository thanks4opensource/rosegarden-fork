
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

#ifndef RG_MIDIPITCHLABEL_H
#define RG_MIDIPITCHLABEL_H

#include <string>
#include <QString>

#include <QCoreApplication>


namespace Rosegarden
{



class MidiPitchLabel
{
    Q_DECLARE_TR_FUNCTIONS(Rosegarden::MidiPitchLabel)

public:
    MidiPitchLabel(int pitch, const char *separator = " ", bool sharps = true);

    std::string getString() const { return
                                    std::string(m_midiNote.toUtf8().data()); }
    QString getQString() const {return m_midiNote; }

    static QString scaleDegreeMajor   (int pitch, bool isSharpKey);
    static QString scaleDegreeMinorAlt(int pitch, bool isSharpKey);

    static QString fixedSolfege  (int pitch, bool isSharpKey);
    static QString movableSolfege(int pitch, bool isSharpKey);



private:
    QString m_midiNote;
    QString m_scaleDegree;
};



}

#endif
