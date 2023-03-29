/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*- vi:set ts=8 sts=4 sw=4: */

/*
    Rosegarden
    A sequencer and musical notation editor.
    Copyright 2000-2023 the Rosegarden development team.
    Modifications and additions Copyright (c) 2022,2023 Mark R. Rubin aka "thanks4opensource" aka "thanks4opensrc"
    See the AUTHORS file for more details.

    This file is Copyright 2002
        Randall Farmer      <rfarme@simons-rock.edu>

    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License as
    published by the Free Software Foundation; either version 2 of the
    License, or (at your option) any later version.  See the file
    COPYING included with this distribution for more information.
*/

#ifndef RG_ANALYSISTYPES_H
#define RG_ANALYSISTYPES_H

#include <string>
#include <map>
#include <set>
#include <vector>

#include "base/NotationTypes.h"

namespace Rosegarden
{

class Segment;
class Event;
class CompositionTimeSliceAdapter;
class Quantizer;
class Composition;
class ChordAnalyzerImpl;
class Key;


class ChordAnalyzer
{
  public:
    // Chord name styles
    static const unsigned MAJOR_BLANK           = 0,
                          MAJOR_M               = 1,
                          MAJOR_MAJ             = 2,

                          MINOR_M               = 0,
                          MINOR_MINUS           = 1,
                          MINOR_MIN             = 2,
                          MINOR_MI              = 3,
                          MINOR_ROMAN_BLANK     = 4,  // internal use only

                          DIMINISHED_DIM        = 0,
                          DIMINISHED_CIRCLE     = 1,

                          AUGMENTED_AUG         = 0,
                          AUGMENTED_PLUS        = 1,

                          MAJOR_7TH_M           = 0,
                          MAJOR_7TH_MAJ         = 1,
                          MAJOR_7TH_DELTA       = 2,

                          ADD_ADD               = 0,
                          ADD_PLUS              = 1,

                          ROMAN_MINOR_UPPERCASE = 0,
                          ROMAN_MINOR_LOWERCASE = 1;

    ChordAnalyzer();
    ~ChordAnalyzer();

    // Set chord names ("aug" vs "+", etc) to user preferences
    // See constants, directly above
    void updateChordNameStyles();

    // Clear and then fill "chordsAndKeys" with chord name and key change
    //   text events. Also add C major key change at beginning of each
    //   segment in "segments"  (and "chordsAndKeys") if no key change at
    //   segment start time.
    // Clear and fill "keys" map with key tonic at each "chordsAndKeys"
    //   key change time.
    // Clear and fill "roots" map with chord root at each "chordsAndKeys"
    //   chord time.
    // Use notes in all "segments" name chords.
    // Use "currentSegment" for key at chord times.
    // If "keysOnly", only fille "chordsAndKeys" with key changes
    //   (no chords).
    // Two different chord analysis algorithms:
    //   If "onePerTimePeriod" true, all notes within "timeWindow"
    //      period (aligned modulo that time). Includes arpeggiated
    //      chords.
    //   Else whenever any note or off, within timeWindow tolerance.
    void labelChords(std::map<const timeT,
                              const std::string>    &chords,
                     std::map<const timeT, int>     &roots,
                     const std::vector<const Segment*>  &segments,
                     const Segment                  *currentSegment,
                     const timeT                     quantizationWindow,
                     const timeT                     arpeggiationWindow,
                     bool                            onePerTimePeriod
                     ,
                     const bool                      wholeComposition,
                     const timeT                     beginTime,
                     const timeT                     endTime);

  protected:
    // Private implemenation, to keep large amount of code and
    // static data exlusively in .cpp file.
    ChordAnalyzerImpl   *m_impl;

};



// All following is very old code, not exposed in user interface
//   at ther time of this (ChordAnalyzer class above, etc) rewrite.
// Potentially contains more sophisticated (context-sensitive)
//   chord/key analysis algorithms.
namespace AnalysisHelper
{
    /**
     * Returns a time signature that is probably reasonable for the
     * given timeslice.
     */
     TimeSignature guessTimeSignature(CompositionTimeSliceAdapter &c);

    /**
     * Returns a guess at the starting key of the given timeslice,
     * based only on notes, not existing key signatures.
     */
    Key guessKey(CompositionTimeSliceAdapter &c);

    /**
     * Returns a guess at the appropriate key at time t, based on
     * existing key signatures.  May fall back to guessKey.
     */
    Key guessKeyAtTime(Composition &comp, timeT t,
                       const Segment *segmentToSkip);

    /**
     * Returns a guess at the appropriate key for segment s at time t.
     */
    Key guessKeyForSegment(timeT t, const Segment *s);

    /**
     * Like labelChords, but the algorithm is more complicated. This tries
     * to guess the chords that should go under a beat even when all of the
     * chord members aren't played at once.
     */
    void guessHarmonies(CompositionTimeSliceAdapter &c, Segment &s);

}  // namespace AnalysisHelper

}  // namespace Rosegarden

#endif
