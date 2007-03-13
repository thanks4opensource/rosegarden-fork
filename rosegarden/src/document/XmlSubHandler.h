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

#ifndef _RG_XMLSUBHANDLER_H_
#define _RG_XMLSUBHANDLER_H_

#include <qstring.h>
#include <qxml.h>

namespace Rosegarden {
    
class XmlSubHandler
{
public:
    XmlSubHandler();
    virtual ~XmlSubHandler();
    
    virtual bool startElement(const QString& namespaceURI,
                              const QString& localName,
                              const QString& qName,
                              const QXmlAttributes& atts) = 0;

    /**
     * @param finished : if set to true on return, means that
     * the handler should be deleted
     */
    virtual bool endElement(const QString& namespaceURI,
                            const QString& localName,
                            const QString& qName,
                            bool& finished) = 0;

    virtual bool characters(const QString& ch) = 0;
};

}

#endif /*_RG_XMLSUBHANDLER_H_*/
