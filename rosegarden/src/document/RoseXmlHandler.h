
/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*- vi:set ts=8 sts=4 sw=4: */

/*
    Rosegarden
    A MIDI and audio sequencer and musical notation editor.

    This program is Copyright 2000-2007
        Guillaume Laurent   <glaurent@telegraph-road.org>,
        Chris Cannam        <cannam@all-day-breakfast.com>,
        Richard Bown        <richard.bown@ferventsoftware.com>

    The moral rights of Guillaume Laurent, Chris Cannam, and Richard
    Bown to claim authorship of this work have been asserted.

    Other copyrights also apply to some parts of this work.  Please
    see the AUTHORS file and individual file headers for details.

    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License as
    published by the Free Software Foundation; either version 2 of the
    License, or (at your option) any later version.  See the file
    COPYING included with this distribution for more information.
*/

#ifndef _RG_ROSEXMLHANDLER_H_
#define _RG_ROSEXMLHANDLER_H_

#include "base/Device.h"
#include "base/MidiProgram.h"
#include "gui/general/ProgressReporter.h"
#include <map>
#include <set>
#include <string>
#include <qstring.h>
#include "base/Event.h"
#include <qxml.h>


class QXmlParseException;
class QXmlAttributes;


namespace Rosegarden
{

class XmlStorableEvent;
class XmlSubHandler;
class Studio;
class Segment;
class RosegardenGUIDoc;
class Instrument;
class Device;
class Composition;
class ColourMap;
class Buss;
class AudioPluginManager;
class AudioPluginInstance;
class AudioFileManager;


/**
 * Handler for the Rosegarden XML format
 */
class RoseXmlHandler : public ProgressReporter, public QXmlDefaultHandler
{
public:

    typedef enum
    {
        NoSection,
        InComposition,
        InSegment,
        InStudio,
        InInstrument,
        InBuss,
        InAudioFiles,
        InPlugin,
        InAppearance
    } RosegardenFileSection;

    /**
     * Construct a new RoseXmlHandler which will put the data extracted
     * from the XML file into the specified composition
     */
    RoseXmlHandler(RosegardenGUIDoc *doc,
                   unsigned int elementCount,
                   bool createNewDevicesWhenNeeded);

    virtual ~RoseXmlHandler();

    /// overloaded handler functions
    virtual bool startDocument();
    virtual bool startElement(const QString& namespaceURI,
                              const QString& localName,
                              const QString& qName,
                              const QXmlAttributes& atts);

    virtual bool endElement(const QString& namespaceURI,
                            const QString& localName,
                            const QString& qName);

    virtual bool characters(const QString& ch);

    virtual bool endDocument (); // [rwb] - for tempo element catch

    bool isDeprecated() { return m_deprecation; }

    bool isCancelled() { return m_cancelled; }

    /// Return the error string set during the parsing (if any)
    QString errorString();

    bool hasActiveAudio() const { return m_hasActiveAudio; }
    std::set<QString> &pluginsNotFound() { return m_pluginsNotFound; }

    bool error(const QXmlParseException& exception);
    bool fatalError(const QXmlParseException& exception);

protected:

    // just for convenience -- just call to the document
    //
    Composition& getComposition();
    Studio& getStudio();
    AudioFileManager& getAudioFileManager();
    AudioPluginManager* getAudioPluginManager();

    void setSubHandler(XmlSubHandler* sh);
    XmlSubHandler* getSubHandler() { return m_subHandler; }

    void addMIDIDevice(QString name, bool createAtSequencer);
    void setMIDIDeviceConnection(QString connection);
    void setMIDIDeviceName(QString name);
    void skipToNextPlayDevice();

    //--------------- Data members ---------------------------------

    RosegardenGUIDoc    *m_doc;
    Segment *m_currentSegment;
    XmlStorableEvent    *m_currentEvent;

    timeT m_currentTime;
    timeT m_chordDuration;
    timeT *m_segmentEndMarkerTime;

    bool m_inChord;
    bool m_inGroup;
    bool m_inComposition;
    bool m_inColourMap;
    std::string m_groupType;
    int m_groupId;
    int m_groupTupletBase;
    int m_groupTupledCount;
    int m_groupUntupledCount;
    std::map<long, long> m_groupIdMap;

    bool m_foundTempo;

    QString m_errorString;
    std::set<QString> m_pluginsNotFound;

    RosegardenFileSection             m_section;
    Device               *m_device;
    DeviceId              m_deviceRunningId;
    bool                              m_percussion;
    MidiByte              m_msb;
    MidiByte              m_lsb;
    Instrument           *m_instrument;
    Buss                 *m_buss;
    AudioPluginInstance  *m_plugin;
    ColourMap            *m_colourMap;
    MidiKeyMapping       *m_keyMapping;
    MidiKeyMapping::KeyNameMap m_keyNameMap;
    unsigned int                      m_pluginId;
    unsigned int                      m_totalElements;
    unsigned int                      m_elementsSoFar;

    XmlSubHandler                    *m_subHandler;
    bool                              m_deprecation;
    bool                              m_createDevices;
    bool                              m_haveControls;
    bool                              m_cancelled;
    bool                              m_skipAllAudio;
    bool                              m_hasActiveAudio;
};


}

#endif
