
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

#ifndef RG_MULTISEGMENTCOLOURCOMMAND_H
#define RG_MULTISEGMENTCOLOURCOMMAND_H

#include "base/Segment.h"
#include "base/Selection.h"
#include "document/Command.h"
#include <QString>
#include <vector>
#include <QCoreApplication>


namespace Rosegarden
{


class MultiSegmentColourCommand : public NamedCommand
{
    Q_DECLARE_TR_FUNCTIONS(Rosegarden::MultiSegmentColourCommand)

public:
    MultiSegmentColourCommand(std::vector<Segment*> &segments);
    ~MultiSegmentColourCommand() override;

    static QString getGlobalName()
        { return tr("Change Segment Color..."); }

    void execute() override;
    void unexecute() override;

protected:
    void exchange();

    std::vector<Segment*>       m_segments;
    std::vector<unsigned int>   m_prevColours;
    bool                        m_firstExecute;
};


}

#endif
