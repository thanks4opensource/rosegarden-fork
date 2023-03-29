/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*- vi:set ts=8 sts=4 sw=4: */

/*
    Rosegarden
    A MIDI and audio sequencer and musical notation editor.
    Copyright 2000-2023 the Rosegarden development team.
    Modifications and additions Copyright (c) 2022,2023 Mark R. Rubin aka "thanks4opensource" aka "thanks4opensrc"

    Other copyrights also apply to some parts of this work.  Please
    see the AUTHORS file and individual file headers for details.

    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License as
    published by the Free Software Foundation; either version 2 of the
    License, or (at your option) any later version.  See the file
    COPYING included with this distribution for more information.
*/

#ifndef RG_CHORDNAMERULER_H
#define RG_CHORDNAMERULER_H

#include <map>
#include <set>
#include <vector>

#include <QFont>
#include <QFontMetrics>
#include <QSize>
#include <QWidget>

#include "base/Event.h"
#include "base/PropertyName.h"
#include "base/Segment.h"

class QAction;
class QMenu;
class QMouseEvent;
class QPaintEvent;


namespace Rosegarden
{

class Studio;
class Segment;
class RulerScale;
class RosegardenDocument;
class Composition;
class ChordAnalyzer;
class Key;
class ConflictingKeyChanges;


/**
 * ChordNameRuler is a widget that shows a strip of text strings
 * describing the chords in a composition.
 */

class ChordNameRuler : public QWidget, public SegmentObserver
{
    Q_OBJECT

public:
    // Display chords (and key changes) at positions calculated by
    //   RulerScale argument.
    // Optionally enable/disable menus for inserting/removing
    //   key changes, and for copying chords into a segment as
    //   text events (via external code's receipt of signal).

    // What window this ruler is in because enabled menu options and
    // connections to set of signals handled differ.
    enum class ParentEditor {
        TRACK,
        MATRIX,
        NOTATION,
    };

    // Will use all segments in composition as sources for notes
    // for chord analysis.
    ChordNameRuler(QWidget* parent,
                   RulerScale *rulerScale,
                   RosegardenDocument *doc,
                   const ParentEditor parentEditor,
                   int height = 0,
                   ConflictingKeyChanges* conflictingKeyChanges = nullptr);

    // Will use all only specified segments as sources for notes
    // for chord analysis.
    ChordNameRuler(QWidget* parent,
                   RulerScale *rulerScale,
                   RosegardenDocument *doc,
                   const std::vector<Segment *> &segments,
                   const ParentEditor parentEditor,
                   int height = 0,
                   ConflictingKeyChanges* conflictingKeyChanges = nullptr);

    ~ChordNameRuler() override;

    // Control of chord naming syle
    enum class ChordNameType {
        PITCH_LETTER = 0,   // Absolute concert pitch: C, C#, Db, ... Bb, B
        PITCH_INTEGER,      // Absolute pitch class: 0, 1, 2, ... 10, 11
        KEY_ROMAN_NUMERAL,  // Key-relative roman numeral: I, #I, bII ... VII
        KEY_INTEGER,        // Key-relative pitch class: 0, 1, 2, ... 10, 11
    };
    enum class ChordSlashType {
        OFF = 0,            // No slash chords (zero for efficient bool check)
        PITCH_OR_KEY,       // ChordNameType::PITCH_x or KEY_x
        CHORD_TONE,         // Chord root relative
    };

    // Use for chord analysis note times, either time range for
    // "per unit time / arpeggios", or as tolerance for "note on/off".
    enum class NoteDuration {
        NONE,
        DUR_128,
        DUR_128_DOT,
        DUR_64,
        DUR_64_DOT,
        DUR_32,
        DUR_32_DOT,
        DUR_16,
        DUR_16_DOT,
        EIGTH,
        EIGTH_DOT,
        QUARTER,
        QUARTER_DOT,
        HALF,
        HALF_DOT,
        WHOLE,
        WHOLE_DOT,
        DOUBLE,
    };

    // Accessors
    //

    const std::map<const timeT, const std::string> &chordNames() const
    { return m_chords; }

    // Used by MatrixElement methods to determine if should label
    // notes with chord degrees.
    bool haveChords() const {
        return (m_isVisible && m_showChords) || m_analyzeWhileHidden;
    }

    // For RosegardenMainViewWidget (main window) which doesn't keep track
    // by/for itself.
    const Segment *getCurrentSegment()
    const {
        return   m_currentSegment == &m_defaultSegment
               ? nullptr
               : m_currentSegment;
    }


    // SegmentObserver notifications
    void endMarkerTimeChanged(const Segment*, bool)   override;
    void startChanged        (const Segment*, timeT)  override;
    void segmentDeleted      (const Segment*)         override;

    // Segment to use for Key at current time for key-relative chord labelling
    void setCurrentSegment(const Segment *segment);

    // Remove/add  segment from current set used as sources of
    // notes for chord analysis
    void removeSegment(Segment *segment);
    void    addSegment(Segment *segment);

    // One or more segments have changed from pitched to percussion or
    // vice-versa. Is brute-force regeneration of all m_segments
    // active flags and Segment notifications signals, but is rare,
    // non-realtime, edge case.
    void resetPercussionSegments();

    // Calls ChordAnalyzer::labelChords() and then Qt update() (redisplay)
    //   conditionally depending on internal states/modes.
    // Public for client/owners (matrix editor) to call in desired order
    //   vs. other processing.
    // Otherwise triggered from slotCommandExecuted() (connected to
    //   CommandHistory::commandExecuted), As well as from internal UI menu
    //   slots and other internal methods (setVisible(), setCurrentSegment(),
    //   etc) if needed as per internal states/modes.
    bool analyzeChords();

    // Ruler drawing size hints and settings
    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;
    void setMinimumWidth(int width) { m_width = width; }

    // Finds chord closest within quantization to given time.
    // Returns pitch class 0..11, or -1 if no chord.
    // For external use labeling note in-chord function, e.g. by MatrixElement.
    int chordRootPitchAtTime(timeT) const;

    // Qt APIs
    void setVisible(const bool visible) override;
    bool isShown() const { return m_isVisible; };  // Can't override isVisible()

    void setAnalyzeWhileHidden(const bool analyze);

signals:
    // To notify matrix editor that might need to update note labels
    //   if are displaying chord degrees.
    // Emitted when UI menu invoked that triggers call to analyzeChords()
    //   which might change chord root notes.
    // Adding/removing key signatures handled separately by Segment
    //  notification of event addition/removal and then later
    //  acted upon when CommandHistory::commandExecuted/commandUnxecuted
    //  signaled.
    void chordAnalysisChanged();

    void insertKeyChange();  // Pop up notation editor key change dialog
    void copyChords();       // Notation editor, copy ruler chords to text


// See documentation for methods at definitions/implementations in .cpp file

public slots:
    void slotScrollHoriz(int x); // Synchronize with rest of GUI view elements
    void slotCommandExecuted();
    void slotNoteAddedOrRemoved(const Segment*, const Event*, bool);
    void slotKeyAddedOrRemoved (const Segment*, const Event*, bool);
    void slotNoteModified      (const Segment*, const Event*);

protected:
    // Qt signals
    void paintEvent           (QPaintEvent*) override;
    void mousePressEvent      (QMouseEvent*) override;
    void mouseReleaseEvent    (QMouseEvent*) override;
    void mouseDoubleClickEvent(QMouseEvent*) override;


protected slots:
    void slotInsertKeyChange();
    void slotCopyChords();
    void slotChooseActiveSegments();
    void slotShowChords();
    void slotShowKeys();
    void slotSetAlgorithm();
    void slotSetQuantization();
    void slotSetUnarpeggiation();
    void slotChordNameType();
    void slotChordSlashType();
    void slotChordAddedBass();
    void slotPreferSlashChords();
    void slotOffsetMinors();
    void slotAltMinors();
    void slotCMajorFlats();
    void slotNonDiatonicChords();
    void slotSetNameStyles();

private:
    class LongestNotes {
        // Keeps track of the longest note in a segment because chord
        //   analysis needs to begin that duration before the first
        //   note in range of times being analyzed (notes starting
        //   before the begin time of the range  but continuing past
        //   it contribute contribute to chords).
        // Sets a miniumum of a whole note which in 99.9% of compositons
        //   will never be exceeded, but the 0.01% of cases with organ
        //   pedal points, bagpipes andor hurdy-gurdy drones, etc, need
        //   to be correctly handled.

      public:
        static const int MINIMUM = 3840;  // Whole note in MIDI ticks

        LongestNotes()
        {
            m_longestNotes[MINIMUM] = 1;
        }

        timeT longest() const
        {
            // Can't fail because always have at least (MINIMUM, 1)
            return std::prev(m_longestNotes.end())->first;
        }

        void add(const timeT noteDuration)
        {
            if (noteDuration <= MINIMUM)
                return;  // Don't bother incrementing/decrementing default.
            auto found = m_longestNotes.find(noteDuration);
            if (found == m_longestNotes.end())
                m_longestNotes[noteDuration] = 1;
            else
                ++found->second;
        }

        void remove(const timeT noteDuration)
        {
            if (noteDuration <= MINIMUM)
                return;  // Don't remove default, longest() relies on having.
            auto found = m_longestNotes.find(noteDuration);
            if (found == m_longestNotes.end())
                return;
            if (found->second == 1)
                m_longestNotes.erase(found);
            else
                --found->second;
        }

      protected:
        // Map of (duration, count)
        // Always has at least (MINIMUMUM, 1) so don't have to
        //   special-case check.
        std::map<timeT, unsigned> m_longestNotes;
    };

    struct SegmentInfo {
        // Active in chord analysis, time range of modified notes,
        // and LongestNotes (see above)

        SegmentInfo()
        :   active        (true),
            isPercussion  (false),
            minChangedTime(std::numeric_limits<timeT>::max()),
            maxChangedTime(std::numeric_limits<timeT>::min())
        {}

        bool haveChangedRange() const
        {
            return maxChangedTime > minChangedTime;
        }

        void reset()
        {
            minChangedTime = std::numeric_limits<timeT>::max();
            maxChangedTime = std::numeric_limits<timeT>::min();
        }

        void addNote(
        const Event* const  note)
        {
            timeT   duration = note->getDuration(),
                    start    = note->getAbsoluteTime(),
                    stop     = start + duration;

            if (start < minChangedTime) minChangedTime = start;
            if (stop  > maxChangedTime) maxChangedTime = stop;

            longestNotes.add(duration);
        }


        void removeNote(
        const Event* const  note)
        {
            // Can't modify min/maxChangedTimes because might be
            //   other notes holding down those limits.
            // Not worth expense of trying to keep track of
            //   counts of multiple notes at time, etc.

            timeT   duration = note->getDuration(),
                    start    = note->getAbsoluteTime(),
                    stop     = start + duration;

            if (start < minChangedTime) minChangedTime = start;
            if (stop  > maxChangedTime) maxChangedTime = stop;

            longestNotes.remove(duration);
        }

        bool            active,          // Use for chord analysis
                        isPercussion;    // For when change to/from
        timeT           minChangedTime,  // If both are 0, use whole
                        maxChangedTime;  // composition time range
        LongestNotes    longestNotes;    // See LongestNotes, above
    };

    void init(const std::vector<Segment*> &segments);

    void createMenuAndToolTip();
    void updateToolTip();

    timeT noteDurationToMidiTicks(const NoteDuration);

    void removeOrderedSegment(const Segment*);

    void setSlashChordMenus();

    RosegardenDocument *m_doc;

    ChordAnalyzer *m_analyzer;  // chord analysis engine(s)

    ParentEditor             m_parentEditor;
    ConflictingKeyChanges   *m_conflictingKeyChanges;
    bool                     m_ownConflictingKeyChanges,
                             m_showChords,
                             m_showKeyChanges,
                             m_hideKeyChanges; // temporary, middle mouse button

    // ruler scale info
    int     m_height;
    int     m_currentXOffset;
    int     m_width;

    RulerScale  *m_rulerScale;
    Composition *m_composition;

    QMenu       *m_menu;
    QAction     *m_showChordsAction,
                *m_quantizeAction,
                *m_unarpeggiateAction,
                *m_quantizationNoneNoteAction,
                *m_quantizationNote128thNoteAction,
                *m_quantizationNote128thDotNoteAction,
                *m_quantizationNote64thNoteAction,
                *m_quantizationNote64thdotNoteAction,
                *m_quantizationNote32ndNoteAction,
                *m_quantizationNote32ndDotNoteAction,
                *m_quantizationNote16thNoteAction,
                *m_quantizationNote16thDotNoteAction,
                *m_quantizationEigthNoteAction,
                *m_quantizationEigthDotNoteAction,
                *m_quantizationQuarterNoteAction,
                *m_quantizationQuarterDotNoteAction,
                *m_quantizationHalfNoteAction,
                *m_quantizationHalfDotNoteAction,
                *m_quantizationWholeNoteAction,
                *m_quantizationWholeDotNoteAction,
                *m_quantizationDoubleNoteAction,
                *m_unarpeggiationNote128thNoteAction,
                *m_unarpeggiationNote128thDotNoteAction,
                *m_unarpeggiationNote64thNoteAction,
                *m_unarpeggiationNote64thdotNoteAction,
                *m_unarpeggiationNote32ndNoteAction,
                *m_unarpeggiationNote32ndDotNoteAction,
                *m_unarpeggiationNote16thNoteAction,
                *m_unarpeggiationNote16thDotNoteAction,
                *m_unarpeggiationEigthNoteAction,
                *m_unarpeggiationEigthDotNoteAction,
                *m_unarpeggiationQuarterNoteAction,
                *m_unarpeggiationQuarterDotNoteAction,
                *m_unarpeggiationHalfNoteAction,
                *m_unarpeggiationHalfDotNoteAction,
                *m_unarpeggiationWholeNoteAction,
                *m_unarpeggiationWholeDotNoteAction,
                *m_unarpeggiationDoubleNoteAction,
                *m_showKeysAction,
                *m_insertKeyChangeAction,
                *m_chordCopyAction,
                *m_pitchLetterAction,
                *m_pitchIntegerAction,
                *m_keyRomanNumeralAction,
                *m_keyIntegerAction,
                *m_slashOffAction,
                *m_slashAbsRelAction,
                *m_slashChordToneAction,
                *m_slashAddedBassAction,
                *m_preferSlashChordsAction,
                *m_cMajorFlats,
                *m_offsetMinorKeys,
                *m_altMinorKeys,
                *m_nonDiatonicChords;

    // For key-relative chord analysis
    const Segment *m_currentSegment;
          Segment  m_defaultSegment;  // for use when no real m_currentSegment

    std::map<const Segment*, SegmentInfo> m_segments;

    // For CheckableListDialog in chooseSegments() so that are in
    // same order as constructed by parent editor (currently only
    // matrix does correctly, notation is wrong and track/main
    // doesn't enable chooseSegments() menu action).
    std::vector<Segment*> m_orderedSegments;

    // Generated by ChordAnalyer::labelChords().
    std::map<const timeT, const std::string>    m_chords;

    // For external use labeling note in-chord function, e.g. by MatrixElement
    std::map<const timeT, int> m_roots;

    // Flags and settings
    bool           m_isVisible,             // see setVisible() in .cpp file
                   m_analyzeWhileHidden,    // for matrix editor chord labels
                   m_doUnarpeggiation,      // which chord analysis algorithm
                   m_keysDirty,             // key changes need recalc/update
                   m_notesDirty,            // need chord analysis
                   m_chordsDirty,           // need to re-analyze chords
                   m_namingModeChanged,     // to force despite !m_keyDependent
                   m_keyDependent;          // some chord analysis modes are
    NoteDuration   m_quantization,          // for "note change" algorithm
                   m_unarpeggiation;        // for "per unit time" algorithm
    timeT          m_quantizationTime,      // for "note change" algorithm
                   m_unarpeggiationTime;    // for "per unit time" algorithm
    ChordNameType  m_chordNameType;         // labeling style
    ChordSlashType m_chordSlashType;        // labeling style slash/inversion

    // Text settings
    QFont        m_font,
                 m_boldFont;
    QFontMetrics m_fontMetrics,
                 m_boldFontMetrics;

};  // class ChordNameRuler

}   // namespace Rosegarden

#endif
