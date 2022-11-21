
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


/**
 * ChordNameRuler is a widget that shows a strip of text strings
 * describing the chords in a composition.
 */

class ChordNameRuler : public QWidget
{
    Q_OBJECT

public:
    // Display chords (and key changes) at positions calculated by
    //   RulerScale argument.
    // Optionally enable/disable menus for inserting/removing
    //   key changes, and for copying chords into a segment as
    //   text events (via external code's receipt of signal).

    // Will use all segments in composition as sources for notes
    // for chord analysis.
    ChordNameRuler(RulerScale *rulerScale,
                   RosegardenDocument *doc,
                   bool keyChangingEnabled = false,
                   bool chordCopyingEnabled = false,
                   int height = 0,
                   QWidget* parent = nullptr);

    // Will use all only specified segments as sources for notes
    // for chord analysis.
    ChordNameRuler(RulerScale *rulerScale,
                   RosegardenDocument *doc,
                   std::vector<Segment *> &segments,
                   bool keyChangingEnabled = false,
                   bool chordCopyingEnabled = false,
                   int height = 0,
                   QWidget* parent = nullptr);

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

    // Segment to use fo Key at current time for key-relative chord labelling
    void setCurrentSegment(const Segment *segment, bool forceRecalc);
    const Segment *chordNamesSegment() const { return m_chordAndKeyNames; }

    // Used by MatrixScene::slotKeySignaturesChanged()
    bool conflictingKeyChanges() const { return m_conflictingKeyChanges; }

    // Remove/add segment from current set used as sources for notes
    // for chord analysis
    void removeSegment(const Segment *segment);
    void addSegment(Segment *segment);

    void analyzeChordsAndKeyChanges(bool emitSignal=true);

    // Ruler drawing size hints and settings
    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;
    void setMinimumWidth(int width) { m_width = width; }


    // For external use labeling note in-key degrees, e.g. by MatrixElement
    const Key keyAtTime(const timeT t) const;
    // For external use by MatrixScene::recreateKeyHighlights()
    timeT getNextKeyTime(const timeT currentKeyTime) const;

    // Pitch class 0..11, or -1 if no chord
    // For external use labeling note in-chord function, e.g. by MatrixElement
    int chordRootPitchAtTime(const timeT) const;

signals:
    void chordAnalysisChanged();
    void insertKeyChange();  // pop up notation editor key change dialog
    void copyChords();       // notation editor copy ruler chords to text


// See documentation for methods at definitions/implementations in .cpp file

public slots:
    void slotScrollHoriz(int x); // To synchronize with rest of GUI view elements

protected:
    void paintEvent           (QPaintEvent*) override;
    void mousePressEvent      (QMouseEvent*) override;
    void mouseDoubleClickEvent(QMouseEvent*) override;

protected slots:
    void slotSegmentContentsChanged();
    void slotInsertKeyChange();
    void slotCopyChords();
    void slotChooseActiveSegments();
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
    struct SegmentAndActive {
        Segment *segment;  // Not const because ChordAnalyzer::labelChords
                           //   inserts C major key change at beginning if
                           //   no key change there.
        bool     active;   // true if segment's notes being used for
                           //   chord analysis
    };

    void createMenuAndTooltip();
    bool chooseSegments(const QString&, const std::vector<Segment*>&);
    std::vector<SegmentAndActive>::iterator findSegment(const Segment*);
    void resetActiveSegmentIndices();
    timeT noteDurationToMidiTicks(const NoteDuration);

    RosegardenDocument *m_doc;

    ChordAnalyzer *m_analyzer;  // chord analysis engine(s)

    bool    m_keyChangingEnabled;
    bool    m_chordCopyingEnabled;

    // ruler scale info
    int     m_height;
    int     m_currentXOffset;
    int     m_width;

    RulerScale  *m_rulerScale;
    Composition *m_composition;

    // For getting current set of segments if not explictly set
    // by second constructor variant and/or modified via removeSegment()
    unsigned int m_compositionRefreshStatusId;

    QMenu       *m_menu;
    QAction     *m_quantizeAction,
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

    bool m_enableChooseActiveSegments;

    // For key-relative chord analysis
    const Segment *m_currentSegment;

    std::vector<SegmentAndActive>  m_segments;

    unsigned m_numActiveSegments;

    std::vector<unsigned> m_activeSegmentIndices;  // for CheckableListDialog

    // Contains text events generated by ChordAnalyer::labelChords(),
    // both analyzed chords and extracted key changes.
    Segment     *m_chordAndKeyNames;

    // For external use labeling note in-key degrees, e.g. by MatrixElement
    std::map<timeT, const Key> m_keys;
    // For external use labeling note in-chord function, e.g. by MatrixElement
    std::map<timeT, int> m_roots;



    // Flags and settings
    bool           m_implicitSegments,       // not maintained by client
                   m_conflictingKeyChanges,  // optimize if re-anlyze needed
                   m_chordNameStylesUpdated, // initialize chord analyzer once
                   m_doUnarpeggiation;       // which chord analysis algorithm
    NoteDuration   m_quantization,           // for "note change" algorithm
                   m_unarpeggiation;         // for "per unit time" algorithm
    timeT          m_quantizationTime,       // for "note change" algorithm
                   m_unarpeggiationTime;     // for "per unit time" algorithm
    ChordNameType  m_chordNameType;          // labeling style
    ChordSlashType m_chordSlashType;         // labeling style slash/inversion

    // Text settings
    QFont        m_font,
                 m_boldFont;
    QFontMetrics m_fontMetrics,
                 m_boldFontMetrics;
};


}

#endif
