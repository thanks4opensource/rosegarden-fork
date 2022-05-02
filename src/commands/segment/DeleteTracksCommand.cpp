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

#define RG_MODULE_STRING "[DeleteTracksCommand]"

#include "DeleteTracksCommand.h"

#include "misc/Debug.h"
#include "base/Composition.h"
#include "base/Segment.h"
#include <QString>


namespace Rosegarden
{

DeleteTracksCommand::DeleteTracksCommand(Composition *composition,
        std::vector<TrackId> trackIds):
        NamedCommand(getGlobalName()),
        m_composition(composition),
        m_trackIds(trackIds),
        m_detached(false)
{
    for (TrackId trackId : m_trackIds)
        m_tracks.push_back(m_composition->getTrackById(trackId));

    // Must be sorted in reverse position order for execute()
    std::sort(m_tracks.begin(),
              m_tracks.end(),
              [](Track *left, Track *right) {
                    return left->getPosition() > right->getPosition();
              });

    // Copy reversed sort order back into m_trackIds
    for (unsigned ndx = 0 ; ndx < m_trackIds.size() ; ++ndx)
        m_trackIds[ndx] = m_tracks[ndx]->getId();
}

DeleteTracksCommand::~DeleteTracksCommand()
{
    if (m_detached) {
        for (size_t i = 0; i < m_oldTracks.size(); ++i)
            delete m_oldTracks[i];

        for (size_t i = 0; i < m_oldSegments.size(); ++i)
            delete m_oldSegments[i];

#if 0  // Unnecessary: just pointers, so no destructors
        m_oldTracks.clear();
        m_oldSegments.clear();
#endif
    }
}

void DeleteTracksCommand::execute()
{
    // Clear out the undo info.
    m_oldSegments.clear();
    m_oldTracks.clear();

    // Alias for readability
    const SegmentMultiSet &segments = m_composition->getSegments();

    // Remove the tracks and their segments.

    // For each track we are deleting.
    for (Track *track : m_tracks) {
        TrackId trackId = track->getId();

        if (track) {
            // ??? The following segment removal code will never find any
            //     segments to remove.  All clients of this class make sure
            //     the segments are removed before the tracks are removed.
            //     This code should be removed and replaced with a check to
            //     make sure that the segments have been removed before
            //     removing the tracks.  Segment removal is a bit tricky
            //     for audio segments.  We should limit the kludge to
            //     SegmentEraseCommand.

            // For each segment in the composition.
            for (SegmentMultiSet::const_iterator j = segments.cbegin();
                 j != segments.end();
                 /* incremented inside */) {
                // Increment before use.  Otherwise detachSegment() will
                // invalidate our iterator.
                SegmentMultiSet::const_iterator k = j++;

                // If this segment is on the track we are deleting
                if ((*k)->getTrack() == trackId) {
                    // Save the segment for undo.
                    m_oldSegments.push_back(*k);
                    // Remove the segment from the composition.
                    m_composition->detachSegment(*k);
                }
            }

            // Save the track for undo.
            m_oldTracks.push_back(track);
            // Remove the track from the composition.
            if (m_composition->detachTrack(track) == false) {
                RG_DEBUG << "DeleteTracksCommand::execute - can't detach track";
            }
        }
    }

    // Adjust the track position numbers to remove any gaps.

    Composition::trackcontainer &tracks = m_composition->getTracks();

    // For each deleted track
    for (std::vector<Track*>::const_iterator
         oldTrackIter = m_oldTracks.cbegin();
         oldTrackIter != m_oldTracks.cend();
         ++oldTrackIter) {
        // For each track left in the composition
        for (Composition::trackconstiterator compTrackIter = tracks.cbegin();
             compTrackIter != tracks.cend();
             ++compTrackIter) {
            // If the composition track was after the deleted track
            if (compTrackIter->second->getPosition() >
                    (*oldTrackIter)->getPosition()) {
                // Decrement the composition track's position to close up
                // the gap in the position numbers.
                int newPosition = compTrackIter->second->getPosition() - 1;
                compTrackIter->second->setPosition(newPosition);
            }
        }
    }

    m_composition->notifyTracksDeleted(m_trackIds);

    m_detached = true;
}

void DeleteTracksCommand::unexecute()
{
    // Add the tracks and the segments back in.

    std::vector<TrackId> trackIds;

    // Both current composition and old-to-be-restored tracks must be
    // sorted in reverse position order. They are currently in forward
    // TrackId order -- note that TrackId and position are numerically
    // the same by default but not necessarily so if tracks are moved,
    // added, or deleted.

    // Current tracks in composition
    std::vector<Track*> currentTracks(m_composition->getTracks().size());
    std::transform(m_composition->getTracks().cbegin(),
                   m_composition->getTracks().cend(),
                   currentTracks.begin(),
                   [](const std::pair<TrackId, Track*> &idAndTrack) {
                       return idAndTrack.second;
                   });
    std::sort(currentTracks.begin(),
              currentTracks.end(),
              [](Track *left, Track *right) {
                    return left->getPosition() < right->getPosition();
              });

    // Old, to-be-restored, tracks
    std::sort(m_oldTracks.begin(),
              m_oldTracks.end(),
              [](Track *left, Track *right) {
                    return left->getPosition() < right->getPosition();
              });

    std::vector<Track*>::iterator currentTracksIter = currentTracks.begin();
    std::vector<Track*>::const_iterator oldTracksIter = m_oldTracks.begin();

    // new position in combined/shuffled tracks
    unsigned position = 0;
    // for "pushing" current track downward in display
    unsigned incrementer = 0;

    // "Shuffle" old tracks being restored and current tracks together
    // in correct order.
    while (true) {
        bool insertOld = false, insertCurrent = false;

        if (oldTracksIter != m_oldTracks.cend() &&
            currentTracksIter != currentTracks.end()) {
            // Still have both current and old tracks to insert.
            // Choose which one.
            if ((*currentTracksIter)->getPosition() + incrementer <
               static_cast<unsigned>((*oldTracksIter)->getPosition())) {
                insertCurrent = true;
            }
            else {
                insertOld = true;
            }
        }
        // At most one of the following can be true because the above
        // clause would have executed instead. Neither is true if both
        // sets of tracks have been inserted.
        else if (currentTracksIter != currentTracks.end())
            insertCurrent = true;
        else if (oldTracksIter != m_oldTracks.cend())
            insertOld = true;

        // Insert the correct track, or are finished.
        if (insertCurrent)
            (*currentTracksIter++)->setPosition(position);
        else if (insertOld) {
            ++oldTracksIter;
            ++incrementer;
        }
        else {  // !insertOld && !insertCurrent
            // Both sets of tracks finished, exit.
            break;
        }

        ++position;
    }

    for (std::vector<Track*>::const_iterator
         oldTrackIter = m_oldTracks.cbegin();
         oldTrackIter != m_oldTracks.cend();
         ++oldTrackIter) {
        m_composition->addTrack(*oldTrackIter);
        trackIds.push_back((*oldTrackIter)->getId());
    }

    // Add the old segments back in.
    // ??? This is not good enough for audio segments.  See
    //     SegmentEraseCommand::unexecute().
    for (size_t i = 0; i < m_oldSegments.size(); ++i)
        m_composition->addSegment(m_oldSegments[i]);

    m_composition->notifyTracksAdded(trackIds);

    m_detached = false;
}

}
