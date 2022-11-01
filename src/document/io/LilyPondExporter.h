/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*- vi:set ts=8 sts=4 sw=4: */

/*
    Rosegarden
    A MIDI and audio sequencer and musical notation editor.
    Copyright 2000-2022 the Rosegarden development team.

    This file is Copyright 2002
        Hans Kieserman      <hkieserman@mail.com>
    with heavy lifting from csoundio as it was on 13/5/2002.

    Numerous additions and bug fixes by
        Michael McIntyre    <dmmcintyr@users.sourceforge.net>

    Some restructuring by Chris Cannam.

    Brain surgery to support LilyPond 2.x export by Heikki Junes.

    Other copyrights also apply to some parts of this work.  Please
    see the AUTHORS file and individual file headers for details.

    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License as
    published by the Free Software Foundation; either version 2 of the
    License, or (at your option) any later version.  See the file
    COPYING included with this distribution for more information.
*/

#ifndef RG_LILYPONDEXPORTER_H
#define RG_LILYPONDEXPORTER_H

#include "base/Event.h"
#include "base/PropertyName.h"
#include "base/Segment.h"
#include "base/Selection.h"
#include "document/io/LilyPondLanguage.h"
#include "gui/editors/notation/NotationView.h"
#include <fstream>
#include <set>
#include <string>
#include <utility>

#include <QCoreApplication>
#include <QPointer>

class QObject;
class QProgressDialog;
class QString;


namespace Rosegarden
{

// This is now in the Rosegarden namespace so it can be used
// in LilyPondOptionsDialog with the LilyVersionAwareCheckBox.
enum {
  LILYPOND_VERSION_2_6,
  LILYPOND_VERSION_2_8,
  LILYPOND_VERSION_2_10,
  LILYPOND_VERSION_2_12,
  LILYPOND_VERSION_2_14
};

class TimeSignature;
class Studio;
class RosegardenMainWindow;
class RosegardenMainViewWidget;
class RosegardenDocument;
class NotationView;
class Key;
class Composition;

const char* headerDedication();
const char* headerTitle();
const char* headerSubtitle();
const char* headerSubsubtitle();
const char* headerPoet();
const char* headerComposer();
const char* headerMeter();
const char* headerOpus();
const char* headerArranger();
const char* headerInstrument();
const char* headerPiece();
const char* headerCopyright();
const char* headerTagline();

/**
 * LilyPond scorefile export
 */

class ROSEGARDENPRIVATE_EXPORT LilyPondExporter
{
    Q_DECLARE_TR_FUNCTIONS(Rosegarden::LilyPondExporter)

public:
    typedef EventContainer eventstartlist;
    typedef std::multiset<Event*, Event::EventEndCmp> eventendlist;

public:
    LilyPondExporter(RosegardenDocument *doc,
                     const SegmentSelection &selection,
                     const std::string &fileName,
                     NotationView *parent = nullptr);
    ~LilyPondExporter();

   /**
    * Write the LilyPond score into the specified file.
    * @return true if successfull, false otherwise.
    */
    bool write();

   /**
    * @return an explanatory message on what goes wrong on the last
    * call to write().
    */
    QString getMessage() const { return m_warningMessage; }

    void setProgressDialog(QPointer<QProgressDialog> progressDialog)
            { m_progressDialog = progressDialog; }

private:
    NotationView *m_notationView;
    RosegardenDocument *m_doc;
    Composition *m_composition;
    Studio *m_studio;
    std::string m_fileName;
    Clef m_lastClefFound;
    LilyPondLanguage *m_language;
    SegmentSelection m_selection;

    void readConfigVariables();

    Event *nextNoteInGroup(Segment *s, Segment::iterator it, const std::string &groupType, int barEnd) const;

    // Return true if the given segment has to be print
    // (readConfigVAriables() should have been called before)
    bool isSegmentToPrint(Segment *seg);

    void writeBar(Segment *, int barNo, timeT barStart, timeT barEnd, int col,
                  Rosegarden::Key &key, std::string &lilyText,
                  std::string &prevStyle,
                  eventendlist &preEventsInProgress, eventendlist &postEventsInProgress,
                  std::ofstream &str, int &MultiMeasureRestCount,
                  bool &nextBarIsAlt1, bool &nextBarIsAlt2,
                  bool &nextBarIsDouble, bool &nextBarIsEnd,
                  bool &nextBarIsDot, bool noTimeSignature);

    timeT calculateDuration(Segment *s,
                                        const Segment::iterator &i,
                                        timeT barEnd,
                                        timeT &soundingDuration,
                                        const std::pair<int, int> &tupletRatio,
                                        bool &overlong);

    void handleStartingPreEvents(eventstartlist &preEventsToStart,
                                 const Segment *seg,
                                 const Segment::iterator &j,
                                 std::ofstream &str);
    void handleEndingPreEvents(eventendlist &preEventsInProgress,
                               const Segment::iterator &j,
                               std::ofstream &str);
    void handleStartingPostEvents(eventstartlist &postEventsToStart,
                                  const Segment *seg,
                                  const Segment::iterator &j,
                                  std::ofstream &str);
    void handleEndingPostEvents(eventendlist &postEventsInProgress,
                                const Segment *seg,
                                const Segment::iterator &j,
                                std::ofstream &str);

    // convert note pitch into LilyPond format note name string
    std::string convertPitchToLilyNoteName(int pitch,
                                           Accidental accidental,
                                           const Rosegarden::Key &key);

    // convert note pitch into LilyPond format note name string with octavation
    std::string convertPitchToLilyNote(int pitch,
                                       Accidental accidental,
                                       const Rosegarden::Key &key);

    // compose an appropriate LilyPond representation for various Marks
    static std::string composeLilyMark(std::string eventMark, bool stemUp);

    // find/protect illegal characters in user-supplied strings
    static std::string protectIllegalChars(std::string inStr);

    // return a string full of column tabs
    static std::string indent(const int &column);

    // write a time signature
    static void writeTimeSignature(TimeSignature timeSignature, int col, std::ofstream &str);

    std::pair<int,int> writeSkip(const TimeSignature &timeSig,
				 timeT offset,
				 timeT duration,
				 bool useRests,
				 std::ofstream &);

    /*
     * Handle LilyPond directive.  Returns true if the event was a directive,
     * so subsequent code does not bother to process the event twice
     */
    static bool handleDirective(const Event *textEvent,
                                std::string &lilyText,
                                bool &nextBarIsAlt1, bool &nextBarIsAlt2,
                                bool &nextBarIsDouble, bool &nextBarIsEnd, bool &nextBarIsDot);

    void handleText(const Event *, std::string &lilyText) const;
    static void handleGuitarChord(Segment::iterator i, std::ofstream &str);
    void writePitch(const Event *note, const Rosegarden::Key &key, std::ofstream &);
    void writeStyle(const Event *note, std::string &prevStyle, int col, std::ofstream &, bool isInChord);
    std::pair<int,int> writeDuration(timeT duration, std::ofstream &);
    void writeSlashes(const Event *note, std::ofstream &);

private:
    static const int MAX_DOTS = 4;
    const PropertyName SKIP_PROPERTY;

    unsigned int m_paperSize;
    static const unsigned int PAPER_A3      = 0;
    static const unsigned int PAPER_A4      = 1;
    static const unsigned int PAPER_A5      = 2;
    static const unsigned int PAPER_A6      = 3;
    static const unsigned int PAPER_LEGAL   = 4;
    static const unsigned int PAPER_LETTER  = 5;
    static const unsigned int PAPER_TABLOID = 6;
    static const unsigned int PAPER_NONE    = 7;

    bool m_paperLandscape;
    unsigned int m_fontSize;

    /** Combo box index starts at 0, but our minimum font size is 6, so we add 6
     * to the index to arrive at the real font size for export
     */
    static const unsigned int FONT_OFFSET = 6;
    static const unsigned int FONT_20 = 20 + FONT_OFFSET;

    unsigned int m_exportLyrics;
    static const unsigned int EXPORT_NO_LYRICS = 0;
    static const unsigned int EXPORT_LYRICS_LEFT = 1;
    static const unsigned int EXPORT_LYRICS_CENTER = 2;
    static const unsigned int EXPORT_LYRICS_RIGHT = 3;

    unsigned int m_exportTempoMarks;
    static const unsigned int EXPORT_NONE_TEMPO_MARKS = 0;
    static const unsigned int EXPORT_FIRST_TEMPO_MARK = 1;
    static const unsigned int EXPORT_ALL_TEMPO_MARKS = 2;

    unsigned int m_exportSelection;
    static const unsigned int EXPORT_ALL_TRACKS = 0;
    static const unsigned int EXPORT_NONMUTED_TRACKS = 1;
    static const unsigned int EXPORT_SELECTED_TRACK = 2;
    static const unsigned int EXPORT_SELECTED_SEGMENTS = 3;
    static const unsigned int EXPORT_EDITED_SEGMENTS = 4;

    bool m_exportBeams;
    bool m_exportStaffGroup;

    bool m_raggedBottom;
    bool m_exportEmptyStaves;
    bool m_useShortNames;

    unsigned int m_exportMarkerMode;

    bool m_chordNamesMode;

    enum {
        EXPORT_NO_MARKERS,
        EXPORT_DEFAULT_MARKERS,
        EXPORT_TEXT_MARKERS,
    };


    unsigned int m_exportNoteLanguage;

    int m_languageLevel;
//  There is now an enum global within the Rosegarden namespace (declared
//  at the beggining of this file) that contains values such as:
//     enum {
//         LILYPOND_VERSION_2_6,
//         LILYPOND_VERSION_2_8,
//         ...
//     };
// These are the values used with m_languageLevel.

    int m_repeatMode;
    enum {
        REPEAT_BASIC,
        REPEAT_VOLTA,
        REPEAT_UNFOLD,
    };

    bool m_voltaBar;
    bool m_cancelAccidentals;
    bool m_fingeringsInStaff;
    QString m_warningMessage;

    QPointer<QProgressDialog> m_progressDialog;

    std::pair<int,int> fractionSum(std::pair<int,int> x,std::pair<int,int> y) {
	std::pair<int,int> z(
	    x.first * y.second + x.second * y.first,
	    x.second * y.second);
	return fractionSimplify(z);
    }
    std::pair<int,int> fractionProduct(std::pair<int,int> x,std::pair<int,int> y) {
	std::pair<int,int> z(
	    x.first * y.first,
	    x.second * y.second);
	return fractionSimplify(z);
    }
    std::pair<int,int> fractionProduct(std::pair<int,int> x,int y) {
	std::pair<int,int> z(
	    x.first * y,
	    x.second);
	return fractionSimplify(z);
    }
    static bool fractionSmaller(std::pair<int,int> x,std::pair<int,int> y) {
	return (x.first * y.second < x.second * y.first);
    }
    std::pair<int,int> fractionSimplify(std::pair<int,int> x) {
	return std::pair<int,int>(x.first/gcd(x.first,x.second),
				  x.second/gcd(x.first,x.second));
    }
    static int gcd(int a, int b) {
	// Euclid's algorithm to find the greatest common divisor
	while ( b != 0) {
	    int t = b;
            b = a % b;
            a = t;
	}
        return a;
    }
};


}

#endif
