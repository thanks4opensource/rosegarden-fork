
/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*- vi:set ts=8 sts=4 sw=4: */

/*
    Rosegarden
    A MIDI and audio sequencer and musical notation editor.
    Copyright 2000-2018 the Rosegarden development team.

    Other copyrights also apply to some parts of this work.  Please
    see the AUTHORS file and individual file headers for details.

    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License as
    published by the Free Software Foundation; either version 2 of the
    License, or (at your option) any later version.  See the file
    COPYING included with this distribution for more information.
*/

#ifndef RG_SEGMENTSINGLEREPEATTOCOPYCOMMAND_H
#define RG_SEGMENTSINGLEREPEATTOCOPYCOMMAND_H

#include "document/Command.h"
#include "base/Event.h"

#include <QCoreApplication>


namespace Rosegarden
{

class Segment;
class Composition;


class SegmentSingleRepeatToCopyCommand : public NamedCommand
{
    Q_DECLARE_TR_FUNCTIONS(Rosegarden::SegmentSingleRepeatToCopyCommand)

public:
    SegmentSingleRepeatToCopyCommand(Segment *segment,
                                     timeT time);
    virtual ~SegmentSingleRepeatToCopyCommand();

    void execute() override;
    void unexecute() override;

    Segment *getNewSegment() const { return m_newSegment; }

private:
    Composition *m_composition;
    Segment *m_segment;
    Segment *m_newSegment;
    timeT m_time;
    bool m_detached;
};


}

#endif
