/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*- vi:set ts=8 sts=4 sw=4: */

/*
    Rosegarden
    A sequencer and musical notation editor.
    Copyright 2000-2011 the Rosegarden development team.
    See the AUTHORS file for more details.

    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License as
    published by the Free Software Foundation; either version 2 of the
    License, or (at your option) any later version.  See the file
    COPYING included with this distribution for more information.
*/

#include "SegmentNotationHelper.h"
#include "base/NotationTypes.h"
#include "Quantizer.h"
#include "BasicQuantizer.h"
#include "NotationQuantizer.h"
#include "base/BaseProperties.h"
#include "Composition.h"

#include <iostream>
#include <algorithm>
#include <iterator>
#include <list>

namespace Rosegarden 
{
using std::cerr;
using std::endl;
using std::string;
using std::list;

using namespace BaseProperties;


SegmentNotationHelper::~SegmentNotationHelper() { }


const Quantizer &
SegmentNotationHelper::basicQuantizer() {
    return *(segment().getComposition()->getBasicQuantizer());
}

const Quantizer &
SegmentNotationHelper::notationQuantizer() {
    return *(segment().getComposition()->getNotationQuantizer());
}


//!!! we need to go very carefully through this file and check calls
//to getAbsoluteTime/getDuration -- the vast majority should almost
//certainly now be using getNotationAbsoluteTime/getNotationDuration

Segment::iterator
SegmentNotationHelper::findNotationAbsoluteTime(timeT t)
{
    iterator i(segment().findTime(t));

    // We don't know whether the notation absolute time t will appear
    // before or after the real absolute time t.  First scan backwards
    // until we find a notation absolute time prior to (or equal to)
    // t, and then scan forwards until we find the first one that
    // isn't prior to t

    while (i != begin() &&
	   ((i == end() ? t + 1 : (*i)->getNotationAbsoluteTime()) > t))
	--i;

    while (i != end() &&
	   ((*i)->getNotationAbsoluteTime() < t))
	++i;

    return i;
}

Segment::iterator
SegmentNotationHelper::findNearestNotationAbsoluteTime(timeT t)
{
    iterator i(segment().findTime(t));

    // Exactly findNotationAbsoluteTime, only with the two scan loops
    // in the other order

    while (i != end() &&
	   ((*i)->getNotationAbsoluteTime() < t))
	++i;

    while (i != begin() &&
	   ((i == end() ? t + 1 : (*i)->getNotationAbsoluteTime()) > t))
	--i;

    return i;
}

void
SegmentNotationHelper::setNotationProperties(timeT startTime, timeT endTime)
{
    Segment::iterator from = begin();
    Segment::iterator to = end();

    if (startTime != endTime) {
	from = segment().findTime(startTime);
	to   = segment().findTime(endTime);
    }
/*!!!
    bool justSeenGraceNote = false;
    timeT graceNoteStart = 0;
*/
    for (Segment::iterator i = from;
	 i != to && segment().isBeforeEndMarker(i); ++i) {

	if ((*i)->has(NOTE_TYPE) /*!!! && !(*i)->has(IS_GRACE_NOTE) */) continue;

	timeT duration = (*i)->getNotationDuration();

	if ((*i)->has(BEAMED_GROUP_TUPLET_BASE)) {
	    int tcount = (*i)->get<Int>(BEAMED_GROUP_TUPLED_COUNT);
	    int ucount = (*i)->get<Int>(BEAMED_GROUP_UNTUPLED_COUNT);

	    if (tcount == 0) {
		std::cerr << "WARNING: SegmentNotationHelper::setNotationProperties: zero tuplet count:" << std::endl;
		(*i)->dump(std::cerr);
	    } else {
		// nominal duration is longer than actual (sounding) duration
		duration = (duration / tcount) * ucount;
	    }
	}

	if ((*i)->isa(Note::EventType) || (*i)->isa(Note::EventRestType)) {

	    if ((*i)->isa(Note::EventType)) {
/*!!!		
		if ((*i)->has(IS_GRACE_NOTE) &&
		    (*i)->get<Bool>(IS_GRACE_NOTE)) {

		    if (!justSeenGraceNote) {
			graceNoteStart = (*i)->getNotationAbsoluteTime();
			justSeenGraceNote = true;
		    }

		} else if (justSeenGraceNote) {

		    duration += (*i)->getNotationAbsoluteTime() - graceNoteStart;
		    justSeenGraceNote = false;
		}
*/
	    }

	    Note n(Note::getNearestNote(duration));

	    (*i)->setMaybe<Int>(NOTE_TYPE, n.getNoteType());
	    (*i)->setMaybe<Int>(NOTE_DOTS, n.getDots());
	}
    }
}

timeT
SegmentNotationHelper::getNotationEndTime(Event *e)
{
    return e->getNotationAbsoluteTime() + e->getNotationDuration();
}


Segment::iterator
SegmentNotationHelper::getNextAdjacentNote(iterator i,
					   bool matchPitch,
					   bool allowOverlap)
{
    iterator j(i);
    if (!isBeforeEndMarker(i)) return i;
    if (!(*i)->isa(Note::EventType)) return end();

    timeT iEnd = getNotationEndTime(*i);
    long ip = 0, jp = 0;
    if (!(*i)->get<Int>(PITCH, ip) && matchPitch) return end();

    while (true) {
	if (!isBeforeEndMarker(j) || !isBeforeEndMarker(++j)) return end();
	if (!(*j)->isa(Note::EventType)) continue;

	timeT jStart = (*j)->getNotationAbsoluteTime();
	if (jStart > iEnd) return end();

	if (matchPitch) {
	    if (!(*j)->get<Int>(PITCH, jp) || (jp != ip)) continue;
	}

	if (allowOverlap || (jStart == iEnd)) return j;
    }
}

   
Segment::iterator
SegmentNotationHelper::getPreviousAdjacentNote(iterator i,
					       timeT rangeStart,
					       bool matchPitch,
					       bool allowOverlap)
{ 
    iterator j(i);
    if (!isBeforeEndMarker(i)) return i;
    if (!(*i)->isa(Note::EventType)) return end();

    timeT iStart = (*i)->getNotationAbsoluteTime();
    timeT iEnd   = getNotationEndTime(*i);
    long ip = 0, jp = 0;
    if (!(*i)->get<Int>(PITCH, ip) && matchPitch) return end();

    while (true) {
	if (j == begin()) return end(); else --j;
	if (!(*j)->isa(Note::EventType)) continue;
	if ((*j)->getAbsoluteTime() < rangeStart) return end();

	timeT jEnd = getNotationEndTime(*j);

	// don't consider notes that end after i ends or before i begins

	if (jEnd > iEnd || jEnd < iStart) continue;

	if (matchPitch) {
	    if (!(*j)->get<Int>(PITCH, jp) || (jp != ip)) continue;
	}

	if (allowOverlap || (jEnd == iStart)) return j;
    }
}


Segment::iterator
SegmentNotationHelper::findContiguousNext(iterator el) 
{
    std::string elType = (*el)->getType(),
        reject, accept;
     
    if (elType == Note::EventType) {
        accept = Note::EventType;
        reject = Note::EventRestType;
    } else if (elType == Note::EventRestType) {
        accept = Note::EventRestType;
        reject = Note::EventType;
    } else {
        accept = elType;
        reject = "";
    }

    bool success = false;

    iterator i = ++el;
    
    for(; isBeforeEndMarker(i); ++i) {
        std::string iType = (*i)->getType();

        if (iType == reject) {
            success = false;
            break;
        }
        if (iType == accept) {
            success = true;
            break;
        }
    }

    if (success) return i;
    else return end();
    
}

Segment::iterator
SegmentNotationHelper::findContiguousPrevious(iterator el)
{
    if (el == begin()) return end();

    std::string elType = (*el)->getType(),
        reject, accept;
     
    if (elType == Note::EventType) {
        accept = Note::EventType;
        reject = Note::EventRestType;
    } else if (elType == Note::EventRestType) {
        accept = Note::EventRestType;
        reject = Note::EventType;
    } else {
        accept = elType;
        reject = "";
    }

    bool success = false;

    iterator i = --el;

    while (true) {
        std::string iType = (*i)->getType();

        if (iType == reject) {
            success = false;
            break;
        }
        if (iType == accept) {
            success = true;
            break;
        }
	if (i == begin()) break;
	--i;
    }

    if (success) return i;
    else return end();
}


bool
SegmentNotationHelper::noteIsInChord(Event *note)
{
    iterator i = segment().findSingle(note);
    timeT t = note->getNotationAbsoluteTime();
    
    for (iterator j = i; j != end(); ++j) { // not isBeforeEndMarker, unnecessary here
	if (j == i) continue;
	if ((*j)->isa(Note::EventType)) {
	    timeT tj = (*j)->getNotationAbsoluteTime();
	    if (tj == t) return true;
	    else if (tj > t) break;
	}
    }

    for (iterator j = i; ; ) {
	if (j == begin()) break;
	--j;
	if ((*j)->isa(Note::EventType)) {
	    timeT tj = (*j)->getNotationAbsoluteTime();
	    if (tj == t) return true;
	    else if (tj < t) break;
	}
    }

    return false;

/*!!!
    iterator first, second;
    segment().getTimeSlice(note->getAbsoluteTime(), first, second);

    int noteCount = 0;
    for (iterator i = first; i != second; ++i) {
	if ((*i)->isa(Note::EventType)) ++noteCount;
    }

    return noteCount > 1;
*/
}


//!!! This doesn't appear to be used any more and may well not work.
// Ties are calculated in several different places, and it's odd that
// we don't have a decent API for them 
Segment::iterator
SegmentNotationHelper::getNoteTiedWith(Event *note, bool forwards)
{
    bool tied = false;

    if (!note->get<Bool>(forwards ?
			 BaseProperties::TIED_FORWARD :
                         BaseProperties::TIED_BACKWARD, tied) || !tied) {
        return end();
    }

    timeT myTime = note->getAbsoluteTime();
    timeT myDuration = note->getDuration();
    int myPitch = note->get<Int>(BaseProperties::PITCH);

    iterator i = segment().findSingle(note);
    if (!isBeforeEndMarker(i)) return end();

    for (;;) {
        i = forwards ? findContiguousNext(i) : findContiguousPrevious(i);

        if (!isBeforeEndMarker(i)) return end();
        if ((*i)->getAbsoluteTime() == myTime) continue;

        if (forwards && ((*i)->getAbsoluteTime() != myTime + myDuration)) {
            return end();
        }
        if (!forwards &&
            (((*i)->getAbsoluteTime() + (*i)->getDuration()) != myTime)) {
            return end();
        }
        
        if (!(*i)->get<Bool>(forwards ?
                             BaseProperties::TIED_BACKWARD :
                             BaseProperties::TIED_FORWARD, tied) || !tied) {
            continue;
        }

        if ((*i)->get<Int>(BaseProperties::PITCH) == myPitch) return i;
    }

    return end();
}


bool
SegmentNotationHelper::collapseRestsIfValid(Event* e, bool& collapseForward)
{
    iterator elPos = segment().findSingle(e);
    if (elPos == end()) return false;

    timeT myDuration = (*elPos)->getNotationDuration();

    // findContiguousNext won't return an iterator beyond the end marker
    iterator nextEvent = findContiguousNext(elPos),
	 previousEvent = findContiguousPrevious(elPos);

    // Remark: findContiguousXXX is inadequate for notes, we would
    // need to check adjacency using e.g. getNextAdjacentNote if this
    // method were to work for notes as well as rests.

    // collapse to right if (a) not at end...
    if (nextEvent != end() &&
	// ...(b) rests can be merged to a single, valid unit
 	isCollapseValid((*nextEvent)->getNotationDuration(), myDuration) &&
	// ...(c) event is in same bar (no cross-bar collapsing)
	(*nextEvent)->getAbsoluteTime() <
	    segment().getBarEndForTime(e->getAbsoluteTime())) {

        // collapse right is OK; collapse e with nextEvent
	Event *e1(new Event(*e, e->getAbsoluteTime(),
			    e->getDuration() + (*nextEvent)->getDuration()));

        collapseForward = true;
	erase(elPos);
        erase(nextEvent);
	insert(e1);
	return true;
    }

    // logic is exactly backwards from collapse to right logic above
    if (previousEvent != end() &&
	isCollapseValid((*previousEvent)->getNotationDuration(), myDuration) &&
	(*previousEvent)->getAbsoluteTime() >
	    segment().getBarStartForTime(e->getAbsoluteTime())) {
			    
        // collapse left is OK; collapse e with previousEvent
	Event *e1(new Event(**previousEvent,
			    (*previousEvent)->getAbsoluteTime(),
			    e->getDuration() +
			    (*previousEvent)->getDuration()));

        collapseForward = false;
        erase(elPos);
	erase(previousEvent);
	insert(e1);
	return true;
    }
    
    return false;
}


bool
SegmentNotationHelper::isCollapseValid(timeT a, timeT b)
{
    return (isViable(a + b));
}


bool
SegmentNotationHelper::isSplitValid(timeT a, timeT b)
{
    return (isViable(a) && isViable(b));
}

Segment::iterator
SegmentNotationHelper::splitIntoTie(iterator &i, timeT baseDuration)
{
    if (i == end()) return end();
    iterator i2;
    segment().getTimeSlice((*i)->getAbsoluteTime(), i, i2);
    return splitIntoTie(i, i2, baseDuration);
}

Segment::iterator
SegmentNotationHelper::splitIntoTie(iterator &from, iterator to,
				    timeT baseDuration)
{
    // so long as we do the quantization checks for validity before
    // calling this method, we should be fine splitting precise times
    // in this method. only problem is deciding not to split something
    // if its duration is very close to requested duration, but that's
    // probably not a task for this function

    timeT eventDuration = (*from)->getDuration();
    timeT baseTime = (*from)->getAbsoluteTime();

    long firstGroupId = -1;
    (*from)->get<Int>(BEAMED_GROUP_ID, firstGroupId);

    long nextGroupId = -1;
    iterator ni(to);

    if (segment().isBeforeEndMarker(ni) && segment().isBeforeEndMarker(++ni)) {
	(*ni)->get<Int>(BEAMED_GROUP_ID, nextGroupId);
    }

    list<Event *> toInsert;
    list<iterator> toErase;
          
    // Split all the note and rest events in range [from, to[
    //
    for (iterator i = from; i != to; ++i) {

	if (!(*i)->isa(Note::EventType) &&
	    !(*i)->isa(Note::EventRestType)) continue;

	if ((*i)->getAbsoluteTime() != baseTime) {
	    // no way to really cope with an error, because at this
	    // point we may already have splut some events. Best to
	    // skip this event
	    cerr << "WARNING: SegmentNotationHelper::splitIntoTie(): (*i)->getAbsoluteTime() != baseTime (" << (*i)->getAbsoluteTime() << " vs " << baseTime << "), ignoring this event\n";
	    continue;
	}

        if ((*i)->getDuration() != eventDuration) {
	    if ((*i)->getDuration() == 0) continue;
	    cerr << "WARNING: SegmentNotationHelper::splitIntoTie(): (*i)->getDuration() != eventDuration (" << (*i)->getDuration() << " vs " << eventDuration << "), changing eventDuration to match\n";
            eventDuration = (*i)->getDuration();
        }

        if (baseDuration >= eventDuration) {
//            cerr << "SegmentNotationHelper::splitIntoTie() : baseDuration >= eventDuration, ignoring event\n";
            continue;
        }

	std::pair<Event *, Event *> split =
	    splitPreservingPerformanceTimes(*i, baseDuration);

	Event *eva = split.first;
	Event *evb = split.second;

	if (!eva || !evb) {
	    cerr << "WARNING: SegmentNotationHelper::splitIntoTie(): No valid split for event of duration " << eventDuration << " at " << baseTime << " (baseDuration " << baseDuration << "), ignoring this event\n";
	    continue;
	}

	// we only want to tie Note events:

	if (eva->isa(Note::EventType)) {

	    // if the first event was already tied forward, the
	    // second one will now be marked as tied forward
	    // (which is good).  set up the relationship between
	    // the original (now shorter) event and the new one.

	    evb->set<Bool>(TIED_BACKWARD, true);
	    eva->set<Bool>(TIED_FORWARD, true);
	}

	// we may also need to change some group information: if
	// the first event is in a beamed group but the event
	// following the insertion is not or is in a different
	// group, then the new second event should not be in a
	// group.  otherwise, it should inherit the grouping info
	// from the first event (as it already does, because it
	// was created using the copy constructor).

	// (this doesn't apply to tupled groups, which we want
	// to persist wherever possible.)

	if (firstGroupId != -1 &&
	    nextGroupId != firstGroupId &&
	    !evb->has(BEAMED_GROUP_TUPLET_BASE)) {
	    evb->unset(BEAMED_GROUP_ID);
	    evb->unset(BEAMED_GROUP_TYPE);
	}

	toInsert.push_back(eva);
        toInsert.push_back(evb);
	toErase.push_back(i);
    }

    // erase the old events
    for (list<iterator>::iterator i = toErase.begin();
	 i != toErase.end(); ++i) {
	segment().erase(*i);
    }

    from = end();
    iterator last = end();

    // now insert the new events
    for (list<Event *>::iterator i = toInsert.begin();
         i != toInsert.end(); ++i) {
        last = insert(*i);
	if (from == end()) from = last;
    }

    return last;
}

bool
SegmentNotationHelper::isViable(timeT duration, int dots)
{
    bool viable;

/*!!!
    duration = basicQuantizer().quantizeDuration(duration);

    if (dots >= 0) {
        viable = (duration == Quantizer(Quantizer::RawEventData,
					Quantizer::DefaultTarget,
					Quantizer::NoteQuantize, 1, dots).
		  quantizeDuration(duration));
    } else {
        viable = (duration == notationQuantizer().quantizeDuration(duration));
    }
*/
    
    //!!! what to do about this?

    timeT nearestDuration =
	Note::getNearestNote(duration, dots >= 0 ? dots : 2).getDuration();

//    std::cerr << "SegmentNotationHelper::isViable: nearestDuration is " << nearestDuration << ", duration is " << duration << std::endl;
    viable = (nearestDuration == duration);

    return viable;
}


void
SegmentNotationHelper::makeRestViable(iterator i)
{
    timeT absTime = (*i)->getAbsoluteTime(); 
    timeT duration = (*i)->getDuration();
    erase(i);
    segment().fillWithRests(absTime, absTime + duration);
}


Event *
SegmentNotationHelper::makeThisNoteViable(iterator noteItr, bool splitAtBars)
{
    // We don't use quantized values here; we want a precise division.
    // Even if it doesn't look precise on the score (because the score
    // is quantized), we want any playback to produce exactly the same
    // duration of note as was originally recorded

    std::vector<Event *> toInsert;

    Segment::iterator i = noteItr;
    
	if (!(*i)->isa(Note::EventType) && !(*i)->isa(Note::EventRestType)) {
            return *noteItr;
	}

	if ((*i)->has(BEAMED_GROUP_TUPLET_BASE)) {
            return *noteItr;
	}

	DurationList dl;

	// Behaviour differs from TimeSignature::getDurationListForInterval

	timeT acc = 0;
	timeT required = (*i)->getNotationDuration();
	
	while (acc < required) {
	    timeT remaining = required - acc;
	    if (splitAtBars) {
		timeT thisNoteStart = (*i)->getNotationAbsoluteTime() + acc;
		timeT toNextBar =
		    segment().getBarEndForTime(thisNoteStart) - thisNoteStart;
		if (toNextBar > 0 && remaining > toNextBar) remaining = toNextBar;
	    }
	    timeT component = Note::getNearestNote(remaining).getDuration();
	    if (component > (required - acc)) dl.push_back(required - acc);
	    else dl.push_back(component);
	    acc += component;
	}

	if (dl.size() < 2) {
	    // event is already of the correct duration
            return *noteItr;
	}
    
	acc = (*i)->getNotationAbsoluteTime();
	Event *e = new Event(*(*i));

	bool lastTiedForward = false;
	e->get<Bool>(TIED_FORWARD, lastTiedForward);
	
	e->set<Bool>(TIED_FORWARD, true);
	erase(i);

	for (DurationList::iterator dli = dl.begin(); dli != dl.end(); ++dli) {

	    DurationList::iterator dlj(dli);
	    if (++dlj == dl.end()) {
		// end of duration list
		if (!lastTiedForward) e->unset(TIED_FORWARD);
		toInsert.push_back(e);
		e = 0;
		break;
	    }
	    
	    std::pair<Event *, Event *> splits =
		splitPreservingPerformanceTimes(e, *dli);
	    
	    if (!splits.first || !splits.second) {
		cerr << "WARNING: SegmentNotationHelper::makeNoteViable(): No valid split for event of duration " << e->getDuration() << " at " << e->getAbsoluteTime() << " (split duration " << *dli << "), ignoring remainder\n";
		cerr << "WARNING: This is probably a bug; fix required" << std::endl;
		toInsert.push_back(e);
		e = 0;
		break;
	    }
	    
	    toInsert.push_back(splits.first);
	    delete e;
	    e = splits.second;
	    
	    acc += *dli;
	    
	    e->set<Bool>(TIED_BACKWARD, true);
	}
	
	delete e;

    // Insert new events into segement
    for (std::vector<Event *>::iterator ei = toInsert.begin();
	 ei != toInsert.end(); ++ei) {
	insert(*ei);
    }
    
    // Make assumption that toInsert.begin() != toInsert.end()
    return *(toInsert.begin());

}
void
SegmentNotationHelper::makeNotesViable(iterator from, iterator to,
				       bool splitAtBars)
{
    std::vector<Event *> toInsert;

    for (Segment::iterator i = from, j = i;
	 segment().isBeforeEndMarker(i) && i != to; i = j) {

	++j;
	
	makeThisNoteViable(i, splitAtBars);
    }

}

void
SegmentNotationHelper::makeNotesViable(timeT startTime, timeT endTime,
				       bool splitAtBars)
{
    Segment::iterator from = segment().findTime(startTime);
    Segment::iterator to = segment().findTime(endTime);

    makeNotesViable(from, to, splitAtBars);
}


Segment::iterator
SegmentNotationHelper::insertNote(timeT absoluteTime, Note note, int pitch,
				  Accidental explicitAccidental)
{
    Event *e = new Event(Note::EventType, absoluteTime, note.getDuration());
    e->set<Int>(PITCH, pitch);
    e->set<String>(ACCIDENTAL, explicitAccidental);
    iterator i = insertNote(e);
    delete e;
    return i;
}

Segment::iterator
SegmentNotationHelper::insertNote(Event *modelEvent)
{
    timeT absoluteTime = modelEvent->getAbsoluteTime();
    iterator i = segment().findNearestTime(absoluteTime);

    // If our insertion time doesn't match up precisely with any
    // existing event, and if we're inserting over a rest, split the
    // rest at the insertion time first.

    if (i != end() &&
	(*i)->getAbsoluteTime() < absoluteTime &&
	(*i)->getAbsoluteTime() + (*i)->getDuration() > absoluteTime &&
	(*i)->isa(Note::EventRestType)) {
	i = splitIntoTie(i, absoluteTime - (*i)->getAbsoluteTime());
    }

    timeT duration = modelEvent->getDuration();

    if (i != end() && (*i)->has(BEAMED_GROUP_TUPLET_BASE)) {
	duration = duration * (*i)->get<Int>(BEAMED_GROUP_TUPLED_COUNT) /
	    (*i)->get<Int>(BEAMED_GROUP_UNTUPLED_COUNT);
    }

    //!!! Deal with end-of-bar issues!

    return insertSomething(i, duration, modelEvent, false);
}


Segment::iterator
SegmentNotationHelper::insertRest(timeT absoluteTime, Note note)
{
    iterator i, j;
    segment().getTimeSlice(absoluteTime, i, j);

    //!!! Deal with end-of-bar issues!

    timeT duration(note.getDuration());

    if (i != end() && (*i)->has(BEAMED_GROUP_TUPLET_BASE)) {
	duration = duration * (*i)->get<Int>(BEAMED_GROUP_TUPLED_COUNT) /
	    (*i)->get<Int>(BEAMED_GROUP_UNTUPLED_COUNT);
    }

    Event *modelEvent = new Event(Note::EventRestType, absoluteTime,
				  note.getDuration(),
				  Note::EventRestSubOrdering);

    i = insertSomething(i, duration, modelEvent, false);
    delete modelEvent;
    return i;
}


// return an iterator pointing to the "same" event as the original
// iterator (which will have been replaced)

Segment::iterator
SegmentNotationHelper::collapseRestsForInsert(iterator i,
					      timeT desiredDuration)
{
    // collapse at most once, then recurse

    if (!segment().isBeforeEndMarker(i) ||
	!(*i)->isa(Note::EventRestType)) return i;

    timeT d = (*i)->getDuration();
    iterator j = findContiguousNext(i); // won't return itr after end marker
    if (d >= desiredDuration || j == end()) return i;

    Event *e(new Event(**i, (*i)->getAbsoluteTime(), d + (*j)->getDuration()));
    iterator ii(insert(e));
    erase(i);
    erase(j);

    return collapseRestsForInsert(ii, desiredDuration);
}


Segment::iterator
SegmentNotationHelper::insertSomething(iterator i, int duration,
				       Event *modelEvent, bool tiedBack)
{
    // Rules:
    // 
    // 1. If we hit a bar line in the course of the intended inserted
    // note, we should split the note rather than make the bar the
    // wrong length.  (Not implemented yet)
    // [NB. This is now implemented, but not here -- see end of
    // NoteInsertionCommand::modifySegment --cc, 20110428]
    //
    // 2. If there's nothing at the insertion point but rests (and
    // enough of them to cover the entire duration of the new note),
    // then we should insert the new note/rest literally and remove
    // rests as appropriate.  Rests should never prevent us from
    // inserting what the user asked for.
    // 
    // 3. If there are notes in the way of an inserted note, however,
    // we split whenever "reasonable" and truncate our user's note if
    // not reasonable to split.  We can't always give users the Right
    // Thing here, so to hell with them.

    while (i != end() &&
	   ((*i)->getDuration() == 0 ||
	    !((*i)->isa(Note::EventType) || (*i)->isa(Note::EventRestType))))
	++i;

    if (i == end()) {
	return insertSingleSomething(i, duration, modelEvent, tiedBack);
    }

    // If there's a rest at the insertion position, merge it with any
    // following rests, if available, until we have at least the
    // duration of the new note.
    i = collapseRestsForInsert(i, duration);

    timeT existingDuration = (*i)->getNotationDuration();

//    cerr << "SegmentNotationHelper::insertSomething: asked to insert duration " << duration
//	 << " over event of duration " << existingDuration << ":" << endl;
    (*i)->dump(cerr);

    if (duration == existingDuration) {

        // 1. If the new note or rest is the same length as an
        // existing note or rest at that position, chord the existing
        // note or delete the existing rest and insert.

//	cerr << "Durations match; doing simple insert" << endl;

	return insertSingleSomething(i, duration, modelEvent, tiedBack);

    } else if (duration < existingDuration) {

        // 2. If the new note or rest is shorter than an existing one,
        // split the existing one and chord or replace the first part.

	if ((*i)->isa(Note::EventType)) {

	    if (!isSplitValid(duration, existingDuration - duration)) {

//		cerr << "Bad split, coercing new note" << endl;

		// not reasonable to split existing note, so force new one
		// to same duration instead
		duration = (*i)->getNotationDuration();

	    } else {
//		cerr << "Good split, splitting old event" << endl;
		splitIntoTie(i, duration);
	    }
	} else if ((*i)->isa(Note::EventRestType)) {

//	    cerr << "Found rest, splitting" << endl;
	    iterator last = splitIntoTie(i, duration);

            // Recover viability for the second half of any split rest
	    // (we duck out of this if we find we're in a tupleted zone)

	    if (last != end() && !(*last)->has(BEAMED_GROUP_TUPLET_BASE)) {
		makeRestViable(last);
	    }
	}

	return insertSingleSomething(i, duration, modelEvent, tiedBack);

    } else { // duration > existingDuration

        // 3. If the new note is longer, split the new note so that
        // the first part is the same duration as the existing note or
        // rest, and recurse to step 1 with both the first and the
        // second part in turn.

	bool needToSplit = true;

	// special case: existing event is a rest, and it's at the end
	// of the segment

	if ((*i)->isa(Note::EventRestType)) {
	    iterator j;
	    for (j = i; j != end(); ++j) {
		if ((*j)->isa(Note::EventType)) break;
	    }
	    if (j == end()) needToSplit = false;
	}
	
	if (needToSplit) {

	    //!!! This is not quite right for rests.  Because they
	    //replace (rather than chording with) any events already
	    //present, they don't need to be split in the case where
	    //their duration spans several note-events.  Worry about
	    //that later, I guess.  We're actually getting enough
	    //is-note/is-rest decisions here to make it possibly worth
	    //splitting this method into note and rest versions again

//	    cerr << "Need to split new note" << endl;

	    i = insertSingleSomething
		(i, existingDuration, modelEvent, tiedBack);

	    if (modelEvent->isa(Note::EventType))
		(*i)->set<Bool>(TIED_FORWARD, true);
	    
	    timeT insertedTime = (*i)->getAbsoluteTime();
	    while (i != end() &&
		   ((*i)->getNotationAbsoluteTime() <
		    (insertedTime + existingDuration))) ++i;

	    return insertSomething
		(i, duration - existingDuration, modelEvent, true);

	} else {
//	    cerr << "No need to split new note" << endl;
	    return insertSingleSomething(i, duration, modelEvent, tiedBack);
	}
    }
}

Segment::iterator
SegmentNotationHelper::insertSingleSomething(iterator i, int duration,
					     Event *modelEvent, bool tiedBack)
{
    timeT time;
    timeT notationTime;
    bool eraseI = false;
    timeT effectiveDuration(duration);

    if (i == end()) {
	time = segment().getEndTime();
	notationTime = time;
    } else {
	time = (*i)->getAbsoluteTime();
	notationTime = (*i)->getNotationAbsoluteTime();
	if (modelEvent->isa(Note::EventRestType) ||
	    (*i)->isa(Note::EventRestType)) eraseI = true;
    }

    Event *e = new Event(*modelEvent, time, effectiveDuration,
			 modelEvent->getSubOrdering(), notationTime);

    // If the model event already has group info, I guess we'd better use it!
    if (!e->has(BEAMED_GROUP_ID)) {
	setInsertedNoteGroup(e, i);
    }

    if (tiedBack && e->isa(Note::EventType)) {
        e->set<Bool>(TIED_BACKWARD, true);
    }

    if (eraseI) {
	// erase i and all subsequent events with the same type and
	// absolute time
	timeT time((*i)->getAbsoluteTime());
	std::string type((*i)->getType());
	iterator j(i);
	while (j != end() && (*j)->getAbsoluteTime() == time) {
	    ++j;
	    if ((*i)->isa(type)) erase(i);
	    i = j;
	}
    }

    return insert(e);
}

void
SegmentNotationHelper::setInsertedNoteGroup(Event *e, iterator i)
{
    // Formerly this was posited on the note being inserted between
    // two notes in the same group, but that's quite wrong-headed: we
    // want to place it in the same group as any existing note at the
    // same time, and otherwise leave it alone.

    e->unset(BEAMED_GROUP_ID);
    e->unset(BEAMED_GROUP_TYPE);

    while (isBeforeEndMarker(i) &&
	   (!((*i)->isa(Note::EventRestType)) ||
	    (*i)->has(BEAMED_GROUP_TUPLET_BASE)) &&
	   (*i)->getNotationAbsoluteTime() == e->getAbsoluteTime()) {

	if ((*i)->has(BEAMED_GROUP_ID)) {

	    string type = (*i)->get<String>(BEAMED_GROUP_TYPE);
	    if (type != GROUP_TYPE_TUPLED && !(*i)->isa(Note::EventType)) {
		if ((*i)->isa(Note::EventRestType)) return;
		else {
		    ++i;
		    continue;
		}
	    }

	    e->set<Int>(BEAMED_GROUP_ID, (*i)->get<Int>(BEAMED_GROUP_ID));
	    e->set<String>(BEAMED_GROUP_TYPE, type);

	    if ((*i)->has(BEAMED_GROUP_TUPLET_BASE)) {

		e->set<Int>(BEAMED_GROUP_TUPLET_BASE,
			    (*i)->get<Int>(BEAMED_GROUP_TUPLET_BASE));
		e->set<Int>(BEAMED_GROUP_TUPLED_COUNT,
			    (*i)->get<Int>(BEAMED_GROUP_TUPLED_COUNT));
		e->set<Int>(BEAMED_GROUP_UNTUPLED_COUNT,
			    (*i)->get<Int>(BEAMED_GROUP_UNTUPLED_COUNT));
	    }

	    return;
	}

	++i;
    }
}


Segment::iterator
SegmentNotationHelper::insertClef(timeT absoluteTime, Clef clef)
{
    return insert(clef.getAsEvent(absoluteTime));
}


Segment::iterator
SegmentNotationHelper::insertSymbol(timeT absoluteTime, Symbol symbol)
{
    return insert(symbol.getAsEvent(absoluteTime));
}


Segment::iterator
SegmentNotationHelper::insertKey(timeT absoluteTime, Key key)
{
    return insert(key.getAsEvent(absoluteTime));
}


Segment::iterator
SegmentNotationHelper::insertText(timeT absoluteTime, Text text)
{
    return insert(text.getAsEvent(absoluteTime));
}


void
SegmentNotationHelper::deleteNote(Event *e, bool collapseRest)
{
    iterator i = segment().findSingle(e);

    if (i == end()) return;

    if ((*i)->has(TIED_BACKWARD) && (*i)->get<Bool>(TIED_BACKWARD)) {
	iterator j = getPreviousAdjacentNote(i, segment().getStartTime(),
					     true, false);
	if (j != end()) {
	    (*j)->unset(TIED_FORWARD); // don't even check if it has it set
	}
    }	

    if ((*i)->has(TIED_FORWARD) && (*i)->get<Bool>(TIED_FORWARD)) {
	iterator j = getNextAdjacentNote(i, true, false);
	if (j != end()) {
	    (*j)->unset(TIED_BACKWARD); // don't even check if it has it set
	}
    }	

    // If any notes start at the same time as this one but end first,
    // or start after this one starts but before it ends, then we go
    // for the delete-event-and-normalize-rests option.  Otherwise
    // (the notationally simpler case) we go for the
    // replace-note-by-rest option.  We still lose in the case where
    // another note starts before this one, overlaps it, but then also
    // ends before it does -- but I think we can live with that.
    
    iterator j = i;
    timeT endTime = (*i)->getAbsoluteTime() + (*i)->getDuration();

    while (j != end() && (*j)->getAbsoluteTime() < endTime) {
	
	bool complicatedOverlap = false;

	if ((*j)->getAbsoluteTime() != (*i)->getAbsoluteTime()) {
	    complicatedOverlap = true;
	} else if (((*j)->getAbsoluteTime() + (*j)->getDuration()) < endTime) {
	    complicatedOverlap = true;
	}

	if (complicatedOverlap) {
	    timeT startTime = (*i)->getAbsoluteTime();
	    segment().erase(i);
	    segment().normalizeRests(startTime, endTime);
	    return;
	}

	++j;
    }	

    if (noteIsInChord(e)) {

	erase(i);

    } else {
    if (e->has(BEAMED_GROUP_TUPLET_BASE)==false){
       // replace with a rest
       Event *newRest = new Event(Note::EventRestType,
                                  e->getAbsoluteTime(), e->getDuration(),
                                  Note::EventRestSubOrdering);
       insert(newRest);
       erase(i);
       // collapse the new rest
       if (collapseRest) {
            bool dummy;
            collapseRestsIfValid(newRest, dummy);
        }

    }else{
        int untupled = e->get<Int>(BEAMED_GROUP_UNTUPLED_COUNT);
        iterator begin, end;
        int count = findBorderTuplet(i, begin, end);
        if (count>1){
            // insert rest instead of note
            string type = (*i)->getType();
            int note_type = (*i)->get<Int>(NOTE_TYPE);
            insertRest((*i)->getAbsoluteTime(), Note(note_type,0));
        }else {
            // replace with a rest
            timeT time = (*begin)->getAbsoluteTime();
            Event *newRest = new Event(Note::EventRestType,
                    (*begin)->getAbsoluteTime(),
                    (*begin)->getDuration()*untupled,
                    Note::EventRestSubOrdering);
            segment().erase(begin, end);
            insert(newRest);
            timeT startTime = segment().getStartTime();
            if (time==startTime){
                begin=segment().findTime(startTime);
                (*begin)->unset(BEAMED_GROUP_ID);
                (*begin)->unset(BEAMED_GROUP_TYPE);
                (*begin)->unset(BEAMED_GROUP_TUPLET_BASE);
                (*begin)->unset(BEAMED_GROUP_TUPLED_COUNT);
                (*begin)->unset(BEAMED_GROUP_UNTUPLED_COUNT);
            }

            // collapse the new rest
            if (collapseRest) {
                bool dummy;
                collapseRestsIfValid(newRest, dummy);
            }
        }
    }
    }
}

bool
SegmentNotationHelper::deleteRest(Event *e)
{
    bool collapseForward;
    return collapseRestsIfValid(e, collapseForward);
}

bool
SegmentNotationHelper::deleteEvent(Event *e, bool collapseRest)
{
    bool res = true;

    if (e->isa(Note::EventType)) deleteNote(e, collapseRest);
    else if (e->isa(Note::EventRestType)) res = deleteRest(e);
    else {
        // just plain delete
        iterator i = segment().findSingle(e);
	if (i != end()) erase(i);
    }

    return res;    
}


bool
SegmentNotationHelper::hasEffectiveDuration(iterator i)
{
    bool hasDuration = ((*i)->getDuration() > 0);

    if ((*i)->isa(Note::EventType)) {
	iterator i0(i);
	if (++i0 != end() &&
	    (*i0)->isa(Note::EventType) &&
	    (*i0)->getNotationAbsoluteTime() == 
	     (*i)->getNotationAbsoluteTime()) {
	    // we're in a chord or something
	    hasDuration = false;
	}
    }
    
    return hasDuration;
}


void
SegmentNotationHelper::makeBeamedGroup(timeT from, timeT to, string type)
{
    makeBeamedGroupAux(segment().findTime(from), segment().findTime(to),
		       type, false);
}

void
SegmentNotationHelper::makeBeamedGroup(iterator from, iterator to, string type)
{
    makeBeamedGroupAux
      ((from == end()) ? from : segment().findTime((*from)->getAbsoluteTime()),
	 (to == end()) ? to   : segment().findTime((*to  )->getAbsoluteTime()),
       type, false);
}

void
SegmentNotationHelper::makeBeamedGroupExact(iterator from, iterator to, string type)
{
    makeBeamedGroupAux(from, to, type, true);
}

void
SegmentNotationHelper::makeBeamedGroupAux(iterator from, iterator to,
					  string type, bool groupGraces)
{
//    cerr << "SegmentNotationHelper::makeBeamedGroupAux: type " << type << endl;
//    if (from == to) cerr << "from == to" <<endl;

    int groupId = segment().getNextId();
    bool beamedSomething = false;

    for (iterator i = from; i != to; ++i) {
//	std::cerr << "looking at " << (*i)->getType() << " at " << (*i)->getAbsoluteTime() << std::endl;

	// don't permit ourselves to change the type of an
	// already-grouped event here
	if ((*i)->has(BEAMED_GROUP_TYPE) &&
	    (*i)->get<String>(BEAMED_GROUP_TYPE) != GROUP_TYPE_BEAMED) {
	    continue;
	}

	if (!groupGraces) {
	    if ((*i)->has(IS_GRACE_NOTE) &&
		(*i)->get<Bool>(IS_GRACE_NOTE)) {
		continue;
	    }
	}

	// don't beam anything longer than a quaver unless it's
	// between beamed quavers -- in which case marking it as
	// beamed will ensure that it gets re-stemmed appropriately

	if ((*i)->isa(Note::EventType) &&
	    (*i)->getNotationDuration() >= Note(Note::Crotchet).getDuration()) {
//	    std::cerr << "too long" <<std::endl;
	    if (!beamedSomething) continue;
	    iterator j = i;
	    bool somethingLeft = false;
	    while (++j != to) {
		if ((*j)->getType() == Note::EventType &&
		    (*j)->getNotationAbsoluteTime() > (*i)->getNotationAbsoluteTime() &&
		    (*j)->getNotationDuration() < Note(Note::Crotchet).getDuration()) {
		    somethingLeft = true;
		    break;
		}
	    }
	    if (!somethingLeft) continue;
	}

//	std::cerr << "beaming it" <<std::endl;
        (*i)->set<Int>(BEAMED_GROUP_ID, groupId);
        (*i)->set<String>(BEAMED_GROUP_TYPE, type);
    }
}

void
SegmentNotationHelper::makeTupletGroup(timeT t, int untupled, int tupled,
				       timeT unit)
{
    int groupId = segment().getNextId();

    cerr << "SegmentNotationHelper::makeTupletGroup: time " << t << ", unit "<< unit << ", params " << untupled << "/" << tupled << ", id " << groupId << endl;

    list<Event *> toInsert;
    list<iterator> toErase;
    timeT notationTime = t;
    timeT fillWithRestsTo = t;
    bool haveStartNotationTime = false;

    for (iterator i = segment().findTime(t); i != end(); ++i) {

	if (!haveStartNotationTime) {
	    notationTime = (*i)->getNotationAbsoluteTime();
	    fillWithRestsTo = notationTime + (untupled * unit);
	    haveStartNotationTime = true;
	}

	if ((*i)->getNotationAbsoluteTime() >=
	    notationTime + (untupled * unit)) break;

	timeT offset = (*i)->getNotationAbsoluteTime() - notationTime;
	timeT duration = (*i)->getNotationDuration();

	if ((*i)->isa(Note::EventRestType) &&
	    ((offset + duration) > (untupled * unit))) {
	    fillWithRestsTo = std::max(fillWithRestsTo,
				       notationTime + offset + duration);
	    duration = (untupled * unit) - offset;
	    if (duration <= 0) {
		toErase.push_back(i);
		continue;
	    }
	}

	Event *e = new Event(**i,
			     notationTime + (offset * tupled / untupled),
			     duration * tupled / untupled);

	cerr << "SegmentNotationHelper::makeTupletGroup: made event at time " << e->getAbsoluteTime() << ", duration " << e->getDuration() << endl;

	e->set<Int>(BEAMED_GROUP_ID, groupId);
	e->set<String>(BEAMED_GROUP_TYPE, GROUP_TYPE_TUPLED);

	e->set<Int>(BEAMED_GROUP_TUPLET_BASE, unit);
	e->set<Int>(BEAMED_GROUP_TUPLED_COUNT, tupled);
	e->set<Int>(BEAMED_GROUP_UNTUPLED_COUNT, untupled);

	toInsert.push_back(e);
	toErase.push_back(i);
    }

    for (list<iterator>::iterator i = toErase.begin();
	 i != toErase.end(); ++i) {
	segment().erase(*i);
    }

    for (list<Event *>::iterator i = toInsert.begin();
	 i != toInsert.end(); ++i) {
	segment().insert(*i);
    }

    if (haveStartNotationTime) {
	segment().fillWithRests(notationTime + (tupled * unit),
				fillWithRestsTo);
    }
}

    


void
SegmentNotationHelper::unbeam(timeT from, timeT to)
{
    unbeamAux(segment().findTime(from), segment().findTime(to));
}

void
SegmentNotationHelper::unbeam(iterator from, iterator to)
{
    unbeamAux
     ((from == end()) ? from : segment().findTime((*from)->getAbsoluteTime()),
        (to == end()) ? to   : segment().findTime((*to  )->getAbsoluteTime()));
}

void
SegmentNotationHelper::unbeamAux(iterator from, iterator to)
{
    for (iterator i = from; i != to; ++i) {
	(*i)->unset(BEAMED_GROUP_ID);
	(*i)->unset(BEAMED_GROUP_TYPE);
	(*i)->clearNonPersistentProperties();
    }
}



/*

  Auto-beaming code derived from X11 Rosegarden's ItemListAutoBeam
  and ItemListAutoBeamSub in editor/src/ItemList.c.
  
*/

void
SegmentNotationHelper::autoBeam(timeT from, timeT to, string type)
{
    /*
    std::cerr << "autoBeam from " << from << " to " << to << " on segment start time " << segment().getStartTime() << ", end time " << segment().getEndTime() << ", end marker " << segment().getEndMarkerTime() << std::endl;
    */

    autoBeam(segment().findTime(from), segment().findTime(to), type);
}

void
SegmentNotationHelper::autoBeam(iterator from, iterator to, string type)
{
    // This can only manage whole bars at a time, and it will split
    // the from-to range out to encompass the whole bars in which they
    // each occur

    if (!segment().getComposition()) {
	cerr << "WARNING: SegmentNotationHelper::autoBeam requires Segment be in a Composition" << endl;
	return;
    }

    if (!segment().isBeforeEndMarker(from)) return;

    unbeam(from, to);

    Composition *comp = segment().getComposition();

    int fromBar = comp->getBarNumber((*from)->getAbsoluteTime());
    int toBar = comp->getBarNumber(segment().isBeforeEndMarker(to) ?
				   (*to)->getAbsoluteTime() :
				   segment().getEndMarkerTime());

    for (int barNo = fromBar; barNo <= toBar; ++barNo) {

	std::pair<timeT, timeT> barRange = comp->getBarRange(barNo);
	iterator barStart = segment().findTime(barRange.first);
	iterator barEnd   = segment().findTime(barRange.second);

	// Make sure we're examining the notes defined to be within
	// the bar in notation terms rather than raw terms
	
	while (barStart != segment().end() &&
	       (*barStart)->getNotationAbsoluteTime() < barRange.first) ++barStart;

	iterator scooter = barStart;
	if (barStart != segment().end()) {
	    while (scooter != segment().begin()) {
		--scooter;
		if ((*scooter)->getNotationAbsoluteTime() < barRange.first) break;
		barStart = scooter;
	    }
	}

	while (barEnd != segment().end() &&
	       (*barEnd)->getNotationAbsoluteTime() < barRange.second) ++barEnd;

	scooter = barEnd;
	if (barEnd != segment().end()) {
	    while (scooter != segment().begin()) {
		--scooter;
		if ((*scooter)->getNotationAbsoluteTime() < barRange.second) break;
		barEnd = scooter;
	    }
	}

	TimeSignature timeSig =
	    segment().getComposition()->getTimeSignatureAt(barRange.first);

	autoBeamBar(barStart, barEnd, timeSig, type);
    }
}


/*

  Derived from (and no less mystifying than) X11 Rosegarden's
  ItemListAutoBeamSub in editor/src/ItemList.c.

  "Today I want to celebrate "Montreal" by Autechre, because of
  its sleep-disturbing aura, because it sounds like the sort of music
  which would be going around in the gunman's head as he trains a laser
  sight into your bedroom through the narrow gap in your curtains and
  dances the little red dot around nervously on your wall."
  
*/

void
SegmentNotationHelper::autoBeamBar(iterator from, iterator to,
				   TimeSignature tsig, string type)
{
    int num = tsig.getNumerator();
    int denom = tsig.getDenominator();

    timeT average;
    timeT minimum = 0;

    // If the denominator is 2 or 4, beam in twos (3/4, 6/2 etc).
    
    if (denom == 2 || denom == 4) {

        if (num % 3) {
            average = Note(Note::Quaver).getDuration();
        } else {
            average = Note(Note::Semiquaver).getDuration();
            minimum = average;
        }

    } else {

        if (num % 3 == 0 && denom == 8) { // special hack for 6/8, 12/8 etc...
            average = 3 * Note(Note::Quaver).getDuration();

        } else {
            // find a divisor (at least 2) for the numerator
            int n = 2;
            while (num >= n && num % n != 0) ++n;
            average = n * Note(Note::Semiquaver).getDuration();
        }
    }

    if (minimum == 0) minimum = average / 2;
    if (denom > 4) average /= 2;

    autoBeamBar(from, to, average, minimum, average * 4, type);
}


void
SegmentNotationHelper::autoBeamBar(iterator from, iterator to,
				   timeT average, timeT minimum,
				   timeT maximum, string type)
{
    timeT accumulator = 0;
    timeT crotchet    = Note(Note::Crotchet).getDuration();
    timeT semiquaver  = Note(Note::Semiquaver).getDuration();

    iterator e = end();

    for (iterator i = from; i != to && i != e; ++i) {

        // only look at one note in each chord, and at rests
        if (!hasEffectiveDuration(i)) continue;
        timeT idur = (*i)->getNotationDuration();

	if (accumulator % average == 0 &&  // "beamable duration" threshold
	    idur < crotchet) {

	    // This could be the start of a beamed group.  We maintain
	    // two sorts of state as we scan along here: data about
	    // the best group we've found so far (beamDuration,
	    // prospective, k etc), and data about the items we're
	    // looking at (count, beamable, longerThanDemi etc) just
	    // in case we find a better candidate group before the
	    // eight-line conditional further down makes us give up
	    // the search, beam our best shot, and start again.

	    // I hope this is clear.

	    iterator k = end(); // best-so-far last item in group;
				// end() indicates that we've found nothing

	    timeT tmin         = minimum;
	    timeT count        = 0;
	    timeT prospective  = 0;
	    timeT beamDuration = 0;

	    int beamable       = 0;
	    int longerThanDemi = 0;

	    for (iterator j = i; j != to; ++j) {

		if (!hasEffectiveDuration(j)) continue;
                timeT jdur = (*j)->getNotationDuration();

		if ((*j)->isa(Note::EventType)) {
		    if (jdur < crotchet) ++beamable;
		    if (jdur >= semiquaver) ++longerThanDemi;
		}

		count += jdur;

		if (count % tmin == 0) {

		    k = j;
		    beamDuration = count;
		    prospective = accumulator + count;

		    // found a group; now accept only double this
		    // group's length for a better one
		    tmin *= 2;
		}

		// Stop scanning and make the group if our scan has
		// reached the maximum length of beamed group, we have
		// more than 4 semis or quavers, we're at the end of
		// our run, the next chord is longer than the current
		// one, or there's a rest ahead.  (We used to check
		// that the rest had non-zero duration, but the new
		// quantization regime should ensure that this doesn't
		// happen unless we really are displaying completely
		// unquantized data in which case anything goes.)

		iterator jnext(j);

		if ((count > maximum)
		    || (longerThanDemi > 4)
		    || (++jnext == to)     
		    || ((*j    )->isa(Note::EventType) &&
			(*jnext)->isa(Note::EventType) &&
			(*jnext)->getNotationDuration() > jdur)
		    || ((*jnext)->isa(Note::EventRestType))) {

		    if (k != end() && beamable >= 2) {

			iterator knext(k);
			++knext;

			makeBeamedGroup(i, knext, type);
		    }

		    // If this group is at least as long as the check
		    // threshold ("average"), its length must be a
		    // multiple of the threshold and hence we can
		    // continue scanning from the end of the group
		    // without losing the modulo properties of the
		    // accumulator.

		    if (k != end() && beamDuration >= average) {

			i = k;
			accumulator = prospective;

		    } else {

			// Otherwise, we continue from where we were.
			// (This must be safe because we can't get
			// another group starting half-way through, as
			// we know the last group is shorter than the
			// check threshold.)

			accumulator += idur;
		    }

		    break;
		}
	    }
	} else {

	    accumulator += idur;
	}
    }
}


// based on X11 Rosegarden's GuessItemListClef in editor/src/MidiIn.c

Clef
SegmentNotationHelper::guessClef(iterator from, iterator to)
{
    long totalHeight = 0;
    int noteCount = 0;

    // just the defaults:
    Clef clef;
    Key key;

    for (iterator i = from; i != to; ++i) {
        if ((*i)->isa(Note::EventType)) {
//!!!            NotationDisplayPitch p((*i)->get<Int>(PITCH), clef, key);
	    try {
		Pitch p(**i);
		totalHeight += p.getHeightOnStaff(clef, key);
		++noteCount;
	    } catch (Exception e) {
		// no pitch in note
	    }
        }
    }

    if    (noteCount == 0) return Clef(Clef::Treble);

    int average = totalHeight / noteCount;

    // Let's try these new extents and see how the fare.  Ideally these should
    // pick plain treble and bass clefs a reasonable amount of the time, and not
    // be too prone to picking transposed clefs, while picking transposed clefs
    // when really necessary.
    if      (average < -12) return Clef(Clef::Bass, -2);
    else if (average < - 9) return Clef(Clef::Bass, -1);
    else if (average <  -6) return Clef(Clef::Bass);
    else if (average <  -3) return Clef(Clef::Tenor);
    else if (average <   1) return Clef(Clef::Alto);
    else if (average <  12) return Clef(Clef::Treble);
    else if (average <  24) return Clef(Clef::Treble, 1);
    else if (average <  48) return Clef(Clef::Treble, 2);
    else                    return Clef(Clef::Treble);
}


bool
SegmentNotationHelper::removeRests(timeT time, timeT &duration, bool testOnly)
{
    Event dummy("dummy", time, 0, MIN_SUBORDERING);
    
    std::cerr << "SegmentNotationHelper::removeRests(" << time
	      << ", " << duration << ")" << std::endl;

    iterator from = segment().lower_bound(&dummy);

    // ignore any number of zero-duration events at the start
    while (from != segment().end() &&
	   (*from)->getAbsoluteTime() == time &&
	   (*from)->getDuration() == 0) ++from;
    if (from == segment().end()) return false;
    
    iterator to = from;

    timeT eventTime = time;
    timeT finalTime = time + duration;

    //!!! We should probably not use an accumulator, but instead
    // calculate based on each event's absolute time + duration --
    // in case we've somehow ended up with overlapping rests

    // Iterate on events, checking if all are rests
    //
    while ((eventTime < finalTime) && (to != end())) {

        if (!(*to)->isa(Note::EventRestType)) {
            // a non-rest was found
	    duration = (*to)->getAbsoluteTime() - time;
            return false;
        }

        timeT nextEventDuration = (*to)->getDuration();

        if ((eventTime + nextEventDuration) <= finalTime) {
            eventTime += nextEventDuration;
	    duration = eventTime - time;
	} else break;

        ++to;
    }

    bool checkLastRest = false;
    iterator lastEvent = to;
    
    if (eventTime < finalTime) {
        // shorten last event's duration, if possible


        if (lastEvent == end()) {
	    duration = segment().getEndTime() - time;
            return false;
        }

	if (!testOnly) {
	    // can't safely change the absolute time of an event in a segment
	    Event *newEvent = new Event(**lastEvent, finalTime,
					(*lastEvent)->getDuration() -
					(finalTime - eventTime));
	    duration = finalTime + (*lastEvent)->getDuration() - time;
	    bool same = (from == to);
	    segment().erase(lastEvent);
	    to = lastEvent = segment().insert(newEvent);
	    if (same) from = to;
	    checkLastRest = true;
	}
    }

    if (testOnly) return true;

    segment().erase(from, to);

    // we must defer calling makeRestViable() until after erase,
    // because it will invalidate 'to'
    //
    if (checkLastRest) makeRestViable(lastEvent);

    return true;
}


void
SegmentNotationHelper::collapseRestsAggressively(timeT startTime,
						 timeT endTime)
{
    reorganizeRests(startTime, endTime,
		    &SegmentNotationHelper::mergeContiguousRests);
}


void
SegmentNotationHelper::reorganizeRests(timeT startTime, timeT endTime,
				       Reorganizer reorganizer)
{
    iterator ia = segment().findTime(startTime);
    iterator ib = segment().findTime(endTime);
    
    if (ia == end()) return;

    std::vector<iterator> erasable;
    std::vector<Event *> insertable;

//    cerr << "SegmentNotationHelper::reorganizeRests (" << startTime << ","
//	 << endTime << ")" << endl;

    for (iterator i = ia; i != ib; ++i) {

	if ((*i)->isa(Note::EventRestType)) {

	    timeT startTime = (*i)->getAbsoluteTime();
	    timeT duration = 0;
	    iterator j = i;

	    for ( ; j != ib; ++j) {

		if ((*j)->isa(Note::EventRestType)) {
		    duration += (*j)->getDuration();
		    erasable.push_back(j);
		} else break;
	    }

	    (this->*reorganizer)(startTime, duration, insertable);
	    if (j == ib) break;
	    i = j;
	}
    }

    for (size_t ei = 0; ei < erasable.size(); ++ei)
	segment().erase(erasable[ei]);

    for (size_t ii = 0; ii < insertable.size(); ++ii)
	segment().insert(insertable[ii]);
}


void
SegmentNotationHelper::normalizeContiguousRests(timeT startTime,
						timeT duration,
						std::vector<Event *> &toInsert)
{
    TimeSignature ts;
    timeT sigTime =
	segment().getComposition()->getTimeSignatureAt(startTime, ts);

//    cerr << "SegmentNotationHelper::normalizeContiguousRests:"
//	 << " startTime = " << startTime << ", duration = "
//	 << duration << endl;

    DurationList dl;
    ts.getDurationListForInterval(dl, duration, startTime - sigTime);

    timeT acc = startTime;

    for (DurationList::iterator i = dl.begin(); i != dl.end(); ++i) {
	Event *e = new Event(Note::EventRestType, acc, *i,
			     Note::EventRestSubOrdering);
	toInsert.push_back(e);
	acc += *i;
    }
}


void
SegmentNotationHelper::mergeContiguousRests(timeT startTime,
					    timeT duration,
					    std::vector<Event *> &toInsert)
{
    while (duration > 0) {

	timeT d = Note::getNearestNote(duration).getDuration();

	Event *e = new Event(Note::EventRestType, startTime, d,
			     Note::EventRestSubOrdering);
	toInsert.push_back(e);

	startTime += d;
	duration -= d;
    }
}


Segment::iterator
SegmentNotationHelper::collapseNoteAggressively(Event *note,
						timeT rangeEnd)
{
    iterator i = segment().findSingle(note);
    if (i == end()) return end();

    iterator j = getNextAdjacentNote(i, true, true);
    if (j == end() || (*j)->getAbsoluteTime() >= rangeEnd) return end();

    timeT iEnd = (*i)->getAbsoluteTime() + (*i)->getDuration();
    timeT jEnd = (*j)->getAbsoluteTime() + (*j)->getDuration();

    Event *newEvent = new Event
	(**i, (*i)->getAbsoluteTime(),
	 (std::max(iEnd, jEnd) - (*i)->getAbsoluteTime()));

    newEvent->unset(TIED_BACKWARD);
    newEvent->unset(TIED_FORWARD);

    // Unset any tied notes for these since this will trigger extra
    // deselecting fro mthe selection set.
    (*i)->unset(TIED_BACKWARD);
    (*i)->unset(TIED_FORWARD);
    
    (*j)->unset(TIED_BACKWARD);
    (*j)->unset(TIED_FORWARD);

    segment().erase(i);
    segment().erase(j);
    return segment().insert(newEvent);
}

std::pair<Event *, Event *>
SegmentNotationHelper::splitPreservingPerformanceTimes(Event *e, timeT q1)
{
    timeT ut = e->getAbsoluteTime();
    timeT ud = e->getDuration();
    timeT qt = e->getNotationAbsoluteTime();
    timeT qd = e->getNotationDuration();

    timeT u1 = (qt + q1) - ut;
    timeT u2 = (ut + ud) - (qt + q1);

//    std::cerr << "splitPreservingPerformanceTimes: (ut,ud) (" << ut << "," << ud << "), (qt,qd) (" << qt << "," << qd << ") q1 " << q1 << ", u1 " << u1 << ", u2 " << u2 << std::endl;

    if (u1 <= 0 || u2 <= 0) { // can't do a meaningful split
	return std::pair<Event *, Event *>(0, 0);
    }

    Event *e1 = new Event(*e, ut, u1, e->getSubOrdering(), qt, q1);
    Event *e2 = new Event(*e, ut + u1, u2, e->getSubOrdering(), qt + q1, qd - q1);

    e1->set<Bool>(TIED_FORWARD, true);
    e2->set<Bool>(TIED_BACKWARD, true);

    return std::pair<Event *, Event *>(e1, e2);
}

void
SegmentNotationHelper::deCounterpoint(timeT startTime, timeT endTime)
{
    // How this should work: scan through the range and, for each
    // note "n" found, if the next following note "m" not at the same
    // absolute time as n starts before n ends, then split n at m-n.

    // also, if m starts at the same time as n but has a different
    // duration, we should split the longer of n and m at the shorter
    // one's duration.

    for (Segment::iterator i = segment().findTime(startTime);
	 segment().isBeforeEndMarker(i); ) {

	timeT t = (*i)->getAbsoluteTime();
	if (t >= endTime) break;

#ifdef DEBUG_DECOUNTERPOINT
	std::cerr << "SegmentNotationHelper::deCounterpoint: event at " << (*i)->getAbsoluteTime() << " notation " << (*i)->getNotationAbsoluteTime() << ", duration " << (*i)->getNotationDuration() << ", type " << (*i)->getType() << std::endl;
#endif	    

	if (!(*i)->isa(Note::EventType)) { ++i; continue; }

	timeT ti = (*i)->getNotationAbsoluteTime();
	timeT di = (*i)->getNotationDuration();

#ifdef DEBUG_DECOUNTERPOINT
	std::cerr<<"looking for k"<<std::endl;
#endif	    

	// find next event that's either at a different time or (if a
	// note) has a different duration
	Segment::iterator k = i;
	while (segment().isBeforeEndMarker(k)) {
	    if ((*k)->isa(Note::EventType)) {
#ifdef DEBUG_DECOUNTERPOINT
		std::cerr<<"abstime "<<(*k)->getAbsoluteTime()<< std::endl;
#endif	    
		if ((*k)->getNotationAbsoluteTime() > ti ||
		    (*k)->getNotationDuration() != di) break;
	    }
	    ++k;
	}

	if (!segment().isBeforeEndMarker(k)) break; // no split, no more notes

#ifdef DEBUG_DECOUNTERPOINT
	std::cerr << "k is at " << (k == segment().end() ? -1 : (*k)->getAbsoluteTime()) << ", notation " << (*k)->getNotationAbsoluteTime() << ", duration " << (*k)->getNotationDuration() << std::endl;
#endif	    

	timeT tk = (*k)->getNotationAbsoluteTime();
	timeT dk = (*k)->getNotationDuration();

	Event *e1 = 0, *e2 = 0;
	std::pair<Event *, Event *> splits;
	Segment::iterator toGo = segment().end();

	if (tk == ti && dk != di) {
	    // do the same-time-different-durations case
	    if (di > dk) { // split *i
#ifdef DEBUG_DECOUNTERPOINT
		std::cerr << "splitting i into " << dk << " and "<< (di-dk) << std::endl;
#endif	    
		splits = splitPreservingPerformanceTimes(*i, dk);

		toGo = i;
	    } else { // split *k
#ifdef DEBUG_DECOUNTERPOINT
		std::cerr << "splitting k into " << di << " and "<< (dk-di) << std::endl;
#endif	    
		splits = splitPreservingPerformanceTimes(*k, di);

		toGo = k;
	    }
	} else if (tk - ti > 0 && tk - ti < di) { // split *i
#ifdef DEBUG_DECOUNTERPOINT
	    std::cerr << "splitting i[*] into " << (tk-ti) << " and "<< (di-(tk-ti)) << std::endl;
#endif	    
	    splits = splitPreservingPerformanceTimes(*i, tk - ti);

	    toGo = i;
	}
	
	e1 = splits.first;
	e2 = splits.second;

	if (e1 && e2) { // e2 is the new note

	    e1->set<Bool>(TIED_FORWARD, true);
	    e2->set<Bool>(TIED_BACKWARD, true);

#ifdef DEBUG_DECOUNTERPOINT
	    std::cerr<<"Erasing:"<<std::endl;
	    (*toGo)->dump(std::cerr);
#endif	    

	    segment().erase(toGo);

#ifdef DEBUG_DECOUNTERPOINT
	    std::cerr<<"Inserting:"<<std::endl;
	    e1->dump(std::cerr);
#endif	    

	    segment().insert(e1);

#ifdef DEBUG_DECOUNTERPOINT
	    std::cerr<<"Inserting:"<<std::endl;
	    e2->dump(std::cerr);
#endif	    

	    segment().insert(e2);

	    i = segment().findTime(t);

#ifdef DEBUG_DECOUNTERPOINT
	    std::cerr<<"resync at " << t << ":" << std::endl;
	    if (i != segment().end()) (*i)->dump(std::cerr);
	    else std::cerr << "(end)" << std::endl;
#endif	    

	} else {

	    // no split here

#ifdef DEBUG_DECOUNTERPOINT
	    std::cerr<<"no split"<<std::endl;
#endif	    
	    ++i;
	}
    }

    segment().normalizeRests(startTime, endTime);
}


void
SegmentNotationHelper::autoSlur(timeT startTime, timeT endTime, bool legatoOnly)
{
    iterator from = segment().findTime(startTime);
    iterator to = segment().findTime(endTime);

    timeT potentialStart = segment().getEndTime();
    long  groupId = -1;
    timeT prevTime = startTime;
    int   count = 0;
    bool  thisLegato = false, prevLegato = false;

    for (iterator i = from; i != to && segment().isBeforeEndMarker(i); ++i) {

	timeT t = (*i)->getNotationAbsoluteTime();

	long newGroupId = -1;
	if ((*i)->get<Int>(BEAMED_GROUP_ID, newGroupId)) {
	    if (groupId == newGroupId) { // group continuing
		if (t > prevTime) {
		    ++count;
		    prevLegato = thisLegato;
		    thisLegato = Marks::hasMark(**i, Marks::Tenuto);
		}
		prevTime = t;
		continue;
	    }
	} else {
	    if (groupId == -1) continue; // no group
	}

	// a group has ended (and a new one might have begun)

	if (groupId >= 0 && count > 1 && (!legatoOnly || prevLegato)) {
	    Indication ind(Indication::Slur, t - potentialStart);
	    segment().insert(ind.getAsEvent(potentialStart));
	    if (legatoOnly) {
		for (iterator j = segment().findTime(potentialStart); j != i; ++j) {
		    Marks::removeMark(**j, Marks::Tenuto);
		}
	    }
	}

	potentialStart = t;
	groupId = newGroupId;
	prevTime = t;
	count = 0;
	thisLegato = false;
	prevLegato = false;
    }

    if (groupId >= 0 && count > 1 && (!legatoOnly || prevLegato)) {
	Indication ind(Indication::Slur, endTime - potentialStart);
	segment().insert(ind.getAsEvent(potentialStart));
	if (legatoOnly) {
	    for (iterator j = segment().findTime(potentialStart);
		 segment().isBeforeEndMarker(j) && j != to; ++j) {
		Marks::removeMark(**j, Marks::Tenuto);
	    }
	}
    }
}

int SegmentNotationHelper::findBorderTuplet(iterator it, iterator &start, iterator &end){
    iterator beginB = segment().findTime(segment().getBarStartForTime((*it)->getAbsoluteTime()));
    iterator endB = segment().findTime(segment().getBarEndForTime((*it)->getAbsoluteTime()));
    int maxcount = 0;
    int count = 0;
    int index = 0;
    bool ourTuplet = false;
    bool newTuplet = true;

    if ((*beginB)->getType()=="clefchange"){
        beginB++;
    }

    for ( ;beginB!=endB; ++beginB){
        index++;
        if (index>maxcount){
            index=1;
            count = 0;
            newTuplet=true;
        }
        if ((*beginB)->has(BEAMED_GROUP_TUPLET_BASE)==true){
            maxcount = (*beginB)->get<Int>(BEAMED_GROUP_UNTUPLED_COUNT);
            if ((*beginB)->getType() == "note"){
                count++;
            }
            if (it == beginB) ourTuplet = true;
            if (newTuplet){
                start = beginB;
                newTuplet=false;
            }
            if (ourTuplet && index==maxcount){
                end = ++beginB;
                return count;
            }
            continue;
        }else maxcount=0;
        if (ourTuplet == true){
            end = beginB;
            return count;
        }
        newTuplet = true;
        count = 0;
    }
    end=endB;
    return count;
}
} // end of namespace

