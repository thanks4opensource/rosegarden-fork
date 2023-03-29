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

#include <algorithm>
#include <array>
#include <cmath>    // fabs(), pow()
#include <iterator> // prev()
#include <map>
#include <string>

#ifdef CHORD_NAME_RULER_AND_CONFLICTING_KEY_CHANGES_DEBUG
#include <iostream>
#include <iomanip>
#endif

#include "base/NotationTypes.h"
#include "AnalysisTypes.h"
#include "Event.h"
#include "base/Segment.h"
#include "CompositionTimeSliceAdapter.h"
#include "base/BaseProperties.h"
#include "Composition.h"
#include "misc/ConfigGroups.h"
#include "misc/Preferences.h"
#include "gui/rulers/ChordNameRuler.h"
#include "document/RosegardenDocument.h"   // t4osDEBUG

#include "Sets.h"
#include "Quantizer.h"

#include <assert.h>


namespace Rosegarden
{

namespace {

inline
unsigned
bitCount(
const unsigned bits)
{
#if 0   // Incorrect result on some GCC versions with sizeof(bits) instead of 16
    return std::bitset<16>(bits).count();
#else   // GCC extension, works, may or may not be faster
    return __builtin_popcount(bits);
#endif
}

class ChordType {
  public:
    // Abstract chord name styles, used for chord names in
    // namespace ChordTypes, below. Get replaced by concrete
    // strings depending on user preferences / menu settings,
    // e.g. MIN can be "-", "m", or "min".
    enum ChordClass {   // not enum class, so can implicitly convert to int
        NON,    // none, blank
        MAJ,    // major
        MIN,    // minor
        DIM,    // diminished
        AUG,    // augmented
        MJ7,    // major 7th
        ADD,    // add scale degree note, e.g. "add9"
        MAX_CHORD_CLASSES,  // number of enums not including this one
    };

    ChordType()
    :   m_classes{NON, NON},  // 1 or 2, e.g. "major" or "minor add9"
        m_variant(""),        // textual addition, e.g. "sus2"
        m_name   ("")         // chord root name in various forms, e.g.
                              //   "A", "VI", "ii", etc.
    {}

    ChordType(const ChordClass class1,
              const ChordClass class2,
              const std::string &variant,
              const char *const name = "")
    :   m_classes{class1, class2},
        m_variant(variant),
        m_name   (name)
    {}

    ChordType(const ChordType &other)
    :   m_classes(other.m_classes),
        m_variant(other.m_variant),
        m_name   (other.m_name)
    {}

    static const unsigned  NUM_CLASSES = 2;

    const std::array<ChordClass, NUM_CLASSES> classes() const
    { return m_classes; }

    const std::string &variant() const { return m_variant; }

    const std::string &name() const    { return m_name; }
    void setName(const std::string &s) { m_name = s; }


  protected:
    const std::array<ChordClass, NUM_CLASSES>   m_classes;
    const std::string                           m_variant;
          std::string                           m_name;
};  // class ChordType


namespace ChordTypes
{

// Chord names (abstract names) definitions. Used in per-number-of-notes
// ChordMaps::ChordMaps tables, below.

// 0 notes, needs to be special case constructed to get name because
// name not dynamically set in ChordMaps::updateTypeNames()
ChordType NoChord = ChordType(ChordType::NON,
                              ChordType::NON,
                              "",               // variant
                              "n/c");           // name

#define CT(ONE, TWO, VAR) ChordType(ChordType::ONE, ChordType::TWO, VAR)
ChordType
            // 2 notes ...
            Fifth                   = CT(NON, NON, "5"),
            // 3 notes ...
            Major                   = CT(MAJ, NON, ""),             // ""
            Minor                   = CT(MIN, NON, ""),             // m
            Diminished              = CT(DIM, NON, ""),             // dim
            Augmented               = CT(AUG, NON, ""),             // aug
            Sus2                    = CT(NON, NON, "sus2"),         // sus2
            Sus4                    = CT(NON, NON, "sus4"),         // sus4
            DominantSeventhNo5      = CT(NON, NON, "7no5"),         // 7no5
            DominantSeventhNo3      = CT(NON, NON, "7no3"),         // 7no3
            MinorSeventhNo5         = CT(MIN, NON, "7no5"),         // m7no5
            MajorSeventhNo5         = CT(MJ7, NON, "7no5"),         // maj7no5
            Add2No5                 = CT(MAJ, ADD, "2no5"),         // add2no5
            Add4No5                 = CT(MAJ, ADD, "4no5"),         // add4no5
            MinorAdd9No5            = CT(MIN, ADD, "9no5"),         // madd9no5
            MinorMajorSeventhNo5    = CT(MIN, MJ7, "7no5"),         // mM7no5
            Flat5                   = CT(NON, NON, u8"\u266d5"),    // b5
            VienneseTrichord        = CT(NON, NON,
                                         u8"\u266d2\u266d5no3"),    // viennese
            // 4 notes ...
            DominantSeventh         = CT(NON, NON, "7"),            // 7
            MinorSeventh            = CT(MIN, NON, "7"),            // m7
            MajorSeventh            = CT(MJ7, NON, "7"),            // maj7
            HalfDiminishedSeventh   = CT(MIN, NON, "7b5"),          // m7b5
            DiminishedSeventh       = CT(DIM, NON, "7"),            // dim7
            DiminishedMajorSeventh  = CT(DIM, MJ7, "7"),            // dimM7
            SeventhFlatFive         = CT(NON, NON, "7b5"),          // 7b5
            Sixth                   = CT(NON, NON, "6"),            // 6
            MinorSixth              = CT(MIN, NON, "6"),            // m6
            Add2                    = CT(NON, ADD, "2"),            // add2
            Add4                    = CT(NON, ADD, "4"),            // add4
            SevenSus4               = CT(NON, NON, "7sus4"),        // 7sus4
            MinorAdd9               = CT(MIN, ADD, "9"),            // madd9
            MinorMajorSeventh       = CT(MIN, MJ7, "7"),            // mM7
            AugmentedSeventh        = CT(AUG, NON, "7"),            // +7
            AugmentedMajorSeventh   = CT(AUG, MJ7, "7"),            // +M7
            NinthNo5                = CT(NON, NON, "9no5"),         // 9no5
            MinorNinthNo5           = CT(MIN, NON, "9no5"),         // m9no5
            MajorNinthNo5           = CT(MJ7, NON, "9no5"),         // M9no5
            SeventhFlatNineNo5      = CT(NON, NON, u8"7\u266d9no5"),// 7b9no5
            SeventhSharpNineNo5     = CT(NON, NON, u8"7\u266f9no5"),// 7#9no5
            MajorSeventhSharp11No5  = CT(MJ7, NON, u8"7\u266f11no5"),// M7#11no5
            SixthAddNineNo5         = CT(NON, NON, "6add9no5"),     // 6add9no5
            MinorSixthAddNineNo5    = CT(MIN, NON, "6add9no5"),     // m6add9no5
            EleventhNo5No9          = CT(NON, NON, "11no5,9"),      // 11no5no9
            MajorEleventhNo5No9     = CT(MJ7, NON, "11no5,9"),      // M11no5no9
            // 5 notes ...
            Ninth                   = CT(NON, NON, "9"),            // 9
            MinorNinth              = CT(MIN, NON, "9"),            // m9
            MajorNinth              = CT(MJ7, NON, "9"),            // M9
            SeventhFlatNine         = CT(NON, NON, u8"7\u266d9"),   // 7b9
            SeventhSharpNine        = CT(NON, NON, u8"7\u266f9"),   // 7#9
            SeventhSharpEleven      = CT(NON, NON, u8"7\u266f11"),  // 7#11
            MajorSeventhSharpEleven = CT(MJ7, NON, u8"7\u266f11"),  // M7#11
            SixthAddNine            = CT(NON, NON, "6add9"),        // 6add9
            MinorSixthAddNine       = CT(MIN, NON, "6add9"),        // m6add9
            EleventhNo9             = CT(NON, NON, "11no9"),        // 11no9
            EleventhNo5             = CT(NON, NON, "11no5"),        // 11no5
            MinorEleventhNo5        = CT(MIN, NON, "11no5"),        // m11no5
            MajorEleventhNo5        = CT(MJ7, NON, "11no5"),        // M11no5
            AugmentedEleventhNo5    = CT(NON, NON, u8"\u266f11no5"),// #11no5
            MinorThirteenthNo5No7   = CT(MIN, NON, "13no5,7"),      // m13no5no7
            ThirteenthNo5No9        = CT(NON, NON, "13no5,9"),      // 13no5no9
            // 6 notes ...
            Eleventh                = CT(NON, NON, "11"),           // 11
            MinorEleventh           = CT(MIN, NON, "11"),           // m11
            MajorEleventh           = CT(MJ7, NON, "11"),           // M11
            AugmentedEleventh       = CT(NON, NON, u8"\u266f11"),   // #11
            ThirteenthNo5           = CT(NON, NON, "13no5"),        // 13no5
            MajorThirteenthNo5      = CT(MJ7, NON, "13no5"),        // M13no5
            MinorThirteenthNo7      = CT(MIN, NON, "13no7"),        // m13no7
            MajorThirteenthNo9      = CT(MJ7, NON, "13no9"),        // M13no9
            // 7 notes ...
            Thirteenth              = CT(NON, NON, "13"),           // 13
            MinorThirteenth         = CT(MIN, NON, "13"),           // m13
            MajorThirteenth         = CT(MJ7, NON, "13");
}  // namespace ChordTypes


// Actual concrete chord names, set (and reset) from user preferences
// and ChordTypes, above.

struct ChordData
{
    ChordData()
    :   m_type     (ChordTypes::NoChord),
        m_rootPitch(0),
        m_isValid  (false)
    {}

    ChordData(ChordType& type, int rootPitch, bool isValid = false) :
        m_type(type),
        m_rootPitch(rootPitch),
        m_isValid(isValid) { };

    ChordData(const ChordData &other)
    :   m_type     (other.m_type),
        m_rootPitch(other.m_rootPitch),
        m_isValid  (other.m_isValid)
    {}

    ChordType &m_type;
    unsigned m_rootPitch;
    int m_inversion; // t4os: Not implemented but needed for checkHarmonyTable()
    bool m_isValid;
};  // struct ChordData

class ChordMaps {
  public:
    ChordMaps();

    const ChordData *findChord(const Key&,
                               const unsigned mask,
                               const unsigned bass,
                                     unsigned numPitches);

    static const unsigned NUMBER_OF_CHORD_MAPS  = 6;   // 2 to 7 notes

    using ChordMap = std::multimap<unsigned, ChordData>;

    const ChordMap chordMaps(const unsigned map) const {
        return m_chordMaps[map];
    }

  protected:
    static const unsigned NOTES_MAPS_NDX_OFFSET = 2,   // 2 notes == index 0
                          CHORD_MAPS_MIN_NOTES  = 2,
                          CHORD_MAPS_MAX_NOTES  = 7;

    ChordMap m_chordMaps[NUMBER_OF_CHORD_MAPS];
};
ChordMaps chordMaps;

// Engine for analysize notes into chord names (ChordTypes and ChordMaps,
// above.
// Much of separation between ChordTypes, ChordData, and ChordMaps
// carried over from old unused code/algorithms (see comment in .h file).
class ChordLabel
{
  public:
    static const unsigned NUM_PITCHES_UNSET = 13;

    ChordLabel() : m_data(nullptr) {}
    ChordLabel(const Key &key,
               unsigned  mask,
               unsigned bass,
               unsigned numPitches = NUM_PITCHES_UNSET);
    ChordLabel(ChordType& ctype, unsigned rootPitch, int inversion = 0) :
        m_data(new ChordData(ctype, rootPitch, inversion)) { };

    ChordLabel& operator=(const ChordLabel &other)
    {
        m_data = other.m_data;
        return *this;
    }

    unsigned           rootPitch() const { return m_data->m_rootPitch; }
    const std::string &name()      const { return m_data->m_type.name(); }

    const std::array<ChordType::ChordClass, ChordType::NUM_CLASSES>
    classes()
    const
    {
        return m_data->m_type.classes();
    }

    bool isValid() const { return m_data->m_isValid; }

    // Only used by never-called "harmony" functions, and used
    // removed getName() (for arbitrary sorting of HarmonyTable, etc)
    // If ever needed, should probably save mask and bass into
    // member variables and use those instead.
    bool operator<(const ChordLabel& other) const { return   name()
                                                           < other.name(); }
    bool operator==(const ChordLabel& other) const { return    name()
                                                            == other.name(); }

  private:
    static const unsigned NUMBER_OF_CHORD_MAPS  = 6,   // 2 to 7 notes
                          NOTES_MAPS_NDX_OFFSET = 2,   // 2 notes == index 0
                          CHORD_MAPS_MIN_NOTES  = 2,
                          CHORD_MAPS_MAX_NOTES  = 7;

    const ChordData *m_data;
};  // class ChordLabel


// Various chord root naming styles.
//
namespace ChordLabels {

// Unicode music symbols.
//    u8"\u266d"    flat
//    u8"\u266f"    sharp
//    u8"\u2075"    superscript 5
//    u8"\u00B0"    degree symbol (circle) (for diminished)
//    u8"\u0394"    delta (for major 7th)
//    u8"\u2077"    superscript 7 (for dimished 7th)
//

static const std::string
sharpLetters[] =                  {"C",             //  0   C
                                   u8"C\u266f",     //  1   C#
                                   "D",             //  2   D
                                   u8"D\u266f",     //  3   D#
                                   "E",             //  4   E
                                   "F",             //  5   F
                                   u8"F\u266f",     //  6   F#
                                   "G",             //  7   G
                                   u8"G\u266f",     //  8   G#
                                   "A",             //  9   A
                                   u8"A\u266f",     // 10   A#
                                   "B"},            // 11   B

flatLetters[] =                   {"C",             //  0   C
                                   u8"D\u266d",     //  1   Db
                                   "D",             //  2   D
                                   u8"E\u266d",     //  3   Eb
                                   "E",             //  4   E
                                   "F",             //  5   F
                                   u8"G\u266d",     //  6   Gb
                                   "G",             //  7   G
                                   u8"A\u266d",     //  8   Ab
                                   "A",             //  9   A
                                   u8"B\u266d",     // 10   Bb
                                   "B"},            // 11   B
sharpDegrees[] =                  {"1",             //  0   1
                                   u8"\u266f1",     //  1   #1
                                   "2",             //  2   2
                                   u8"\u266f2",     //  3   #2
                                   "3",             //  4   3
                                   "4",             //  5   4
                                   u8"\u266f4",     //  6   #4
                                   "5",             //  7   5
                                   u8"\u266f5",     //  8   #5
                                   "6",             //  9   6
                                   u8"\u266f6",     // 10   #6
                                   "7"},            // 11   7

flatDegrees[] =                   {"1",             //  0   1
                                   u8"\u266d2",     //  1   b2
                                   "2",             //  2   2
                                   u8"\u266d3",     //  3   b3
                                   "3",             //  4   3
                                   "4",             //  5   4
                                   u8"\u266d5",     //  6   b5
                                   "5",             //  7   5
                                   u8"\u266d6",     //  8   b6
                                   "6",             //  9   6
                                   u8"\u266d7",     // 10   b7
                                   "7"},            // 11   7

integer[] =                       {"0",
                                   "1",
                                   "2",
                                   "3",
                                   "4",
                                   "5",
                                   "6",
                                   "7",
                                   "8",
                                   "9",
                                   "10",
                                   "11"},

offsetInteger[] =                 {"9",
                                   "10",
                                   "11",
                                   "0",
                                   "1",
                                   "2",
                                   "3",
                                   "4",
                                   "5",
                                   "6",
                                   "7",
                                   "8"},

romanUpperMajorSharp[] =          {"I",             //  0   I
                                   u8"\u266fI",     //  1   #I
                                   "II",            //  2   II
                                   u8"\u266fII",    //  3   #II
                                   "III",           //  4   III
                                   "IV",            //  5   IV
                                   u8"\u266fIV",    //  6   #IV
                                   "V",             //  7   V
                                   u8"\u266fV",     //  8   #V
                                   "VI",            //  9   VI
                                   u8"\u266fVI",    // 10   #VI
                                   "VII"},          // 11   VII

romanUpperMajorFlat[] =           {"I",             //  0   I
                                   u8"\u266dII",    //  1   bII
                                   "II",            //  2   II
                                   u8"\u266dIII",   //  3   bIII
                                   "III",           //  4   III
                                   "IV",            //  5   IV
                                   u8"\u266dV",     //  6   bV
                                   "V",             //  7   V
                                   u8"\u266dVI",    //  8   bVI
                                   "VI",            //  9   VI
                                   u8"\u266dVII",   // 10   bVII
                                   "VII"},          // 11   VII

romanLowerMajorSharp[] =          {"i",             //  0   i
                                   u8"\u266fi",     //  1   #i
                                   "ii",            //  2   ii
                                   u8"\u266fii",    //  3   #ii
                                   "iii",           //  4   iii
                                   "iv",            //  5   iv
                                   u8"\u266fiv",    //  6   #iv
                                   "v",             //  7   v
                                   u8"\u266fv",     //  8   #v
                                   "vi",            //  9   vi
                                   u8"\u266fvi",    // 10   #vi
                                   "vii"},          // 11   vii

romanLowerMajorFlat[] =           {"i",             //  0   i
                                   u8"\u266dii",    //  1   bii
                                   "ii",            //  2   ii
                                   u8"\u266diii",   //  3   biii
                                   "iii",           //  4   iii
                                   "iv",            //  5   iv
                                   u8"\u266dv",     //  6   bv
                                   "v",             //  7   v
                                   u8"\u266dvi",    //  8   bvi
                                   "vi",            //  9   vi
                                   u8"\u266dvii",   // 10   bvii
                                   "vii"},          // 11   vii

romanUpperOffsetSharp[] =         {"VI",            //  9   VI
                                   u8"\u266fVI",    // 10   #VI
                                   "VII",           // 11   VII
                                   "I",             //  0   I
                                   u8"\u266fI",     //  1   #I
                                   "II",            //  2   II
                                   u8"\u266fII",    //  3   #II
                                   "III",           //  4   III
                                   "IV",            //  5   IV
                                   u8"\u266fIV",    //  6   #IV
                                   "V",             //  7   V
                                   u8"\u266fV"},        //  8   #V

romanUpperOffsetFlat[] =          {"VI",            //  9   VI
                                   u8"\u266dVII",   // 10   bVII
                                   "VII",           // 11   VII
                                   "I",             //  0   I
                                   u8"\u266dII",    //  1   bII
                                   "II",            //  2   II
                                   u8"\u266dIII",   //  3   bIII
                                   "III",           //  4   III
                                   "IV",            //  5   IV
                                   u8"\u266dV",     //  6   bV
                                   "V",             //  7   V
                                   u8"\u266dVI"},   //  8   bVI

romanLowerOffsetSharp[] =         {"vi",            //  9   vi
                                   u8"\u266fvi",    // 10   #vi
                                   "vii",           // 11   vii
                                   "i",             //  0   i
                                   u8"\u266fi",     //  1   #i
                                   "ii",            //  2   ii
                                   u8"\u266fii",    //  3   #ii
                                   "iii",           //  4   iii
                                   "iv",            //  5   iv
                                   u8"\u266fiv",    //  6   #iv
                                   "v",             //  7   v
                                   u8"\u266fv"},        //  8   #v

romanLowerOffsetFlat[] =          {"vi",            //  9   vi
                                   u8"\u266dvii",   // 10   bvii
                                   "vii",           // 11   vii
                                   "i",             //  0   i
                                   u8"\u266dii",    //  1   bii
                                   "ii",            //  2   ii
                                   u8"\u266diii",   //  3   biii
                                   "iii",           //  4   iii
                                   "iv",            //  5   iv
                                   u8"\u266dv",     //  6   bv
                                   "v",             //  7   v
                                   u8"\u266dvi"},   //  8   bvi

romanUpperMinorAltSharp[] =       {"I",             //  0   I
                                   u8"\u266fI",     //  1   #I
                                   "II",            //  2   II
                                   "III",           //  3   III
                                   u8"\u266fIII",   //  4   #III
                                   "IV",            //  5   IV
                                   u8"\u266fIV",    //  6   #IV
                                   "V",             //  7   V
                                   "VI",            //  8   VI
                                   u8"\u266fVI",    //  9   #VI
                                   "VII",           // 10   VII
                                   u8"\u266fVII"},  // 11   #VII

romanUpperMinorAltFlat[] =        {"I",             //  0   I
                                   u8"\u266dII",    //  1   bII
                                   "II",            //  2   II
                                   "III",           //  3   III
                                   u8"\u266dIV",    //  4   bIV
                                   "IV",            //  5   IV
                                   u8"\u266dV",     //  6   bV
                                   "V",             //  7   V
                                   "VI",            //  8   VI
                                   u8"\u266dVII",   //  9   bVII
                                   "VII",           // 10   VII
                                   u8"\u266dI"},    // 11   bI

romanLowerMinorAltSharp[] =       {"i",             //  0   i
                                   u8"\u266fi",     //  1   #i
                                   "ii",            //  2   ii
                                   u8"\u266fii",    //  3   #ii
                                   "iii",           //  4   iii
                                   "iv",            //  5   iv
                                   u8"\u266fiv",    //  6   #iv
                                   "v",             //  7   v
                                   u8"\u266fv",     //  8   #v
                                   "vi",            //  9   vi
                                   u8"\u266fvi",    // 10   #vi
                                   "vii"},          // 11   vii

romanLowerMinorAltFlat[] =        {"i",             //  0   i
                                   u8"\u266dii",    //  1   bii
                                   "ii",            //  2   ii
                                   u8"\u266diii",   //  3   biii
                                   "iii",           //  4   iii
                                   "iv",            //  5   iv
                                   u8"\u266dv",     //  6   bv
                                   "v",             //  7   v
                                   u8"\u266dvi",    //  8   bvi
                                   "vi",            //  9   vi
                                   u8"\u266dvii",   // 10   bvii
                                   "vii"};          // 11   vii
};  // namespace ChordLabels

}  // namespace (anonymous)


// Private implementation class to keep above static data out of .h file.
class ChordAnalyzerImpl
{
    friend class ChordAnalyzer;

  protected:
    ChordAnalyzerImpl();
    ~ChordAnalyzerImpl() {}

    void updateChordNameStyles();

    // Implementation of ChordAnalyzer::labelChords(). See .h file
    // for description of input and output arguments.
    void labelChords(std::map<const timeT,
                              const std::string>    &chords,
                     std::map<const timeT, int>     &roots,
                     const std::vector<const Segment*>   &segments,
                     const Segment                  *currentSegment,
                     const timeT                     quantizationWindow,
                     const timeT                     arpeggiationWindow,
                     bool                            onePerTimePeriod
                     ,
                     const bool                      wholeComposition,
                     const timeT                     beginTime,
                     const timeT                     endTime);

    // Analyze notes and add resulting chord text name to "chords",
    // and root to "roots".
    void addChord(std::map<const timeT,
                           const std::string>   &chords,
                  std::map<const timeT,
                            int>        &roots,
                  const Key             &key,
                  const unsigned         mask,
                  const unsigned         bass,
                  const unsigned         tenor,
                  const timeT            time);

    // Construct concrete ChordType::m_name from abstract chord definitions
    // in ChordType and ChordMap tables, and explicit chord name style
    // m_labelMode. Done so don't have to generate per-chord at
    // labelChords() time.
    void setChordLabels();

#ifdef CHORD_NAME_RULER_AND_CONFLICTING_KEY_CHANGES_DEBUG
  public:
#endif
    // Notes and key changes in ChordAnalyzer::labelChords() segments
    // combined into one data structure.
    struct NoteOrKey {
        enum OnOffKey {ON, OFF, KEY};

        NoteOrKey(const unsigned char p, const NoteOrKey::OnOffKey ook)
        : count(1), pitch(p), onOffKey(ook) {}

        NoteOrKey(const Key &k)
        :   key(k),
            onOffKey(KEY)
        {}

        Key                 key;

        // Note info
        unsigned            count;  // # of simultaneous same pitch on/off notes
        unsigned short      pitch;
        OnOffKey            onOffKey;
    };
    using NotesAndKeys = std::multimap<timeT, NoteOrKey>;

#ifdef CHORD_NAME_RULER_AND_CONFLICTING_KEY_CHANGES_DEBUG
  protected:  // (because debug-made above public)
#endif
    // Collect user-specified settings into one place so don't
    //   have to go back to external Preferences class to fetch.
    // Also so different chord name rulers can have different settings
    //   at same time despite each writing their own/latest back
    //   to Preferences.
    struct LabelMode {
        LabelMode() : nameType      (  ChordNameRuler
                                     ::ChordNameType
                                     ::PITCH_LETTER),
                      slashType     (ChordNameRuler::ChordSlashType::OFF),
                      romanMinorCase(ChordAnalyzer::ROMAN_MINOR_UPPERCASE),
                      offsetMinor   (false),  // see setChordLabels()
                      altMinor      (false),  //  "        "       ()
                      keyRelative   (false),  // see addChord()
                      addedBass     (false),  //  "     "    ()
                      preferSlash   (false)   //  "     "    ()
        {}

        ChordNameRuler::ChordNameType   nameType;
        ChordNameRuler::ChordSlashType  slashType;
        unsigned                        romanMinorCase;
        bool                            offsetMinor,
                                        altMinor,
                                        keyRelative,
                                        addedBass,
                                        preferSlash;
    };

    struct KeyInfo {
        KeyInfo() : tonic(0), isMinor(false), isSharp(false) {}

        int  tonic;
        bool isMinor,
             isSharp;
    };

    // The two analysis algorithms
    //
    void labelChordsAtNotesOnOff(const NotesAndKeys             &notesAndKeys,
                                 std::map<const timeT,
                                          const std::string>    &chords,
                                 std::map<const timeT, int>     &roots);

    void labelChordsOnePerTimePeriod(const NotesAndKeys           &notesAndKeys,
                                     std::map<const timeT,
                                              const std::string>  &chords,
                                    std::map<const timeT, int>    &roots,
                                    const timeT                   timePeriod);

    ChordMaps            m_chordMaps;
    LabelMode            m_labelMode;
    KeyInfo              m_activeKey;
    bool                 m_romanLowerCase;
    const std::string   *m_chordLabels,
                        *m_romanLowerChordLabels,
                        *m_slashLabels;
};  // class ChordAnalyzerImpl

#if 0  // #ifdef CHORD_NAME_RULER_AND_CONFLICTING_KEY_CHANGES_DEBUG
namespace {

static const char *ON_OFF_KEY_STRINGS[] = {"ON ", "OFF", "KEY"};

std::ostream& operator<<(
std::ostream                                            &stream,
const std::pair<timeT, ChordAnalyzerImpl::NoteOrKey>     noteOrKey)
{
    bool isKey(   noteOrKey.second.onOffKey
               == ChordAnalyzerImpl::NoteOrKey::OnOffKey::KEY);

    stream << std::setw(6)
           << noteOrKey.first
           << "  "
           << std::setw(2)
           << (isKey ? 0 : noteOrKey.second.count)
           << "#  "
           << std::setw(3)
           << (isKey ? 0 : noteOrKey.second.pitch)
           << "   "
           << ON_OFF_KEY_STRINGS[static_cast<unsigned>(
                                 noteOrKey.second.onOffKey)]
           << ' '
           << (isKey ? noteOrKey.second.key.getName() : "");
    return stream;
}
}  // namespace (anonymous)
#endif


ChordAnalyzerImpl::ChordAnalyzerImpl()
:
    m_romanLowerCase(false),
    // The following will always get set correctly by setChordLabels()
    // and/or updateChordNameStyles() before being used.
    m_chordLabels          (ChordLabels::integer),
    m_romanLowerChordLabels(ChordLabels::integer),
    m_slashLabels          (ChordLabels::integer)
{}


// Add chord text string to "chords", and root pitch to "roots"
void
ChordAnalyzerImpl::addChord(
std::map<const timeT,
         const std::string> &chords,
std::map<const timeT, int>  &roots,
const Key                   &key,
const unsigned               mask,    // bits set to chord notes pitch classes
const unsigned               bass,    // lowest note in chord
const unsigned               tenor,   // second lowest note in chord
const timeT                  time)    // time of chord
{
#if 0  // #ifdef CHORD_NAME_RULER_AND_CONFLICTING_KEY_CHANGES_DEBUG
    std::cerr << "ChordAnalyzerImpl::addChord() "
              << key.getName()
              << " 0x"
              << std::hex
              << std::setfill('0')
              << std::setw(3)
              << mask
              << std::dec
              << std::setfill(' ')
              << " "
              << bass
              << " "
              << tenor
              << " "
              << time
              << std::endl;
#endif
    if (!mask) return;  // output nothing, not even "n/c", if no notes

    // Analyze chord from notes
    //
    ChordLabel chordLabel;
    bool       foundAdded = false;

    // If prefer slash chords, first try without bass note.
    // Note updateChordNameStyles() forced preferSlash false if slashType==OFF
    if (m_labelMode.preferSlash) {
        unsigned basslessMask = (mask & ~(1 << (bass % 12)));
        unsigned numPitches = bitCount(mask);

        // Don't do e.g. "C5/E" instead of "C/E" for E-C-G low-to-high.
        // Will catch e.g. "D5/C" for C-D-A below after full chord fails.
        if (numPitches > 3) {
            chordLabel = ChordLabel(key, basslessMask, tenor, numPitches - 1);
            if (chordLabel.isValid())
                foundAdded = true;
        }
    }

    // Normal analysis with full chord (if above slash chord analysis
    // either wasn't done, or failed).
    if (!foundAdded)
        chordLabel = ChordLabel(key, mask, bass);

    // If not a known chord and user requested, try again without bass
    //   note to see if is slash chord with added non-chord bass.
    // Note updateChordNameStyles() forced addedBass false if slashType==OFF
    if (m_labelMode.addedBass && !chordLabel.isValid()) {
        unsigned basslessMask = (mask & ~(1 << (bass % 12)));
        unsigned numPitches   = bitCount(mask);
        chordLabel            = ChordLabel(key,
                                           basslessMask,
                                           tenor,
                                           numPitches - 1);
        if (chordLabel.isValid())
            foundAdded = true;
    }

    if (chordLabel.isValid()) {
        unsigned adjustedPitch = chordLabel.rootPitch();

        // Always absolute (not key-relative)
        roots[time] = chordLabel.rootPitch();

        if (m_labelMode.keyRelative)
            adjustedPitch = (adjustedPitch + 12 - key.getTonicPitch()) % 12;

        std::string name;

        // Combine root string with chord name string.
        // Handle special case of lowercase roman numerals, where
        //   minor chords ("ii", "vi", etc) don't have minor text
        //   string ("m", "-", "min") appended as do all other
        //   naming styles ("A-", "VIm", "9minor", etc)
        if (   m_romanLowerCase
            && (   chordLabel.classes()[0] == ChordType::MIN
                || chordLabel.classes()[0] == ChordType::DIM))
            name = m_romanLowerChordLabels[adjustedPitch] + chordLabel.name();
        else
            name = m_chordLabels[adjustedPitch] + chordLabel.name();

        // Optionally add "slash" chord name (chord tones only, not
        // non-chord altered note)
        if (m_labelMode.slashType != ChordNameRuler::ChordSlashType::OFF) {
            unsigned bassDegree = bass % 12;
            if (bassDegree != chordLabel.rootPitch() || foundAdded) {
                if (   m_labelMode.nameType !=   ChordNameRuler
                                               ::ChordNameType
                                               ::PITCH_LETTER
                    && m_labelMode.nameType !=   ChordNameRuler
                                               ::ChordNameType
                                               ::PITCH_INTEGER)
                {
                    if (m_labelMode.slashType ==   ChordNameRuler
                                                 ::ChordSlashType
                                                 ::CHORD_TONE)
                        bassDegree =   (  bassDegree
                                        + 12
                                        - chordLabel.rootPitch())
                                     % 12;
                    else if (m_labelMode.keyRelative)
                        bassDegree =   (bassDegree + 12 - key.getTonicPitch())
                                     % 12;
                }
                name += "/" + m_slashLabels[bassDegree];
            }
        }

        chords.emplace(time, name);
    }
    else {
        chords.emplace(time, ChordTypes::NoChord.name());
        roots[time] = -1;
    }
}  // ChordAnalyzerImpl::addChord()

namespace {
bool
isSharpKey(const Key &key)
{
    // Use GeneralOptionsConfigGroup, not MatrixViewConfigGroup, so
    // chord name ruler and notes in matrix editor grid can have different
    // settings (and so don't have to do live synchronization between the two).
    if (   key.getTonicPitch() == 0
        && Preferences::getPreference(ChordAnalysisGroup,
                                      "c_major_flats",
                                      true))
        return false;
    else
        return key.isSharp();
}
}  // namespace (anonymous)


// Set concrete chord name strings from abstract definitions in
// ChordTypes and ChordMaps and user preferences / choices.
void ChordAnalyzerImpl::setChordLabels()
{
    // Almost always false except one special case set true.
    m_romanLowerCase = false;

    switch (m_labelMode.nameType) {
        case ChordNameRuler::ChordNameType::PITCH_LETTER:
            if (m_activeKey.isSharp) {
                m_chordLabels = ChordLabels::sharpLetters;
                m_slashLabels = ChordLabels::sharpLetters;
            }
            else {
                m_chordLabels = ChordLabels::flatLetters;
                m_slashLabels = ChordLabels::flatLetters;
            }
        break;

        case ChordNameRuler::ChordNameType::PITCH_INTEGER:
#if 0   // t4os -- Set in ChordAnalyzerImpl::updateChordNameStyles()
        //         and invariant with key change.
            m_chordLabels = ChordLabels::integer;
            m_slashLabels = ChordLabels::integer;
#endif
        break;

        case ChordNameRuler::ChordNameType::KEY_INTEGER:
            if (m_activeKey.isMinor && m_labelMode.offsetMinor)
                m_chordLabels = ChordLabels::offsetInteger;
            else
                m_chordLabels = ChordLabels::integer;
            m_slashLabels = ChordLabels::integer;
        break;

        case ChordNameRuler::ChordNameType::KEY_ROMAN_NUMERAL:
            if (m_activeKey.isMinor)
                if (m_labelMode.offsetMinor)
                    if (m_activeKey.isSharp)
                          m_chordLabels
                        = ChordLabels::romanUpperOffsetSharp;
                    else
                          m_chordLabels
                        = ChordLabels::romanUpperOffsetFlat;
                else if (m_labelMode.altMinor)    // 3rd == III
                    if (m_activeKey.isSharp)
                          m_chordLabels
                        = ChordLabels::romanUpperMinorAltSharp;
                    else
                          m_chordLabels
                        = ChordLabels::romanUpperMinorAltFlat;
                else  // normal 3rd == bIII
                    if (m_activeKey.isSharp)
                          m_chordLabels
                        = ChordLabels::romanUpperMajorSharp;
                    else
                          m_chordLabels
                        = ChordLabels::romanUpperMajorFlat;
            else  // major
                if (m_activeKey.isSharp)
                      m_chordLabels
                    = ChordLabels::romanUpperMajorSharp;
                else
                      m_chordLabels
                    = ChordLabels::romanUpperMajorFlat;

            // Only set if will be used
            if (   m_labelMode.romanMinorCase
                == ChordAnalyzer::ROMAN_MINOR_LOWERCASE) {
                m_romanLowerCase = true;
                if (m_activeKey.isMinor)
                    if (m_labelMode.offsetMinor)
                        if (m_activeKey.isSharp)
                              m_romanLowerChordLabels
                            = ChordLabels::romanLowerOffsetSharp;
                        else
                              m_romanLowerChordLabels
                            = ChordLabels::romanLowerOffsetFlat;
                    else if (m_labelMode.altMinor)    // 3rd == III
                        if (m_activeKey.isSharp)
                              m_romanLowerChordLabels
                            = ChordLabels::romanLowerMinorAltSharp;
                        else
                              m_romanLowerChordLabels
                            = ChordLabels::romanLowerMinorAltFlat;
                    else  // normal 3rd == bIII
                        if (m_activeKey.isSharp)
                              m_romanLowerChordLabels
                            = ChordLabels::romanLowerMajorSharp;
                        else
                              m_romanLowerChordLabels
                            = ChordLabels::romanLowerMajorFlat;
                else
                    if (m_activeKey.isSharp)
                          m_romanLowerChordLabels
                        = ChordLabels::romanLowerMajorSharp;
                    else
                          m_romanLowerChordLabels
                        = ChordLabels::romanLowerMajorFlat;
            }

            if (m_activeKey.isSharp)
                m_slashLabels = ChordLabels::sharpDegrees;
            else
                m_slashLabels = ChordLabels::flatDegrees;
        break;
    }
}  // ChordAnalyzerImpl::setChordLabels()

void
ChordAnalyzerImpl::labelChordsAtNotesOnOff(
const NotesAndKeys          &notesAndKeys,
std::map<const timeT,
         const std::string> &chords,
std::map<const timeT, int>  &roots)
{
    // Bitmask of pitch classes, 0x1==C,0 0x800==B,11.
    unsigned mask = 0;

    // Active pitch classes, count of active notes for each pitch class.
    std::array<int, 12> pitchClasses({0});

    // Count of how many active notes at each pitch.
    std::map<unsigned, unsigned> pitches; // pitch, count

    // Current key
    Key                   key;
    m_activeKey.tonic   = key.getTonicPitch();
    m_activeKey.isMinor = key.isMinor();
    m_activeKey.isSharp = isSharpKey(key);
    setChordLabels();

    // Go through newly created notesAndKeys which are in strict time order
    auto timeAndNoteOrKeyIter = notesAndKeys.begin();
    while (timeAndNoteOrKeyIter != notesAndKeys.end()) {
        const NoteOrKey &noteOrKey(timeAndNoteOrKeyIter->second);
        timeT   time = timeAndNoteOrKeyIter->first;

#if 0  // #ifdef CHORD_NAME_RULER_AND_CONFLICTING_KEY_CHANGES_DEBUG
        std::cerr << "one per: "
                  << key.getName()
                  << " "
                  << time
                  << " "
                  << *timeAndNoteOrKeyIter
                  << " 0x"
                  << std::hex
                  << std::setw(3)
                  << std::setfill('0')
                  << mask
                  << std::dec
                  << std::setfill(' ')
                  << std::endl;
#endif

        if (noteOrKey.onOffKey == NoteOrKey::KEY) {
            key = noteOrKey.key;

            m_activeKey.tonic   = key.getTonicPitch();
            m_activeKey.isMinor = key.isMinor();
            m_activeKey.isSharp = isSharpKey(key);

            setChordLabels();
            ++timeAndNoteOrKeyIter;
            continue;
        }

        unsigned count      = noteOrKey.count,
                 pitch      = noteOrKey.pitch,
                 pitchClass = pitch % 12;

        if (noteOrKey.onOffKey == NoteOrKey::ON) {
            pitchClasses[pitchClass] += count;
            mask |= (1 << pitchClass);

            auto existingPitch = pitches.find(pitch);
            if (existingPitch == pitches.end())
                pitches.emplace(pitch, 1);
            else
                existingPitch->second += count;
        }
        else { // noteOrKey.onOffKey == NoteOrKey::OFF
            if ((pitchClasses[pitchClass] -= count) == 0)
                mask &= ~(1 << pitchClass);

            auto existingPitch = pitches.find(pitch);
            if (existingPitch != pitches.end()) {  // safety, always true
                if (existingPitch->second <= count)
                    pitches.erase(existingPitch);
                else
                    existingPitch->second -= count;
            }
        }

        // Wait until have all notes at this time.
        // Note times were already quantized in labelChords().
        if (   (++timeAndNoteOrKeyIter == notesAndKeys.end() && mask != 0)
            ||    timeAndNoteOrKeyIter->first > time)
        {
            unsigned bass  = 0,
                     tenor = 0;
            auto pitchIter = pitches.begin();
            for ( ; pitchIter != pitches.end() ; ++pitchIter)
                if (pitchIter->second != 0) {
                    bass = pitchIter->first;
                    break;
                }
            if (   m_labelMode.slashType != ChordNameRuler::ChordSlashType::OFF
                && pitchIter != pitches.end()) {
                for (++pitchIter ; pitchIter != pitches.end() ; ++pitchIter)
                    if (pitchIter->second != 0) {
                        tenor = pitchIter->first;
                        break;
                    }
            }

            addChord(chords,
                     roots,
                     key,
                     mask,
                     bass,
                     tenor,
                     time);
        }
    }
}  // labelChordsAtNotesOnOff()

void
ChordAnalyzerImpl::labelChordsOnePerTimePeriod(
const NotesAndKeys              &notesAndKeys,
std::map<const timeT,
         const std::string>     &chords,
std::map<const timeT, int>      &roots,
const timeT                      timePeriod)
{
    // Bitmask of pitch classes, 0x1==C,0 0x800==B,11.
    unsigned mask = 0;

    // Active pitch classes, count of active notes for each pitch class.
    std::array<int, 12> pitchClasses({0});

    // Count of how many active notes at each pitch.
    std::map<unsigned, unsigned> pitches; // pitch, count

    // Notes to turn off at end of each time period
    std::vector<std::pair<unsigned, unsigned>> pendingOffs;  // pitch, count

    // Current key
    Key                   key;
    m_activeKey.tonic   = key.getTonicPitch();
    m_activeKey.isMinor = key.isMinor();
    m_activeKey.isSharp = isSharpKey(key);
    setChordLabels();

    class PendingRemover {
      public:
        PendingRemover(std::map<unsigned, unsigned> &pitches,
                       std::array<int, 12>          &pitchClasses,
                       unsigned                     &mask)
        : m_pitches     (pitches),
          m_pitchClasses(pitchClasses),
          m_mask        (mask)
        {}

        void operator()(const unsigned pitch,
                        const unsigned count)
        {
            const unsigned pitchClass = pitch % 12;

            if ((m_pitchClasses[pitchClass] -= count) == 0)
                m_mask &= ~(1 << pitchClass);

            auto existingPitch = m_pitches.find(pitch);
            if (existingPitch != m_pitches.end()) {  // safety, always true
                if (existingPitch->second <= count)
                    m_pitches.erase(existingPitch);
                else
                    existingPitch->second -= count;
            }
        }

      private:
        std::map<unsigned, unsigned>    &m_pitches;
        std::array<int, 12>             &m_pitchClasses;
        unsigned                        &m_mask;
    } pendingRemover(pitches, pitchClasses, mask);

    // Checks to prevent outputting duplicate/unchanged chords
    bool  notesChanged  = false;
    timeT beginTime     =    notesAndKeys.begin()->first
                          - (notesAndKeys.begin()->first % timePeriod),
            endTime     = beginTime + timePeriod;
    auto  lastNoteOrKey = std::prev(notesAndKeys.end());

#if 0  // #ifdef CHORD_NAME_RULER_AND_CONFLICTING_KEY_CHANGES_DEBUG
    qDebug() << "ChordAnalyzerImpl::labelChordsOnePerTimePeriod()";
#endif

    // Go through newly created notesAndKeys which are in strict time order
    for (auto   timeAndNoteOrKeyIter  = notesAndKeys.begin() ;
                timeAndNoteOrKeyIter != notesAndKeys.end() ;
              ++timeAndNoteOrKeyIter) {

#if 0  // #ifdef CHORD_NAME_RULER_AND_CONFLICTING_KEY_CHANGES_DEBUG
        std::cerr << "  "   // t4osDEBUG
                  << std::setw(5)
                  << beginTime
                  << " -> "
                  << std::setw(5)
                  << endTime
                  << "  "
                  << (notesChanged ? "chng" : "nope")
                  << "  "
                  << std::setbase(16)
                  << "0x"
                  << std::setw(3)
                  << std::setfill('0')
                  << mask
                  << std::setfill(' ')
                  << std::setbase(10)
                  << *timeAndNoteOrKeyIter
                  << std::endl;
#endif

        const NoteOrKey &noteOrKey(timeAndNoteOrKeyIter->second);
        unsigned count       = noteOrKey.count,
                 pitch       = noteOrKey.pitch,
                 pitchClass  = pitch % 12;
        timeT    time        = timeAndNoteOrKeyIter->first;

        if (noteOrKey.onOffKey == NoteOrKey::KEY) {
            key = noteOrKey.key;

            m_activeKey.tonic   = key.getTonicPitch();
            m_activeKey.isMinor = key.isMinor();
            m_activeKey.isSharp = isSharpKey(key);

            setChordLabels();
            continue;
        }

        // Don't have to check for timeAndNoteOrKeyIter == notesAndKeys.end()
        // because will always have note off (actually full set of them)
        // at end.
        if (time >= endTime || timeAndNoteOrKeyIter == lastNoteOrKey) {
            unsigned bass  = 0,
                     tenor = 0;
            // PendingRemover deletes pitches with count of zero, so
            // don't have to check.
            auto pitchIter = pitches.begin();
            if (pitchIter != pitches.end()) {
                bass = pitchIter->first;
                if (      m_labelMode.slashType
                       != ChordNameRuler::ChordSlashType::OFF
                    && ++pitchIter != pitches.end())
                    tenor = pitchIter->first;
            }

            if (notesChanged) {
                addChord(chords,
                         roots,
                         key,
                         mask,
                         bass,
                         tenor,
                         beginTime);
                notesChanged = false;
            }

            for (const auto &pendingOff : pendingOffs)
                pendingRemover(pendingOff.first, pendingOff.second);
            pendingOffs.clear();

            beginTime  = time;
            endTime    = beginTime + timePeriod;
        }
        else
            notesChanged = true;

        if (noteOrKey.onOffKey == NoteOrKey::ON) {
            pitchClasses[pitchClass] += count;
            mask |= (1 << pitchClass);

            auto existingPitch = pitches.find(pitch);
            if (existingPitch == pitches.end())
                pitches.emplace(pitch, count);
            else
                existingPitch->second += count;
            notesChanged = true;
        }
        else {  // noteOrKey.onOffKey == NoteOrKey::OFF
            if (time == beginTime)
                pendingRemover(pitch, count);
            else
#if 0
                pendingOffs.push_back(std::pair<unsigned, unsigned>(pitch,
                                                                    count));
#else
                pendingOffs.emplace_back(std::piecewise_construct,
                                         std::forward_as_tuple(pitch),
                                         std::forward_as_tuple(count));
#endif
        }
    }
}  // labelChordsOnePerTimePeriod()


namespace {

const ChordData*
ChordMaps::findChord(
const Key      &key,
const unsigned  mask,
const unsigned  bass,
      unsigned  numPitches)
{
    if (numPitches == ChordLabel::NUM_PITCHES_UNSET) // Caller didn't calculate
        numPitches = bitCount(mask);

    static ChordData NoChord;
    if (numPitches < CHORD_MAPS_MIN_NOTES || numPitches > CHORD_MAPS_MAX_NOTES)
        return &NoChord;

    const ChordData *chord = &NoChord;

    // Find matching chord types/names (match == same set of pitch classes)
    const unsigned mapIndex = numPitches - NOTES_MAPS_NDX_OFFSET;
    bool             nonDiatonicChords = Preferences::nonDiatonicChords.get();
    const ChordMap  &chordMap(m_chordMaps[mapIndex]);
    for (ChordMap::const_iterator     i  = chordMap.find(mask) ;
                                      i != chordMap.cend() ;
                                    ++i)
    {

        // Loop initializer finds first. Exit loop as soon as past
        //   possible multiple enharmonic variations.
        // Could use std::multimap::equal_range() but this at least
        //   as efficient if not more so.
        if (i->first != mask)
            break;

        if (nonDiatonicChords) {
            // Include non-diatonic chords.
            chord = &i->second;
            if (i->second.m_rootPitch == (bass % 12))
                // Use this chord vs other possible enharmonic matches.
                break;
            else
                // See if another possible enharmonic match has correct
                // bass note, or will semi-randomly use last match here.
                // Skip "isDiatonic" check below.
                continue;
        }

        // Following is much more efficient than doing
        // Pitch(i->second.m_rootPitch).isDiatonicInKey(key)) {
        static const std::set<unsigned> majorDiatonics = {0, 2, 4, 5, 7, 9, 11},
                                        minorDiatonics = {0, 2, 3, 5, 7, 9, 10};
        unsigned degree =   (i->second.m_rootPitch + 12 - key.getTonicPitch())
                          % 12;
        if (   ( key.isMinor() &&    minorDiatonics.find(degree)
                                  != minorDiatonics.end())
            || (!key.isMinor() &&    majorDiatonics.find(degree)
                                  != majorDiatonics.end())) {
            chord = &i->second;
            if (i->second.m_rootPitch == (bass % 12))
                break;     // See comments above.
            else
                continue;  // See comments above.
        }
    }

    return chord;
}  // ChordMaps::findChord()

ChordLabel::ChordLabel(
const Key   &key,
unsigned     mask,
unsigned     bass,
unsigned     numPitches)
:   m_data(nullptr)
{
    m_data = chordMaps.findChord(key, mask, bass, numPitches);

    // Old, pre-rewrite code:
    /*
      int rootBassInterval = ((bass - m_data.m_rootPitch + 12) % 12);

      // Pretend nobody cares about second and third inversions
      // (i.e., bass must always be either root or third of chord)
      if      (rootBassInterval > 7) m_data.m_type=ChordTypes::NoChord;
      else if (rootBassInterval > 4) m_data.m_type=ChordTypes::NoChord;
      // Mark first-inversion and root-position chords as such
      else if (rootBassInterval > 0) m_data.m_inversion=1;
      else                           m_data.m_inversion=0;
    */
}

// ChordTypes separated into number of notes per chord.
template<typename T, size_t N> constexpr size_t arraySize(T(&)[N]) { return N; }
ChordMaps::ChordMaps()
{
#define SCNST static
    SCNST ChordType chordTypes2[]  = {ChordTypes::Fifth};           // 2 notes
    SCNST ChordType chordTypes3[]  = {ChordTypes::Major,            // 3 notes
                                      ChordTypes::Minor,
                                      ChordTypes::Diminished,
                                      ChordTypes::Augmented,
                                      ChordTypes::Sus2,
                                      ChordTypes::Sus4,
                                      ChordTypes::DominantSeventhNo5,
                                      ChordTypes::DominantSeventhNo3,
                                      ChordTypes::MinorSeventhNo5,
                                      ChordTypes::MajorSeventhNo5,
                                      ChordTypes::Add2No5,
                                      ChordTypes::Add4No5,
                                      ChordTypes::MinorAdd9No5,
                                      ChordTypes::MinorMajorSeventhNo5,
                                      ChordTypes::Flat5,
                                      ChordTypes::VienneseTrichord};
    SCNST ChordType chordTypes4[]  = {ChordTypes::DominantSeventh,  // 4 notes
                                      ChordTypes::MinorSeventh,
                                      ChordTypes::MajorSeventh,
                                      ChordTypes::HalfDiminishedSeventh,
                                      ChordTypes::DiminishedSeventh,
                                      ChordTypes::DiminishedMajorSeventh,
                                      ChordTypes::SeventhFlatFive,
                                      ChordTypes::Sixth,
                                      ChordTypes::MinorSixth,
                                      ChordTypes::Add2,
                                      ChordTypes::Add4,
                                      ChordTypes::SevenSus4,
                                      ChordTypes::MinorAdd9,
                                      ChordTypes::MinorMajorSeventh,
                                      ChordTypes::AugmentedSeventh,
                                      ChordTypes::AugmentedMajorSeventh,
                                      ChordTypes::NinthNo5,
                                      ChordTypes::MinorNinthNo5,
                                      ChordTypes::MajorNinthNo5,
                                      ChordTypes::SeventhFlatNineNo5,
                                      ChordTypes::SeventhSharpNineNo5,
                                      ChordTypes::MajorSeventhSharp11No5,
                                      ChordTypes::SixthAddNineNo5,
                                      ChordTypes::MinorSixthAddNineNo5,
                                      ChordTypes::EleventhNo5No9,
                                      ChordTypes::MajorEleventhNo5No9};
    SCNST ChordType chordTypes5[]  = {ChordTypes::Ninth,            // 5 notes
                                      ChordTypes::MinorNinth,
                                      ChordTypes::MajorNinth,
                                      ChordTypes::SeventhFlatNine,
                                      ChordTypes::SeventhSharpNine,
                                      ChordTypes::SeventhSharpEleven,
                                      ChordTypes::MajorSeventhSharpEleven,
                                      ChordTypes::SixthAddNine,
                                      ChordTypes::MinorSixthAddNine,
                                      ChordTypes::EleventhNo9,
                                      ChordTypes::EleventhNo5,
                                      ChordTypes::MinorEleventhNo5,
                                      ChordTypes::MajorEleventhNo5,
                                      ChordTypes::AugmentedEleventhNo5,
                                      ChordTypes::MinorThirteenthNo5No7,
                                      ChordTypes::ThirteenthNo5No9,
                                     };
    SCNST ChordType chordTypes6[]  = {ChordTypes::Eleventh,         // 6 notes,
                                      ChordTypes::MinorEleventh,
                                      ChordTypes::MajorEleventh,
                                      ChordTypes::AugmentedEleventh,
                                      ChordTypes::ThirteenthNo5,
                                      ChordTypes::MajorThirteenthNo5,
                                      ChordTypes::MinorThirteenthNo7,
                                      ChordTypes::MajorThirteenthNo9,
                                     };
    SCNST ChordType chordTypes7[]  = {ChordTypes::Thirteenth,       // 7 notes
                                      ChordTypes::MinorThirteenth,
                                      ChordTypes::MajorThirteenth};

    // Two-dimensional array with different length second dimension elements.
    SCNST ChordType *basicChordTypes[NUMBER_OF_CHORD_MAPS] = {
        chordTypes2,
        chordTypes3,
        chordTypes4,
        chordTypes5,
        chordTypes6,
        chordTypes7,
    };

    // What the basicChordMasks mean: each bit set in each one represents
    // a pitch class (pitch%12) in a chord. C major has three pitch
    // classes, C, E, and G natural; if you take the MIDI pitches
    // of those notes modulo 12, you get 0, 4, and 7, so the mask for
    // major triads is (1<<0)+(1<<4)+(1<<7). All the masks are for chords
    // built on C.

    // Above is old, pre-rewrite comment. In other words, "pitch class",
    // aka "integer notation"
    // https://en.wikipedia.org/wiki/Pitch_class#Integer_notation
    // as now an option in user interface.

#define SCEXP static constexpr
    SCEXP unsigned chordMasks2[] = {        // 2 notes ...
#define CHORD2(P2) (1 | (1<<(P2)))
        CHORD2(7)                           // Fifth
    };
    SCEXP unsigned chordMasks3[] = {        // 3 notes
#define CHORD3(P2,P3) (CHORD2(P2) | (1<<(P3)))
        CHORD3(4, 7),                       // Major
        CHORD3(3, 7),                       // Minor
        CHORD3(3, 6),                       // Diminished
        CHORD3(4, 8),                       // Augmented
        CHORD3(2, 7),                       // Sus2
        CHORD3(5, 7),                       // Sus4
        CHORD3(4, 10),                      // 7no5
        CHORD3(7, 10),                      // 7no3
        CHORD3(3, 10),                      // m7no5
        CHORD3(4, 11),                      // maj7no5
        CHORD3(2, 4),                       // add2no5
        CHORD3(4, 5),                       // add4no5
        CHORD3(2, 3),                       // madd9no5
        CHORD3(3, 11),                      // mM7no5
        CHORD3(4, 6),                       // b5
        CHORD3(1, 6),                       // viennese
    };
    SCEXP unsigned chordMasks4[] = {        // 4 notes ...
#define CHORD4(P2,P3,P4) (CHORD3(P2,P3) | (1<<(P4)))
        CHORD4(4, 7, 10),                   // DominantSeventh
        CHORD4(3, 7, 10),                   // MinorSeventh
        CHORD4(4, 7, 11),                   // MajorSeventh
        CHORD4(3, 6, 10),                   // HalfDiminishedSeventh
        CHORD4(3, 6,  9)                ,   // DiminishedSeventh
        CHORD4(3, 6, 11),                   // DiminishedMajorSeventh
        CHORD4(4, 6, 10),                   // SeventhFlatFive
        CHORD4(4, 7,  9),                   // Sixth
        CHORD4(3, 7,  9),                   // MinorSixth
        CHORD4(2, 4,  7),                   // Add2
        CHORD4(4, 5,  7),                   // Add4
        CHORD4(5, 7, 10),                   // SevenSus4
        CHORD4(2, 3,  7),                   // MinorAdd9
        CHORD4(3, 7, 11),                   // MinorMajorSeventh
        CHORD4(4, 8, 10),                   // AugmentedSeventh
        CHORD4(4, 8, 11),                   // AugmentedMajorSeventh
        CHORD4(2, 4, 10),                   // 9no5
        CHORD4(2, 3, 10),                   // m9no5
        CHORD4(2, 4, 11),                   // M9no5
        CHORD4(1, 4, 10),                   // 7b9no5
        CHORD4(3, 4, 10),                   // 7#9no5
        CHORD4(4, 6, 11),                   // M7#11no5
        CHORD4(2, 4,  9),                   // 6add9no5
        CHORD4(2, 3,  9),                   // m6add9no5
        CHORD4(4, 5, 10),                   // 11no5no9
        CHORD4(4, 5, 11),                   // M11no5no9
    };
    SCEXP unsigned chordMasks5[] = {        // 5 notes ...
#define CHORD5(P2,P3,P4,P5) (CHORD4(P2,P3,P4) | (1<<(P5)))
        CHORD5(4, 7, 10,  2),               // Ninth
        CHORD5(3, 7, 10,  2),               // MinorNinth
        CHORD5(4, 7, 11,  2),               // MajorNinth
        CHORD5(4, 7, 10,  1),               // SeventhFlatNine
        CHORD5(4, 7, 10,  3),               // SeventhSharpNine
        CHORD5(4, 7, 10,  6),               // SeventhSharpEleven
        CHORD5(4, 7, 11,  6),               // MajorSeventhSharpEleven
        CHORD5(4, 7,  9,  2),               // SixthAddNine
        CHORD5(3, 7,  9,  2),               // MinorSixthAddNine
        CHORD5(4, 5,  7, 10),               // 11no9
        CHORD5(2, 4,  5, 10),               // 11no5
        CHORD5(2, 3,  5, 10),               // m11no5
        CHORD5(2, 4,  5, 11),               // M11no5
        CHORD5(2, 4,  6, 10),               // +11no5
        CHORD5(2, 3,  5,  9),               // m13no5no7
        CHORD5(4, 5,  9, 10),               // 13no5no9
    };
    SCEXP unsigned chordMasks6[] = {        // 6 notes ...
#define CHORD6(P2,P3,P4,P5,P6) (CHORD5(P2,P3,P4,P5) | (1<<(P6)))
        CHORD6(4, 7, 10, 2,  5),            // Eleventh
        CHORD6(3, 7, 10, 2,  5),            // MinorEleventh
        CHORD6(4, 7, 11, 2,  5),            // MajorEleventh
        CHORD6(4, 7, 10, 2,  6),            // AugmentedEleventh
        CHORD6(2, 4,  5, 9, 10),            // 13no5
        CHORD6(2, 4,  5, 9, 11),            // M13no5
        CHORD6(2, 3,  5, 7,  9),            // m13no7
        CHORD6(4, 5,  7, 9, 11),            // M13no9
    };
    SCEXP unsigned chordMasks7[] = {        // 7 notes ...
#define CHORD7(P2,P3,P4,P5,P6,P7) (CHORD6(P2,P3,P4,P5,P6) | (1<<(P7)))
        CHORD7(4, 7, 10, 2, 5, 9),          // Thirteenth
        CHORD7(3, 7, 10, 2, 5, 9),          // MinorThirteenth
        CHORD7(4, 7, 11, 2, 5, 9),          // MajorThirteenth
    };
#undef CHORD2
#undef CHORD3
#undef CHORD4
#undef CHORD5
#undef CHORD6
#undef CHORD7

    SCEXP const unsigned *basicChordMasks[NUMBER_OF_CHORD_MAPS] = {
        chordMasks2,
        chordMasks3,
        chordMasks4,
        chordMasks5,
        chordMasks6,
        chordMasks7,
    };

    SCEXP unsigned basicChordCounts[NUMBER_OF_CHORD_MAPS] = {
        arraySize(chordMasks2 ),
        arraySize(chordMasks3 ),
        arraySize(chordMasks4 ),
        arraySize(chordMasks5 ),
        arraySize(chordMasks6 ),
        arraySize(chordMasks7 ),
    };

#define CHORD_TABLE_ASSERT(N)                                               \
    static_assert(arraySize(chordTypes##N) == basicChordCounts[N - 2])
    CHORD_TABLE_ASSERT( 2);
    CHORD_TABLE_ASSERT( 3);
    CHORD_TABLE_ASSERT( 4);
    CHORD_TABLE_ASSERT( 5);
    CHORD_TABLE_ASSERT( 6);
    CHORD_TABLE_ASSERT( 7);
#undef CHORD_TABLE_ASSERT

#undef SCEXP
#undef SCNST

    // Each mask is inserted into the map rotated twelve ways; each
    // rotation is a mask you would get by transposing the chord
    // to have a new root (i.e., C, C#, D, D#, E, F...)
    for (unsigned map = 0 ; map < NUMBER_OF_CHORD_MAPS ; ++map) {
        for (unsigned chord = 0 ; chord < basicChordCounts[map] ; ++chord)
            for (int root = 0; root < 12; ++root) {
                unsigned mask =    (     (basicChordMasks[map][chord] << root)
                                    & ((1 << 12) - 1))
                                |  (     basicChordMasks[map][chord]
                                      >> (12 - root));
                m_chordMaps[map].emplace(std::piecewise_construct,
                                         std::forward_as_tuple(mask),
                                         std::forward_as_tuple(basicChordTypes
                                                               [map]
                                                               [chord],
                                                               root,
                                                               true));
            }
    }

#if 0   // Don't do this at static instance constructor because
        // Preferences not set up yet, so will get defaults,
        // and, worse, cache same so will not get looked up
        // after that.
        // ChordNameRuler ctor will do once, first time used.
    updateTypeNames();
#endif
}  // ChordMaps::ChordMaps()

}  // namespace (anonymous)

void
ChordAnalyzerImpl::updateChordNameStyles()
{
    static const std::vector<const char*>
    typeNames[ChordType::MAX_CHORD_CLASSES] = {
        {""},
        {"",                  // MAJOR_BLANK            = 0
         "M",                 // MAJOR_M                = 1
         "maj",               // MAJOR_MAJ              = 2
        },
        {"m",                 // MINOR_M                = 0,
         "-",                 // MINOR_MINUS            = 1,
         "min",               // MINOR_MIN              = 2,
         "mi",                // MINOR_MI               = 3,
         "",                  // MINOR_ROMAN_BLANK      = 4, // internal use only
        },
        {"dim",               // DIMINISHED_DIM         = 0,
         u8"\u00B0",          // DIMINISHED_CIRCLE      = 1,
        },
        {"aug",               // AUGMENTED_AUG          = 0,
         "+",                 // AUGMENTED_PLUS         = 1,
        },
        {"M",                 // MAJOR_7TH_M            = 0,
         "maj",               // MAJOR_7TH_MAJ          = 1,
         u8"\u0394",          // MAJOR_7TH_DELTA        = 2,
        },
        {"add",               // ADD_ADD                = 0,
         "+",                 // ADD_PLUS               = 1,
        },
    };

#define CLS(PREF, STYLE)                                \
    static_cast<ChordType::ChordClass>(                 \
    Preferences::getPreference(ChordAnalysisGroup,      \
                               PREF,                    \
                               ChordAnalyzer::STYLE))
    unsigned classes[] = {
        0,                             // NON
        CLS("major_chord_style",       MAJOR_BLANK),
        CLS("minor_chord_style",       MINOR_M),
        CLS("diminished_chord_style",  DIMINISHED_DIM),
        CLS("augmented_chord_style",   AUGMENTED_AUG),
        CLS("major_7th_chord_style",   MAJOR_7TH_M),
        CLS("add_chord_style",         ADD_ADD),
     // CLS("minor_roman_chord_style", ROMAN_MINOR_LOWERCASE),
    };
#undef CLS

    // Check to see if doing roman numerals, and if so and are
    // set to lowercase minor chords override minor prefix with
    // blank so e.g. "ii" instead of "iim","ii-", etc.
    ChordNameRuler::ChordNameType
    chordNameType = static_cast<ChordNameRuler::ChordNameType>(
                    Preferences::getPreference(ChordAnalysisGroup,
                                               "chord_name_type",
                                               static_cast<unsigned>(
                                                 ChordNameRuler
                                               ::ChordNameType
                                               ::PITCH_LETTER)));

    unsigned romanMinorCase = ChordAnalyzer::ROMAN_MINOR_UPPERCASE;
    if (   chordNameType == ChordNameRuler::ChordNameType::KEY_ROMAN_NUMERAL) {
        static const unsigned MINOR_STYLE_NDX = 2;
        romanMinorCase = Preferences::getPreference(ChordAnalysisGroup,
                                                    "minor_roman_chord_style",
                                                      ChordAnalyzer
                                                    ::ROMAN_MINOR_UPPERCASE);
        if (romanMinorCase == ChordAnalyzer::ROMAN_MINOR_LOWERCASE)
            classes[MINOR_STYLE_NDX] = ChordAnalyzer::MINOR_ROMAN_BLANK;
    }

    for (unsigned map = 0 ; map < ChordMaps::NUMBER_OF_CHORD_MAPS ; ++map)
        for (const auto &maskChord : m_chordMaps.chordMaps(map)) {
            ChordType &chord(maskChord.second.m_type);
            unsigned class0 = chord.classes()[0],
                     class1 = chord.classes()[1];

            chord.setName(  std::string(typeNames[class0][classes[class0]])
                          + std::string(typeNames[class1][classes[class1]])
                          + chord.variant());
        }

    // Cache so don't have to look up each time in labelChords()
    m_labelMode.nameType       = chordNameType;
    m_labelMode.romanMinorCase = romanMinorCase;
    m_labelMode.altMinor       = Preferences::getPreference(ChordAnalysisGroup,
                                                            "alt_minors",
                                                            false);
    m_labelMode.offsetMinor    = Preferences::getPreference(ChordAnalysisGroup,
                                                            "offset_minors",
                                                            false);

    switch (chordNameType) {
        case ChordNameRuler::ChordNameType::PITCH_LETTER:
            m_labelMode.keyRelative = false;
        break;

        case ChordNameRuler::ChordNameType::PITCH_INTEGER:
            m_labelMode.keyRelative = false;
            // Invariant during labeChords() regardless of key changes
            m_chordLabels = ChordLabels::integer;
            m_slashLabels = ChordLabels::integer;
        break;

        case ChordNameRuler::ChordNameType::KEY_ROMAN_NUMERAL:
        case ChordNameRuler::ChordNameType::KEY_INTEGER:
            m_labelMode.keyRelative = true;
        break;
    }

      m_labelMode.slashType
    = static_cast<ChordNameRuler::ChordSlashType>(
                  Preferences::getPreference(ChordAnalysisGroup,
                                             "chord_slash_type",
                                             static_cast<unsigned>(
                                               ChordNameRuler
                                             ::ChordSlashType
                                             ::OFF)));

    m_labelMode.addedBass   = Preferences::getPreference(ChordAnalysisGroup,
                                                         "added_bass",
                                                         false);
    m_labelMode.preferSlash = Preferences::getPreference(ChordAnalysisGroup,
                                                         "prefer_slash_chords",
                                                         false);

    // So don't have to check both slashType and one of the other two.
    if (m_labelMode.slashType == ChordNameRuler::ChordSlashType::OFF) {
        m_labelMode.addedBass   = false;
        m_labelMode.preferSlash = false;
    }
}  // ChordAnalyzerImpl::updateChordNameStyles()


// Old, pre-rewrite code. See comments in .h file
//
namespace {

///////////////////////////////////////////////////////////////////////////
// Harmony guessing
///////////////////////////////////////////////////////////////////////////

// ### THESE NAMES ARE AWFUL. MUST GREP THEM OUT OF EXISTENCE.
typedef std::pair<double, ChordLabel> ChordPossibility;
typedef std::vector<ChordPossibility> HarmonyGuess;
typedef std::vector<std::pair<timeT, HarmonyGuess> > HarmonyGuessList;
struct cp_less : public std::binary_function<ChordPossibility,
                                             ChordPossibility,
                                             bool>
{
    bool operator()(ChordPossibility l, ChordPossibility r);
};

/// For use by guessHarmonies
void makeHarmonyGuessList(CompositionTimeSliceAdapter &c,
                          HarmonyGuessList &l);

/// For use by guessHarmonies
void refineHarmonyGuessList(CompositionTimeSliceAdapter &c,
                            HarmonyGuessList& l,
                            Segment &);

/// For use by guessHarmonies (makeHarmonyGuessList)
class PitchProfile
{
public:
    PitchProfile();
    double& operator[](int i);
    const double& operator[](int i) const;
    double distance(const PitchProfile &other);
    double dotProduct(const PitchProfile &other) const;
    double productScorer(const PitchProfile &other) const;
    PitchProfile normalized();
    PitchProfile& operator*=(double d);
    PitchProfile& operator+=(const PitchProfile &d);
private:
    double m_data[12];
};

/// For use by guessHarmonies (makeHarmonyGuessList)
typedef std::vector<std::pair<PitchProfile, ChordLabel> > HarmonyTable;
HarmonyTable m_harmonyTable;

/// For use by guessHarmonies (makeHarmonyGuessList)
void checkHarmonyTable();

/// For use by guessHarmonies (refineHarmonyGuessList)
// #### grep ProgressionMap to something else
struct ChordProgression {
    ChordProgression(const ChordLabel& first_,
                     const ChordLabel& second_ = ChordLabel(),
                     const Key& key_ = Key());
    ChordLabel first;
    ChordLabel second;
    Key homeKey;
    // double commonness...
    bool operator<(const ChordProgression& other) const;
    };
typedef std::set<ChordProgression> ProgressionMap;
       ProgressionMap m_progressionMap;

/// For use by guessHarmonies (refineHarmonyGuessList)
void checkProgressionMap();

/// For use by checkProgressionMap
void addProgressionToMap(Key k,
                         int firstChordNumber,
                         int secondChordNumber);


// #### explain how this works:
// in terms of other functions (simple chord labelling, key guessing)
// in terms of basic concepts (pitch profile, harmony guess)
// in terms of flow

void
makeHarmonyGuessList(CompositionTimeSliceAdapter &c, HarmonyGuessList &l)
{
    if (*c.begin() == *c.end()) return;

    checkHarmonyTable();

    PitchProfile p; // defaults to all zeroes
    TimeSignature timeSig;
    timeT timeSigTime = 0;
    timeT nextSigTime = (*c.begin())->getAbsoluteTime();

    // Walk through the piece labelChords style

    // no increment (the first inner loop does the incrementing)
    for (CompositionTimeSliceAdapter::iterator i = c.begin(); i != c.end();  )
    {

        // 2. Update the pitch profile

        timeT time = (*i)->getAbsoluteTime();

        if (time >= nextSigTime) {
            Composition *comp = c.getComposition();
            int sigNo = comp->getTimeSignatureNumberAt(time);
            if (sigNo >= 0) {
                std::pair<timeT, TimeSignature> sig = comp->getTimeSignatureChange(sigNo);
                timeSigTime = sig.first;
                timeSig = sig.second;
            }
            if (sigNo < comp->getTimeSignatureCount() - 1) {
                nextSigTime = comp->getTimeSignatureChange(sigNo + 1).first;
            } else {
                nextSigTime = comp->getEndMarker();
            }
        }

        double emphasis =
            double(timeSig.getEmphasisForTime(time - timeSigTime));

        // Scale all the components of the pitch profile down so that
        // 1. notes that are a beat/bar away have less weight than notes
        //    from this beat/bar
        // 2. the difference in weight depends on the metrical importance
        //    of the boundary between the notes: the previous beat should
        //    get less weight if this is the first beat of a new bar

        // ### possibly too much fade here
        //     also, fade should happen w/reference to how many notes played?

        PitchProfile delta;
        int noteCount = 0;

        // no initialization
        for (  ; i != c.end() && (*i)->getAbsoluteTime() == time; ++i)
        {
            if ((*i)->isa(Note::EventType))
            {
                try {
                    int pitch = (*i)->get<Int>(BaseProperties::PITCH);
                    delta[pitch % 12] += 1 << int(emphasis);
                    ++noteCount;
                } catch (...) {
                    std::cerr << "No pitch for note at " << time << "!" << std::endl;
                }
            }
        }

        p *= 1. / (pow(2, emphasis) + 1 + noteCount);
        p += delta;

        // 1. If there could have been a chord change, compare the current
        //    pitch profile with all of the profiles in the table to figure
        //    out which chords we are now nearest.

        // (If these events weren't on a beat boundary, assume there was no
        // chord change and continue -- ### will need this back)
/*        if ((!(i != c.end())) ||
           timeSig.getEmphasisForTime((*i)->getAbsoluteTime() - timeSigTime) < 3)
        {
            continue;
        }*/

        // (If the pitch profile hasn't changed much, continue)

        PitchProfile np = p.normalized();

        HarmonyGuess possibleChords;

        possibleChords.reserve(m_harmonyTable.size());

        for (HarmonyTable::iterator j = m_harmonyTable.begin();
             j != m_harmonyTable.end();
             ++j)
        {
            double score = np.productScorer(j->first);
            possibleChords.push_back(ChordPossibility(score, j->second));
        }

        // 3. Save a short list of the nearest chords in the
        // HarmonyGuessList passed in from guessHarmonies()

        l.push_back(std::pair<timeT, HarmonyGuess>(time, HarmonyGuess()));

        HarmonyGuess& smallerGuess = l.back().second;

        // Have to learn to love this:

        smallerGuess.resize(10);

        partial_sort_copy(possibleChords.begin(),
                          possibleChords.end(),
                          smallerGuess.begin(),
                          smallerGuess.end(),
                          cp_less());

#ifdef  GIVE_HARMONYGUESS_DETAILS
        std::cerr << "Time: " << time << std::endl;

        std::cerr << "Profile: ";
        for (int k = 0; k < 12; ++k)
               std::cerr << np[k] << " ";
        std::cerr << std::endl;

        std::cerr << "Best guesses: " << std::endl;
        for (HarmonyGuess::iterator debugi = smallerGuess.begin();
             debugi != smallerGuess.end();
             ++debugi)
        {
            std::cerr << debugi->first << ": " << debugi->second.getName(Key()) << std::endl;
        }
#endif
    }
}


// Comparison function object -- can't declare this in the headers because
// this only works with pair<double, ChordLabel> instantiated,
// pair<double, ChordLabel> can't be instantiated while ChordLabel is an
// incomplete type, and ChordLabel is still an incomplete type at that
// point in the headers.
bool
cp_less::operator()(ChordPossibility l, ChordPossibility r)
{
    // Change name from "less?"
    return l.first > r.first;
}

void
refineHarmonyGuessList(CompositionTimeSliceAdapter &/* c */,
                                       HarmonyGuessList &l, Segment &segment)
{
    // (Fetch the piece's starting key from the key guesser)
    Key key;

    checkProgressionMap();

    if (l.size() < 2)
    {
        l.clear();
        return;
    }

    // Look at the list of harmony guesses two guesses at a time.

    HarmonyGuessList::iterator i = l.begin();
    // j stays ahead of i
    HarmonyGuessList::iterator j = i + 1;

    ChordLabel bestGuessForFirstChord, bestGuessForSecondChord;
    while (j != l.end())
    {

        double highestScore = 0;

        // For each possible pair of chords (i.e., two for loops here)
        for (HarmonyGuess::iterator k = i->second.begin();
             k != i->second.end();
             ++k)
        {
            for (HarmonyGuess::iterator l = j->second.begin();
                 l != j->second.end();
                 ++l)
            {
                // Print the guess being processed:

                //    std::cerr << k->second.getName(Key()) << "->" << l->second.getName(Key()) << std::endl;

                // For a first approximation, let's say the probability that
                // a chord guess is correct is proportional to its score. Then
                // the probability that a pair is correct is the product of
                // its scores.

                double currentScore;
                currentScore = k->first * l->first;

                //    std::cerr << currentScore << std::endl;

                // Is this a familiar progression? Bonus if so.

                bool isFamiliar = false;

                // #### my ways of breaking up long function calls are haphazard
                //      also, does this code belong here?

                ProgressionMap::iterator pmi =
                    m_progressionMap.lower_bound(
                        ChordProgression(k->second, l->second)
                    );

                // no initialization
                for ( ;
                     pmi != m_progressionMap.end()
                     && pmi->first == k->second
                     && pmi->second == l->second;
                     ++pmi)
                {
                    // key doesn't have operator== defined
                    if (key.getName() == pmi->homeKey.getName())
                    {
//                        std::cerr << k->second.getName(Key()) << "->" << l->second.getName(Key()) << " is familiar" << std::endl;
                        isFamiliar = true;
                        break;
                    }
                }

                if (isFamiliar) currentScore *= 5; // #### arbitrary

                // (Are voice-leading rules followed? Penalty if not)

                // Is this better than any pair examined so far? If so, set
                // some variables that should end up holding the best chord
                // progression
                if (currentScore > highestScore)
                {
                    bestGuessForFirstChord  = k->second;
                    bestGuessForSecondChord = l->second;
                    highestScore = currentScore;
                }

            }
        }

        // Since we're not returning any results right now, print them
        std::cerr << "Time: " << j->first << std::endl;
        std::cerr << "Best chords: "
          << bestGuessForFirstChord.name() << ", "
          << bestGuessForSecondChord.name() << std::endl;
        std::cerr << "Best score: " << highestScore << std::endl;

        // Using the best pair of chords:

        // Is the first chord diatonic?

            // If not, is it a secondary function?
                // If so, change the current key
                // If not, set an "implausible progression" flag

        // (Is the score of the best pair of chords reasonable?
        // If not, set the flag.)

        // Was the progression plausible?

            // If so, replace the ten or so chords in the first guess with the
            // first chord of the best pair, set
            // first-iterator=second-iterator, and ++second-iterator
            // (and possibly do the real key-setting)

            // If not, h.erase(second-iterator++)

        // Temporary hack to get _something_ interesting out:
        Event *e;
        e = Text(bestGuessForFirstChord.name(), Text::ChordName).
            getAsEvent(j->first);
        segment.insert(new Event(*e, e->getAbsoluteTime(),
                                 e->getDuration(), e->getSubOrdering()-1));
        delete e;

        e = Text(bestGuessForSecondChord.name(), Text::ChordName).
            getAsEvent(j->first);
        segment.insert(e);

        // For now, just advance:
        i = j;
        ++j;
    }
}

void
checkHarmonyTable()
{
    if (!m_harmonyTable.empty()) return;

    // Identical to basicChordTypes in ChordLabel::checkMap
    ChordType basicChordTypes[8] =
        {ChordTypes::Major, ChordTypes::Minor, ChordTypes::Diminished,
         ChordTypes::MajorSeventh, ChordTypes::DominantSeventh,
         ChordTypes::MinorSeventh, ChordTypes::HalfDiminishedSeventh,
         ChordTypes::DiminishedSeventh};

    // Like basicChordMasks in ChordLabel::checkMap(), only with
    // ints instead of bits
    const int basicChordProfiles[8][12] =
    {
    //   0  1  2  3  4  5  6  7  8  9 10 11
        {1, 0, 0, 0, 1, 0, 0, 1, 0, 0, 0, 0},   // major
        {1, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 0},   // minor
        {1, 0, 0, 1, 0, 0, 1, 0, 0, 0, 0, 0},   // diminished
        {1, 0, 0, 0, 1, 0, 0, 1, 0, 0, 0, 1},   // major 7th
        {1, 0, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0},   // dominant 7th
        {1, 0, 0, 1, 0, 0, 0, 1, 0, 0, 1, 0},   // minor 7th
        {1, 0, 0, 1, 0, 0, 1, 0, 0, 0, 1, 0},   // half-diminished 7th
        {1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0}    // diminished 7th
    };

    for (int i = 0; i < 8; ++i)
    {
        for (int j = 0; j < 12; ++j)
        {
            PitchProfile p;

            for (int k = 0; k < 12; ++k)
                p[(j + k) % 12] = (basicChordProfiles[i][k] == 1)
                                  ? 1.
                                  : -1.;

            PitchProfile np = p.normalized();

            ChordLabel c(basicChordTypes[i], j);

            m_harmonyTable.push_back(std::pair<PitchProfile, ChordLabel>(np, c));
        }
    }

}

void
checkProgressionMap()
{
    if (!m_progressionMap.empty()) return;
    // majorProgressionFirsts[0] = 5, majorProgressionSeconds[0]=1, so 5->1 is
    // a valid progression. Note that the chord numbers are 1-based, like the
    // Roman numeral symbols
    const int majorProgressionFirsts[9] =
        {5, 2, 6, 3, 7, 4, 4, 3, 5};
    const int majorProgressionSeconds[9] =
        {1, 5, 2, 6, 1, 2, 5, 4, 6};

    // For each major key
    for (int i = 0; i < 12; ++i)
    {
        // Make the key
        Key k(0, false); // tonicPitch = i, isMinor = false
        // Add the common progressions
        for (int j = 0; j < 9; ++j)
        {
            std::cerr << majorProgressionFirsts[j] << ", " << majorProgressionSeconds[j] << std::endl;
            addProgressionToMap(k,
                                majorProgressionFirsts[j],
                                majorProgressionSeconds[j]);
        }
        // Add I->everything
        for (int j = 1; j < 8; ++j)
        {
            addProgressionToMap(k, 1, j);
        }
        // (Add the progressions involving seventh chords)
        // (Add I->seventh chords)
    }
    // (For each minor key)
        // (Do what we just did for major keys)

}

void
addProgressionToMap(Key k,
                                    int firstChordNumber,
                                    int secondChordNumber)
{
    // majorScalePitches[1] should be the pitch of the first step of
    // the scale, so there's padding at the beginning of both these
    // arrays.
    const int majorScalePitches[] = {0, 0, 2, 4, 5, 7, 9, 11};
    ChordType majorDiationicTriadTypes[] =
        {ChordTypes::NoChord, ChordTypes::Major, ChordTypes::Minor,
         ChordTypes::Minor, ChordTypes::Major, ChordTypes::Major,
         ChordTypes::Minor, ChordTypes::Diminished};

    int offset = k.getTonicPitch();

    if (!k.isMinor())
    {
        ChordLabel firstChord
        (
            majorDiationicTriadTypes[firstChordNumber],
            (majorScalePitches[firstChordNumber] + offset) % 12
        );
        ChordLabel secondChord
        (
            majorDiationicTriadTypes[secondChordNumber],
            (majorScalePitches[secondChordNumber] + offset) % 12
        );
        ChordProgression p(firstChord, secondChord, k);
        m_progressionMap.insert(p);
    }
    // else handle minor-key chords

}


/////////////////////////////////////////////////
ChordProgression::ChordProgression(const ChordLabel& first_,
                                                   const ChordLabel& second_,
                                                   const Key& key_) :
    first(first_),
    second(second_),
    homeKey(key_)
{
    // nothing else
}

bool
ChordProgression::operator<(const ChordProgression& other) const
{
    // no need for:
    // if (first == other.first) return second < other.second;
    return first < other.first;
}


/////////////////////////////////////////////////

PitchProfile::PitchProfile()
{
    for (int i = 0; i < 12; ++i) m_data[i] = 0;
}

double&
PitchProfile::operator[](int i)
{
    return m_data[i];
}

const double&
PitchProfile::operator[](int i) const
{
    return m_data[i];
}

#if 0   // not used
double
PitchProfile::distance(const PitchProfile &other)
{
    double distance = 0;

    for (int i = 0; i < 12; ++i)
    {
        distance += fabs(other[i] - m_data[i]);
    }

    return distance;
}

double
PitchProfile::dotProduct(const PitchProfile &other) const
{
    double product = 0;

    for (int i = 0; i < 12; ++i)
    {
        product += other[i] * m_data[i];
    }

    return product;
}
#endif

double
PitchProfile::productScorer(const PitchProfile &other) const
{
    double cumulativeProduct = 1;
    double numbersInProduct = 0;

    for (int i = 0; i < 12; ++i)
    {
        if (other[i] > 0)
        {
            cumulativeProduct *= m_data[i];
            ++numbersInProduct;
        }
    }

    if (numbersInProduct > 0)
        return pow(cumulativeProduct, 1/numbersInProduct);

    return 0;
}

PitchProfile
PitchProfile::normalized()
{
    double size = 0;
    PitchProfile normedProfile;

    for (int i = 0; i < 12; ++i)
    {
        size += fabs(m_data[i]);
    }

    if (size == 0) size = 1;

    for (int i = 0; i < 12; ++i)
    {
        normedProfile[i] = m_data[i] / size;
    }

    return normedProfile;
}

PitchProfile&
PitchProfile::operator*=(double d)
{

    for (int i = 0; i < 12; ++i)
    {
        m_data[i] *= d;
    }

    return *this;
}

PitchProfile&
PitchProfile::operator+=(const PitchProfile& d)
{

    for (int i = 0; i < 12; ++i)
    {
        m_data[i] += d[i];
    }

    return *this;
}

}  // namespace (anonymous)


ChordAnalyzer::ChordAnalyzer() : m_impl(new ChordAnalyzerImpl) {}
ChordAnalyzer::~ChordAnalyzer() { delete m_impl; }

void ChordAnalyzer::updateChordNameStyles() { m_impl->updateChordNameStyles(); }

// See documentation in .h file
void
ChordAnalyzer::labelChords(
std::map<const timeT,
         const std::string>         &chords,
std::map<const timeT, int>          &roots,
const std::vector<const Segment*>   &segments,
const Segment                       *currentSegment,
const timeT                          quantizationWindow,
const timeT                          arpeggiationWindow,
bool                                 onePerTimePeriod,
const bool                           wholeComposition,
const timeT                          beginTime,
const timeT                          endTime)
{
    m_impl->labelChords(chords,
                        roots,
                        segments,
                        currentSegment,
                        quantizationWindow,
                        arpeggiationWindow,
                        onePerTimePeriod
                        ,
                        wholeComposition,
                        beginTime,
                        endTime
                       );
}  // ChordAnalyzer::labelChords()

void
ChordAnalyzerImpl::labelChords(
std::map<const timeT,
         const std::string>         &chords,
std::map<const timeT, int>          &roots,
const std::vector<const Segment*>   &segments,
const Segment                       *currentSegment,
const timeT                          quantizationWindow,
const timeT                          arpeggiationWindow,
bool                                 onePerTimePeriod,
const bool                           wholeComposition,
const timeT                          beginTime,
const timeT                          endTime)
{
#ifdef CHORD_NAME_RULER_AND_CONFLICTING_KEY_CHANGES_DEBUG
    Composition &composition(  RosegardenDocument
                             ::currentDocument
                             ->getComposition());

    qDebug() << "ChordAnalyzerImpl::labelChords()"
             << (onePerTimePeriod ? "UNARPEGGIATE" : "ON-OFF")
             << " qnt:"
             << quantizationWindow
             << " un:"
             << arpeggiationWindow
             << "  *****";
    if (wholeComposition)
        qDebug() << "  whole composition";
    else {
        qDebug() << "  rng:"
                 << beginTime
                 << '='
                 << composition.getMusicalTimeStringForAbsoluteTime(beginTime)
                 << " -> "
                 << endTime
                 << composition.getMusicalTimeStringForAbsoluteTime(endTime);
    }
#endif

    if (segments.empty()) return;

    class NoteOrKeyEqual {
      public:
        NoteOrKeyEqual(const unsigned pitch, const NoteOrKey::OnOffKey onOff)
        : m_pitch(pitch), m_onOff(onOff)
        {}

        bool operator()(std::pair<const unsigned, const NoteOrKey&> iter) {
            return    iter.second.onOffKey == m_onOff
                   && iter.second.pitch    == m_pitch;
        }
      private:
        unsigned            m_pitch;
        NoteOrKey::OnOffKey m_onOff;
    };

    NotesAndKeys notesAndKeys;

    if (!currentSegment)  // Safety check, shouildn't ever be called without
        currentSegment = segments[0];   // Already checked for empty() above

#ifdef CHORD_NAME_RULER_AND_CONFLICTING_KEY_CHANGES_DEBUG
    unsigned numberOfKeys = 0;
#endif
    auto getBeginAndEndIters = [wholeComposition,
                                onePerTimePeriod,
                                arpeggiationWindow,
                                beginTime,
                                endTime]
    (
    Segment::iterator   &beginIter,
    Segment::iterator   &endIter,
    const Segment       *adjustSegment)
    {
        if (wholeComposition) {
            beginIter = adjustSegment->begin();
            endIter   = adjustSegment->  end();
        }
        else {
            timeT   adjustedBegin = beginTime,
                    adjustedEnd   =   endTime;

            if ( onePerTimePeriod) {
                adjustedBegin -= beginTime % arpeggiationWindow;

                adjustedEnd += arpeggiationWindow;
                adjustedEnd -= adjustedEnd % arpeggiationWindow;
            }

            if (adjustedBegin < adjustSegment->getStartTime())
                adjustedBegin = adjustSegment->getStartTime();
            if (  adjustedEnd > adjustSegment->getEndTime  ())
                  adjustedEnd = adjustSegment->getEndTime  ();

            beginIter = adjustSegment->findTimeConst(adjustedBegin);
              endIter = adjustSegment->findTimeConst(adjustedEnd  );
        }
    };  // getBeginAndEndIters()

    // Get key changes from current segment
    // Must do before notes (from all segments) so if at same time
    //   as note will come before in multimap ordering.
    auto keys    = currentSegment->keySignatureMap();
    auto keyIter = keys.begin();

    // Make sure key at beginning of notesAndKeys so always have
    // key when iterating and hit notes.
    if (keyIter == keys.end())
        // Default C major at
        notesAndKeys.emplace(std::piecewise_construct,
                             std::forward_as_tuple(  currentSegment
                                                   ->getStartTime()),
                             std::forward_as_tuple(Key()));
    else if (keyIter->first != currentSegment->getStartTime()) {
        // Copy first key to beginning in case not already there so never
        // without key. Matches matrix editor and ConflictingKeyChanges
        // semantics that first key "propgates" to beginning of segment,
        // not default C major up until first key change.
#if 0  // #ifdef CHORD_NAME_RULER_AND_CONFLICTING_KEY_CHANGES_DEBUG
        qDebug() << "  first key:"
                 << keyIter->second.getName()
                 << '@'
                 << keyIter->first
                 << " but instead"
                 << currentSegment->getStartTime();
#endif
        notesAndKeys.emplace(currentSegment->getStartTime(), keyIter->second);
    }

    // Insert all keys (if any) at correct times
    for ( ; keyIter != keys.end() ; ++keyIter) {
#if 0  // #ifdef CHORD_NAME_RULER_AND_CONFLICTING_KEY_CHANGES_DEBUG
        qDebug() << "  Nth key:  "   // t4osDEBUG
                 << keyIter->second.getName()
                 << '@'
                 << keyIter->first;
#endif
        notesAndKeys.emplace(keyIter->first, keyIter->second);
    }

    timeT halfQuantization = quantizationWindow / 2;
    auto  quantize         = [halfQuantization, quantizationWindow](
                             timeT t) {
                                 t += halfQuantization;
                                 return t - t % quantizationWindow;
                             };

    Segment::iterator   beginIter,
                          endIter;
    for (const Segment* const segment : segments) {
        getBeginAndEndIters(beginIter, endIter, segment);
#if 0  // #ifdef CHORD_NAME_RULER_AND_CONFLICTING_KEY_CHANGES_DEBUG
        std::cerr << "  segment: " << segment->getLabel();
        std::cerr << "  begin: ";
        if (beginIter == segment->end())
            std::cerr << "<end()>";
        else
            std::cerr << (*beginIter)->getAbsoluteTime()
                      << '='
                      <<  composition
                         .getMusicalTimeStringForAbsoluteTime(
                          (*beginIter)->getAbsoluteTime());
        std::cerr << "  end: ";
        if (endIter == segment->end())
            std::cerr << "<end()>";
        else
            std::cerr << (*endIter)->getAbsoluteTime()
                      << '='
                      <<  composition
                         .getMusicalTimeStringForAbsoluteTime(
                          (*endIter)->getAbsoluteTime());
        std::cerr << std::endl;
#endif

        for (auto eventIter = beginIter ; eventIter != endIter ; ++eventIter) {
            auto event = *eventIter;
            if (!event->isa(Note::EventType))
                continue;  // Keys already handled above, ignore anything else.

            timeT time         = event->getAbsoluteTime(),
                  offTime      = time + event->getDuration();
            unsigned pitch     = event->get<Int>(BaseProperties::PITCH);
            auto     allAtTime = notesAndKeys.equal_range(time);

            if (onePerTimePeriod) {
                // Pretend that all notes span full modulo-aligned time period
                if (offTime % arpeggiationWindow != 0)
                    offTime += arpeggiationWindow;
                offTime  = offTime - offTime % arpeggiationWindow;
                time     =    time -    time % arpeggiationWindow;
            }
            else if (quantizationWindow > 0) {  // doing chords at notes on/off
                // Round to nearest quantizationWindow
                time    = quantize(time);
                offTime = quantize(offTime);
            }

            if (offTime == time)
                continue;  // Zero-length note, maybe due to quantization

            if (allAtTime.first == allAtTime.second) {
                notesAndKeys.emplace(std::piecewise_construct,
                                     std::forward_as_tuple(time),
                                     std::forward_as_tuple(pitch,
                                                           NoteOrKey::ON));
                notesAndKeys.emplace(std::piecewise_construct,
                                     std::forward_as_tuple(offTime),
                                     std::forward_as_tuple(pitch,
                                                           NoteOrKey::OFF));
                continue;
            }

            auto found = std::find_if(allAtTime.first,
                                      allAtTime.second,
                                      NoteOrKeyEqual(pitch, NoteOrKey::ON));
            if (found == allAtTime.second)
                notesAndKeys.emplace(std::piecewise_construct,
                                     std::forward_as_tuple(time),
                                     std::forward_as_tuple(pitch,
                                                           NoteOrKey::ON));
            else
                ++found->second.count;

            allAtTime = notesAndKeys.equal_range(offTime);
            found = std::find_if(allAtTime.first,
                                 allAtTime.second,
                                 NoteOrKeyEqual(pitch, NoteOrKey::OFF));
            if (found == allAtTime.second)
                notesAndKeys.emplace(std::piecewise_construct,
                                     std::forward_as_tuple(offTime),
                                     std::forward_as_tuple(pitch,
                                                           NoteOrKey::OFF));
            else
                ++found->second.count;
        }
    }

#ifdef CHORD_NAME_RULER_AND_CONFLICTING_KEY_CHANGES_DEBUG
    qDebug() << " "
             << notesAndKeys.size()
             << "notes and keys "
             << numberOfKeys;
#endif

    if (notesAndKeys.empty()) return;

    if (onePerTimePeriod) labelChordsOnePerTimePeriod(notesAndKeys,
                                                      chords,
                                                      roots,
                                                      arpeggiationWindow);
    else                  labelChordsAtNotesOnOff    (notesAndKeys,
                                                      chords,
                                                      roots);
}  // ChordAnalyzerImpl::labelChords()


namespace AnalysisHelper
{

///////////////////////////////////////////////////////////////////////////
// Time signature guessing
///////////////////////////////////////////////////////////////////////////


void
guessHarmonies(CompositionTimeSliceAdapter &c, Segment &s)
{
    HarmonyGuessList l;

    // 1. Get the list of possible harmonies
    makeHarmonyGuessList(c, l);

    // 2. Refine the list of possible harmonies by preferring chords in the
    //    current key and looking for familiar progressions and
    //    tonicizations.
    refineHarmonyGuessList(c, l, s);

    // 3. Put labels in the Segment.  For the moment we just do the
    //    really naive thing with the segment arg to refineHarmonyGuessList:
    //    could do much better here
}

// #### this is too long
// should use constants for basic lengths, not numbers
TimeSignature
guessTimeSignature(CompositionTimeSliceAdapter &c)
{
    bool haveNotes = false;

    // 1. Guess the duration of the beat. The right beat length is going
    //    to be a common note length, and beat boundaries should be likely
    //    to have notes starting on them.

    std::vector<int> beatScores(4, 0);

    // durations of quaver, dotted quaver, crotchet, dotted crotchet:
    static const int commonBeatDurations[4] = {48, 72, 96, 144};

    int j = 0;
    for (CompositionTimeSliceAdapter::iterator i = c.begin();
         i != c.end() && j < 100;
         ++i, ++j)
    {

        // Skip non-notes
        if (!(*i)->isa(Note::EventType)) continue;
        haveNotes = true;

        for (int k = 0; k < 4; ++k)
        {

            // Points for any note of the right length
            if ((*i)->getDuration() == commonBeatDurations[k])
                beatScores[k]++;

            // Score for the probability that a note starts on a beat
            // boundary.

            // Normally, to get the probability that a random beat boundary
            // has a note on it, you'd add a constant for each note on a
            // boundary and divide by the number of beat boundaries.
            // Instead, this multiplies the constant (1/24) by
            // commonBeatDurations[k], which is inversely proportional to
            // the number of beat boundaries.

            if ((*i)->getAbsoluteTime() % commonBeatDurations[k] == 0)
                beatScores[k] += commonBeatDurations[k] / 24;

        }

    }

    if (!haveNotes) return TimeSignature();

    int beatDuration = 0,
        bestScore = 0;

    for (int j = 0; j < 4; ++j)
    {
        if (beatScores[j] >= bestScore)
        {
            bestScore = beatScores[j];
            beatDuration = commonBeatDurations[j];
        }
    }

    // 2. Guess whether the measure has two, three or four beats. The right
    //    measure length should make notes rarely cross barlines and have a
    //    high average length for notes at the start of bars.

    std::vector<int> measureLengthScores(5, 0);

    for (CompositionTimeSliceAdapter::iterator i = c.begin();
         i != c.end() && j < 100;
         ++i, ++j)
    {

        if (!(*i)->isa(Note::EventType)) continue;

        // k is the guess at the number of beats in a measure
        for (int k = 2; k < 5; ++k)
        {

            // Determine whether this note crosses a barline; points for the
            // measure length if it does NOT.

            int noteOffset = ((*i)->getAbsoluteTime() % (beatDuration * k));
            int noteEnd    = noteOffset + (*i)->getDuration();
            if ( !(noteEnd > (beatDuration * k)) )
                measureLengthScores[k] += 10;


            // Average length of notes at measure starts

            // Instead of dividing by the number of measure starts, this
            // multiplies by the number of beats per measure, which is
            // inversely proportional to the number of measure starts.

            if ((*i)->getAbsoluteTime() % (beatDuration * k) == 0)
                measureLengthScores[k] +=
                    (*i)->getDuration() * k / 24;

        }

    }

    int measureLength = 0;

    bestScore = 0;  // reused from earlier

    for (int j = 2; j < 5; ++j)
    {
        if (measureLengthScores[j] >= bestScore)
        {
            bestScore = measureLengthScores[j];
            measureLength = j;
        }
    }

    //
    // 3. Put the result in a TimeSignature object.
    //

    int numerator = 0, denominator = 0;

    if (beatDuration % 72 == 0)
    {

        numerator = 3 * measureLength;

        // 144 means the beat is a dotted crotchet, so the beat division
        // is a quaver, so you want 8 on bottom
        denominator = (144 * 8) / beatDuration;

    }
    else
    {

        numerator = measureLength;

        // 96 means that the beat is a crotchet, so you want 4 on bottom
        denominator = (96 * 4) / beatDuration;

    }

    TimeSignature ts(numerator, denominator);

    return ts;

}

///////////////////////////////////////////////////////////////////////////
// Key guessing
///////////////////////////////////////////////////////////////////////////
Key
guessKey(CompositionTimeSliceAdapter &c)
{
    if (c.begin() == c.end()) return Key();

    // 1. Figure out the distribution of emphasis over the twelve
    //    pitch clases in the first few bars. Pitches that occur
    //    more often have greater emphasis, and pitches that occur
    //    at stronger points in the bar have greater emphasis.

    std::vector<int> weightedNoteCount(12, 0);
    TimeSignature timeSig;
    timeT timeSigTime = 0;
    timeT nextSigTime = (*c.begin())->getAbsoluteTime();

    int j = 0;
    for (CompositionTimeSliceAdapter::iterator i = c.begin();
         i != c.end() && j < 100; ++i, ++j)
    {
        timeT time = (*i)->getAbsoluteTime();

        if (time >= nextSigTime) {
            Composition *comp = c.getComposition();
            int sigNo = comp->getTimeSignatureNumberAt(time);
            if (sigNo >= 0) {
                std::pair<timeT, TimeSignature> sig = comp->getTimeSignatureChange(sigNo);
                timeSigTime = sig.first;
                timeSig = sig.second;
            }
            if (sigNo < comp->getTimeSignatureCount() - 1) {
                nextSigTime = comp->getTimeSignatureChange(sigNo + 1).first;
            } else {
                nextSigTime = comp->getEndMarker();
            }
        }

        // Skip any other non-notes
        if (!(*i)->isa(Note::EventType)) continue;

        try {
            // Get pitch, metric strength of this event
            int pitch = (*i)->get<Int>(BaseProperties::PITCH)%12;
            int emphasis =
                1 << timeSig.getEmphasisForTime((*i)->getAbsoluteTime() - timeSigTime);

            // Count notes
            weightedNoteCount[pitch] += emphasis;

        } catch (...) {
            std::cerr << "No pitch for note at " << time << "!" << std::endl;
        }
    }

    // 2. Figure out what key best fits the distribution of emphasis.
    //    Notes outside a piece's key are rarely heavily emphasized,
    //    and the tonic and dominant of the key are likely to appear.

    // This part is longer than it should be.

    int bestTonic = -1;
    bool bestKeyIsMinor = false;
    int lowestCost = 999999999;

    for (int k = 0; k < 12; ++k)
    {
        int cost =
            // accidentals are costly
              weightedNoteCount[(k + 1 ) % 12]
            + weightedNoteCount[(k + 3 ) % 12]
            + weightedNoteCount[(k + 6 ) % 12]
            + weightedNoteCount[(k + 8 ) % 12]
            + weightedNoteCount[(k + 10) % 12]
            // tonic is very good
            - weightedNoteCount[ k           ] * 5
            // dominant is good
            - weightedNoteCount[(k + 7 ) % 12];
        if (cost < lowestCost)
        {
            bestTonic = k;
            lowestCost = cost;
        }
    }

    for (int k = 0; k < 12; ++k)
    {
        int cost =
            // accidentals are costly
              weightedNoteCount[(k + 1 ) % 12]
            + weightedNoteCount[(k + 4 ) % 12]
            + weightedNoteCount[(k + 6 ) % 12]
            // no cost for raised steps 6/7 (k+9, k+11)
            // tonic is very good
            - weightedNoteCount[ k           ] * 5
            // dominant is good
            - weightedNoteCount[(k + 7 ) % 12];
        if (cost < lowestCost)
        {
            bestTonic = k;
            bestKeyIsMinor = true;
            lowestCost = cost;
        }
    }

    return Key(bestTonic, bestKeyIsMinor);

}

// Guess the appropriate key signature at this time.  First tries to
// find the most common key signature, then falls back to guessKey.
// @returns Key in concert pitch
// @param comp is the composition
// @param t is the target time
// @param segmentToSkip is the segment to skip, presumably because it
// is getting a new key signature so its old one isn't relevant.  It
// may be nullptr.
// @author Tom Breton (Tehom)
Key
guessKeyAtTime(Composition &comp, timeT t,
                               const Segment *segmentToSkip)
{
    typedef std::map<Key,unsigned int> MapKeys;
    MapKeys keyCounts;
    SegmentMultiSet& segs = comp.getSegments();
    for (SegmentMultiSet::iterator i = segs.begin();
         i != segs.end();
         ++i) {
        Segment *s = *i;
        // If this segment is relevant...
        if ((s != segmentToSkip) &&
            (s->getStartTime() <= t) &&
            (s->getEndMarkerTime() > t)) {
            // ...get its current key.
            Key segKey = s->getKeyAtTime(t);
            // Adjust it in case s is transposing.
            int transposition = s->getTranspose();
            if (transposition != 0) {
                // This works reasonably well without finding the
                // right height, which is often ambiguous anyways.
                segKey = segKey.transpose(transposition, 0);
            }
            // Increment our count of that key, creating it in map if
            // needed.
            MapKeys::iterator found = keyCounts.find(segKey);
            if (found != keyCounts.end()) {
                ++found->second;
            } else {
                keyCounts.insert(MapKeys::value_type(segKey, 1));
            }
        }
    }

    // Return the most common one, if any.
    if (!keyCounts.empty()) {
        unsigned int mostFound = 0;
        Key bestKey = Key();
        for (MapKeys::iterator i = keyCounts.begin();
             i != keyCounts.end();
             ++i) {
            if (i->second > mostFound) {
                bestKey   = i->first;
                mostFound = i->second;
            }
        }
        return bestKey;
    }

    // Otherwise fall back to GuessKey.
    CompositionTimeSliceAdapter adapter(&comp, t, comp.getDuration());
    return guessKey(adapter);
}

// Guess the appropriate key signature for segment at this time.
// @returns Key in concert pitch
// @param t is the target time
// @param segment is the target segment
// @author Tom Breton (Tehom)
Key
guessKeyForSegment(timeT t, const Segment *segment)
{
    return guessKeyAtTime(*segment->getComposition(), t, segment);
}

}  // namespace AnalysisHelper

}  // namespace Rosegarden
