// -*- c-basic-offset: 4 -*-

/*
    Rosegarden-4 v0.1
    A sequencer and musical notation editor.

    This program is Copyright 2000-2001
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

#ifndef _RG21IO_H_
#define _RG21IO_H_

#include <qstringlist.h>
#include <qfile.h>
#include <qtextstream.h>

#include "NotationTypes.h"

class Rosegarden::Composition;
class Rosegarden::Segment;

/**
 * Rosegarden 2.1 file import
 */
class RG21Loader 
{
public:
    /**
     * Load and parse the RG2.1 file \a fileName
     */
    RG21Loader(const QString& fileName);

    ~RG21Loader();
    
    /**
     * Return the Composition generated by the parsing of the file
     */
    Rosegarden::Composition* getComposition() { return m_composition; }

protected:

    // RG21 note mods
    enum { ModSharp   = (1<<0),
           ModFlat    = (1<<1),
           ModNatural = (1<<2)
    };

    bool parse();
    bool parseClef();
    bool parseKey();
    bool parseChordItem();
    bool parseRest();
    bool parseGroupStart();
    bool parseMarkStart();

    void closeGroup();
    void closeMark();
    void closeSegmentOrComposition();

    void setGroupProperties(Rosegarden::Event *);

    long convertRG21Pitch(long rg21pitch, int nodeModifier);
    Rosegarden::timeT convertRG21Duration(QStringList::Iterator&);

    bool readNextLine();

    QFile m_file;
    QTextStream *m_stream;

    Rosegarden::Composition* m_composition;
    Rosegarden::Segment* m_currentSegment;
    unsigned int m_currentSegmentTime;
    unsigned int m_currentSegmentNb;
    Rosegarden::Clef m_currentClef;
    Rosegarden::Key m_currentKey;

    bool m_inGroup;
    unsigned int m_groupId;
    std::string m_groupType;
    Rosegarden::timeT m_groupStartTime;
    int m_groupTupledLength;
    int m_groupTupledCount;
    int m_groupUntupledLength;

    int m_tieStatus; // 0 -> none, 1 -> tie started, 2 -> seen one note

    QString m_currentLine;
    QString m_currentStaffName;

    QStringList m_tokens;

    unsigned int m_nbStaves;
};


#endif /* _MUSIC_IO_ */
