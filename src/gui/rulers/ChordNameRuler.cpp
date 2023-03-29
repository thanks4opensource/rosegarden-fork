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

#define RG_MODULE_STRING "[ChordNameRuler]"

#define RG_NO_DEBUG_PRINT 1

#include "ChordNameRuler.h"

#include "base/AnalysisTypes.h"
#include "base/Composition.h"
#include "base/ConflictingKeyChanges.h"
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

#ifdef CHORD_NAME_RULER_AND_CONFLICTING_KEY_CHANGES_DEBUG
#include <iomanip>
#endif

#include <limits>


namespace Rosegarden
{

// See documentation in .h file
ChordNameRuler::ChordNameRuler(QWidget *parent,
                               RulerScale *rulerScale,
                               RosegardenDocument *doc,
                               const ParentEditor parentEditor,
                               int height,
                               ConflictingKeyChanges *conflictingKeyChanges) :
        QWidget                          (parent),
        m_doc                            (doc),
        m_analyzer                       (new ChordAnalyzer),
        m_parentEditor                   (parentEditor),
        m_conflictingKeyChanges          (conflictingKeyChanges),
        m_ownConflictingKeyChanges       (!conflictingKeyChanges),
        m_showChords                     (parentEditor != ParentEditor::TRACK),
        m_showKeyChanges                 (true),
        m_hideKeyChanges                 (false),
        m_height                         (height),
        m_currentXOffset                 (0),
        m_width                          ( -1),
        m_rulerScale                     (rulerScale),
        m_composition                    (&doc->getComposition()),
        m_menu                           (nullptr),
        m_showChordsAction               (nullptr),
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
        m_insertKeyChangeAction          (nullptr),
        m_chordCopyAction                (nullptr),
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
        m_currentSegment                 (nullptr),
        m_isVisible                      (false),
        m_analyzeWhileHidden             (false),
        m_doUnarpeggiation               (false),
        m_keysDirty                      (true),
        m_notesDirty                     (true),
        m_chordsDirty                    (true),
        m_namingModeChanged              (false),
        m_keyDependent                   (true),  // will be set in init()
        m_quantization                   (NoteDuration::NONE),
        m_unarpeggiation                 (NoteDuration::QUARTER),
        m_quantizationTime               (0),
        m_unarpeggiationTime             (0),
        m_chordNameType                  (ChordNameType::PITCH_LETTER),
        m_chordSlashType                 (ChordSlashType::OFF),
        m_fontMetrics                    (m_font),
        m_boldFontMetrics                (m_boldFont)
{
    std::vector<Segment*> segments;
    for (Segment *segment : *m_composition) segments.push_back(segment);
    init(segments);
}

// See documentation in .h file
ChordNameRuler::ChordNameRuler(QWidget *parent,
                               RulerScale *rulerScale,
                               RosegardenDocument *doc,
                               const std::vector<Segment *> &segments,
                               const ParentEditor parentEditor,
                               int height,
                               ConflictingKeyChanges *conflictingKeyChanges) :
        QWidget                          (parent),
        m_doc                            (doc),
        m_analyzer                       (new ChordAnalyzer),
        m_parentEditor                   (parentEditor),
        m_conflictingKeyChanges          (conflictingKeyChanges),
        m_ownConflictingKeyChanges       (!conflictingKeyChanges),
        m_showChords                     (parentEditor != ParentEditor::TRACK),
        m_showKeyChanges                 (true),
        m_hideKeyChanges                 (false),
        m_height                         (height),
        m_currentXOffset                 (0),
        m_width                          ( -1),
        m_rulerScale                     (rulerScale),
        m_composition                    (&doc->getComposition()),
        m_menu                           (nullptr),
        m_showChordsAction               (nullptr),
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
        m_insertKeyChangeAction          (nullptr),
        m_chordCopyAction                (nullptr),
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
        m_currentSegment                 (nullptr),
        m_isVisible                      (false),
        m_analyzeWhileHidden             (false),
        m_doUnarpeggiation               (false),
        m_keysDirty                      (true),
        m_notesDirty                     (true),
        m_chordsDirty                    (true),
        m_namingModeChanged              (false),
        m_keyDependent                   (true),  // will be set in init()
        m_quantization                   (NoteDuration::NONE),
        m_unarpeggiation                 (NoteDuration::QUARTER),
        m_quantizationTime               (0),
        m_unarpeggiationTime             (0),
        m_chordNameType                  (ChordNameType::PITCH_LETTER),
        m_chordSlashType                 (ChordSlashType::OFF),
        m_fontMetrics                    (m_font),
        m_boldFontMetrics                (m_boldFont)
{
    // Note won't be in this constructor if m_parentEditor == TRACK
    init(segments);
}

ChordNameRuler::~ChordNameRuler()
{
    for (const auto &segment : m_segments)
        if (m_parentEditor != ParentEditor::MATRIX)
            const_cast<Segment*>(segment.first)->removeObserver(this);
        // else was never added as observer if MATRIX

    if (m_ownConflictingKeyChanges) delete m_conflictingKeyChanges;

    delete m_analyzer;
}

void ChordNameRuler::init(
const std::vector<Segment*> &segments)
{
#ifdef CHORD_NAME_RULER_AND_CONFLICTING_KEY_CHANGES_DEBUG
    qDebug() << "ChordNameRuler::init() @"
             << static_cast<void*>(this)
             << ' '
             << segments.size()
             << "segments  conflicting:"
             << static_cast<void*>(m_conflictingKeyChanges)
             << m_ownConflictingKeyChanges;
#endif

    // Must call at least once because ctor of static ChordMaps can't
    // because Prefernces not set up yet at that time.
    m_analyzer->updateChordNameStyles();

    if (!m_conflictingKeyChanges)  // i.e. m_ownConflictingKeyChanges == true
        m_conflictingKeyChanges = new ConflictingKeyChanges(segments);

    for (Segment *segment : segments)
        addSegment(segment);

    m_font.setPointSize(11);
    m_font.setPixelSize(12);
    m_boldFont.setPointSize(11);
    m_boldFont.setPixelSize(12);
    m_boldFont.setBold(true);
    m_fontMetrics     = QFontMetrics(m_font);
    m_boldFontMetrics = QFontMetrics(m_boldFont);

    createMenuAndToolTip();

    if (m_parentEditor != ParentEditor::MATRIX)
        connect( CommandHistory::getInstance(),
                &CommandHistory::commandExecuted,
                 this,
                &ChordNameRuler::slotCommandExecuted);
    // else matrix editor calls slots here  explicitly,
    // in desired order with note label, etc. updates
}  // init()

void
ChordNameRuler::updateToolTip()
{
    if (m_parentEditor == ParentEditor::TRACK)  // is in main window
        setToolTip(tr("Use Chord/Key Ruler in matrix or notation\n"
                      "editor to analyze chords and add/remove\n"
                      "key signature changes."));
    else if (m_showKeyChanges)
        setToolTip(tr("Right-click for menu.\n"
                      "Hold middle mouse button to hide key changes.\n"
                      "Double-click to remove key change."));
    else
        setToolTip(tr("Right-click for menu.\n"
                      "Double-click to remove key change."));
}

void
ChordNameRuler::createMenuAndToolTip()
{
    if (m_menu) return;

    updateToolTip();

    m_menu = new QMenu(tr("Key changes and chord names"), this);
    QAction *action;

    QLabel          *label;
    QWidgetAction   *labelWidgetAction;
    QString          style(QString("QLabel {color: gray; "
                                   "padding-left: 1;}"));   // indent a little

    m_showChordsAction = m_menu->addAction("do_chords");
    m_showChordsAction->setText(tr("Analyze chords"));
    m_showChordsAction->setCheckable(true);
    m_showChordsAction->setChecked(m_showChords);
    connect(m_showChordsAction, &QAction::triggered,
            this,               &ChordNameRuler::slotShowChords);

    action = m_menu->addAction(tr("choose_active_segments"));
    action->setText(tr("Choose chord note segments..."));
    connect(action, &QAction::triggered,
            this,   &ChordNameRuler::slotChooseActiveSegments);

    if (m_parentEditor == ParentEditor::NOTATION) {
        m_chordCopyAction = m_menu->addAction(tr("copy_chords_to_text"));
        m_chordCopyAction->setText(tr("Copy chords to text"));
        connect(m_chordCopyAction, &QAction::triggered,
                this,              &ChordNameRuler::slotCopyChords);
    }

    label = new QLabel(tr("Analysis mode:"));
    label->setStyleSheet(style);
    label->setForegroundRole(QPalette::ButtonText);
    labelWidgetAction = new QWidgetAction(this);
    labelWidgetAction->setDefaultWidget(label);
    m_menu->addAction(labelWidgetAction);

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


    QMenu *quantizeMenu = m_menu->addMenu(tr("Note quantization time"));
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

    m_nonDiatonicChords = m_menu->addAction(tr("non_diatonic"));
    m_nonDiatonicChords->setText(tr("Include non-diatonic chords"));
    m_nonDiatonicChords->setCheckable(true);
    connect(m_nonDiatonicChords, &QAction::triggered,
            this,                &ChordNameRuler::slotNonDiatonicChords);
    m_nonDiatonicChords->setChecked(Preferences::nonDiatonicChords.get());

    label = new QLabel(tr("Chord root names:"));
    label->setStyleSheet(style);
    label->setForegroundRole(QPalette::ButtonText);
    labelWidgetAction = new QWidgetAction(this);
    labelWidgetAction->setDefaultWidget(label);
    m_menu->addAction(labelWidgetAction);

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
            m_keyDependent = false;
            break;

        case ChordNameType::PITCH_INTEGER:
            m_pitchIntegerAction->setChecked(true);
            m_keyDependent = false;
            break;

        case ChordNameType::KEY_ROMAN_NUMERAL:
            m_keyRomanNumeralAction->setChecked(true);
            m_keyDependent = true;
            break;

        case ChordNameType::KEY_INTEGER:
            m_keyIntegerAction->setChecked(true);
            m_keyDependent = true;
            break;
    }

    label = new QLabel(tr("Slash chords:"));
    label->setStyleSheet(style);
    label->setForegroundRole(QPalette::ButtonText);
    labelWidgetAction = new QWidgetAction(this);
    labelWidgetAction->setDefaultWidget(label);
    m_menu->addAction(labelWidgetAction);

    QActionGroup *slashNameActionGroup = new QActionGroup(m_menu);

    m_slashOffAction = m_menu->addAction(tr("pitch_or_key_type"));
    m_slashOffAction->setText(tr("Off"));
    m_slashOffAction->setCheckable(true);
    slashNameActionGroup->addAction(m_slashOffAction);
    connect(m_slashOffAction, &QAction::triggered,
            this,             &ChordNameRuler::slotChordSlashType);

    m_slashAbsRelAction = m_menu->addAction(tr("pitch_or_key_type"));
    m_slashAbsRelAction->setText(tr("Concert pitch or in-key"));
    m_slashAbsRelAction->setCheckable(true);
    slashNameActionGroup->addAction(m_slashAbsRelAction);
    connect(m_slashAbsRelAction, &QAction::triggered,
            this,                &ChordNameRuler::slotChordSlashType);

    m_slashChordToneAction = m_menu->addAction(tr("chord_tone_type"));
    m_slashChordToneAction->setText(tr("Chord root relative (requires "
                                       "in-key roman or integer)"));
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
    m_preferSlashChordsAction->setText(tr(u8"Prefer slash chords (e.g."
                                       " Cm/A\u266d not A\u266dmaj7)"));
    m_preferSlashChordsAction->setCheckable(true);
    connect(m_preferSlashChordsAction, &QAction::triggered,
            this,                      &ChordNameRuler::slotPreferSlashChords);

    m_preferSlashChordsAction->setChecked(  Preferences
                                          ::getPreference(ChordAnalysisGroup,
                                                          "prefer_slash_chords",
                                                           false));

    setSlashChordMenus();  // Fix chord slash type as per chord name type


    label = new QLabel(tr("Chord name options:"));
    label->setStyleSheet(style);
    label->setForegroundRole(QPalette::ButtonText);
    labelWidgetAction = new QWidgetAction(this);
    labelWidgetAction->setDefaultWidget(label);
    m_menu->addAction(labelWidgetAction);

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

    action = m_menu->addAction(tr("chord_name_styles"));
    action->setText(tr("Chord name styles..."));
    connect(action, &QAction::triggered,
            this,   &ChordNameRuler::slotSetNameStyles);


    // Key changes

    m_menu->addSeparator();

    m_showKeysAction = m_menu->addAction("do_keys");
    m_showKeysAction->setText(tr("Show key changes"));
    m_showKeysAction->setCheckable(true);
    m_showKeysAction->setChecked(m_showKeyChanges);
    connect(m_showKeysAction, &QAction::triggered,
            this,               &ChordNameRuler::slotShowKeys);

    m_insertKeyChangeAction = m_menu->addAction(tr("insert_key_change"));
    m_insertKeyChangeAction->setText(tr("Insert Key Change at "
                                        "Playback Position..."));
    connect(m_insertKeyChangeAction, &QAction::triggered,
            this,                    &ChordNameRuler::slotInsertKeyChange);

}  // createMenuAndToolTip()

void
ChordNameRuler::setSlashChordMenus()
{
    if (   m_chordNameType == ChordNameType::PITCH_LETTER
        || m_chordNameType == ChordNameType::PITCH_INTEGER)
    {
        // Disable because has no effect.
        m_slashChordToneAction->setChecked(false);
        m_slashChordToneAction->setEnabled(false);

        if (m_chordSlashType == ChordSlashType::CHORD_TONE) {
            // Leave slash chords on instead of turning off, but set
            //   to workable mode.
            // Unchecks m_slashChordToneAction (??)
            m_slashAbsRelAction->setChecked(true);
            m_chordSlashType = ChordSlashType::PITCH_OR_KEY;
            // Save state
            Preferences::setPreference(ChordAnalysisGroup,
                                       "chord_slash_type",
                                       static_cast<unsigned>(m_chordSlashType));
        }
        // Else leave as is: ChordSlashType::OFF or PITCH_OR_KEY
    }
    else   // is KEY_ROMAN_NUMERAL or KEY_INTEGER
        // In case had been disabled.
        m_slashChordToneAction->setEnabled  (true);
}

void
ChordNameRuler::setVisible(
bool visible)
{
#ifdef CHORD_NAME_RULER_AND_CONFLICTING_KEY_CHANGES_DEBUG
    qDebug() << "ChordNameRuler::setVisible()"
             << (  m_parentEditor == ParentEditor::TRACK ? "TRACK"
                 : m_parentEditor == ParentEditor::MATRIX ? "MATRIX"
                 : m_parentEditor == ParentEditor::NOTATION ? "NOTATION"
                 :                                            "<unknown>")
             << '@'
             << static_cast<void*>(this)
             << (visible ? "visible" : "invisible");
#endif

    // Need this separate/internal flag because at start of parent
    // editor QWidget::isVisible() returns false even though
    // QWidget::setVisible(true) was called.
    m_isVisible = visible;

    QWidget::setVisible(visible);

    if (visible) analyzeChords();
}

void
ChordNameRuler::setAnalyzeWhileHidden(
const bool analyze)
{
#ifdef CHORD_NAME_RULER_AND_CONFLICTING_KEY_CHANGES_DEBUG
    qDebug() << "ChordNameRuler::setAnalyzeWhileHidden()"
             << (  m_parentEditor == ParentEditor::TRACK ? "TRACK"
                 : m_parentEditor == ParentEditor::MATRIX ? "MATRIX"
                 : m_parentEditor == ParentEditor::NOTATION ? "NOTATION"
                 :                                            "<unknown>")
             << '@'
             << static_cast<void*>(this)
             << analyze;
#endif
    m_analyzeWhileHidden = analyze;
    if (analyze) analyzeChords();
}

void
ChordNameRuler::setCurrentSegment(const Segment *segment)
{
#ifdef CHORD_NAME_RULER_AND_CONFLICTING_KEY_CHANGES_DEBUG
    qDebug() << "ChordNameRuler::setCurrentSegment()"
             << (  m_parentEditor == ParentEditor::TRACK ? "TRACK"
                 : m_parentEditor == ParentEditor::MATRIX ? "MATRIX"
                 : m_parentEditor == ParentEditor::NOTATION ? "NOTATION"
                 :                                            "<unknown>")
             << (m_segments.find(segment) == m_segments.end() ? "UNKNOWN!"
                                                              : "known")
             << "\n  segs/conflicts:"
             << m_conflictingKeyChanges->numSegments()
             << m_conflictingKeyChanges->numConflicts()
             << "conflicts\n  was:"
             << static_cast<const void*>(m_currentSegment)
             << (m_currentSegment ? m_currentSegment->getLabel() : "<nullptr>")
             << "\n  new:"
             << static_cast<const void*>(segment)
             << (segment ? segment->getLabel() : "<nullptr>");
#endif

    if (segment == m_currentSegment)
        return;

    if (m_segments.find(segment) == m_segments.end())
        return;  // shouldn't ever be called with segment before addSegment()

    if (!segment || !m_currentSegment)
        m_chordsDirty = m_keysDirty = true;
    else if (   m_conflictingKeyChanges
             && m_conflictingKeyChanges->hasConflicts()
             && m_conflictingKeyChanges->areConflicting(m_currentSegment,
                                                        segment))
    {
#ifdef CHORD_NAME_RULER_AND_CONFLICTING_KEY_CHANGES_DEBUG
        qDebug() << "  conflicting";
#endif
        m_keysDirty = true;
        if (m_keyDependent && m_isVisible)  // not needed if hidden in matrix
            m_chordsDirty = true;
    }

    m_currentSegment = segment;  // after above conflicting check

    if (!segment) {
        m_chords.clear();
        m_roots .clear();
        update();
    }
    else if (m_parentEditor != ParentEditor::MATRIX)
        analyzeChords();   // matrix editor handles separately
}  // setCurrentSegment()

// Remove from m_segments
void
ChordNameRuler::removeSegment(Segment *segment)
{
    auto found = m_segments.find(segment);

    if (found == m_segments.end()) return;

    m_conflictingKeyChanges->removeSegment(segment);

    if (segment->size() && found->second.active)  // Segment had notes that
        m_notesDirty = true;                      // were being used for
                                                  // chord analysis.

    const Segment *prevCurrentSegment = m_currentSegment;

    if (found->first == m_currentSegment) {
        if (m_segments.size() == 1)
            m_currentSegment = nullptr;
        else
            m_currentSegment = m_segments.begin()->first;
    }

    m_segments.erase(found);

    removeOrderedSegment(segment);

    if (m_parentEditor != ParentEditor::MATRIX)
        segment->removeObserver(this);
    // else was never added as observer if MATRIX

    bool conflicting = (  m_currentSegment && prevCurrentSegment
                        ? m_conflictingKeyChanges->areConflicting(
                                                    prevCurrentSegment,
                                                    m_currentSegment)
                        : false);
    if (conflicting) m_keysDirty = true;

        m_conflictingKeyChanges->removeSegment(segment);

    if (m_segments.empty()) {
        m_chords.clear();
        m_roots.clear();
        return;
    }

    if (m_currentSegment == prevCurrentSegment)
        return;

    if (conflicting) analyzeChords();
}

void
ChordNameRuler::segmentDeleted(     // SegmentObserver
const Segment *segment)
{
    removeSegment(const_cast<Segment*>(segment));
}

void
ChordNameRuler::addSegment(Segment *segment)
{
    if (!segment) return;

    // Do want to know about all key changes, even in edge-case percussion
    // segments.
    m_conflictingKeyChanges->addSegment(segment);

    // Add to m_segments with appropriate active flag.
    // See resetPercussionSegments() for why need own isPercussion.
    bool    isPercussion = segment->isPercussion();
    m_segments[segment].active       = !isPercussion;
    m_segments[segment].isPercussion =  isPercussion;

    // Do put in ordered segments because need full set to regenerate
    // in edge cases of changing between percussion and normal.
    m_orderedSegments.push_back(segment);

    // But don't connect percussion segments. If ever change will
    // regenerate and connect as necessary in resetPercussionSegments().
    if (m_parentEditor != ParentEditor::MATRIX) {
        // Still doing hybrid of both Observer notifications ...
        segment->addObserver(this);
        // ... and Qt signals
        connect(segment, &Segment       ::keyAddedOrRemoved,
                this,    &ChordNameRuler::slotKeyAddedOrRemoved);
        if (!isPercussion) {
            connect(segment, &Segment       ::noteAddedOrRemoved,
                    this,    &ChordNameRuler::slotNoteAddedOrRemoved);
            connect(segment, &Segment       ::noteModified,
                    this,    &ChordNameRuler::slotNoteModified);
        }
    }
}  // addSegment

void
ChordNameRuler::resetPercussionSegments()
{
    for (const Segment *segment : m_orderedSegments) {
        bool    wasPercussion = m_segments[segment].isPercussion,
                nowPercussion =            segment->isPercussion();

        if (nowPercussion != wasPercussion) {
            if (wasPercussion) {
                disconnect(segment, &Segment        ::noteAddedOrRemoved,
                           this,    &ChordNameRuler::slotNoteAddedOrRemoved);
                disconnect(segment, &Segment        ::noteModified,
                           this,    &ChordNameRuler::slotNoteModified);
                // Leave keyAddedOrRemoved connected.
            }
            else {  // Must be isPercussion
                connect(segment, &Segment       ::noteAddedOrRemoved,
                        this,    &ChordNameRuler::slotNoteAddedOrRemoved);
                connect(segment, &Segment       ::noteModified,
                        this,    &ChordNameRuler::slotNoteModified);
            }

            m_segments[segment].isPercussion =  nowPercussion;
            m_segments[segment].active       = !nowPercussion;
        }
    }

    // Brute force, might not be necessary at all, but this is
    // rare, non-realtime, edge case.
    m_notesDirty = m_keysDirty = m_chordsDirty = true;
    analyzeChords();
}

void
ChordNameRuler::removeOrderedSegment(
const Segment *segment)
{
    auto found = std::find(m_orderedSegments.begin(),
                           m_orderedSegments.end(),
                           segment);
    if (found != m_orderedSegments.end())
        // Expensive linear time, but rarely used.
        m_orderedSegments.erase(found);
}

void
ChordNameRuler::slotNoteAddedOrRemoved(
const Segment   *segment,
const Event     *event,
bool             added)
{
#ifdef CHORD_NAME_RULER_AND_CONFLICTING_KEY_CHANGES_DEBUG
    qDebug() << "ChordNameRuler::slotNoteAddedOrRemoved()"
             << (  m_parentEditor == ParentEditor::TRACK ? "TRACK"
                 : m_parentEditor == ParentEditor::MATRIX ? "MATRIX"
                 : m_parentEditor == ParentEditor::NOTATION ? "NOTATION"
                 :                                            "<unknown>")
             << (segment ? segment->getLabel() : "<nullptr>")
             << (added   ? "added"             : "removed");
#endif

    if (segment->isPercussion())  // Safety check, shouldn't ever happen.
        return;

    auto found = m_segments.find(segment);
    if (found == m_segments.end()) return;  // Can't happen, but check anyway

    if (added) found->second.   addNote(event);  // Update min/max/longest notes
    else       found->second.removeNote(event);  //   "     "   "     "      "

    if (found->second.active)
        m_notesDirty = true;
}

void
ChordNameRuler::slotNoteModified(
const Segment   *segment,
const Event     *event)
{
#ifdef CHORD_NAME_RULER_AND_CONFLICTING_KEY_CHANGES_DEBUG
    qDebug() << "\nChordNameRuler::slotNoteModified()"
             << (  m_parentEditor == ParentEditor::TRACK ? "TRACK"
                 : m_parentEditor == ParentEditor::MATRIX ? "MATRIX"
                 : m_parentEditor == ParentEditor::NOTATION ? "NOTATION"
                 :                                            "<unknown>")
             << (segment ? segment->getLabel() : "<nullptr>");
#endif

    if (segment->isPercussion())  // Safety check, shouldn't ever happen.
        return;

    auto found = m_segments.find(segment);
    if (found == m_segments.end()) return;  // Can't happen, but check anyway

    found->second.addNote(event);  // Update min/max/longest notes

    if (found->second.active)
        m_notesDirty = true;
}

void
ChordNameRuler::slotKeyAddedOrRemoved(
const Segment   *segment,
const Event     *,
#ifdef CHORD_NAME_RULER_AND_CONFLICTING_KEY_CHANGES_DEBUG
bool            added
#else
bool
#endif
)
{
#ifdef CHORD_NAME_RULER_AND_CONFLICTING_KEY_CHANGES_DEBUG
    qDebug() << "ChordNameRuler::slotKeyAddedOrRemoved()"
             << (  m_parentEditor == ParentEditor::TRACK ? "TRACK"
                 : m_parentEditor == ParentEditor::MATRIX ? "MATRIX"
                 : m_parentEditor == ParentEditor::NOTATION ? "NOTATION"
                 :                                            "<unknown>")
             << "\n "
             << (segment ? segment->getLabel() : "<nullptr>")
             << (added ? "added" : "removed");
#endif

    m_conflictingKeyChanges->updateSegment(segment);
    m_keysDirty  = true;
}

void
ChordNameRuler::startChanged(   // SegmentObserver
const Segment   *segment,
timeT          /* t */)
{
    m_conflictingKeyChanges->updateSegment(segment);
    m_notesDirty = m_keysDirty = true;
}

void
ChordNameRuler::endMarkerTimeChanged(   // SegmentObserver
const Segment   *segment,
bool             shortened)
{
    if (shortened) {  // Maybe don't need anyway, but too difficult to
                      // check if notes/keys were affected.
        m_conflictingKeyChanges->updateSegment(segment);
        m_notesDirty = m_keysDirty = true;
    }
}

// See documentation in .h file
int
ChordNameRuler::chordRootPitchAtTime(timeT t)
const
{
     if (m_doUnarpeggiation)
        t = t - t % m_unarpeggiationTime;  // can never be zero
     else if (m_quantizationTime != 0)
        t = t - t % m_quantizationTime;    // is chords at notes on/off

    auto lower = m_roots.lower_bound(t);
    if (lower == m_roots.end())
        return -1;
    else
        return lower->second;

}  // chordRootPitchAtTime()

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
ChordNameRuler::slotCommandExecuted()
{
#ifdef CHORD_NAME_RULER_AND_CONFLICTING_KEY_CHANGES_DEBUG
    qDebug() << "\nChordNameRuler::slotCommandExecuted():"
             << (  m_parentEditor == ParentEditor::TRACK ? "TRACK"
                 : m_parentEditor == ParentEditor::MATRIX ? "MATRIX"
                 : m_parentEditor == ParentEditor::NOTATION ? "NOTATION"
                 :                                            "<unknown>")
             << '@'
             << static_cast<void*>(this)
             << (  CommandHistory::getInstance()->macroCommandIsExecuting()
                 ? "in_macro" : "plain");
#endif

    if (!CommandHistory::getInstance()->macroCommandIsExecuting())
        analyzeChords();
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
#ifdef CHORD_NAME_RULER_AND_CONFLICTING_KEY_CHANGES_DEBUG
    qDebug() << "\n\n\nChordNameRuler::slotInsertKeyChange()";
#endif

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

#ifdef CHORD_NAME_RULER_AND_CONFLICTING_KEY_CHANGES_DEBUG
    qDebug() << "\n\n\nChordNameRuler::slotChooseActiveSegments()";
#endif

    std::vector<Segment*>   segments;
    std::vector<unsigned>   activeIndices,
                            prevActives;
    std::vector<QColor>     foregrounds,
                            backgrounds;
    QStringList             segmentNames;
    std::vector<unsigned>   activeToOrdered;
    unsigned                drumOffset  = 0;

    activeToOrdered.reserve(m_orderedSegments.size());

    for (unsigned ndx = 0 ; ndx < m_orderedSegments.size() ; ++ndx) {
        const Segment *segment = m_orderedSegments[ndx];

        if (segment->isPercussion()) {
            // Would also be m_segments[segment].active       == false
            // and           m_segments[segment].isPercussion == true
            // but this is an easier check.
            ++drumOffset;
            continue;
        }

        segmentNames << QString::fromStdString(segment->getLabel());

        QColor textColor = segment->getPreviewColour();
        foregrounds.push_back(textColor);
        backgrounds.push_back(segment->getColour());

        if (m_segments[segment].active) {
            activeIndices.push_back(ndx - drumOffset);
            prevActives  .push_back(ndx - drumOffset);
        }
        activeToOrdered.push_back(ndx);
    }

    CheckableListDialog dialog(this,
                               QString(tr("Construct chords from "
                                          "notes in segments:")),
                               segmentNames,
                               foregrounds,
                               backgrounds,
                               activeIndices);

    if (dialog.exec() == QDialog::Accepted && activeIndices != prevActives) {
        // Set active/inactive segments
        for (auto &segment : m_segments)
            segment.second.active = false;

        for (unsigned activeIndex : activeIndices)
                m_segments[m_orderedSegments[activeToOrdered[activeIndex]]]
              .active
            = true;

        m_chordsDirty = true;
        analyzeChords();

        // Inform e.g. matrix editor that chord roots might have changed
        emit chordAnalysisChanged();
    }
}  // slotChooseActiveSegments()

void
ChordNameRuler::slotShowChords()
{
#ifdef CHORD_NAME_RULER_AND_CONFLICTING_KEY_CHANGES_DEBUG
    qDebug() << "\n\n\nChordNameRuler::slotShowChords()";
#endif

    m_showChords = m_showChordsAction->isChecked();

    if (m_chordCopyAction)
        m_chordCopyAction->setEnabled(m_showChords);

    if (m_showChords && !m_analyzeWhileHidden) {
        // Hadn't been analyzing chords and now need to.
        m_chordsDirty = true;
        analyzeChords();
    }
    else
        update();  // Just clear out displayed chords
}

void
ChordNameRuler::slotShowKeys()
{
#ifdef CHORD_NAME_RULER_AND_CONFLICTING_KEY_CHANGES_DEBUG
    qDebug() << "\n\n\nChordNameRuler::slotShowKeys()";
#endif

    m_showKeyChanges = m_showKeysAction->isChecked();

    if (m_insertKeyChangeAction)
        m_insertKeyChangeAction->setEnabled(m_showKeyChanges);

    updateToolTip();
    update();  // Just display (or not) key changes
}

void
ChordNameRuler::slotSetAlgorithm()
{
#ifdef CHORD_NAME_RULER_AND_CONFLICTING_KEY_CHANGES_DEBUG
    qDebug() << "\n\n\nChordNameRuler::slotSetAlgorithm()";
#endif

    if (sender() == m_quantizeAction)          m_doUnarpeggiation = false;
    else if (sender() == m_unarpeggiateAction) m_doUnarpeggiation = true;
    // nothing else possible

    Preferences::setPreference(ChordAnalysisGroup,
                               "chord_name_unarpeggiate",
                               m_doUnarpeggiation);

    m_chordsDirty = true;  // Need re-analysis.
    analyzeChords();

    // Inform e.g. matrix editor that chord roots might have changed
    emit chordAnalysisChanged();
}

void
ChordNameRuler::slotSetQuantization()
{
#ifdef CHORD_NAME_RULER_AND_CONFLICTING_KEY_CHANGES_DEBUG
    qDebug() << "\n\n\nChordNameRuler::slotSetQuantization()";
#endif

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
    m_chordsDirty      = true;  // Need re-analysis.

    analyzeChords();

    // Inform e.g. matrix editor that chord roots might have changed
    emit chordAnalysisChanged();
}

void
ChordNameRuler::slotSetUnarpeggiation()
{
#ifdef CHORD_NAME_RULER_AND_CONFLICTING_KEY_CHANGES_DEBUG
    qDebug() << "\n\n\nChordNameRuler::slotSetUnarpeggiation()";
#endif

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
    m_chordsDirty        = true;  // Need re-analysis.

    analyzeChords();

    // Inform e.g. matrix editor that chord roots might have changed
    emit chordAnalysisChanged();
}

void
ChordNameRuler::slotChordNameType()
{
#ifdef CHORD_NAME_RULER_AND_CONFLICTING_KEY_CHANGES_DEBUG
    qDebug() << "\n\n\nChordNameRuler::slotChordNameType()";
#endif

    if (sender() == m_pitchLetterAction) {
        m_chordNameType = ChordNameType::PITCH_LETTER;
        m_keyDependent = false;
    }
    else if (sender() == m_pitchIntegerAction) {
        m_chordNameType = ChordNameType::PITCH_INTEGER;
        m_keyDependent = false;
    }
    else if (sender() == m_keyRomanNumeralAction) {
        m_chordNameType = ChordNameType::KEY_ROMAN_NUMERAL;
        m_keyDependent  = true;
    }
    else if (sender() == m_keyIntegerAction) {
        m_chordNameType = ChordNameType::KEY_INTEGER;
        m_keyDependent  = true;
    }
    else
        qWarning() << "ChordNameRuler::slotChordNameType() UNKNOWN MENU CHOICE";

    Preferences::setPreference(ChordAnalysisGroup,
                               "chord_name_type",
                               static_cast<unsigned>(m_chordNameType));

    setSlashChordMenus();

    m_analyzer->updateChordNameStyles();

    m_namingModeChanged = true;
    m_chordsDirty       = true;
    analyzeChords();
}

void
ChordNameRuler::slotChordSlashType()
{
#ifdef CHORD_NAME_RULER_AND_CONFLICTING_KEY_CHANGES_DEBUG
    qDebug() << "\n\n\nChordNameRuler::slotChordSlashType()";
#endif

    bool wasOff = (m_chordSlashType == ChordSlashType::OFF);

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

    m_chordsDirty = true;  // Need re-analysis.
    analyzeChords();

    if (   ( wasOff && m_chordSlashType != ChordSlashType::OFF)
        || (!wasOff && m_chordSlashType == ChordSlashType::OFF))
        // Inform e.g. matrix editor that chord roots might have changed
        emit chordAnalysisChanged();
}

void
ChordNameRuler::slotChordAddedBass()
{
#ifdef CHORD_NAME_RULER_AND_CONFLICTING_KEY_CHANGES_DEBUG
    qDebug() << "\n\n\nChordNameRuler::slotChordAddedBass()";
#endif

    Preferences::setPreference(ChordAnalysisGroup,
                               "added_bass",
                               m_slashAddedBassAction->isChecked());

    m_analyzer->updateChordNameStyles();

    if (m_chordSlashType != ChordSlashType::OFF) {
        // Has no effect until ChordSlashType::PITCH_OR_KEY or CHORD_TONE
        m_chordsDirty = true;  // Need re-analysis.
        analyzeChords();
        // Inform e.g. matrix editor that chord roots might have changed
        emit chordAnalysisChanged();
    }
}

void
ChordNameRuler::slotPreferSlashChords()
{
#ifdef CHORD_NAME_RULER_AND_CONFLICTING_KEY_CHANGES_DEBUG
    qDebug() << "\n\n\nChordNameRuler::slotPreferSlashChords()";
#endif

    Preferences::setPreference(ChordAnalysisGroup,
                               "prefer_slash_chords",
                               m_preferSlashChordsAction ->isChecked());


    m_analyzer->updateChordNameStyles();

    if (m_chordSlashType != ChordSlashType::OFF) {
        // Has no effect until ChordSlashType::PITCH_OR_KEY or CHORD_TONE
        m_chordsDirty = true;  // Need re-analysis.
        analyzeChords();
        // Inform e.g. matrix editor that chord roots might have changed
        emit chordAnalysisChanged();
    }
}

void
ChordNameRuler::slotCMajorFlats()
{
#ifdef CHORD_NAME_RULER_AND_CONFLICTING_KEY_CHANGES_DEBUG
    qDebug() << "\n\n\nChordNameRuler::slotCMajorFlats()";
#endif

    // Use ChordAnalysisGroup, not MatrixViewConfigGroup, so
    // notes in matrix editor grid can have different settings (and so
    // don't have to do live synchronization between the two).
    Preferences::setPreference(ChordAnalysisGroup,
                               "c_major_flats",
                               m_cMajorFlats->isChecked());

    m_analyzer->updateChordNameStyles();

    m_chordsDirty = true;  // Need re-analysis.
    analyzeChords();
}

void
ChordNameRuler::slotOffsetMinors()
{
#ifdef CHORD_NAME_RULER_AND_CONFLICTING_KEY_CHANGES_DEBUG
    qDebug() << "\n\n\nChordNameRuler::slotOffsetMinors()";
#endif

    // Use ChordAnalysisGroup, not MatrixViewConfigGroup, so
    // notes in matrix editor grid can have different settings (and so
    // don't have to do live synchronization between the two).
    Preferences::setPreference(ChordAnalysisGroup,
                               "offset_minors",
                               m_offsetMinorKeys->isChecked());

    m_analyzer->updateChordNameStyles();

    m_chordsDirty = true;  // Need re-analysis.
    analyzeChords();
}

void
ChordNameRuler::slotAltMinors()
{
#ifdef CHORD_NAME_RULER_AND_CONFLICTING_KEY_CHANGES_DEBUG
    qDebug() << "\n\n\nChordNameRuler::slotAltMinors()";
#endif

    // Use ChordAnalysisGroup, not MatrixViewConfigGroup, so
    // notes in matrix editor grid can have different settings (and so
    // don't have to do live synchronization between the two).
    Preferences::setPreference(ChordAnalysisGroup,
                               "alt_minors",
                               m_altMinorKeys->isChecked());

    m_analyzer->updateChordNameStyles();

    m_chordsDirty = true;  // Need re-analysis.
    analyzeChords();
}

void
ChordNameRuler::slotNonDiatonicChords()
{
#ifdef CHORD_NAME_RULER_AND_CONFLICTING_KEY_CHANGES_DEBUG
    qDebug() << "\n\n\nChordNameRuler::slotNonDiatonicChords()";
#endif

    Preferences::nonDiatonicChords.set(m_nonDiatonicChords->isChecked());

    m_chordsDirty = true;  // Need re-analysis.
    analyzeChords();

    // Inform e.g. matrix editor that chord roots might have changed
    emit chordAnalysisChanged();
}

void
ChordNameRuler::slotSetNameStyles()
{
#ifdef CHORD_NAME_RULER_AND_CONFLICTING_KEY_CHANGES_DEBUG
    qDebug() << "\n\n\nChordNameRuler::slotSetNameStyles()";
#endif

    ChordNameStylesDialog chordNameStylesDialog(this);
    auto result = chordNameStylesDialog.exec();
    if (result == QDialog::Accepted) {
        m_analyzer->updateChordNameStyles();
        m_chordsDirty = true;  // Need re-analysis.
        analyzeChords();
    }
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

// Analyze chords if needed.
bool ChordNameRuler::analyzeChords()
{
    const CommandHistory *commandHistory = CommandHistory::getInstance();

#ifdef CHORD_NAME_RULER_AND_CONFLICTING_KEY_CHANGES_DEBUG
    qDebug() << "ChordNameRuler::analyzeChords():"
             << (  m_parentEditor == ParentEditor::TRACK ? "TRACK"
                 : m_parentEditor == ParentEditor::MATRIX ? "MATRIX"
                 : m_parentEditor == ParentEditor::NOTATION ? "NOTATION"
                 :                                            "<unknown>")
             << '@'
             << static_cast<void*>(this)
             << "\n  current segment:"
             << (m_currentSegment ? m_currentSegment->getLabel() : "???")
             << "\n "
             << (m_showChords ? "shwChrds" : "noChrds")
             << "  vis:"
             << (isVisible() ? "QT"  : "!qt")
             << (m_isVisible ? "OWN" : "!own")
             << (m_analyzeWhileHidden ? "HIDDEN" : "!hidden")
             << " "
             << m_segments.size()
             << "segments\n  cmd:"
             << commandHistory->commandIsExecuting()
             << commandHistory->macroCommandIsExecuting()
             << "  dirty:"
             << (m_keysDirty   ? "KEYS"   : "!keys")
             << (m_notesDirty  ? "NOTES"  : "!notes")
             << (m_chordsDirty ? "CHORDS" : "!chords")
             << (m_keyDependent ? "KEYDEP" : "!keydep")
             << (m_namingModeChanged ? "NAMCHG" : "namchg");

    if (   m_currentSegment
        && m_segments.find(m_currentSegment) != m_segments.end())
        qDebug() << "  percussion:"
                 << (m_currentSegment->isPercussion()          ? "DRUM"
                                                               : "NOTES")
                 << (m_segments[m_currentSegment].isPercussion ? "drum"
                                                               : "notes");
#endif

    if (   commandHistory->commandIsExecuting()
        || commandHistory->macroCommandIsExecuting())
        // Wait for all changes and re-analyze/display then.
        return false;

    // Get and reset one-time flag
    bool namingModeChanged = m_namingModeChanged;
    m_namingModeChanged = false;

    // Checks for if expensive ChordAnalyzer::labelChords() needs to be called.
    // If not, will do so next time called via show(), setVisible(true),
    //   setAnalyzeWhileHidden(true), etc. invoked.
    //
    if (m_isVisible) {
        if (   (!m_showChords && !m_analyzeWhileHidden)
            || (   !m_notesDirty
                && !m_chordsDirty
                && !m_keyDependent
                && !namingModeChanged))
        {
            if (m_keysDirty) {
                update();   // will just do keys
                m_keysDirty = false;
                return true;
            }
            else
                return false;
        }
    }
    else if (!m_analyzeWhileHidden) // matrix editor doesn't need chords
        return false;
    else if (!m_notesDirty && !m_chordsDirty)   // no need for chord reanalyzsis
        return false;
    else if (m_segments.size() == 0)  // rare (or impossible?), so check last
        return false;


    bool didAnalysis = false;
    if (   (m_showChords || m_analyzeWhileHidden)
        && (m_notesDirty || m_chordsDirty || (m_keysDirty && m_keyDependent))) {
        timeT   maxDuration = LongestNotes::MINIMUM,
                minTime     = std::numeric_limits<timeT>::max(),
                maxTime     = std::numeric_limits<timeT>::min();
        std::vector<const Segment*> segments;

        bool haveChangedRanges = false;
        for (const auto &segment : m_segments) {
            if (!segment.second.active)
                continue;

            segments.push_back(segment.first);

            const SegmentInfo &segmentInfo(segment.second);
            if (segmentInfo.longestNotes.longest() > maxDuration)
                maxDuration = segmentInfo.longestNotes.longest();

            if (segmentInfo.haveChangedRange()) {
                haveChangedRanges = true;

                if (segmentInfo.minChangedTime < minTime)
                    minTime = segmentInfo.minChangedTime;
                if (segmentInfo.maxChangedTime > maxTime)
                    maxTime = segmentInfo.maxChangedTime;
            }
        }

#ifdef CHORD_NAME_RULER_AND_CONFLICTING_KEY_CHANGES_DEBUG
        qDebug() << "  enter:"
                 << m_chords.size()
                 << "chords "
                 << m_roots.size()
                 << "roots";

        if (haveChangedRanges)
            qDebug() << "  changes:"
                     << haveChangedRanges
                     << "  min/max:"
                     << minTime
                     << maxTime
                     << maxDuration
                     << minTime - maxDuration;
        else
            qDebug() << "  changes: whole composition";
#endif

        minTime -= maxDuration;  // OK if goes before start of composition

        if (haveChangedRanges) {
            m_chords.erase(m_chords.lower_bound(minTime),
                           m_chords.lower_bound(maxTime));
            m_roots .erase(m_roots .lower_bound(minTime),
                           m_roots .lower_bound(maxTime));
        }
        else {
            m_chords.clear();
            m_roots .clear();
        }

#ifdef CHORD_NAME_RULER_AND_CONFLICTING_KEY_CHANGES_DEBUG
        qDebug() << "  befor:"
                 << m_chords.size()
                 << "chords "
                 << m_roots.size()
                 << "roots";
#endif

        m_analyzer->labelChords(m_chords,
                                m_roots,
                                segments,
                                m_currentSegment,
                                m_quantizationTime,
                                m_unarpeggiationTime,
                                m_doUnarpeggiation,
                                !haveChangedRanges,
                                minTime,
                                maxTime);

        didAnalysis = true;

        for (auto &segment : m_segments)  // All, even ones that weren't active
            segment.second.reset();
    }

#ifdef CHORD_NAME_RULER_AND_CONFLICTING_KEY_CHANGES_DEBUG
    qDebug() << "ChordNameRuler::analyzeChords() after:"
             << m_chords.size()
             << "chords,"
             << m_roots.size()
             << "roots";
#endif

    // Only as necessary, e.g. if Matrix and  not just m_analyzeWhileHidden
    if (m_isVisible && (didAnalysis || m_keysDirty))
        update();

    m_keysDirty = m_notesDirty = m_chordsDirty = false;

    return true;

} // analyzeChords()

// Pop up menu or temporarily hide key changes
void
ChordNameRuler::mousePressEvent(QMouseEvent *e)
{
    if (m_parentEditor == ParentEditor::TRACK) {
        QMessageBox messageBox(QMessageBox::Information,
                               tr("Chord/Key Ruler"),
                               tr("Use Chord/Key Ruler in matrix or notation\n"
                                  "editor to analyze chords and add/remove\n"
                                  "key signature changes."),
                                QMessageBox::Ok,
                                this);
        messageBox.move(e->globalPos());
        messageBox.exec();
        return;
    }

    if (e->button() == Qt::RightButton)
        m_menu->exec(QCursor::pos());
    else if (e->button() == Qt::MiddleButton) {
        m_hideKeyChanges = true;
        update();
    }
}

// Re-show temporarily hidden key changes
void
ChordNameRuler::mouseReleaseEvent(QMouseEvent *e)
{
    if (m_parentEditor == ParentEditor::TRACK)
        return;
    else if (e->button() == Qt::MiddleButton) {
        m_hideKeyChanges = false;
        update();
    }
}

void
ChordNameRuler::mouseDoubleClickEvent(QMouseEvent *e)
{
    if (m_parentEditor == ParentEditor::TRACK) return;

    if (!m_showKeyChanges) {
        QMessageBox messageBox(QMessageBox::Information,
                               tr("Chord/Key Ruler"),
                               tr("Use right-click menu and\n"
                                  "check \"Show key changes\"\n"
                                  "to enable double-click removal\n"
                                  "of key changes(s)."),
                                QMessageBox::Ok,
                                this);
        messageBox.move(e->globalPos());
        messageBox.exec();
        return;
    }

    const int   keyIndent = m_boldFont.pixelSize(), // really /2 but slop after
                clickX    = e->pos().x() + m_currentXOffset;
    const timeT clickedAt = m_rulerScale->getTimeForX(clickX);

    // Can't happen. Early abort above if in main window, and
    // impossible to be in matrix or notation editor without segments
    if (m_segments.empty()) return;

    if (!m_currentSegment) return;

    timeT maxKeyTime = std::numeric_limits<timeT>::min();

    auto keySig = m_currentSegment->getKeySignatureIterAtTime(clickedAt);
    if (m_currentSegment->keySignatureIterIsValid(keySig)) {
        timeT   keyTime    = keySig->first;
        int     keyX       = m_rulerScale->getXForTime(keyTime);
        QString keyName    (strtoqstr(keySig->second.getUnicodeName()));
        QRect   textBounds = m_boldFontMetrics.boundingRect(keyName);
        int delta          = clickX - keyX /* - keyIndent */;

        if (   delta >= 0
            && delta < textBounds.width() + keyIndent
            && keyTime > maxKeyTime)
            maxKeyTime = keyTime;
    }

    // This no longer happens as all segments in matrix and notation
    // editors have explicit key changes at their starts (default
    // C major added if missing) and main/track editor doesn't
    // implement double-click.
    // change at their starts (
    if (maxKeyTime == std::numeric_limits<timeT>::min()) {
        QMessageBox messageBox(QMessageBox::Warning,
                               tr("Chord/Key Ruler"),
                               tr("Double-click on key change to delete."),
                               QMessageBox::Ok,
                               this);
        messageBox.move(e->globalPos());
        messageBox.exec();
        return;
    }

    // Pop up dialog

    std::vector<QColor> foregrounds,
                        backgrounds;

    QStringList                 keyChangesStrings;
    std::vector<Segment*>       keyChangeSegments;
    std::vector<const Event*>   keyChangeEvents;

    for (Segment *segment : m_orderedSegments)
    {
        timeT   keyTime;
        Key      key   = segment->getKeyAtTime(clickedAt, keyTime);
        const Event *event = key.getEvent();

        if (keyTime != maxKeyTime || event == nullptr) continue;

        keyChangeSegments.push_back(segment);
        keyChangeEvents  .push_back(event);

        keyChangesStrings << (   QString::fromStdString(key.getUnicodeName())
                           + tr(" in segment ")
                           + QString::fromStdString(  segment
                                                    ->getLabel()));

        QColor textColor = segment->getPreviewColour();
        foregrounds.push_back(textColor);
        backgrounds.push_back(segment->getColour());
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

#if 0   // will get triggered when command executes
        analyzeChords();
#endif
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
// Plus really should redesign UI to have separate key change and
//   chord name rulers, and/or other way to always display current
//   key regardless of whether most recent key change is visible
//   given current UI zoom/scroll. (Yes, notation editor also loses
//   key change display in staves when scrolling, but centuries-old
//   prior practice with printed sheet music has key signature at
//   beginning of each staff as measures are broken up by page width
//   constraints.)
// Update: New "extantKeyLabels" solve above problem.
// Update: New hold-middle-mouse temporary hide key changes eliminate need
//   for proposed "wide gray key change text underneath transparent
//   background chord texts".
void
ChordNameRuler::paintEvent(QPaintEvent* e)
{
    if (!m_composition)
        return ;

#ifdef CHORD_NAME_RULER_AND_CONFLICTING_KEY_CHANGES_DEBUG
    qDebug() << "ChordNameRuler::paintEvent()"
             << (  m_parentEditor == ParentEditor::TRACK    ? "TRACK"
                 : m_parentEditor == ParentEditor::MATRIX   ? "MATRIX"
                 : m_parentEditor == ParentEditor::NOTATION ? "NOTATION"
                 :                                            "???")
             << '@'
             << static_cast<void*>(this)
             << "\n  show:"
             << (m_showChords ? "chords" : "-")
             << (m_showKeyChanges ? "keys" : "-")
             << (m_hideKeyChanges ? "hide" : "-")
             << " chs/kys:"
             << m_chords.size()
             << (  m_currentSegment
                 ? m_currentSegment->keySignatureMap().size()
                 : 0)
             << " x:"
             << m_currentXOffset
             << "\n  segment:"
             << (m_currentSegment ? m_currentSegment->getLabel() : "???")
             << '@'
             << static_cast<const void*>(m_currentSegment);
#endif

    QPainter paint(this);

    // In a stylesheet world...  Yadda yadda.  Fix the stupid background to
    // rescue it from QWidget Hack Black. (Ahhh.  Yes, after being thwarted for
    // a month or something, I'm really enjoying getting this solved.)
    QBrush bg = QBrush(GUIPalette::getColour(GUIPalette::ChordNameRulerBackground));
    paint.fillRect(e->rect(), bg);

    paint.setPen(GUIPalette::getColour(GUIPalette::ChordNameRulerForeground));

    paint.setClipRegion(e->region());
    paint.setClipRect(e->rect().normalized());

    QRect     clipRect        = paint.clipRegion().boundingRect();
    QRect     boundsForHeight = m_fontMetrics.boundingRect("^j|lM");
    const int fontHeight      = boundsForHeight.height(),
              rulerHeight     = height(),
              textY           = (height() - 6) / 2 + fontHeight / 2,
              keySpacing      = m_boldFont.pixelSize() / 3;

    int   beginX    = 0,
          currentX  = beginX,
          endX      = beginX + clipRect.width();
    timeT beginTime =   m_rulerScale->getTimeForX(beginX)
                      + m_rulerScale->getTimeForX(m_currentXOffset),
          endTime   =   m_rulerScale->getTimeForX(endX)
                      + m_rulerScale->getTimeForX(m_currentXOffset);
    auto  chordIter = m_chords.lower_bound(beginTime);

    auto timeToX = [this](const timeT time)
    {
        return   static_cast<int>(std::round( this
                                             ->m_rulerScale
                                             ->getXForTime(time)))
               - this->m_currentXOffset;
    };

    auto keyIterToX = [this, timeToX, endX](
    const Segment::KeySignatureMap::const_iterator  iter)
    {
        if (this->m_currentSegment->keySignatureIterIsValid(iter))
            return timeToX(iter->first);
        else
            return endX;
    };

    enum class Mode {
        NONE = 0,
        CHORDS_AND_KEYS,
        ONLY_KEYS,
        ONLY_CHORDS,
    };

    const bool showChords =     m_showChords
                            && !m_chords.empty(),
               showKeys   =     m_currentSegment
                            &&  m_showKeyChanges
                            && !m_hideKeyChanges
                            && !m_currentSegment->keySignatureMap().empty();
    const Mode mode       = (  showChords && showKeys ? Mode::CHORDS_AND_KEYS
                             :               showKeys ? Mode::ONLY_KEYS
                             : showChords             ? Mode::ONLY_CHORDS
                             :                          Mode::NONE);

#ifdef CHORD_NAME_RULER_AND_CONFLICTING_KEY_CHANGES_DEBUG
    qDebug() << "  mode:"
             << (  mode == Mode::NONE            ? "NONE"
                 : mode == Mode::CHORDS_AND_KEYS ? "CHORDS_AND_KEYS"
                 : mode == Mode::ONLY_KEYS       ? "ONLY_KEYS"
                 : mode == Mode::ONLY_CHORDS     ? "ONLY_CHORDS"
                 :                                 "???");
#endif

    if (mode == Mode::NONE) return;

    if (mode == Mode::ONLY_KEYS) {
        Segment::KeySignatureMap::const_iterator
            keysEnd = m_currentSegment->keySignatureMap().cend(),
            keyIter = m_currentSegment->getKeySignatureIterAtTime(beginTime);

        paint.setFont(m_boldFont);
        for ( ;
               keyIter != keysEnd && keyIter->first < endTime ;
            ++keyIter) {
            int keyX = keyIterToX(keyIter);

            paint.drawLine(keyX, 0, keyX, rulerHeight);

            if (keyX >= currentX) {
                currentX = keyX + keySpacing;
                QString keyName(strtoqstr(keyIter->second.getUnicodeName()));
                paint.drawText(currentX, textY, keyName);
                currentX = keyX + m_fontMetrics.boundingRect(keyName).width();
            }
        }
        return;
    }

    if (mode == Mode::CHORDS_AND_KEYS) {
        Segment::KeySignatureMap::const_iterator
            keysEnd  = m_currentSegment->keySignatureMap().cend(),
            keyIter = m_currentSegment->getKeySignatureIterAtTime(beginTime);
        int  nextKeyX = (keyIter == keysEnd ? endX : keyIterToX(keyIter));

        while (currentX < endX) {
            if (   keyIter != keysEnd
                && (   chordIter == m_chords.end()
                    || keyIter->first <= chordIter->first)) {
                int keyX = keyIterToX(keyIter);
                paint.drawLine(keyX, 0, keyX, rulerHeight);
                if (keyX >= currentX) {
                    paint.setFont(m_boldFont);
                    QString keyName(strtoqstr(  keyIter
                                              ->second
                                               .getUnicodeName()));
                    keyX += keySpacing;
                    paint.drawText(keyX, textY, keyName);
                    currentX =   keyX
                               +  m_boldFontMetrics
                                 .boundingRect(keyName)
                                 .width();
                }
                nextKeyX = keyIterToX(++keyIter);
            }
            else if (chordIter != m_chords.end()) {
                int chordX = timeToX(chordIter->first);
                paint.drawLine(chordX, rulerHeight - 4, chordX, rulerHeight);
                if (chordX >= currentX) {
                    QString chordName(strtoqstr(chordIter->second));
                    int     width  =  m_fontMetrics
                                     .boundingRect(chordName)
                                     .width();
                    if (chordX + width < nextKeyX) {
                        paint.setFont(m_font);
                        paint.drawText(chordX, textY, chordName);
                        currentX = chordX + width;
                    }
                    else currentX = chordX;

                }
                ++chordIter;
            }

            if (chordIter == m_chords.end() && keyIter == keysEnd)
                break;;
        }

        return;
    }

    if (mode == Mode::ONLY_CHORDS) {
        paint.setFont(m_font);
        for ( ;
             chordIter != m_chords.end() && chordIter->first <= endTime ;
             ++chordIter) {
            int     chordX = timeToX(chordIter->first);

            paint.drawLine(chordX, rulerHeight - 4, chordX, rulerHeight);

            if (chordX >= currentX) {
                QString chordName(strtoqstr(chordIter->second));
                paint.drawText(chordX, textY, chordName);
                currentX =   chordX
                           + m_fontMetrics.boundingRect(chordName).width();
            }
        }
    }
}  // ChordNameRuler::paintEvent(QPaintEvent* e)

}  // namespace Rosegarden
