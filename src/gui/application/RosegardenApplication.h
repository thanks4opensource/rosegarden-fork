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

#ifndef RG_ROSEGARDENAPPLICATION_H
#define RG_ROSEGARDENAPPLICATION_H

#include <QApplication>
#include <QByteArray>
#include <QString>

#include <rosegardenprivate_export.h>

class QSessionManager;
class QProcess;


namespace Rosegarden
{

/**
 * RosegardenApplication
 *
 * Handles RosegardenMainWindow's perceived uniqueness for us.
 *
 */
class ROSEGARDENPRIVATE_EXPORT RosegardenApplication : public QApplication
{
    Q_OBJECT
public:
    RosegardenApplication(int &argc, char **argv);

    virtual void saveState(QSessionManager&);

    void initPreferences();

    //!!!
    //@@@ Need session manager commitData() call

signals:
    // connect this to RosegardenMainWindow
    void aboutToSaveState();

protected:

    bool notify(QObject * receiver, QEvent * event) override;
};

}

#endif
