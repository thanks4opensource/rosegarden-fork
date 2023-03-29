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

#include <map>

#include "base/NotationTypes.h"
#include "document/RosegardenDocument.h"

#undef CONFLICTING_KEY_CHANGES_DEBUG
#include "ConflictingKeyChanges.h"


namespace Rosegarden
{

ConflictingKeyChanges::ConflictingKeyChanges(
const std::vector<Segment*> &segments)
{
#ifdef CONFLICTING_KEY_CHANGES_DEBUG
    qDebug() << "ConflictingKeyChanges::ConflictingKeyChanges() @"
             << static_cast<void*>(this)
             << segments.size()
             << "incoming segments";
#endif

    for (Segment *segment : segments)
        m_segments.insert(segment);

    analyzeConflicts();

    DEBUG_DUMP_CONFLICTING("ConflictingKeyChanges::ConflictingKeyChanges")
}

ConflictingKeyChanges::~ConflictingKeyChanges()
{
#ifdef CONFLICTING_KEY_CHANGES_DEBUG
    qDebug() << "ConflictingKeyChanges::~ConflictingKeyChanges()"
             << static_cast<void*>(this)
             << m_segments.size()
             << "segments";
#endif
}

// Never used. ConflictingKeyChanges only used in matrix editor, which
// never has segments added post-construction.
void
ConflictingKeyChanges::addSegment(
const Segment *segment)
{
#ifdef CONFLICTING_KEY_CHANGES_DEBUG
    qDebug() << "ConflictingKeyChanges::addSegment() @"
             << static_cast<void*>(this)
             << static_cast<const void*>(segment)
             << (segment ? segment->getLabel() : "<unlabeled>")
             << " enter:"
             << m_segments.size()
             << '/'
             << m_conflicts.size();
#endif

    if (!segment) return;

    if (m_segments.find(segment) != m_segments.end()) {
#ifdef CONFLICTING_KEY_CHANGES_DEBUG
        qDebug() << "  *** already have!";
#endif
        return;
    }

    m_segments.insert(segment);

    for (const Segment* const existing : m_segments)
        if (conflicting(segment, existing)) {
            // For fast lookup without having to check A < B
            m_conflicts.emplace(SegmentPair(segment,  existing));
            m_conflicts.emplace(SegmentPair(existing, segment ));
        }

#ifdef CONFLICTING_KEY_CHANGES_DEBUG
    qDebug() << "ConflictingKeyChanges::addSegment() exit:"
             << m_segments.size()
             << '/'
             << m_conflicts.size();
#endif

    DEBUG_DUMP_CONFLICTING("ConflictingKeyChanges::addSegment")
}

void
ConflictingKeyChanges::removeSegment(
const Segment *segment)
{
#ifdef CONFLICTING_KEY_CHANGES_DEBUG
    std::cerr << "ConflictingKeyChanges::removeSegment() "
              << (segment ? segment->getLabel() : "<nullptr>")
              << " @ "
              << static_cast<const void*>(segment)
              << "  enter: "
              << m_segments.size()
              << " ... "
              << std::endl;
#endif

    if (!segment) return;

    if (m_segments.find(segment) == m_segments.end()) {
#ifdef CONFLICTING_KEY_CHANGES_DEBUG
        qDebug() << "  ... already removed or unknown";
#endif
        return;
    }

    for (const Segment *other : m_segments) {
        auto found = m_conflicts.find(SegmentPair(segment, other));
        if (found != m_conflicts.end()) {
            m_conflicts.erase(found);
            m_conflicts.erase(SegmentPair(other, segment));  // reversed pair
        }
    }
    m_segments.erase(segment);

    DEBUG_DUMP_CONFLICTING("ConflictingKeyChanges::removeSegment")
}

void
ConflictingKeyChanges::updateSegment(
const Segment *segment)
{
    // Also checks if nullptr
    if (m_segments.find(segment) == m_segments.end()) return;

    for (const Segment *other : m_segments) {
        auto found = m_conflicts.find(SegmentPair(segment, other));
        if (found != m_conflicts.end()) {
            m_conflicts.erase(found);
            m_conflicts.erase(SegmentPair(other, segment));  // reversed pair
        }
    }

    for (const Segment* const existing : m_segments)
        if (conflicting(segment, existing)) {
            // For fast lookup without having to check A < B
            m_conflicts.emplace(SegmentPair(segment,  existing));
            m_conflicts.emplace(SegmentPair(existing, segment ));
        }

    DEBUG_DUMP_CONFLICTING("ConflictingKeyChanges::updateSegment")
}

void
ConflictingKeyChanges::analyzeConflicts()
{
#ifdef CONFLICTING_KEY_CHANGES_DEBUG
    qDebug() << "ConflictingKeyChanges::analyzeConflicts() enter:"
             << m_conflicts.size()
             << "conflicts "
             << m_segments.size()
             << "segments";
#endif

    m_conflicts.clear();

    for (auto   segmentA  = m_segments.begin() ;
                segmentA != m_segments.end() ;
              ++segmentA)
        for (auto   segmentB  = std::next(segmentA) ;
                    segmentB != m_segments.end() ;
                  ++segmentB)
            if (conflicting(*segmentA, *segmentB)) {
                // For fast lookup without having to check A < B
                m_conflicts.emplace(SegmentPair(*segmentA, *segmentB));
                m_conflicts.emplace(SegmentPair(*segmentB, *segmentA));
            }
}

bool
ConflictingKeyChanges::conflicting(
const Segment   *segmentA,
const Segment   *segmentB)
{
    const auto  &aKeys(segmentA->keySignatureMap()),
                &bKeys(segmentB->keySignatureMap());

    switch (aKeys.size() + bKeys.size()) {
        case 0:  return false;
        case 1:  return true;
        default: break;
    }

    std::set<timeT> keyTimes;
    for (const auto iter : aKeys) keyTimes.insert(iter.first);
    for (const auto iter : bKeys) keyTimes.insert(iter.first);

    // Checked above, have at least one key change in each segment.
    auto a = aKeys.begin(),
         b = bKeys.begin();
    for (const auto keyTime : keyTimes) {
        if (a == aKeys.end() && b == bKeys.end())
            break;

        if (a->second != b->second)
            return true;

        if (a->first == keyTime) ++a;
        if (b->first == keyTime) ++b;

        if (a == aKeys.end()) --a;
        if (b == bKeys.end()) --b;
    }

    return false;
}

#ifdef COMPILE_DEBUG_DUMP_CONFLICTING
void
ConflictingKeyChanges::dumpConflicting(
const char *caption)
{
    std::cerr << caption
              << ": "
              << m_conflicts.size()
              << " conflicts:"
              << std::endl;

    for (const auto &conflict : m_conflicts)
        qDebug() << "  "
                 << (conflict.first  ? conflict.first->getLabel()
                                     : "<unlabeled>")
                 << (conflict.second ? conflict.second->getLabel()
                                     : "<unlabeled>");
}
#endif

}  // namespace Rosegarden
