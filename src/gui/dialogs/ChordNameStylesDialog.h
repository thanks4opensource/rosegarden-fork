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

#ifndef RG_CHORDNAMESTYLESDIALOG_H
#define RG_CHORDNAMESTYLESDIALOG_H

#include <vector>

#include <QColor>
#include <QDialog>

class QComboBox;


namespace Rosegarden
{

// Dialog for use by ChordNameRuler.
// Allows user choice between chord label types for various types of chords.

class ChordNameStylesDialog : public QDialog
{
    Q_OBJECT

  public:
    ChordNameStylesDialog(QWidget *parent);

  public slots:
    void accepted();

  protected:
    QComboBox *m_majorStyles,
              *m_minorStyles,
              *m_diminishedStyles,
              *m_augmentedStyles,
              *m_major7thStyles,
              *m_minorRomanStyles,
              *m_addStyles;
};

}  // namespace Rosegarden

#endif  // #ifndef RG_CHORDNAMESTYLESDIALOG_H
