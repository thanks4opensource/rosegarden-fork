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


#include "AddMarkerCommand.h"

#include "base/Marker.h"
#include "document/RosegardenDocument.h"
#include <QString>


namespace Rosegarden
{

AddMarkerCommand::AddMarkerCommand(RosegardenDocument *doc,
                                   timeT time,
                                   const std::string &name,
                                   const std::string &description):
        NamedCommand(getGlobalName()),
        m_document(doc),
        m_detached(true)
{
    m_marker = new Marker(time, name, description);
}

AddMarkerCommand::~AddMarkerCommand()
{
    if (m_detached)
        delete m_marker;
}

void
AddMarkerCommand::execute()
{
    m_document->addMarker(m_marker);
    m_detached = false;
}

void
AddMarkerCommand::unexecute()
{
    m_document->deleteMarker(m_marker);
    m_detached = true;
}

}
