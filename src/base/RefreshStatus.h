/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*- vi:set ts=8 sts=4 sw=4: */


/*
    Rosegarden
    A sequencer and musical notation editor.
    Copyright 2000-2023 the Rosegarden development team.
    Modifications and additions Copyright (c) 2022,2023 Mark R. Rubin aka "thanks4opensource" aka "thanks4opensrc"
    See the AUTHORS file for more details.

    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License as
    published by the Free Software Foundation; either version 2 of the
    License, or (at your option) any later version.  See the file
    COPYING included with this distribution for more information.
*/

#ifndef RG_REFRESH_STATUS_H
#define RG_REFRESH_STATUS_H

#include <QtGlobal>


namespace Rosegarden
{

/// Flag indicating that a refresh is needed.
/**
 * This is a flag indicating that a refresh may be required for
 * some part of the user interface.  The implementation is
 * a simple wrapper around a bool.
 *
 * See SegmentRefreshStatus which derives from this and adds a
 * time range.
 *
 * See RefreshStatusArray which supports multiple observers.
 */
class RefreshStatus
{
public:
    RefreshStatus() : m_needsRefresh(false) {}

    bool needsRefresh() const { return m_needsRefresh; }
    void setNeedsRefresh(bool s) { m_needsRefresh = s; }

protected:
    bool m_needsRefresh;
};

/// Polled notification mechanism.
/**
 * This supports providing refresh notification flags (e.g. RefreshStatus)
 * for a number of observers.  An observer can check the flag
 * periodically and perform a refresh when the flag is set.  Then they
 * can clear the flag.
 *
 * Polling mechanisms such as this are useful when changes to an underlying
 * data object are fast and frequent but the observers would like to update
 * at a slower pace (usually to save CPU time).
 *
 * See Composition which instantiates this with RefreshStatus and
 * Segment which instantiates this with SegmentRefreshStatus (which adds
 * a time range to the refresh flag in RefreshStatus).
 */
template<class RS>
class RefreshStatusArray
{
public:
    RefreshStatusArray();

    /// Creates a new refresh status object for an observer.
    /**
     * Use the returned ID when calling getRefreshStatus() to check whether
     * a refresh is needed.  This ID identifies a specific observer.  That
     * observer can clear these refresh flags without affecting other
     * observers.
     */
    unsigned int getNewRefreshStatusId();

    void deleteRefreshStatusId(const unsigned id);

    // iterators to underlying std::map
    typename std::map<unsigned, RS>::iterator       begin (),
                                                    end   ();
    typename std::map<unsigned, RS>::const_iterator cbegin(),
                                                    cend  ();

    bool haveRefreshStatus(unsigned int id) const;

    size_t size() const { return m_refreshStatuses.size(); }

    /// Returns the refresh status object for a particular observer.
    /**
     * Observers can set or clear the flags via the reference that is
     * returned.
     */
    RS& getRefreshStatus(unsigned int id);  // see template, below

    /// Sets all the refresh flags to true.
    /**
     * rename: needsRefresh()
     */
    void updateRefreshStatuses();

protected:
    unsigned                m_nextId;   // continuously increment, never reused
    std::map<unsigned, RS>  m_refreshStatuses;
};

template<class RS>
RefreshStatusArray<RS>::RefreshStatusArray()
:   m_nextId(0)
{}

template<class RS>
typename std::map<unsigned, RS>::iterator RefreshStatusArray<RS>::begin()
{
    return m_refreshStatuses.begin();
}

template<class RS>
typename std::map<unsigned, RS>::iterator RefreshStatusArray<RS>::end()
{
    return m_refreshStatuses.end();
}

template<class RS>
typename std::map<unsigned, RS>::const_iterator RefreshStatusArray<RS>::cbegin()
{
    return m_refreshStatuses.cbegin();
}

template<class RS>
typename std::map<unsigned, RS>::const_iterator RefreshStatusArray<RS>::cend()
{
    return m_refreshStatuses.cend();
}

template<class RS>
RS& RefreshStatusArray<RS>::getRefreshStatus(unsigned int id)
{
    auto found = m_refreshStatuses.find(id);
    Q_ASSERT_X(found != m_refreshStatuses.end(),
               "RefreshStatusArray::getRefreshStatus()",
               "ID out of bounds");
    return found->second;
}

template<class RS>
bool RefreshStatusArray<RS>::haveRefreshStatus(unsigned int id) const
{
    return m_refreshStatuses.find(id) != m_refreshStatuses.end();
}

template<class RS>
unsigned int RefreshStatusArray<RS>::getNewRefreshStatusId()
{
    m_refreshStatuses[m_nextId] = RS();
    return m_nextId++;
}

template<class RS>
void RefreshStatusArray<RS>::deleteRefreshStatusId(
const unsigned id)
{
    auto found = m_refreshStatuses.find(id);
    if (found != m_refreshStatuses.end())
        m_refreshStatuses.erase(found);
}

// Defined in Composition.cpp.
void breakpoint();

template<class RS>
void RefreshStatusArray<RS>::updateRefreshStatuses()
{
    // breakpoint(); // for debug purposes, so one can set a breakpoint
    // in this template code (set it in breakpoint() itself which is in
    // Composition.cpp).

    // Set all the refresh flags to true.
    for (auto &status : m_refreshStatuses)
        status.second.setNeedsRefresh(true);
}

}

#endif
