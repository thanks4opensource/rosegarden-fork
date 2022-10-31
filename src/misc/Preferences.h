/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*- vi:set ts=8 sts=4 sw=4: */

/*
    Rosegarden
    A sequencer and musical notation editor.
    Copyright 2000-2022 the Rosegarden development team.
    See the AUTHORS file for more details.

    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License as
    published by the Free Software Foundation; either version 2 of the
    License, or (at your option) any later version.  See the file
    COPYING included with this distribution for more information.
*/

#pragma once

#include <QSettings>
#include <QVariant>
#include <QString>

#include "misc/ConfigGroups.h"


namespace Rosegarden
{


/// Preferences
/**
 * Namespace for preferences getters and setters.  Gathering these into
 * a central location has the following benefits:
 *
 *   - Single location for .conf entry names and defaults.
 *   - Avoids confusing calls to QSettings::beginGroup()/endGroup().
 *   - Performance.  Caching to avoid hitting the .conf file.
 *   - Avoiding restarts.  By calling the setters, the
 *     Preferences dialogs can have a direct effect on cached
 *     values.  The user need not restart Rosegarden to see
 *     these changes take effect.
 *   - Consistent high-performance "write on first read" behavior to
 *     make it easier to find new .conf values during development.
 *
 */
namespace Preferences
{
    extern std::map<std::pair<const char*, const char*>, QVariant>
                preferences;

    template <typename T> T setPreference(const char *group,
                                          const char *preference,
                                          T value)
    {
        std::pair<const char*, const char*> grpPref{group, preference};
        auto iter = preferences.find(grpPref);

        if (iter == preferences.end() || iter->second != value) {
            QSettings settings;
            settings.beginGroup(group);
            settings.setValue(preference, value);
            preferences[grpPref] = value;
        }
        return value;
    }

    template <typename T> T getPreference(const char *group,
                                          const char *preference,
                                          T value)
    {
        QVariant qv = value;
        std::pair<const char*, const char*> grpPref{group, preference};
        auto iter = preferences.find(grpPref);

        if (iter == preferences.end()) {
            QSettings settings;
            settings.beginGroup(group);
            qv = settings.value(preference, value);
            preferences[grpPref] = qv;
        }
        else
            qv = iter->second;
        return qv.value<T>();
    }

    void init();


    // Special case: Too performance-critical (once per note label in
    //   MatrixElement::reconfigure()) to use map lookup in
    //   Preferences::setPreference() and getPreference() template functions
    // Used to map MIDI octave (pitch number (0-127) modulo 12) to one of
    //   several commonly used but not unified/standardized octave numbers,
    //   typically middle C == 4 or 5 (offset == -1 or 0 respectively)
    class MidiOctaveNumberOffset {
      public:
        MidiOctaveNumberOffset()
        :   m_isCurrent(false),
            m_offset(-2)  // Should be -1 for middle C == C4
        {}

        // Make sure initted so get() doesn't have to check
        void init()
        {
            if (!m_isCurrent) {
                  m_offset
                = Preferences::getPreference(GeneralOptionsConfigGroup,
                                             "midipitchoctave",
                                             -2);
                m_isCurrent = true;
            }
        }

        int get() const { return m_offset; }

        void set(const int offset)
        {
            m_offset    = offset;
            m_isCurrent = true;
            Preferences::setPreference(GeneralOptionsConfigGroup,
                                       "midipitchoctave",
                                       offset);
        }

      protected:
        bool m_isCurrent;
        int  m_offset;
    };
    extern MidiOctaveNumberOffset midiOctaveNumberOffset;


    // Special case: Too performance-critical (once per chord in
    // ChordLabel::ChordLabel() in AnalysisTypes.cpp) to use
    // map lookup in Preferences::setPreference() and
    // getPreference() template functions.
    class NonDiatonicChords {
      public:
        NonDiatonicChords()
        :   m_isCurrent(false),
            m_nonDiatonicChords(false)
        {}

        // Make sure initted so get() doesn't have to check
        void init()
        {
            if (!m_isCurrent) {
                  m_nonDiatonicChords
                = Preferences::getPreference(ChordAnalysisGroup,
                                             "non_diatonic_chords",
                                             false);
                m_isCurrent = true;
            }
        }

        int get() const { return m_nonDiatonicChords; }

        void set(const bool include )
        {
            m_nonDiatonicChords = include;
            m_isCurrent = true;
            Preferences::setPreference(ChordAnalysisGroup,
                                       "non_diatonic_chords",
                                       include);
        }

      protected:
        bool m_isCurrent;
        int  m_nonDiatonicChords;
    };
    extern NonDiatonicChords nonDiatonicChords;


    // The following should all go away, replaced by use of generic
    // setPreference() and getPreference() template functions, above

    void setSendProgramChangesWhenLooping(bool value);
    bool getSendProgramChangesWhenLooping();

    void setSendControlChangesWhenLooping(bool value);
    bool getSendControlChangesWhenLooping();

    // ??? Move ChannelManager.cpp:allowReset() and forceChannelSetups() here.

    void setUseNativeFileDialogs(bool value);
    bool getUseNativeFileDialogs();

    void setStopAtEnd(bool value);
    bool getStopAtEnd();

    // AudioFileLocationDialog settings

    void setAudioFileLocationDlgDontShow(bool dontShow);
    bool getAudioFileLocationDlgDontShow();

    // See AudioFileLocationDialog::Location enum.
    void setDefaultAudioLocation(int location);
    int getDefaultAudioLocation();

    void setCustomAudioLocation(QString location);
    QString getCustomAudioLocation();

    // Experimental
    bool getBug1623();

    void setAutoChannels(bool value);
    bool getAutoChannels();

}


}
