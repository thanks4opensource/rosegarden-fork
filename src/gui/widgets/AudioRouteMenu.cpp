/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*- vi:set ts=8 sts=4 sw=4: */

/*
    Rosegarden
    A MIDI and audio sequencer and musical notation editor.
 
    This program is Copyright 2000-2006
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


#include "AudioRouteMenu.h"

#include <klocale.h>
#include "base/Instrument.h"
#include "base/Studio.h"
#include "gui/studio/StudioControl.h"
#include "sound/MappedCommon.h"
#include "sound/MappedStudio.h"
#include <kcombobox.h>
#include <qcursor.h>
#include <qobject.h>
#include <qpoint.h>
#include <qstring.h>
#include <qwidget.h>
#include "WheelyButton.h"


namespace Rosegarden
{

AudioRouteMenu::AudioRouteMenu(QWidget *par,
                               Direction direction,
                               Format format,
                               Studio *studio,
                               Instrument *instrument) :
        QObject(par),
        m_studio(studio),
        m_instrument(instrument),
        m_direction(direction),
        m_format(format)
{
    switch (format) {

    case Compact: {
            m_combo = 0;
            m_button = new WheelyButton(par);
            connect(m_button, SIGNAL(wheel(bool)), this, SLOT(slotWheel(bool)));
            connect(m_button, SIGNAL(clicked()), this, SLOT(slotShowMenu()));
            break;
        }

    case Regular: {
            m_button = 0;
            m_combo = new KComboBox(par);
            connect(m_combo, SIGNAL(activated(int)), this, SLOT(slotEntrySelected(int)));
            break;
        }

    }

    slotRepopulate();
}

QWidget *
AudioRouteMenu::getWidget()
{
    if (m_button)
        return m_button;
    else
        return m_combo;
}

void
AudioRouteMenu::slotRepopulate()
{
    switch (m_format) {

    case Compact:
        m_button->setText(getEntryText(getCurrentEntry()));
        break;

    case Regular:
        m_combo->clear();
        for (int i = 0; i < getNumEntries(); ++i) {
            m_combo->insertItem(getEntryText(i));
        }
        m_combo->setCurrentItem(getCurrentEntry());
        break;
    }
}

void
AudioRouteMenu::slotSetInstrument(Studio *studio,
                                  Instrument *instrument)
{
    m_studio = studio;
    m_instrument = instrument;
    slotRepopulate();
}

void
AudioRouteMenu::slotWheel(bool up)
{
    int current = getCurrentEntry();
    if (up) { // actually moves down the list
        if (current > 0)
            slotEntrySelected(current - 1);
    } else {
        if (current < getNumEntries() - 1)
            slotEntrySelected(current + 1);
    }
}

void
AudioRouteMenu::slotShowMenu()
{
    if (getNumEntries() == 0)
        return ;

    BlahPopupMenu *menu = new BlahPopupMenu((QWidget *)parent());

    for (int i = 0; i < getNumEntries(); ++i) {

        menu->insertItem(getEntryText(i), this, SLOT(slotEntrySelected(int)),
                         0, i);
        menu->setItemParameter(i, i);
    }

    int itemHeight = menu->itemHeight(0) + 2;
    QPoint pos = QCursor::pos();

    pos.rx() -= 10;
    pos.ry() -= (itemHeight / 2 + getCurrentEntry() * itemHeight);

    menu->popup(pos);
}

int
AudioRouteMenu::getNumEntries()
{
    if (!m_instrument)
        return 0;

    switch (m_direction) {

    case In: {
            int stereoIns =
                m_studio->getRecordIns().size() +
                m_studio->getBusses().size();

            if (m_instrument->getAudioChannels() > 1) {
                return stereoIns;
            } else {
                return stereoIns * 2;
            }

            break;
        }

    case Out:
        return m_studio->getBusses().size();
    }

    return 0;
}

int
AudioRouteMenu::getCurrentEntry()
{
    if (!m_instrument)
        return 0;

    switch (m_direction) {

    case In: {
            bool stereo = (m_instrument->getAudioChannels() > 1);

            bool isBuss;
            int channel;
            int input = m_instrument->getAudioInput(isBuss, channel);

            if (isBuss) {
                int recordIns = m_studio->getRecordIns().size();
                if (stereo) {
                    return recordIns + input;
                } else {
                    return recordIns * 2 + input * 2 + channel;
                }
            } else {
                if (stereo) {
                    return input;
                } else {
                    return input * 2 + channel;
                }
            }

            break;
        }

    case Out:
        return m_instrument->getAudioOutput();
    }

    return 0;
}

QString
AudioRouteMenu::getEntryText(int entry)
{
    switch (m_direction) {

    case In: {
            bool stereo = (m_instrument->getAudioChannels() > 1);
            int recordIns = m_studio->getRecordIns().size();

            if (stereo) {
                if (entry < recordIns) {
                    return i18n("In %1").arg(entry + 1);
                } else if (entry == recordIns) {
                    return i18n("Master");
                } else {
                    return i18n("Sub %1").arg(entry - recordIns);
                }
            } else {
                int channel = entry % 2;
                entry /= 2;
                if (entry < recordIns) {
                    return (channel ? i18n("In %1 R") :
                            i18n("In %1 L")).arg(entry + 1);
                } else if (entry == recordIns) {
                    return (channel ? i18n("Master R") :
                            i18n("Master L"));
                } else {
                    return (channel ? i18n("Sub %1 R") :
                            i18n("Sub %1 L")).arg(entry - recordIns);
                }
            }
            break;
        }

    case Out:
        if (entry == 0)
            return i18n("Master");
        else
            return i18n("Sub %1").arg(entry);
    }

    return QString();
}

void
AudioRouteMenu::slotEntrySelected(int i)
{
    switch (m_direction) {

    case In: {
            bool stereo = (m_instrument->getAudioChannels() > 1);

            bool oldIsBuss;
            int oldChannel;
            int oldInput = m_instrument->getAudioInput(oldIsBuss, oldChannel);

            bool newIsBuss;
            int newChannel = 0;
            int newInput;

            int recordIns = m_studio->getRecordIns().size();

            if (stereo) {
                newIsBuss = (i >= recordIns);
                if (newIsBuss) {
                    newInput = i - recordIns;
                } else {
                    newInput = i;
                }
            } else {
                newIsBuss = (i >= recordIns * 2);
                newChannel = i % 2;
                if (newIsBuss) {
                    newInput = i / 2 - recordIns;
                } else {
                    newInput = i / 2;
                }
            }

            MappedObjectId oldMappedId = 0, newMappedId = 0;

            if (oldIsBuss) {
                Buss *buss = m_studio->getBussById(oldInput);
                if (buss)
                    oldMappedId = buss->getMappedId();
            } else {
                RecordIn *in = m_studio->getRecordIn(oldInput);
                if (in)
                    oldMappedId = in->getMappedId();
            }

            if (newIsBuss) {
                Buss *buss = m_studio->getBussById(newInput);
                if (!buss)
                    return ;
                newMappedId = buss->getMappedId();
            } else {
                RecordIn *in = m_studio->getRecordIn(newInput);
                if (!in)
                    return ;
                newMappedId = in->getMappedId();
            }

            if (oldMappedId != 0) {
                StudioControl::disconnectStudioObjects
                (oldMappedId, m_instrument->getMappedId());
            } else {
                StudioControl::disconnectStudioObject
                (m_instrument->getMappedId());
            }

            StudioControl::setStudioObjectProperty
            (m_instrument->getMappedId(),
             MappedAudioFader::InputChannel,
             MappedObjectValue(newChannel));

            if (newMappedId != 0) {
                StudioControl::connectStudioObjects
                (newMappedId, m_instrument->getMappedId());
            }

            if (newIsBuss) {
                m_instrument->setAudioInputToBuss(newInput, newChannel);
            } else {
                m_instrument->setAudioInputToRecord(newInput, newChannel);
            }

            break;
        }

    case Out: {
            BussId bussId = m_instrument->getAudioOutput();
            Buss *oldBuss = m_studio->getBussById(bussId);
            Buss *newBuss = m_studio->getBussById(i);
            if (!newBuss)
                return ;

            if (oldBuss) {
                StudioControl::disconnectStudioObjects
                (m_instrument->getMappedId(), oldBuss->getMappedId());
            } else {
                StudioControl::disconnectStudioObject
                (m_instrument->getMappedId());
            }

            StudioControl::connectStudioObjects
            (m_instrument->getMappedId(), newBuss->getMappedId());

            m_instrument->setAudioOutput(i);
            break;
        }
    }

    slotRepopulate();
    emit changed();
}

}
