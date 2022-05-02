/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*- vi:set ts=8 sts=4 sw=4: */

/*
    Rosegarden
    A MIDI and audio sequencer and musical notation editor.
    Copyright 2000-2022 the Rosegarden development team.

    This file is Copyright 2003
        Mark Hymers         <markh@linuxfromscratch.org>

    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License as
    published by the Free Software Foundation; either version 2 of the
    License, or (at your option) any later version.  See the file
    COPYING included with this distribution for more information.
*/

#ifndef RG_COLOURMAP_H
#define RG_COLOURMAP_H

#include <QColor>

#include <cstdint>
#include <map>
#include <string>

namespace Rosegarden
{


/// Maps a colour ID to a QColor and a name.
/**
 * ??? Quite a bit of this is actually unused as there is no way to launch
 *     the color table editor (ColourConfigurationPage).
 */
class ColourMap
{
public:
    /// Create a ColourMap with only the default segment colour.
    ColourMap();

    static const QColor defaultSegmentColour;

    /**
     * If the colourID isn't in the map, the routine returns the value of
     * the default colour (at ID 0 in the table).  This means that if
     * somehow some of the Segments get out of sync with the ColourMap and
     * have invalid colour values, they'll be set to the Composition default
     * colour.
     */
    QColor getColour(unsigned colourID) const;

    /**
     * If the colourID isn't in the map, the name of the entry at ID 0 is
     * returned.  Usually this is "", for internationalization reasons.
     */
    std::string getName(unsigned colourID) const;


    unsigned size() const { return m_colours.size(); }

    // *** Data

    class Entry
    {
      public:
        Entry() :
            colour(defaultSegmentColour),
            name()
        {
        }

        Entry(const QColor &i_colour, const std::string &i_name) :
            colour(i_colour),
            name(i_name)
        {
        }

        Entry(const char *i_name) :
            colour(QColor(i_name)),
            name(i_name)
        {
        }

        Entry(const unsigned char i_red,
              const unsigned char i_green,
              const unsigned char i_blue,
              const char* const i_name) :
            colour(QColor(i_red, i_green, i_blue)),
            name(i_name)
        {
        }

        QColor colour;
        std::string name;
    };

    typedef std::map<unsigned /* colourID */, Entry> MapType;

    MapType::const_iterator begin() const { return m_colours.begin(); }
    MapType::const_iterator end()   const { return m_colours.end(); }

    int id(const QColor colour) const;

    /// For RosegardenDocument.
    std::string toXmlString(std::string name) const;

    // *** Interface for ColourConfigurationPage.

    // These functions are essentially unused as there is no way to
    // launch the ColourConfigurationPage.
    //
    // "Were unused". Made maps private, require these be used
    // to keep inverseMap up-to-date.

    /// Add a colour entry using the lowest available ID.
    unsigned addEntry(QColor colour, std::string name);

    // Add new colour entry at ID, or overwrite existing at that ID
    void addEntry(unsigned colourID, QColor colour, std::string name);

    void modifyName(unsigned colourID, std::string name);
    void modifyColour(unsigned colourID, QColor colour);
    void deleteEntry(unsigned colourID);

protected:

    typedef std::map<uint32_t /*rgb*/, unsigned /*colourID*/> InverseMapType;
           MapType m_colours      ;
    InverseMapType m_colourToID ;

};


}

#endif
