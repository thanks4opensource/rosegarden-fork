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

#define RG_MODULE_STRING "[ChordNameStylesDialog]"


#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QGridLayout>
#include <QLabel>

#include "base/AnalysisTypes.h"
#include "misc/ConfigGroups.h"
#include "misc/Preferences.h"

#include "ChordNameStylesDialog.h"


namespace Rosegarden
{

ChordNameStylesDialog::ChordNameStylesDialog(QWidget *parent)
:   QDialog(parent)
{
    setWindowTitle(tr("Chord name styles:"));

    QGridLayout *mainLayout = new QGridLayout;
    mainLayout->addWidget(new QLabel(tr("Chord name styles")), 0, 0, 1, 2);

    QLabel *label;

    label = new QLabel(tr("Major"));
    label->setToolTip(tr("Symbol for major chords"));
    mainLayout->addWidget(label, 1, 0);
    m_majorStyles = new QComboBox();
    m_majorStyles->addItem(tr("<blank>"), ChordAnalyzer::MAJOR_BLANK);
    m_majorStyles->addItem(tr("M"),       ChordAnalyzer::MAJOR_M);
    m_majorStyles->addItem(tr("maj"),     ChordAnalyzer::MAJOR_MAJ);
    m_majorStyles->setCurrentIndex(
                   Preferences::getPreference(ChordAnalysisGroup,
                                              "major_chord_style",
                                              ChordAnalyzer::MAJOR_BLANK));
    mainLayout->addWidget(m_majorStyles, 1, 1);

    label = new QLabel(tr("Minor"));
    label->setToolTip(tr("Symbol for minor chords"));
    mainLayout->addWidget(label, 2, 0);
    m_minorStyles = new QComboBox();
    m_minorStyles->addItem(tr("m"),   ChordAnalyzer::MINOR_M);
    m_minorStyles->addItem(tr("-"),   ChordAnalyzer::MINOR_MINUS);
    m_minorStyles->addItem(tr("min"), ChordAnalyzer::MINOR_MIN);
    m_minorStyles->addItem(tr("mi"),  ChordAnalyzer::MINOR_MI);
    m_minorStyles->setCurrentIndex(
                   Preferences::getPreference(ChordAnalysisGroup,
                                              "minor_chord_style",
                                              ChordAnalyzer::MINOR_M));
    mainLayout->addWidget(m_minorStyles, 2, 1);

    label = new QLabel(tr("Diminished"));
    label->setToolTip(tr("Symbol for dimished chords"));
    mainLayout->addWidget(label, 3, 0);
    m_diminishedStyles = new QComboBox();
    m_diminishedStyles->addItem(tr("dim"),  ChordAnalyzer::DIMINISHED_DIM);
    m_diminishedStyles->addItem(u8"\u00B0", ChordAnalyzer::DIMINISHED_CIRCLE);
    m_diminishedStyles->setCurrentIndex(
                        Preferences::getPreference(ChordAnalysisGroup,
                                                   "diminished_chord_style",
                                                     ChordAnalyzer
                                                   ::DIMINISHED_DIM));
    mainLayout->addWidget(m_diminishedStyles, 3, 1);

    label = new QLabel(tr("Augmented"));
    label->setToolTip(tr("Symbol for augmented chords"));
    mainLayout->addWidget(label, 4, 0);
    m_augmentedStyles = new QComboBox();
    m_augmentedStyles->addItem(tr("aug"), ChordAnalyzer::AUGMENTED_AUG);
    m_augmentedStyles->addItem("+",       ChordAnalyzer::AUGMENTED_PLUS);
    m_augmentedStyles->setCurrentIndex(
                       Preferences::getPreference(ChordAnalysisGroup,
                                                  "augmented_chord_style",
                                                  ChordAnalyzer::AUGMENTED_AUG));
    mainLayout->addWidget(m_augmentedStyles, 4, 1);

    label = new QLabel(tr("Major 7th, etc"));
    label->setToolTip(tr("Symbol for major 7th, 9th, etc. chords"));
    mainLayout->addWidget(label, 5, 0);
    m_major7thStyles = new QComboBox();
    m_major7thStyles->addItem(tr("M7"),    ChordAnalyzer::MAJOR_7TH_M);
    m_major7thStyles->addItem(tr("maj7"),  ChordAnalyzer::MAJOR_7TH_MAJ);
    m_major7thStyles->addItem(u8"\u03947", ChordAnalyzer::MAJOR_7TH_DELTA);
    m_major7thStyles->setCurrentIndex(
                      Preferences::getPreference(ChordAnalysisGroup,
                                                 "major_7th_chord_style",
                                                 ChordAnalyzer::MAJOR_7TH_M));
    mainLayout->addWidget(m_major7thStyles, 5, 1);

    label = new QLabel(tr("Add"));
    label->setToolTip(tr("Symbol for \"add\" chords (add9, +4, etc.),"));
    mainLayout->addWidget(label, 6, 0);
    m_addStyles = new QComboBox();
    m_addStyles->addItem(tr("add"), ChordAnalyzer::ADD_ADD);
    m_addStyles->addItem("+",       ChordAnalyzer::ADD_PLUS);
    m_addStyles->setCurrentIndex(
                 Preferences::getPreference(ChordAnalysisGroup,
                                            "add_chord_style",
                                            ChordAnalyzer::ADD_ADD));
    mainLayout->addWidget(m_addStyles, 6, 1);

    label = new QLabel(tr("IIm vs ii, etc."));
    label->setToolTip(tr("Symbol for roman numeral minor chords,\n"
                         "either uppercase with minor chord\n"
                         "symbol (\"-\", \"m\", \"min\", see above) or\n"
                         "lowercase without."));
    mainLayout->addWidget(label, 7, 0);
    m_minorRomanStyles = new QComboBox();
    m_minorRomanStyles->addItem("IIm", ChordAnalyzer::ROMAN_MINOR_UPPERCASE);
    m_minorRomanStyles->addItem("ii",  ChordAnalyzer::ROMAN_MINOR_LOWERCASE);
    m_minorRomanStyles->setCurrentIndex(
                        Preferences::getPreference(ChordAnalysisGroup,
                                                   "minor_roman_chord_style",
                                                     ChordAnalyzer
                                                   ::ROMAN_MINOR_UPPERCASE));
    mainLayout->addWidget(m_minorRomanStyles, 7, 1);

    QDialogButtonBox   *buttonBox
                     = new QDialogButtonBox(  QDialogButtonBox::Ok
                                            | QDialogButtonBox::Cancel);

    connect(buttonBox,
            &QDialogButtonBox::accepted,
            this,
            &ChordNameStylesDialog::accepted);

    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

    mainLayout->addWidget(buttonBox, 8, 0, 1, 2);

    setLayout(mainLayout);
}

void
ChordNameStylesDialog::accepted()
{
    Preferences::setPreference(ChordAnalysisGroup,
                               "major_chord_style",
                               m_majorStyles->currentData().toInt());
    Preferences::setPreference(ChordAnalysisGroup,
                               "minor_chord_style",
                               m_minorStyles->currentData().toInt());
    Preferences::setPreference(ChordAnalysisGroup,
                               "diminished_chord_style",
                               m_diminishedStyles->currentData().toInt());
    Preferences::setPreference(ChordAnalysisGroup,
                               "augmented_chord_style",
                               m_augmentedStyles->currentData().toInt());
    Preferences::setPreference(ChordAnalysisGroup,
                               "major_7th_chord_style",
                               m_major7thStyles->currentData().toInt());
    Preferences::setPreference(ChordAnalysisGroup,
                               "add_chord_style",
                               m_addStyles->currentData().toInt());
    Preferences::setPreference(ChordAnalysisGroup,
                               "minor_roman_chord_style",
                               m_minorRomanStyles->currentData().toInt());


    emit done(QDialog::Accepted);
}


}  // namespace Rosegarden
