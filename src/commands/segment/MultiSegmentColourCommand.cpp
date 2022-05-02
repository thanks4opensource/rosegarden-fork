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

#define RG_MODULE_STRING "[MultiSegmentColourCommand]"

#include "MultiSegmentColourCommand.h"

#include "base/Segment.h"
#include "base/Selection.h"
#include "document/RosegardenDocument.h"

#include <QString>


namespace Rosegarden
{

MultiSegmentColourCommand::MultiSegmentColourCommand(
    std::vector<Segment*> &segments):
        NamedCommand(tr("Change Segment Colors")),
        m_firstExecute(true)
{
    // ??? std::copy()
    for (std::vector<Segment*>::iterator i = segments.begin();
         i != segments.end();
         ++i) {
        m_segments.push_back(*i);
        m_prevColours.push_back((*i)->getColourIndex());
    }
}

MultiSegmentColourCommand::~MultiSegmentColourCommand()
{
}

void
MultiSegmentColourCommand::execute()
{
    if (m_firstExecute) m_firstExecute = false;
    else                exchange();

    RosegardenDocument::currentDocument->slotDocColoursChanged();
}

void
MultiSegmentColourCommand::unexecute()
{
    exchange();

    RosegardenDocument::currentDocument->slotDocColoursChanged();
}

void
MultiSegmentColourCommand::exchange()
{
    for (size_t i = 0; i < m_segments.size(); ++i) {
        unsigned colour = m_segments[i]->getColourIndex();
        m_segments[i]->setColourIndex(m_prevColours[i]);
        m_prevColours[i] = colour;
    }
}

}
