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

#include "MatrixSegment.h"

#include "MatrixElement.h"
#include "MatrixMouseEvent.h"
#include "MatrixScene.h"
#include "MatrixToolBox.h"
#include "MatrixViewSegment.h"
#include "MatrixWidget.h"

#include "base/Segment.h"
#include "base/ViewElement.h"
#include "commands/matrix/MatrixEraseCommand.h"
#include "document/CommandHistory.h"
#include "misc/Debug.h"

#include <Qt>

namespace Rosegarden
{

QCursor *MatrixSegment::m_cursor(nullptr);

MatrixSegment::MatrixSegment(MatrixWidget *parent, MatrixToolBox *toolbox) :
    MatrixTool("MatrixSegment", parent, toolbox),
    m_segment(nullptr)
{
    m_role = Role::OVERLAY;  // override default in MatrixTool::MatrixTool

    if (!m_cursor) m_cursor = makeCursor("active_segment_cursor", 15, 15);

    createMenu();
}

void MatrixSegment::handleLeftButtonPress(const MatrixMouseEvent *e)
{
    m_segment = segmentIfIsntActive(e);

    if (m_segment)
        setContextHelp(tr("Release mouse button to switch to") +
                          m_widget->segmentTrackInstrumentLabel(
                                " %5 (Track %1, %2, %3, %4)",
                                m_segment));
}

// Override so that double-click in non-active segement note doesn't
// delete it. If not overridden, first click of double click changes
// active segment to note, then generic MatrixTool::handleMouseDoubleClick()
// deletes it because its segment is now active segment, which makes
// sense from programming logic standpoint but is contrary to UI metaphor
// that only active segment can be modified.
void MatrixSegment::handleMouseDoubleClick(const MatrixMouseEvent*)
{
#if 0   // No, is too confusing. Shows change and undoes. Leave
        // so double-click is same as single click
    // First click of double-click changed segment, so change it back.
    if (m_widget->segmentWhenClicked())
        changeActiveSegment(m_widget->segmentWhenClicked());
#endif

    // Was changed to current tool's on mouse button release from first
    // click, but change back until button released from second click.
    setCursor();

    setContextHelp(tr("Switched to new active segment. Delete noting only "
                      "done for notes in active segment at start of "
                      "double-click."));

    // Will be truly finished when get mouse button release
    m_isFinished = false;
}

void
MatrixSegment::handleMidButtonPress(const MatrixMouseEvent *e)
{
    m_toolbox->popDispatchTool();
    m_toolbox->getActiveTool()->handleMidButtonPress(e);
}

FollowMode MatrixSegment::handleMouseMove(const MatrixMouseEvent *e)
{
    if (!e) return NO_FOLLOW;

    const Segment *otherSegment = segmentIfIsntActive(e);

    if (e->buttons == Qt::NoButton && !otherSegment) {
        m_toolbox->popDispatchTool();
        m_isFinished = true;
        m_toolbox->getActiveTool()->readyAtPos(e);
        return NO_FOLLOW;
    }

    if (otherSegment != m_segment)
        m_segment = otherSegment;

    if (m_segment) {
        if (e->buttons)
            setContextHelp(tr("Release mouse button to switch to") +
                              m_widget->segmentTrackInstrumentLabel(
                                    " %5 (Track %1, %2, %3, %4)",
                                    m_segment));
        else
            setContextHelp(tr("Click to switch to") +
                              m_widget->segmentTrackInstrumentLabel(
                                    " %5 (Track %1, %2, %3, %4)",
                                    m_segment) +
                              tr(", or right-click for tool/undo/segment "
                                 "menu"));
    }
    else
        setContextHelp(tr("Release mouse button to return "
                          "to current tool"));

    return NO_FOLLOW;
}

void MatrixSegment::handleMouseRelease(const MatrixMouseEvent *e)
{
    // Don't need to do anything, was already done on mouse button
    // release from first click.
    if (m_toolbox->hadDoubleClick()) {
        m_isFinished = true;
        return;
    }

    m_segment = segmentIfIsntActive(e);

    if (m_segment) {
        m_widget->clearSelection();
        m_widget->getScene()->setCurrentSegment(m_segment);
    }

    m_isFinished = true;
}

bool
MatrixSegment::handleKeyPress(const MatrixMouseEvent *e, const int key)
{
    if (key != ALTERNATE_TOOL_QT_KEY || !m_isFinished) return false;
    m_toolbox->popDispatchTool();
    return m_toolbox->getActiveTool()->handleKeyPress(e, key);
}

void
MatrixSegment::readyAtPos(const MatrixMouseEvent *event)
{
    m_segment = nullptr;
    m_isFinished = true;
    setCursor();
    setContextHelpForPos(event);
}

const Segment*
MatrixSegment::segmentIfIsntActive(const MatrixMouseEvent *e) const
{
    if (e->element && e->element->getSegment() != m_scene->getCurrentSegment())
        return e->element->getSegment();
    else
        return nullptr;
}

void
MatrixSegment::setCursor()
{
    if (m_widget) {
        if (m_cursor) m_widget->setCanvasCursor(m_cursor);
        else          m_widget->setCanvasCursor(Qt::PointingHandCursor);
    }
}

QString MatrixSegment::ToolName() { return "segment"; }

}
