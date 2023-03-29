/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*- vi:set ts=8 sts=4 sw=4: */

/*
    Rosegarden
    A MIDI and audio sequencer and musical notation editor.
    Copyright 2000-2023 the Rosegarden development team.
    Modifications and additions Copyright (c) 2022,2023 Mark R. Rubin aka "thanks4opensource" aka "thanks4opensrc"

    Other copyrights also apply to some parts of this work.  Please
    see the AUTHORS file and individual file headers for details.

    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License as
    published by the Free Software Foundation; either version 2 of the
    License, or (at your option) any later version.  See the file
    COPYING included with this distribution for more information.
*/

#ifndef RG_MATRIXVIEWSEGMENT_H
#define RG_MATRIXVIEWSEGMENT_H

#include "base/ViewSegment.h"

namespace Rosegarden
{

class MatrixScene;
class Segment;
class MatrixElement;
class MidiKeyMapping;

class MatrixViewSegment : public ViewSegment
{
public:
    MatrixViewSegment(MatrixScene *,
                      Segment*,
                      const bool drumMode,
                      const unsigned segmentIndex);
    ~MatrixViewSegment() override;

    // SegmentObserver (base class ViewSegment) notifications
    void eventAdded          (const Segment*, Event*) override;
    void eventRemoved        (const Segment*, Event*) override;
    void endMarkerTimeChanged(const Segment*, bool)   override;
    void startChanged        (const Segment*, timeT)  override;

    void colourChanged(const Segment*) override;
    void labelChanged(const Segment*) override;
#if 0  // Respond to finer-grained colourChanged() and labelChanged() instead
    void appearanceChanged(const Segment *segment) override;
#endif

    SegmentRefreshStatus &getRefreshStatus() const;
    void resetRefreshStatus();

    void updateElements(timeT from, timeT to);

    void reconfigureNotes();
    void updateNoteLabels();
    void updateNoteColors();
    void setNotesCurrentSegment(bool current);

    void updateTiedUntied(const EventContainer &notes);

    void clearAllSelected();

    MatrixScene* getMatrixScene() const { return m_scene; }

    bool isDrumMode() const { return m_drum; }

    void setDrumMode(bool drumMode);

    std::vector<MatrixElement*> getSelectedElements();

protected:
//!!!    const MidiKeyMapping *getKeyMapping() const;

    /**
     * Override from ViewSegment
     * Wrap only notes
     */
    bool wrapEvent(Event*) override;

    ViewElement* makeViewElement(Event *) override;

    MatrixScene *m_scene;
    bool m_drum;
    const unsigned m_segmentIndex;
    unsigned int m_refreshStatusId;
};

}

#endif
