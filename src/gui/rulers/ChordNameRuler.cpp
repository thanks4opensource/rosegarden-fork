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

#define RG_MODULE_STRING "[ChordNameRuler]"

#define RG_NO_DEBUG_PRINT 1

#include "ChordNameRuler.h"

#include "base/AnalysisTypes.h"
#include "base/Composition.h"
#include "base/Instrument.h"
#include "base/NotationQuantizer.h"
#include "base/NotationTypes.h"
#include "base/PropertyName.h"
#include "base/RefreshStatus.h"
#include "base/RulerScale.h"
#include "base/Segment.h"
#include "base/Selection.h"
#include "base/Studio.h"
#include "base/Track.h"
#include "commands/edit/EraseCommand.h"
#include "document/Command.h"
#include "document/CommandHistory.h"
#include "document/RosegardenDocument.h"
#include "gui/dialogs/CheckableListDialog.h"
#include "gui/dialogs/ChordNameStylesDialog.h"
#include "gui/general/GUIPalette.h"
#include "misc/ConfigGroups.h"
#include "misc/Debug.h"
#include "misc/Preferences.h"
#include "misc/Strings.h"

#include <QFont>
#include <QFontMetrics>
#include <QLabel>
#include <QMenu>
#include <QMessageBox>
#include <QObject>
#include <QPainter>
#include <QPaintEvent>
#include <QRect>
#include <QSettings>
#include <QSize>
#include <QToolTip>
#include <QWidget>
#include <QWidgetAction>

#include <limits>


namespace Rosegarden
{

// See documentation in .h file
ChordNameRuler::ChordNameRuler(RulerScale *rulerScale,
                               RosegardenDocument *doc,
                               bool keyChangingEnabled,
                               bool chordCopyingEnabled,
                               int height,
                               QWidget *parent) :
        QWidget                          (parent),
        m_doc                            (doc),
        m_analyzer                       (new ChordAnalyzer),
        m_keyChangingEnabled             (keyChangingEnabled),
        m_chordCopyingEnabled            (chordCopyingEnabled),
        m_height                         (height),
        m_currentXOffset                 (0),
        m_width                          ( -1),
        m_rulerScale                     (rulerScale),
        m_composition                    (&doc->getComposition()),
        m_compositionRefreshStatusId     (0),
        m_menu                           (nullptr),
        m_quantizeAction                 (nullptr),
        m_unarpeggiateAction             (nullptr),
        m_quantizationNoneNoteAction          (nullptr),
        m_quantizationNote128thNoteAction     (nullptr),
        m_quantizationNote128thDotNoteAction  (nullptr),
        m_quantizationNote64thNoteAction      (nullptr),
        m_quantizationNote64thdotNoteAction   (nullptr),
        m_quantizationNote32ndNoteAction      (nullptr),
        m_quantizationNote32ndDotNoteAction   (nullptr),
        m_quantizationNote16thNoteAction      (nullptr),
        m_quantizationNote16thDotNoteAction   (nullptr),
        m_quantizationEigthNoteAction         (nullptr),
        m_quantizationEigthDotNoteAction      (nullptr),
        m_quantizationQuarterNoteAction       (nullptr),
        m_quantizationQuarterDotNoteAction    (nullptr),
        m_quantizationHalfNoteAction          (nullptr),
        m_quantizationHalfDotNoteAction       (nullptr),
        m_quantizationWholeNoteAction         (nullptr),
        m_quantizationWholeDotNoteAction      (nullptr),
        m_quantizationDoubleNoteAction        (nullptr),
        m_unarpeggiationNote128thNoteAction   (nullptr),
        m_unarpeggiationNote128thDotNoteAction(nullptr),
        m_unarpeggiationNote64thNoteAction    (nullptr),
        m_unarpeggiationNote64thdotNoteAction (nullptr),
        m_unarpeggiationNote32ndNoteAction    (nullptr),
        m_unarpeggiationNote32ndDotNoteAction (nullptr),
        m_unarpeggiationNote16thNoteAction    (nullptr),
        m_unarpeggiationNote16thDotNoteAction (nullptr),
        m_unarpeggiationEigthNoteAction       (nullptr),
        m_unarpeggiationEigthDotNoteAction    (nullptr),
        m_unarpeggiationQuarterNoteAction     (nullptr),
        m_unarpeggiationQuarterDotNoteAction  (nullptr),
        m_unarpeggiationHalfNoteAction        (nullptr),
        m_unarpeggiationHalfDotNoteAction     (nullptr),
        m_unarpeggiationWholeNoteAction       (nullptr),
        m_unarpeggiationWholeDotNoteAction    (nullptr),
        m_unarpeggiationDoubleNoteAction      (nullptr),
        m_pitchLetterAction              (nullptr),
        m_pitchIntegerAction             (nullptr),
        m_keyRomanNumeralAction          (nullptr),
        m_keyIntegerAction               (nullptr),
        m_slashOffAction                 (nullptr),
        m_slashAbsRelAction              (nullptr),
        m_slashChordToneAction           (nullptr),
        m_slashAddedBassAction           (nullptr),
        m_preferSlashChordsAction        (nullptr),
        m_cMajorFlats                    (nullptr),
        m_offsetMinorKeys                (nullptr),
        m_altMinorKeys                   (nullptr),
        m_nonDiatonicChords              (nullptr),
        m_enableChooseActiveSegments     (true),
        m_currentSegment                 (nullptr),
        m_numActiveSegments              (0),
        m_chordAndKeyNames               (new Segment()),
        m_implicitSegments               (true),
        m_conflictingKeyChanges          (false),
        m_chordNameStylesUpdated         (false),
        m_doUnarpeggiation               (false),
        m_quantization                   (NoteDuration::NONE),
        m_unarpeggiation                 (NoteDuration::QUARTER),
        m_quantizationTime               (0),
        m_unarpeggiationTime             (0),
        m_chordNameType                  (ChordNameType::PITCH_LETTER),
        m_chordSlashType                 (ChordSlashType::OFF),
        m_fontMetrics                    (m_font),
        m_boldFontMetrics                (m_boldFont)
{
    m_font.setPointSize(11);
    m_font.setPixelSize(12);
    m_boldFont.setPointSize(11);
    m_boldFont.setPixelSize(12);
    m_boldFont.setBold(true);
    m_fontMetrics     = QFontMetrics(m_font);
    m_boldFontMetrics = QFontMetrics(m_boldFont);
    m_compositionRefreshStatusId = m_composition->getNewRefreshStatusId();

    createMenuAndTooltip();

    // Populate m_chordNote with all segments in composition
    // Used for main window -- matrix and notation register
    //   explicit list of segments via other construtor.
    const Studio &studio(doc->currentDocument->getStudio());
    for (Composition::iterator   ci  = m_composition->begin() ;
                                 ci != m_composition->end() ;
                               ++ci) {
        TrackId ti = (*ci)->getTrack();
        Instrument *instr = studio.getInstrumentById(  m_composition
                                                     ->getTrackById(ti)
                                                     ->getInstrument());

        if (instr && instr->isPercussion())
            continue;

        m_segments.push_back(SegmentAndActive{*ci, true});

        connect(*ci,  &Segment::contentsChanged,
                this, &ChordNameRuler::slotSegmentContentsChanged);
    }
    m_numActiveSegments = m_segments.size();

    resetActiveSegmentIndices();

    analyzeChordsAndKeyChanges(false);  // don't emit chordAnalysisChanged()
}

// See documentation in .h file
ChordNameRuler::ChordNameRuler(RulerScale *rulerScale,
                               RosegardenDocument *doc,
                               std::vector<Segment *> &segments,
                               bool keyChangingEnabled,
                               bool chordCopyingEnabled,
                               int height,
                               QWidget *parent) :
        QWidget                          (parent),
        m_doc                            (doc),
        m_analyzer                       (new ChordAnalyzer),
        m_keyChangingEnabled             (keyChangingEnabled),
        m_chordCopyingEnabled            (chordCopyingEnabled),
        m_height                         (height),
        m_currentXOffset                 (0),
        m_width                          ( -1),
        m_rulerScale                     (rulerScale),
        m_composition                    (&doc->getComposition()),
        m_compositionRefreshStatusId     (0),
        m_menu                           (nullptr),
        m_quantizeAction                 (nullptr),
        m_unarpeggiateAction             (nullptr),
        m_quantizationNoneNoteAction          (nullptr),
        m_quantizationNote128thNoteAction     (nullptr),
        m_quantizationNote128thDotNoteAction  (nullptr),
        m_quantizationNote64thNoteAction      (nullptr),
        m_quantizationNote64thdotNoteAction   (nullptr),
        m_quantizationNote32ndNoteAction      (nullptr),
        m_quantizationNote32ndDotNoteAction   (nullptr),
        m_quantizationNote16thNoteAction      (nullptr),
        m_quantizationNote16thDotNoteAction   (nullptr),
        m_quantizationEigthNoteAction         (nullptr),
        m_quantizationEigthDotNoteAction      (nullptr),
        m_quantizationQuarterNoteAction       (nullptr),
        m_quantizationQuarterDotNoteAction    (nullptr),
        m_quantizationHalfNoteAction          (nullptr),
        m_quantizationHalfDotNoteAction       (nullptr),
        m_quantizationWholeNoteAction         (nullptr),
        m_quantizationWholeDotNoteAction      (nullptr),
        m_quantizationDoubleNoteAction        (nullptr),
        m_unarpeggiationNote128thNoteAction   (nullptr),
        m_unarpeggiationNote128thDotNoteAction(nullptr),
        m_unarpeggiationNote64thNoteAction    (nullptr),
        m_unarpeggiationNote64thdotNoteAction (nullptr),
        m_unarpeggiationNote32ndNoteAction    (nullptr),
        m_unarpeggiationNote32ndDotNoteAction (nullptr),
        m_unarpeggiationNote16thNoteAction    (nullptr),
        m_unarpeggiationNote16thDotNoteAction (nullptr),
        m_unarpeggiationEigthNoteAction       (nullptr),
        m_unarpeggiationEigthDotNoteAction    (nullptr),
        m_unarpeggiationQuarterNoteAction     (nullptr),
        m_unarpeggiationQuarterDotNoteAction  (nullptr),
        m_unarpeggiationHalfNoteAction        (nullptr),
        m_unarpeggiationHalfDotNoteAction     (nullptr),
        m_unarpeggiationWholeNoteAction       (nullptr),
        m_unarpeggiationWholeDotNoteAction    (nullptr),
        m_unarpeggiationDoubleNoteAction      (nullptr),
        m_pitchLetterAction              (nullptr),
        m_pitchIntegerAction             (nullptr),
        m_keyRomanNumeralAction          (nullptr),
        m_keyIntegerAction               (nullptr),
        m_slashOffAction                 (nullptr),
        m_slashAbsRelAction              (nullptr),
        m_slashChordToneAction           (nullptr),
        m_slashAddedBassAction           (nullptr),
        m_preferSlashChordsAction        (nullptr),
        m_cMajorFlats                    (nullptr),
        m_offsetMinorKeys                (nullptr),
        m_altMinorKeys                   (nullptr),
        m_nonDiatonicChords              (nullptr),
        m_enableChooseActiveSegments     (false),
        m_currentSegment                 (nullptr),
        m_numActiveSegments              (0),
        m_chordAndKeyNames               (new Segment()),
        m_implicitSegments               (false),
        m_conflictingKeyChanges          (false),
        m_chordNameStylesUpdated         (false),
        m_doUnarpeggiation               (false),
        m_quantization                   (NoteDuration::NONE),
        m_unarpeggiation                 (NoteDuration::QUARTER),
        m_quantizationTime               (0),
        m_unarpeggiationTime             (0),
        m_chordNameType                  (ChordNameType::PITCH_LETTER),
        m_chordSlashType                 (ChordSlashType::OFF),
        m_fontMetrics                    (m_font),
        m_boldFontMetrics                (m_boldFont)
{
    m_font.setPointSize(11);
    m_font.setPixelSize(12);
    m_boldFont.setPointSize(11);
    m_boldFont.setPixelSize(12);
    m_boldFont.setBold(true);
    m_fontMetrics     = QFontMetrics(m_font);
    m_boldFontMetrics = QFontMetrics(m_boldFont);

    m_compositionRefreshStatusId = m_composition->getNewRefreshStatusId();

#if 0
    // Save specified segments
    for (std::vector<Segment *>::iterator i = segments.begin();
            i != segments.end(); ++i)
        insertSegment(*i, true);  // true==also into m_segments
    resetActiveSegmentIndices();
#else
    RosegardenDocument *document(RosegardenDocument::currentDocument);
    Composition &composition(document->getComposition());
    for (Segment *segment : segments) {
        TrackId trackId = segment->getTrack();
        Track *track = composition.getTrackById(trackId);
        Instrument *instr = document->
                            getStudio().
                            getInstrumentById(track->getInstrument());

        // Only regular, not percussion segments
        if (!instr->getKeyMapping()) {
            m_segments.push_back(SegmentAndActive{segment, true});
            connect(segment, &Segment::contentsChanged,
                    this,    &ChordNameRuler::slotSegmentContentsChanged);
        }
    }
    m_numActiveSegments = m_segments.size();
    resetActiveSegmentIndices();
#endif

    createMenuAndTooltip();

    analyzeChordsAndKeyChanges(false);  // don't emit chordAnalysisChanged()
}

ChordNameRuler::~ChordNameRuler()
{
    delete m_analyzer;
    delete m_chordAndKeyNames;
}

void
ChordNameRuler::createMenuAndTooltip()
{
    // Must call AnalysisHelper::updateChordNameStyles() at least once
    // because ctor of static ChordMaps can't, because Prefernces not
    // set up yet at that time
    if (!m_chordNameStylesUpdated) {
        m_analyzer->updateChordNameStyles();
        m_chordNameStylesUpdated = true;
    }

    if (m_menu) return;

    m_menu = new QMenu(tr("Key changes and chord names"), this);
    QAction *action;

    // Only supported in notation and matrix editors, not main/tracks window
    if (!m_enableChooseActiveSegments) {
        action = m_menu->addAction(tr("choose_active_segments"));
        action->setText(tr("Choose chord note segments..."));
        connect(action, &QAction::triggered,
                this,   &ChordNameRuler::slotChooseActiveSegments);
        m_menu->addSeparator();
    }

    QActionGroup *algorithmActionGroup = new QActionGroup(m_menu);

    m_quantizeAction = m_menu->addAction("on_off_quantize");
    m_quantizeAction->setText(tr("Chords when notes on/off"));
    m_quantizeAction->setCheckable(true);
    algorithmActionGroup->addAction(m_quantizeAction);
    connect(m_quantizeAction, &QAction::triggered,
            this,             &ChordNameRuler::slotSetAlgorithm);

    m_unarpeggiateAction = m_menu->addAction("one_per_time_unarpeggiate");
    m_unarpeggiateAction->setText(tr("Unarpeggiate / one chord per unit time"));
    m_unarpeggiateAction->setCheckable(true);
    algorithmActionGroup->addAction(m_unarpeggiateAction);
    connect(m_unarpeggiateAction, &QAction::triggered,
            this,                 &ChordNameRuler::slotSetAlgorithm);

    m_doUnarpeggiation = Preferences::getPreference(ChordAnalysisGroup,
                                                    "chord_name_unarpeggiate",
                                                    false);
    if (m_doUnarpeggiation) m_unarpeggiateAction->setChecked(true);
    else                    m_quantizeAction    ->setChecked(true);


    QMenu *quantizeMenu = m_menu->addMenu(tr("Notes on/off quantization time"));
    QActionGroup *quantizeActionGroup = new QActionGroup(m_menu);

#define ADD_ACTION(ACTION, NAME, TEXT)                                  \
    m_quantization##ACTION##NoteAction = quantizeMenu->addAction(NAME); \
    m_quantization##ACTION##NoteAction->setText(tr(TEXT));              \
    m_quantization##ACTION##NoteAction->setCheckable(true);             \
    quantizeActionGroup->addAction(m_quantization##ACTION##NoteAction); \
    connect(m_quantization##ACTION##NoteAction, &QAction::triggered,    \
            this,                               &ChordNameRuler         \
                                                ::slotSetQuantization)

    ADD_ACTION(None,         "none",           "0 (none/zero)");
    ADD_ACTION(Note128th,    "note_128th",     "128th");
    ADD_ACTION(Note128thDot, "note_128th_dot", "dotted 128th");
    ADD_ACTION(Note64th,     "note_64th",      "64th (hemidemisemiquaver)");
    ADD_ACTION(Note64thdot,  "note_64th_dot",  "dotted 64th");
    ADD_ACTION(Note32nd,     "note_32nd",      "32nd (demisemiquaver)");
    ADD_ACTION(Note32ndDot,  "note_32nd_dot",  "dotted 32nd ()");
    ADD_ACTION(Note16th,     "note_16th",      "16th (semiquaver )");
    ADD_ACTION(Note16thDot,  "note_16th_dot",  "dotted 16th ()");
    ADD_ACTION(Eigth,        "eigth",          "eigth (quaver)");
    ADD_ACTION(EigthDot,     "eigth_dot",      "dotted eigth");
    ADD_ACTION(Quarter,      "quarter",        "quarter (crotchet)");
    ADD_ACTION(QuarterDot,   "quarter_dot",    "dotted quarter");
    ADD_ACTION(Half,         "half",           "half (minim)");
    ADD_ACTION(HalfDot,      "half_dot",       "dotted half");
    ADD_ACTION(Whole,        "whole",          "whole (semibrev)");
    ADD_ACTION(WholeDot,     "whole_dot",      "dotted whole");
    ADD_ACTION(Double,       "double",         "double (brev)");
#undef ADD_ACTION

    m_quantization = static_cast<NoteDuration>(
                      Preferences::getPreference(ChordAnalysisGroup,
                                                 "chord_name_quantization",
                                                 static_cast<unsigned>(
                                                 NoteDuration::NONE)));

    m_quantizationTime = noteDurationToMidiTicks(m_quantization);

    switch (m_quantization) {
#define CASE(QUANT, ACTION)                                         \
        case NoteDuration::QUANT:                                   \
            m_quantization##ACTION##NoteAction->setChecked(true); \
            break

        CASE(NONE,        None);
        CASE(DUR_128,     Note128th);
        CASE(DUR_128_DOT, Note128thDot);
        CASE(DUR_64,      Note64th);
        CASE(DUR_64_DOT,  Note64thdot);
        CASE(DUR_32,      Note32nd);
        CASE(DUR_32_DOT,  Note32ndDot);
        CASE(DUR_16,      Note16th);
        CASE(DUR_16_DOT,  Note16thDot);
        CASE(EIGTH,       Eigth);
        CASE(EIGTH_DOT,   EigthDot);
        CASE(QUARTER,     Quarter);
        CASE(QUARTER_DOT, QuarterDot);
        CASE(HALF,        Half);
        CASE(HALF_DOT,    HalfDot);
        CASE(WHOLE,       Whole);
        CASE(WHOLE_DOT,   WholeDot);
        CASE(DOUBLE,      Double);
#undef CASE
    }


    QMenu *unarpeggiationMenu = m_menu->addMenu(tr("Unarpeggiation and "
                                                   "\"one chord per\" "
                                                   "time"));
    QActionGroup *unarpeggiationActionGroup = new QActionGroup(m_menu);

#define ADD_ACTION(ACTION, NAME, TEXT)                                      \
    m_unarpeggiation##ACTION##NoteAction = unarpeggiationMenu->addAction(NAME);\
    m_unarpeggiation##ACTION##NoteAction->setText(tr(TEXT));                \
    m_unarpeggiation##ACTION##NoteAction->setCheckable(true);               \
    unarpeggiationActionGroup->addAction(m_unarpeggiation##ACTION##NoteAction);\
    connect(m_unarpeggiation##ACTION##NoteAction, &QAction::triggered,      \
            this,                               &ChordNameRuler             \
                                                ::slotSetUnarpeggiation)
    ADD_ACTION(Note128th,    "note_128th",     "128th");
    ADD_ACTION(Note128thDot, "note_128th_dot", "dotted 128th");
    ADD_ACTION(Note64th,     "note_64th",      "64th (hemidemisemiquaver)");
    ADD_ACTION(Note64thdot,  "note_64th_dot",  "dotted 64th");
    ADD_ACTION(Note32nd,     "note_32nd",      "32nd (demisemiquaver)");
    ADD_ACTION(Note32ndDot,  "note_32nd_dot",  "dotted 32nd ()");
    ADD_ACTION(Note16th,     "note_16th",      "16th (semiquaver )");
    ADD_ACTION(Note16thDot,  "note_16th_dot",  "dotted 16th ()");
    ADD_ACTION(Eigth,        "eigth",          "eigth (quaver)");
    ADD_ACTION(EigthDot,     "eigth_dot",      "dotted eigth");
    ADD_ACTION(Quarter,      "quarter",        "quarter (crotchet)");
    ADD_ACTION(QuarterDot,   "quarter_dot",    "dotted quarter");
    ADD_ACTION(Half,         "half",           "half (minim)");
    ADD_ACTION(HalfDot,      "half_dot",       "dotted half");
    ADD_ACTION(Whole,        "whole",          "whole (semibrev)");
    ADD_ACTION(WholeDot,     "whole_dot",      "dotted whole");
    ADD_ACTION(Double,       "double",         "double (brev)");
#undef ADD_ACTION

    m_unarpeggiation = static_cast<NoteDuration>(
                       Preferences::getPreference(ChordAnalysisGroup,
                                                  "chord_name_unarpeggiation",
                                                  static_cast<unsigned>(
                                                  NoteDuration::QUARTER)));

    m_unarpeggiationTime = noteDurationToMidiTicks(m_unarpeggiation);

    switch (m_unarpeggiation) {
#define CASE(UNARP, ACTION)                                         \
        case NoteDuration::UNARP:                                   \
            m_unarpeggiation##ACTION##NoteAction->setChecked(true); \
            break

        case NoteDuration::NONE:
            break;
        CASE(DUR_128,     Note128th);
        CASE(DUR_128_DOT, Note128thDot);
        CASE(DUR_64,      Note64th);
        CASE(DUR_64_DOT,  Note64thdot);
        CASE(DUR_32,      Note32nd);
        CASE(DUR_32_DOT,  Note32ndDot);
        CASE(DUR_16,      Note16th);
        CASE(DUR_16_DOT,  Note16thDot);
        CASE(EIGTH,       Eigth);
        CASE(EIGTH_DOT,   EigthDot);
        CASE(QUARTER,     Quarter);
        CASE(QUARTER_DOT, QuarterDot);
        CASE(HALF,        Half);
        CASE(HALF_DOT,    HalfDot);
        CASE(WHOLE,       Whole);
        CASE(WHOLE_DOT,   WholeDot);
        CASE(DOUBLE,      Double);
#undef CASE
    }


    // Optional menus
    //

    if (m_keyChangingEnabled || m_chordCopyingEnabled)
        m_menu->addSeparator();

    if (m_keyChangingEnabled) {
        action = m_menu->addAction(tr("insert_key_change"));
        action->setText(tr("Insert Key Change at Playback Position..."));
        connect(action, &QAction::triggered,
                this,   &ChordNameRuler::slotInsertKeyChange);
    }

    if (m_chordCopyingEnabled) {
        action = m_menu->addAction(tr("copy_chords_to_text"));
        action->setText(tr("Copy chords to text"));
        connect(action, &QAction::triggered,
                this,   &ChordNameRuler::slotCopyChords);
    }


    // Back to always-enabled menus
    //

    QString style(QString("QLabel {color: gray; "
                          "padding-left: 1;}"));    // indent a little
    m_menu->addSeparator();

    QLabel *namesLabel = new QLabel(tr("Chord root names:"));
    namesLabel->setStyleSheet(style);
    namesLabel->setForegroundRole(QPalette::ButtonText);
    QWidgetAction *nameWidgetAction = new QWidgetAction(this);
    nameWidgetAction->setDefaultWidget(namesLabel);
    m_menu->addAction(nameWidgetAction);

    QActionGroup *chordNameActionGroup = new QActionGroup(m_menu);

    m_pitchLetterAction = m_menu->addAction(tr("pitch_letter_names"));
    m_pitchLetterAction->setText(tr(
                          "Concert pitch letters (C, C#/Db, D, ... B)"));
    m_pitchLetterAction->setCheckable(true);
    chordNameActionGroup->addAction(m_pitchLetterAction);
    connect(m_pitchLetterAction, &QAction::triggered,
            this,                &ChordNameRuler::slotChordNameType);

    m_pitchIntegerAction = m_menu->addAction(tr("pitch_integer_chord_names"));
    m_pitchIntegerAction->setText(tr("Concert pitch integers "
                                     "(0, 1, 2, ... 11)"));
    m_pitchIntegerAction->setCheckable(true);
    chordNameActionGroup->addAction(m_pitchIntegerAction);
    connect(m_pitchIntegerAction, &QAction::triggered,
            this,                 &ChordNameRuler::slotChordNameType);

    m_keyRomanNumeralAction = m_menu->addAction(tr("key_roman_numeral_names"));
    m_keyRomanNumeralAction->setText(tr("In-key roman numerals "
                                        "(I, #I/bII, II, ... VII), "
                                        "e.g. 3rd in minor key = \"III\""));
    m_keyRomanNumeralAction->setCheckable(true);
    chordNameActionGroup->addAction(m_keyRomanNumeralAction);
    connect(m_keyRomanNumeralAction, &QAction::triggered,
            this,                    &ChordNameRuler::slotChordNameType);

    m_keyIntegerAction = m_menu->addAction(tr("key_integer_names"));
    m_keyIntegerAction->setText(tr("In-key integers (0, 1, 2, ... 11)"));
    m_keyIntegerAction->setCheckable(true);
    chordNameActionGroup->addAction(m_keyIntegerAction);
    connect(m_keyIntegerAction, &QAction::triggered,
            this,               &ChordNameRuler::slotChordNameType);

    m_chordNameType = static_cast<ChordNameType>(
                      Preferences::getPreference(ChordAnalysisGroup,
                                                 "chord_name_type",
                                                 static_cast<unsigned>(
                                                 ChordNameType::PITCH_LETTER)));

    switch (m_chordNameType) {
        case ChordNameType::PITCH_LETTER:
            m_pitchLetterAction->setChecked(true);
            break;

        case ChordNameType::PITCH_INTEGER:
            m_pitchIntegerAction->setChecked(true);
            break;

        case ChordNameType::KEY_ROMAN_NUMERAL:
            m_keyRomanNumeralAction->setChecked(true);
            break;

        case ChordNameType::KEY_INTEGER:
            m_keyIntegerAction->setChecked(true);
            break;
    }

    QLabel *slashLabel = new QLabel(tr("Slash chords:"));
    slashLabel->setStyleSheet(style);
    slashLabel->setForegroundRole(QPalette::ButtonText);
    QWidgetAction *slashWidgetAction = new QWidgetAction(this);
    slashWidgetAction->setDefaultWidget(slashLabel);
    m_menu->addAction(slashWidgetAction);

    QActionGroup *slashNameActionGroup = new QActionGroup(m_menu);

    m_slashOffAction = m_menu->addAction(tr("pitch_or_key_type"));
    m_slashOffAction->setText(tr("Off"));
    m_slashOffAction->setCheckable(true);
    slashNameActionGroup->addAction(m_slashOffAction);
    connect(m_slashOffAction, &QAction::triggered,
            this,             &ChordNameRuler::slotChordSlashType);

    m_slashAbsRelAction = m_menu->addAction(tr("pitch_or_key_type"));
    m_slashAbsRelAction->setText(tr("Concert pitch or key relative"));
    m_slashAbsRelAction->setCheckable(true);
    slashNameActionGroup->addAction(m_slashAbsRelAction);
    connect(m_slashAbsRelAction, &QAction::triggered,
            this,                &ChordNameRuler::slotChordSlashType);

    m_slashChordToneAction = m_menu->addAction(tr("chord_tone_type"));
    m_slashChordToneAction->setText(tr("Chord root relative"));
    m_slashChordToneAction->setCheckable(true);
    slashNameActionGroup->addAction(m_slashChordToneAction);
    connect(m_slashChordToneAction, &QAction::triggered,
            this,                   &ChordNameRuler::slotChordSlashType);

    m_chordSlashType = static_cast<ChordSlashType>(
                       Preferences::getPreference(ChordAnalysisGroup,
                                                  "chord_slash_type",
                                                  static_cast<unsigned>(
                                                  ChordSlashType::OFF)));

    switch (m_chordSlashType) {
        case ChordSlashType::OFF:
            m_slashOffAction->setChecked(true);
            break;

        case ChordSlashType::PITCH_OR_KEY:
            m_slashAbsRelAction->setChecked(true);
            break;

        case ChordSlashType::CHORD_TONE:
            m_slashChordToneAction->setChecked(true);
            break;
    }

    m_slashAddedBassAction = m_menu->addAction(tr("added_bass_type"));
    m_slashAddedBassAction->setText(tr(u8"Non-chord bass (e.g. E/B\u266d)"));
    m_slashAddedBassAction->setCheckable(true);
    connect(m_slashAddedBassAction, &QAction::triggered,
            this,                   &ChordNameRuler::slotChordAddedBass);
    m_slashAddedBassAction->setChecked(  Preferences
                                       ::getPreference(ChordAnalysisGroup,
                                                       "added_bass",
                                                       false));

    m_preferSlashChordsAction = m_menu->addAction(tr("prefer_slash_type"));
    m_preferSlashChordsAction->setText(tr(u8"Prefer slass chords (e.g."
                                       " Cm/A\u266d vs A\u266dmaj7)"));
    m_preferSlashChordsAction->setCheckable(true);
    connect(m_preferSlashChordsAction, &QAction::triggered,
            this,                      &ChordNameRuler::slotPreferSlashChords);

    m_preferSlashChordsAction->setChecked(  Preferences
                                          ::getPreference(ChordAnalysisGroup,
                                                          "prefer_slash_chords",
                                                           false));


    m_menu->addSeparator();


    m_cMajorFlats = m_menu->addAction(tr("c_major_flats"));
    m_cMajorFlats->setText(tr("C major flats"));
    m_cMajorFlats->setCheckable(true);
    connect(m_cMajorFlats, &QAction::triggered,
            this,          &ChordNameRuler::slotCMajorFlats);
    // Use ChordAnalysisGroup, not MatrixViewConfigGroup, so
    // notes in matrix editor grid can have different settings (and so
    // don't have to do live synchronization between the two).
    m_cMajorFlats->setChecked( Preferences
                              ::getPreference(ChordAnalysisGroup,
                                              "c_major_flats",
                                              true));

    m_offsetMinorKeys = m_menu->addAction(tr("offset_minors"));
    m_offsetMinorKeys->setText(tr(u8"Major in-key minor: 6,\u266f6/u266d7,7..."
                                    "\u266f5/u266d6, 9,10,11,0...7,8"));
    m_offsetMinorKeys->setCheckable(true);
    connect(m_offsetMinorKeys, &QAction::triggered,
            this,              &ChordNameRuler::slotOffsetMinors);
    // Use ChordAnalysisGroup, not MatrixViewConfigGroup, so
    // notes in matrix editor grid can have different settings (and so
    // don't have to do live synchronization between the two).
    m_offsetMinorKeys->setChecked(  Preferences
                                  ::getPreference(ChordAnalysisGroup,
                                                  "offset_minors",
                                                  false));

    m_altMinorKeys = m_menu->addAction(tr("alt_minors"));
    m_altMinorKeys->setText(tr(u8"Alternate in-key minor degrees: "
                                 "1,\u266f1/\u266d2,2,\u266f2/\u266d3,3"
                                 "...\u266f6/\u266d7,7"));
    m_altMinorKeys->setCheckable(true);
    connect(m_altMinorKeys, &QAction::triggered,
            this,           &ChordNameRuler::slotAltMinors);
    // Use ChordAnalysisGroup, not MatrixViewConfigGroup, so
    // notes in matrix editor grid can have different settings (and so
    // don't have to do live synchronization between the two).
    m_altMinorKeys->setChecked(  Preferences
                               ::getPreference(ChordAnalysisGroup,
                                               "alt_minors",
                                               false));

    m_nonDiatonicChords = m_menu->addAction(tr("c_major_flats"));
    m_nonDiatonicChords->setText(tr("Include non-diatonic chords"));
    m_nonDiatonicChords->setCheckable(true);
    connect(m_nonDiatonicChords, &QAction::triggered,
            this,                &ChordNameRuler::slotNonDiatonicChords);
    m_nonDiatonicChords->setChecked(Preferences::nonDiatonicChords.get());

    action = m_menu->addAction(tr("chord_name_styles"));
    action->setText(tr("Chord name styles..."));
    connect(action, &QAction::triggered,
            this,   &ChordNameRuler::slotSetNameStyles);


    if (m_keyChangingEnabled)
        setToolTip(tr("Right-click for key change insert, and chord name "
                      "type, menu.\n"
                      "Double-click for remove key change dialog."));
    else
        setToolTip(tr("Right-click for chord name type menu."));

}

void
ChordNameRuler::setCurrentSegment(const Segment *segment, bool forceRecalc)
{
    if (   (segment && segment != m_currentSegment && m_conflictingKeyChanges)
        || forceRecalc) {
        m_currentSegment = segment;

        if (isVisible()) analyzeChordsAndKeyChanges();

        // For matrix editor to redraw key highlight stripes if mode requres
        m_doc->signalKeySignaturesChanged(true);  // true==inserted, (arbitrary)

#if 0   // t4os -- done in analyzeChordsAndKeyChanges(), don't redo
        //         even though slightly nicer to do after signalKey
        emit chordAnalysisChanged();
#endif
    }
    else
        m_currentSegment = segment;
}

// Remove from m_segments
void
ChordNameRuler::removeSegment(const Segment *segment)
{
    auto found  = findSegment(segment);
    if (found != m_segments.end()) {
        Segment *foundSegment = found->segment;

        if (found->active) --m_numActiveSegments;

        disconnect(found->segment,
                   &Segment::contentsChanged,
                   this,
                   &ChordNameRuler::slotSegmentContentsChanged);

        m_segments.erase(found);

        if (m_segments.empty()) {
            m_currentSegment = nullptr;
            return;
        }

        if (foundSegment == m_currentSegment)
            m_currentSegment = m_segments[0].segment;
    }

    analyzeChordsAndKeyChanges();
}

void
ChordNameRuler::addSegment(Segment *segment)
{
    RosegardenDocument *document(RosegardenDocument::currentDocument);
    Composition &composition(document->getComposition());
    TrackId trackId = segment->getTrack();
    Track *track = composition.getTrackById(trackId);
    Instrument *instr = document->
                        getStudio().
                        getInstrumentById(track->getInstrument());
    // Only regular, not percussion segments
    if (instr->getKeyMapping()) return;

    m_segments.push_back(SegmentAndActive{segment, true});
    ++m_numActiveSegments;
    resetActiveSegmentIndices();  // Don't really need, only this only
                                  // used my main window which doesn't
                                  // allow choice of segments.
    m_currentSegment = segment;   // Don't really need, ibid.

    analyzeChordsAndKeyChanges();

    connect(segment, &Segment::contentsChanged,
            this,    &ChordNameRuler::slotSegmentContentsChanged);
}

// See documentation in .h file
const Key
ChordNameRuler::keyAtTime(const timeT t)
const
{
#if 0   // DEBUG
    std::cerr << "ChordNameRuler::keyAtTime("
              << t
              << ")  "
              << m_keys.size()
              << " keys:";
    for (auto key : m_keys)
        std::cerr << ' '
                  << key.first
                  << '/'
                  << key.second.getTonicPitch();
    std::cerr << std::endl;
#endif

    if (m_keys.empty()) return Key();
    auto upper = m_keys.upper_bound(t);
    if (upper != m_keys.begin())
        --upper;
    return upper->second;
}

// See documentation in .h file
timeT
ChordNameRuler::getNextKeyTime(
const timeT  currentKeyTime)
const
{
    Composition &composition(m_doc->getComposition());

    if (m_keys.size() <= 1)
        return composition.getMaxSegmentEndTime();

    if (currentKeyTime < m_keys.begin()->first)
        return m_keys.begin()->first;

    auto iter = m_keys.find(currentKeyTime);
    if (iter == m_keys.end() || ++iter == m_keys.end())
        return composition.getMaxSegmentEndTime();

    return iter->first;
}

// See documentation in .h file
int
ChordNameRuler::chordRootPitchAtTime(const timeT t)
const
{
    if (m_roots.empty()) return -1;
    auto upper = m_roots.upper_bound(t);
    if (upper != m_roots.begin())
        --upper;
    return upper->second;
}

// See documentation in .h file
void
ChordNameRuler::slotScrollHoriz(int x)
{
    m_currentXOffset = x;

#if 0   // Can't do these kinds of optimizations because is called
        // for zooming/scaling as well as panning and x doesn't change
        // in that case but redraw is still needed.
        // Should really have separate slot for zoom/scale.
    if (dx == 0)
        return ;

    if (dx > w*7 / 8 || dx < -w*7 / 8) {
        update();
        return ;
    }
#endif

    update();
}

void
ChordNameRuler::slotSegmentContentsChanged()
{
    const Segment *segment = dynamic_cast<const Segment*>(sender());
    auto found = findSegment(segment);
    if (found != m_segments.end() && found->active)
        analyzeChordsAndKeyChanges();
}

// Qt ruler internals
QSize
ChordNameRuler::sizeHint() const
{
    double width =
        m_rulerScale->getBarPosition(m_rulerScale->getLastVisibleBar()) +
        m_rulerScale->getBarWidth(m_rulerScale->getLastVisibleBar());

    RG_DEBUG << "sizeHint(): Returning chord-label ruler width as " << width;

    QSize res(std::max(int(width), m_width), m_height);

    return res;
}

// Qt ruler internals
QSize
ChordNameRuler::minimumSizeHint() const
{
    double firstBarWidth = m_rulerScale->getBarWidth(0);
    QSize res = QSize(int(firstBarWidth), m_height);
    return res;
}

// Notify parent, e.g. matrix editor so can re-label notes.
void
ChordNameRuler::slotInsertKeyChange()
{
    emit insertKeyChange();
}

// Notify parent, e.g. notation editor so can copy chord texts.
void
ChordNameRuler::slotCopyChords()
{
    emit copyChords();
}

// Invoke dialog for user to select segments' notes to use for chord analysis.
void
ChordNameRuler::slotChooseActiveSegments()
{
    // Shouldn't ever happen: Matrix and notation editors destruct
    // if and when all of their segments are removed in main window,
    // and main window's chord name ruler's menu doesn't have the
    // action connected to this slot.
    if (m_segments.empty()) return;

    std::vector<Segment*> segments;
    for (auto segment : m_segments) segments.push_back(segment.segment);

    QString header(tr("Construct chords from notes in segments:"));

    // Pop up dialog and abort if "Cancel" instead of "OK"
    if (!chooseSegments(header, segments)) return;

    // Set m_segments "active" flags  from user choices in dialog
    // Also check to make sure m_currentSegment is in m_segments,
    //   otherwise arbitrarily set to first one (unless empty in which
    //   case set to nullptr.)
    for (auto &seg : m_segments)
        seg.active = false;
    for (int index : m_activeSegmentIndices)
        m_segments[index].active = true;
    m_numActiveSegments = m_activeSegmentIndices.size();

    auto foundCurrent = findSegment(m_currentSegment);
    if (   foundCurrent != m_segments.end()   // should always be true
        || !foundCurrent->active)
        m_currentSegment = m_segments[0].segment;  // arbitrary choice

    analyzeChordsAndKeyChanges();

    // For matrix editor to redraw key highlight stripes if mode requres
    // Need to do regardless of m_conflictingKeyChanges because
    //   might have gone from conflicting to non-, or vice-versa.
    m_doc->signalKeySignaturesChanged(true);  // true==inserted, (arbitrary)

#if 0   // t4os -- already done in analyzeChordsAndKeyChanges();
        //         in analyzeChordsAndKeyChanges() despite being
        //         slightly nicer to wait until after signalKey
    emit chordAnalysisChanged();
#endif
}

void
ChordNameRuler::slotSetAlgorithm()
{
    if (sender() == m_quantizeAction)          m_doUnarpeggiation = false;
    else if (sender() == m_unarpeggiateAction) m_doUnarpeggiation = true;
    // nothing else possible

    Preferences::setPreference(ChordAnalysisGroup,
                               "chord_name_unarpeggiate",
                               m_doUnarpeggiation);

    analyzeChordsAndKeyChanges();
}

void
ChordNameRuler::slotSetQuantization()
{
#define IF_ELSE(ACTION, QUANTIZATION) \
    if (sender() == m_quantization##ACTION##NoteAction) \
        m_quantization = NoteDuration::QUANTIZATION
         IF_ELSE(None,         NONE);
    else IF_ELSE(Note128th,    DUR_128);
    else IF_ELSE(Note128thDot, DUR_128_DOT);
    else IF_ELSE(Note64th,     DUR_64);
    else IF_ELSE(Note64thdot,  DUR_64_DOT);
    else IF_ELSE(Note32nd,     DUR_32);
    else IF_ELSE(Note32ndDot,  DUR_32_DOT);
    else IF_ELSE(Note16th,     DUR_16);
    else IF_ELSE(Note16thDot,  DUR_16_DOT);
    else IF_ELSE(Eigth,        EIGTH);
    else IF_ELSE(EigthDot,     EIGTH_DOT);
    else IF_ELSE(Quarter,      QUARTER);
    else IF_ELSE(QuarterDot,   QUARTER_DOT);
    else IF_ELSE(Half,         HALF);
    else IF_ELSE(HalfDot,      HALF_DOT);
    else IF_ELSE(Whole,        WHOLE);
    else IF_ELSE(WholeDot,     WHOLE_DOT);
    else IF_ELSE(Double,       DOUBLE);
#undef IF_ELSE

    Preferences::setPreference(ChordAnalysisGroup,
                               "chord_name_quantization",
                               static_cast<unsigned>(m_quantization));

    m_quantizationTime = noteDurationToMidiTicks(m_quantization);
    analyzeChordsAndKeyChanges();
}

void
ChordNameRuler::slotSetUnarpeggiation()
{
#define IF_ELSE(ACTION, UNARPEGGIATION) \
    if (sender() == m_unarpeggiation##ACTION##NoteAction) \
        m_unarpeggiation = NoteDuration::UNARPEGGIATION
         IF_ELSE(Note128th,    DUR_128);
    else IF_ELSE(Note128thDot, DUR_128_DOT);
    else IF_ELSE(Note64th,     DUR_64);
    else IF_ELSE(Note64thdot,  DUR_64_DOT);
    else IF_ELSE(Note32nd,     DUR_32);
    else IF_ELSE(Note32ndDot,  DUR_32_DOT);
    else IF_ELSE(Note16th,     DUR_16);
    else IF_ELSE(Note16thDot,  DUR_16_DOT);
    else IF_ELSE(Eigth,        EIGTH);
    else IF_ELSE(EigthDot,     EIGTH_DOT);
    else IF_ELSE(Quarter,      QUARTER);
    else IF_ELSE(QuarterDot,   QUARTER_DOT);
    else IF_ELSE(Half,         HALF);
    else IF_ELSE(HalfDot,      HALF_DOT);
    else IF_ELSE(Whole,        WHOLE);
    else IF_ELSE(WholeDot,     WHOLE_DOT);
    else IF_ELSE(Double,       DOUBLE);
#undef IF_ELSE

    Preferences::setPreference(ChordAnalysisGroup,
                               "chord_name_unarpeggiation",
                               static_cast<unsigned>(m_unarpeggiation));

    m_unarpeggiationTime = noteDurationToMidiTicks(m_unarpeggiation);
    analyzeChordsAndKeyChanges();
}

void
ChordNameRuler::slotChordNameType()
{
    if (sender() == m_pitchLetterAction)
        m_chordNameType = ChordNameType::PITCH_LETTER;
    else if (sender() == m_pitchIntegerAction)
        m_chordNameType = ChordNameType::PITCH_INTEGER;
    else if (sender() == m_keyRomanNumeralAction)
        m_chordNameType = ChordNameType::KEY_ROMAN_NUMERAL;
    else if (sender() == m_keyIntegerAction)
        m_chordNameType = ChordNameType::KEY_INTEGER;
    else
        qWarning() << "ChordNameRuler::slotChordNameType() -- UNKNOWN";

    Preferences::setPreference(ChordAnalysisGroup,
                               "chord_name_type",
                               static_cast<unsigned>(m_chordNameType));

    m_analyzer->updateChordNameStyles();
    analyzeChordsAndKeyChanges();
}

void
ChordNameRuler::slotChordSlashType()
{
    if (sender() == m_slashOffAction)
        m_chordSlashType = ChordSlashType::OFF;
    else if (sender() == m_slashAbsRelAction)
        m_chordSlashType = ChordSlashType::PITCH_OR_KEY;
    else if (sender() == m_slashChordToneAction)
        m_chordSlashType = ChordSlashType::CHORD_TONE;
    else
        qWarning() << "ChordSlashRuler::slotChordSlashType() -- UNKNOWN";

    Preferences::setPreference(ChordAnalysisGroup,
                               "chord_slash_type",
                               static_cast<unsigned>(m_chordSlashType));

    m_analyzer->updateChordNameStyles();
    analyzeChordsAndKeyChanges();
}

void
ChordNameRuler::slotChordAddedBass()
{
    Preferences::setPreference(ChordAnalysisGroup,
                               "added_bass",
                               m_slashAddedBassAction->isChecked());

    m_analyzer->updateChordNameStyles();
    analyzeChordsAndKeyChanges();
}

void
ChordNameRuler::slotPreferSlashChords()
{
    Preferences::setPreference(ChordAnalysisGroup,
                               "prefer_slash_chords",
                               m_preferSlashChordsAction ->isChecked());

    m_analyzer->updateChordNameStyles();
    analyzeChordsAndKeyChanges();
}

void
ChordNameRuler::slotCMajorFlats()
{
    // Use ChordAnalysisGroup, not MatrixViewConfigGroup, so
    // notes in matrix editor grid can have different settings (and so
    // don't have to do live synchronization between the two).
    Preferences::setPreference(ChordAnalysisGroup,
                               "c_major_flats",
                               m_cMajorFlats->isChecked());

    m_analyzer->updateChordNameStyles();
    analyzeChordsAndKeyChanges();
}

void
ChordNameRuler::slotOffsetMinors()
{
    // Use ChordAnalysisGroup, not MatrixViewConfigGroup, so
    // notes in matrix editor grid can have different settings (and so
    // don't have to do live synchronization between the two).
    Preferences::setPreference(ChordAnalysisGroup,
                               "offset_minors",
                               m_offsetMinorKeys->isChecked());

    m_analyzer->updateChordNameStyles();
    analyzeChordsAndKeyChanges();
}

void
ChordNameRuler::slotAltMinors()
{
    // Use ChordAnalysisGroup, not MatrixViewConfigGroup, so
    // notes in matrix editor grid can have different settings (and so
    // don't have to do live synchronization between the two).
    Preferences::setPreference(ChordAnalysisGroup,
                               "alt_minors",
                               m_altMinorKeys->isChecked());

    m_analyzer->updateChordNameStyles();
    analyzeChordsAndKeyChanges();
}

void
ChordNameRuler::slotNonDiatonicChords()
{
    Preferences::nonDiatonicChords.set(m_nonDiatonicChords->isChecked());
    analyzeChordsAndKeyChanges();
}

void
ChordNameRuler::slotSetNameStyles()
{
    ChordNameStylesDialog chordNameStylesDialog(this);
    auto result = chordNameStylesDialog.exec();
    if (result == QDialog::Accepted) {
        m_analyzer->updateChordNameStyles();
        analyzeChordsAndKeyChanges();
    }
}

// Pop up dialog for user to select segments' notes to use for chord analysis.
// See slotChooseActiveSegments(), above.
bool
ChordNameRuler::chooseSegments(const QString               &header,
                               const std::vector<Segment*> &possibleSegments)
{
    if (possibleSegments.empty())
        return false;

    std::vector<QColor> foregrounds,
                        backgrounds;
    QStringList         segmentNames;

    for (Segment *segment : possibleSegments) {
        segmentNames << QString::fromStdString(segment->getLabel());

        QColor textColor = segment->getPreviewColour();
        foregrounds.push_back(textColor);
        backgrounds.push_back(segment->getColour());
    }

    CheckableListDialog dialog(this,
                               header,
                               segmentNames,
                               foregrounds,
                               backgrounds,
                               m_activeSegmentIndices);

    return dialog.exec() == QDialog::Accepted;
}

std::vector<ChordNameRuler::SegmentAndActive>::iterator
ChordNameRuler::findSegment(
const Segment *segment)
{
    struct Finder {
        Finder(const Segment *seg) : m_seg(seg) {};
        bool operator()(const SegmentAndActive &seg)
                       { return seg.segment == m_seg ; }
        const Segment *m_seg;
    };

    return std::find_if(m_segments.begin(),
                        m_segments.end(),
                        Finder(segment));
}

// Set all to inactive.
void
ChordNameRuler::resetActiveSegmentIndices()
{
    m_activeSegmentIndices.resize(m_segments.size());
    std::iota(m_activeSegmentIndices.begin(), m_activeSegmentIndices.end(), 0);
}

timeT
ChordNameRuler::noteDurationToMidiTicks(
const NoteDuration  noteDuration)
{
    static const std::map<NoteDuration, timeT> QUANTIZATION_TIMES = {
        {NoteDuration::NONE,           0},
        {NoteDuration::DUR_128,       30},
        {NoteDuration::DUR_128_DOT,   45},
        {NoteDuration::DUR_64,        60},
        {NoteDuration::DUR_64_DOT,    90},
        {NoteDuration::DUR_32,       120},
        {NoteDuration::DUR_32_DOT,   180},
        {NoteDuration::DUR_16,       240},
        {NoteDuration::DUR_16_DOT,   360},
        {NoteDuration::EIGTH,        480},
        {NoteDuration::EIGTH_DOT,    720},
        {NoteDuration::QUARTER,      960},
        {NoteDuration::QUARTER_DOT, 1440},
        {NoteDuration::HALF,        1920},
        {NoteDuration::HALF_DOT,    2880},
        {NoteDuration::WHOLE,       3840},
        {NoteDuration::WHOLE_DOT,   5760},
        {NoteDuration::DOUBLE,      7680},
    };

    auto quantizationIter = QUANTIZATION_TIMES.find(noteDuration);
    if (quantizationIter == QUANTIZATION_TIMES.end())
        return 0;
    else
        return quantizationIter->second;
}

// Analyze chords and redraw ruler.
void ChordNameRuler::analyzeChordsAndKeyChanges(bool emitSignal)
{
    m_chordAndKeyNames->clear();
    m_keys.clear();
    m_roots.clear();

    std::vector<Segment*> segments;
    for (auto segment : m_segments)
        if (segment.active) segments.push_back(segment.segment);

    m_analyzer->labelChords(*m_chordAndKeyNames,
                            m_keys,
                            m_roots,
                            m_conflictingKeyChanges,
                            segments,
                            m_currentSegment,
                            m_doUnarpeggiation ? m_unarpeggiationTime
                                               : m_quantizationTime,
                            m_doUnarpeggiation);

    update();

    if (emitSignal) emit chordAnalysisChanged();
}

// Pop up menu.
void
ChordNameRuler::mousePressEvent(QMouseEvent *e)
{
    if (e->button() == Qt::RightButton)
        m_menu->exec(QCursor::pos());
}

// Remove key change event.
// Clumsy user interface: Removes event at or before time at mouse
//   click time, even if way before time (including offscreen to left).
// Really would prefer only if clicked on displayed key change
//   text but difficult to implement (needs feedback about font size,
//   displayed width of text, etc.)
// Accepting this implementation because canonical case is user clicking
//   on text, and even if not always pops up up dialog to specify which
//   of possible multiple key changes in different segments to delete, so
//   no risk of accidentally deleting off-screen key change(s).
void
ChordNameRuler::mouseDoubleClickEvent(QMouseEvent *e)
{
    if (!m_keyChangingEnabled) return;

    int   clickX       = e->pos().x();
    timeT clickedAt    = m_rulerScale->getTimeForX(clickX + m_currentXOffset);

    // Can't happen. Early abort above if in main window, and
    // impossible to be in matrix or notation editor without sesgments
    if (m_segments.empty()) return;

    // Segment::getKeyAtTime() searches backwards in time for key change.
    // Only want those which match the most recent in any segment.
    timeT maxKeyTime = std::numeric_limits<timeT>::min();
    for (auto segment : m_segments) {
        timeT keyTime;
        Key key = segment.segment->getKeyAtTime(clickedAt, keyTime);

        if (key.getEvent() == nullptr)
            continue;
        else if (keyTime > maxKeyTime) maxKeyTime = keyTime;
    }

    if (maxKeyTime == std::numeric_limits<timeT>::min()) {
        QMessageBox messageBox;
        messageBox.setText(tr("No key changes to delete at or\n"
                              "before this time (or only implicit\n"
                              "C major at composition start)."));
        messageBox.addButton(QMessageBox::Ok);
        messageBox.exec();
        return;
    }

    // Pop up dialog

    std::vector<QColor> foregrounds,
                        backgrounds;

    QStringList               keyChangesStrings;
    std::vector<Segment*>     keyChangeSegments;
    std::vector<const Event*> keyChangeEvents;

    for (auto segment : m_segments) {
        timeT keyTime;
        Key          key   = segment.segment->getKeyAtTime(clickedAt, keyTime);
        const Event *event = key.getEvent();

        if (keyTime != maxKeyTime || event == nullptr) continue;

        keyChangeSegments.push_back(segment.segment);
        keyChangeEvents  .push_back(event);

        keyChangesStrings << (   QString::fromStdString(key.getName())
                           + tr(" in segment ")
                           + QString::fromStdString(  segment
                                                     .segment
                                                    ->getLabel()));

        QColor textColor = segment.segment->getPreviewColour();
        foregrounds.push_back(textColor);
        backgrounds.push_back(segment.segment->getColour());
    }

    Composition &comp(RosegardenDocument::currentDocument->getComposition());
    QString timeString(comp.getMusicalTimeStringForAbsoluteTime(maxKeyTime));
    QString header( tr("Remove key change at time %1 in segments:")
                   .arg(timeString));
    std::vector<unsigned> selecteds;

    selecteds.resize(keyChangesStrings.size());
    std::iota(selecteds.begin(), selecteds.end(), 0);  // init all checked on

    CheckableListDialog dialog(this,
                               header,
                               keyChangesStrings,
                               foregrounds,
                               backgrounds,
                               selecteds);

    // Whether user clicked "OK" vs "Cancel"
    if (dialog.exec() == QDialog::Accepted && !selecteds.empty()) {
        MacroCommand *macroCommand = new MacroCommand(tr("Remove key changes"));
        for (int selected : selecteds) {
            EventSelection    *eventSelection
                            = new EventSelection(*keyChangeSegments[selected]);
            eventSelection->addEvent(const_cast<Event*>(
                                     keyChangeEvents[selected]));

            EraseCommand   *eraseCommand
                         = new EraseCommand(eventSelection, nullptr);
            macroCommand->addCommand(eraseCommand);
        }
        CommandHistory::getInstance()->addCommand(macroCommand);

        analyzeChordsAndKeyChanges();
        emit chordAnalysisChanged();
    }

}  // ChordNameRuler::mouseDoubleClickEvent(QMouseEvent *e)


// Draw/redraw ruler.
// No longer has "push back" of chord and key change texts if zoom
//   level doesn't allow enough space. Only displays when/where/if
//   space allows.
// Rationale: Better to leave out information than to display false
//   information (chord/keychange at incorrect time). Plus in use
//   often displayed chords/keys going off to "infinity and beyond"
//   to right in main window / track editor due its canonical zoom
//   level, particularly with new chord analysis algorithms which
//   generate many more chords (beyond triads and simple sevenths)
//   than pre-rewrite version.
// But always does display tick mark for chords or full-height bar for
//   key changes even if not displaying text, so information to user
//   not totally lost.
// Have experimented with displaying key changes in preference to
//   chord names (when space conflicts) but full implementation would
//   require O(N-squared) search at each text event, and code is
//   performance-critical (per mouse movement when scrolling).
// Plus really should redesign UI to have separate key change and
//   chord name rulers, and/or other way to always display current
//   key regardless of whether most recent key change is visible
//   given current UI zoom/scroll. (Yes, notation editor also loses
//   key change display in staves when scrolling, but centuries-old
//   prior practice with printed sheet music has key signature at
//   beginning of each staff as measures are broken up by page width
//   constraints.)
void
ChordNameRuler::paintEvent(QPaintEvent* e)
{
    if (!m_composition)
        return ;

    QPainter paint(this);

    // In a stylesheet world...  Yadda yadda.  Fix the stupid background to
    // rescue it from QWidget Hack Black. (Ahhh.  Yes, after being thwarted for
    // a month or something, I'm really enjoying getting this solved.)
    QBrush bg = QBrush(GUIPalette::getColour(GUIPalette::ChordNameRulerBackground));
    paint.fillRect(e->rect(), bg);

    paint.setPen(GUIPalette::getColour(GUIPalette::ChordNameRulerForeground));

    paint.setClipRegion(e->region());
    paint.setClipRect(e->rect().normalized());

    QRect clipRect = paint.clipRegion().boundingRect();

    if (!m_chordAndKeyNames)
        return ;

    int   beginX    = m_currentXOffset,
          endX      = beginX + clipRect.width();
    timeT beginTime = m_rulerScale->getTimeForX(clipRect.x() + beginX),
          endTime   = m_rulerScale->getTimeForX(clipRect.x() + endX);

    QRect     boundsForHeight = m_fontMetrics.boundingRect("^j|lM");
    const int fontHeight      = boundsForHeight.height(),
              textY           = (height() - 6) / 2 + fontHeight / 2,
              keySpacing      = m_boldFont.pixelSize() / 2;
    int       currentX        = clipRect.x();

    for (Segment::iterator   eventIter  =   m_chordAndKeyNames
                                          ->findTime(beginTime);
                             eventIter != m_chordAndKeyNames->findTime(endTime);
                           ++eventIter) {

        if (   !(*eventIter)->isa(Text::EventType)
            || !(*eventIter)->has(Text::TextPropertyName)
            || !(*eventIter)->has(Text::TextTypePropertyName))
            continue;

        std::string text((*eventIter)->get<String>(Text::TextPropertyName)),
                    type((*eventIter)->get<String>(Text::TextTypePropertyName));


        int textX =   static_cast<int>(
                      std::round(  m_rulerScale
                                 ->getXForTime(  (*eventIter)
                                               ->getAbsoluteTime())))
                    - m_currentXOffset;

        if (textX < currentX) {
            if (type == Text::KeyName)
                paint.drawLine(textX, 0, textX, height());
            else
                paint.drawLine(textX, height() - 4, textX, height());
            if (textX > currentX)
                currentX = textX;
            continue;
        }

        currentX = textX;

        QRect textBounds;

        if (type == Text::KeyName) {
            paint.drawLine(currentX, 0, currentX, height());
            paint.setFont(m_boldFont);
            currentX += keySpacing;
            paint.drawText(currentX, textY, strtoqstr(text));
            textBounds = m_boldFontMetrics.boundingRect(strtoqstr(text));
        }
        else {
            paint.drawLine(currentX, height() - 4, currentX, height());
            paint.setFont(m_font);
            paint.drawText(currentX, textY, strtoqstr(text));
            textBounds = m_fontMetrics.boundingRect(strtoqstr(text));
        }

        if ((currentX += textBounds.width()) > endX) break;
    }

}  // ChordNameRuler::paintEvent(QPaintEvent* e)

}  // namespace Rosegarden
