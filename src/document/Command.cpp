/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*- vi:set ts=8 sts=4 sw=4: */

/*
    Rosegarden
    A MIDI and audio sequencer and musical notation editor.
    Copyright 2000-2023 the Rosegarden development team.
    Modifications and additions Copyright (c) 2022,2023 Mark R. Rubin aka "thanks4opensource" aka "thanks4opensrc"

    Other copyrights also apply to some parts of this work.  Please
    see the AUTHORS file and individual file headers for details.

    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License as
    published by the Free Software Foundation; either version 2 of the
    License, or (at your option) any later version.  See the file
    COPYING included with this distribution for more information.
*/

#include "CommandHistory.h"
#include "Command.h"

namespace Rosegarden
{

MacroCommand::MacroCommand(QString name) :
    m_name(name)
{
}

MacroCommand::~MacroCommand()
{
    for (size_t i = 0; i < m_commands.size(); ++i) {
        delete m_commands[i];
    }
}

void
MacroCommand::addCommand(Command *command)
{
    m_commands.push_back(command);
}

void
MacroCommand::deleteCommand(Command *command)
{
    for (std::vector<Command *>::iterator i = m_commands.begin();
        i != m_commands.end(); ++i) {

        if (*i == command) {
            m_commands.erase(i);
            delete command;
            return;
        }
    }
}

bool
MacroCommand::haveCommands() const
{
    return !m_commands.empty();
}

void
MacroCommand::execute()
{
    CommandHistory::getInstance()->setMacroCommandIsExecuting(true);
    for (size_t i = 0; i < m_commands.size(); ++i) {
        m_commands[i]->execute();
    }
    CommandHistory::getInstance()->setMacroCommandIsExecuting(false);
}

void
MacroCommand::unexecute()
{
    CommandHistory::getInstance()->setMacroCommandIsExecuting(true);
    for (size_t i = 0; i < m_commands.size(); ++i) {
        m_commands[m_commands.size() - i - 1]->unexecute();
    }
    CommandHistory::getInstance()->setMacroCommandIsExecuting(false);
}

QString
MacroCommand::getName() const
{
    return m_name;
}

void
MacroCommand::setName(QString name)
{
    m_name = name;
}

BundleCommand::BundleCommand(QString name) :
    MacroCommand(name)
{
}

BundleCommand::~BundleCommand()
{
}

QString
BundleCommand::getName() const
{
    if (m_commands.size() == 1) return m_name;
    return tr("%1 (%n change(s))", "", m_commands.size()).arg(m_name);
}

}
