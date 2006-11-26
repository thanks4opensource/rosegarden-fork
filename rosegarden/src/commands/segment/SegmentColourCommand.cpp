/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*- vi:set ts=8 sts=4 sw=4: */

/*
    Rosegarden
    A MIDI and audio sequencer and musical notation editor.
 
    This program is Copyright 2000-2006
        Guillaume Laurent   <glaurent@telegraph-road.org>,
        Chris Cannam        <cannam@all-day-breakfast.com>,
        Richard Bown        <richard.bown@ferventsoftware.com>
 
    The moral rights of Guillaume Laurent, Chris Cannam, and Richard
    Bown to claim authorship of this work have been asserted.
 
    Other copyrights also apply to some parts of this work.  Please
    see the AUTHORS file and individual file headers for details.
 
    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License as
    published by the Free Software Foundation; either version 2 of the
    License, or (at your option) any later version.  See the file
    COPYING included with this distribution for more information.
*/


#include "SegmentColourCommand.h"

#include "base/Segment.h"
#include "base/Selection.h"
#include <qstring.h>


namespace Rosegarden
{

SegmentColourCommand::SegmentColourCommand(
    SegmentSelection &segments,
    const unsigned int index):
        KNamedCommand(i18n("Change Segment Color")),
        m_newColourIndex(index)
{
    for (SegmentSelection::iterator i = segments.begin(); i != segments.end(); ++i)
        m_segments.push_back(*i);
}

SegmentColourCommand::~SegmentColourCommand()
{}

void
SegmentColourCommand::execute()
{
    for (unsigned int i = 0; i < m_segments.size(); ++i) {
        m_oldColourIndexes.push_back(m_segments[i]->getColourIndex());
        m_segments[i]->setColourIndex(m_newColourIndex);
    }
}

void
SegmentColourCommand::unexecute()
{
    for (unsigned int i = 0; i < m_segments.size(); ++i)
        m_segments[i]->setColourIndex(m_oldColourIndexes[i]);
}

}
