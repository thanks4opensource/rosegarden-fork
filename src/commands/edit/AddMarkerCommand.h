
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

#ifndef RG_ADDMARKERCOMMAND_H
#define RG_ADDMARKERCOMMAND_H

#include <string>
#include "document/Command.h"
#include <QString>
#include "base/Event.h"
#include <QCoreApplication>




namespace Rosegarden
{

class Marker;
class RosegardenDocument;


class AddMarkerCommand : public NamedCommand
{
    Q_DECLARE_TR_FUNCTIONS(Rosegarden::AddMarkerCommand)

public:
    AddMarkerCommand(RosegardenDocument *doc,
                     timeT time,
                     const std::string &name,
                     const std::string &description);
    ~AddMarkerCommand() override;

    static QString getGlobalName() { return tr("&Add Marker"); }

    void execute() override;
    void unexecute() override;

protected:

    RosegardenDocument  *m_document;
    Marker              *m_marker;
    bool                 m_detached;

};



}

#endif
