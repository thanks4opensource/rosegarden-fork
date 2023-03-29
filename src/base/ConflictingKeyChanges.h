/*
    Rosegarden
    A MIDI and audio sequencer and musical notation editor.
    Copyright (c) 2023 Mark R. Rubin aka "thanks4opensource" aka "thanks4opensrc"

    Other copyrights also apply to some parts of this work.  Please
    see the AUTHORS file and individual file headers for details.

    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License as
    published by the Free Software Foundation; either version 2 of the
    License, or (at your option) any later version.  See the file
    COPYING included with this distribution for more information.
*/

#ifndef RG_CONFLICTINGKEYCHANGES_H
#define RG_CONFLICTINGKEYCHANGES_H

#include <set>


namespace Rosegarden
{

class Segment;

class ConflictingKeyChanges
{
  public:
    ConflictingKeyChanges(const std::vector<Segment*> &segments);

    ~ConflictingKeyChanges();

    bool areConflicting(const Segment *segmentA,
                        const Segment *segmentB)
    const {
#if 0   // Shouldn't need this. Clients shouldn't be asking about
        // unknown segments.
        if (   m_segments.find(segmentA) == m_segments.end()
            || m_segments.find(segmentB) == m_segments.end())
            return true;  // Really unknown, true for safety.
#endif
        return    m_conflicts.find(SegmentPair(segmentA, segmentB))
               != m_conflicts.end();
    }

    bool hasConflicts() const { return !m_conflicts.empty(); }
    bool operator()  () const { return !m_conflicts.empty(); }

    unsigned numSegments() const { return m_segments.size(); }

    // Have two entries, one for A,B and one for B,A
    unsigned numConflicts() const {return m_conflicts.size() / 2; }

    void addSegment   (const Segment *segment);
    void removeSegment(const Segment *segment);
    void updateSegment(const Segment *segment);

    void analyzeConflicts();


  protected:

#ifdef CONFLICTING_KEY_CHANGES_DEBUG
    #define COMPILE_DEBUG_DUMP_CONFLICTING
    void dumpConflicting(const char *caption);
    #define DEBUG_DUMP_CONFLICTING(CAPTION) dumpConflicting(CAPTION);
#else
    #undef COMPILE_DEBUG_DUMP_CONFLICTING
    #define DEBUG_DUMP_CONFLICTING(CAPTION)
#endif

    bool conflicting(const Segment*, const Segment*);

    using SegmentPair = std::pair<const Segment*, const Segment*>;

    std::set<const Segment*>    m_segments;
    std::set<SegmentPair>       m_conflicts;

};  // class ConflictingKeyChanges

}  // namespace Rosegarden
#endif  // #ifndef RG_CONFLICTINGKEYCHANGES_H
