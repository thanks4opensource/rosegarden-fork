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
    bool getChordRulerNonDiatonicChords();
    bool getBug1623();

    void setAutoChannels(bool value);
    bool getAutoChannels();

}


}
