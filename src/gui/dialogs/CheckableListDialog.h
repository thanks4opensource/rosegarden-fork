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

#ifndef RG_CHECkABLELISTDIALOG_H
#define RG_CHECkABLELISTDIALOG_H

#include <vector>

#include <QColor>
#include <QDialog>

class QDialogButtonBox;
class QLabel;
class QListWidget;
class QListWidgetItem;
class QPushButton;
class QPushButton;

namespace Rosegarden
{

// Dialog to present user with list and allow setting checkmarks
//   on one/many/all of them.
// Client specifies color for each entry in list.
// Returns std::vector of subset chosen.
// Note unmodifiable Qt behavior is that clicking anywhere in list
//   element background color to light blue and forground to white, but
//   only clicking checkmark box on or off actually sets data that
//   is returned to client. See "Warning:", below.
// Used by ChordNameRuler to select which segments' notes will
//   be used for chord analysis.

class CheckableListDialog : public QDialog
{
    Q_OBJECT
public:
    // Warning: Don't use Qt::white (or equivalent) in foregroundColors.
    // Qt bug: Checkbox forground is same as item forground (as set
    // from foregroundColors) but checkbox background is always white,
    // so checkmark becomes invisible.
    CheckableListDialog(QWidget                   *parent,
                        const QString             &description,
                        const QStringList         &stringList,
                        const std::vector<QColor> &foregroundColors,
                        const std::vector<QColor> &backgroundColors,
                        std::vector<unsigned>     &checkeds);

public slots:
    void all   ();
    void none  ();
    void ok    ();
    void cancel();

private:
    QListWidget      *m_widget;
    QDialogButtonBox *m_buttonBox;
    QLabel           *m_label;
    QPushButton      *m_allButton,
                     *m_noneButton,
                     *m_okButton,
                     *m_cancelButton;

    void createListWidget();
    void createOtherWidgets();
    void createLayout();
    void createConnections();

    const QString               &m_description;
    const QStringList           &m_stringList;
    const std::vector<QColor>   &m_foregroundColors;
    const std::vector<QColor>   &m_backgroundColors;
    std::vector<unsigned>       &m_checkeds;
};

}  // namespace Rosegarden

#endif  // #ifndef RG_CHECkABLELISTDIALOG_H
