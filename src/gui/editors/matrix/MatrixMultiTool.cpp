/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*- vi:set ts=8 sts=4 sw=4: */

/*
    Rosegarden
    A MIDI and audio sequencer and musical notation editor.
    Copyright 2000-2022 the Rosegarden development team.
    Modifications and additions Copyright (c) 2022 Mark R. Rubin aka "thanks4opensource" aka "thanks4opensrc"

    Other copyrights also apply to some parts of this work.  Please
    see the AUTHORS file and individual file headers for details.

    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License as
    published by the Free Software Foundation; either version 2 of the
    License, or (at your option) any later version.  See the file
    COPYING included with this distribution for more information.
*/

#define RG_MODULE_STRING "[MatrixMultiTool]"
#define RG_NO_DEBUG_PRINT 1

#include "MatrixMultiTool.h"

#include "misc/Strings.h"

#include "base/Event.h"
#include "base/NotationTypes.h"
#include "base/Selection.h"
#include "base/ViewElement.h"
#include "base/SnapGrid.h"

#include "document/CommandHistory.h"
#include "document/RosegardenDocument.h"
#include "misc/ConfigGroups.h"

#include "gui/general/GUIPalette.h"

#include "MatrixElement.h"
#include "MatrixMover.h"
#include "MatrixPainter.h"
#include "MatrixResizer.h"
#include "MatrixSegment.h"
#include "MatrixSelect.h"
#include "MatrixVelocity.h"
#include "MatrixViewSegment.h"
#include "MatrixTool.h"
#include "MatrixToolBox.h"
#include "MatrixWidget.h"
#include "MatrixScene.h"
#include "MatrixMouseEvent.h"
#include "misc/Debug.h"

#include <QSettings>


namespace Rosegarden
{

QCursor *MatrixMultiTool::m_selectCursor(nullptr);


MatrixMultiTool::MatrixMultiTool(MatrixWidget *widget, MatrixToolBox *toolbox) :
    MatrixTool("MatrixMultiTool", widget, toolbox)
{
    createMenu();
    if (!m_selectCursor) m_selectCursor = makeCursor("multitool_select_cursor",
                                                     16, 12);
}

MatrixTool*
MatrixMultiTool::overlayTool(const MatrixMouseEvent *e)
{
    MatrixTool *tool = nullptr;
    switch (moveResizeVelocity(e)) {
        case MoveResizeVelocity::MOVE:
            tool = dynamic_cast<MatrixTool*>(
                   m_toolbox->getTool(MatrixMover::ToolName()));
            break;
        case MoveResizeVelocity::RESIZE:
            tool = dynamic_cast<MatrixTool*>(
                   m_toolbox->getTool(MatrixResizer::ToolName()));
            break;
        case MoveResizeVelocity::VELOCITY:
            tool = dynamic_cast<MatrixTool*>(
                   m_toolbox->getTool(MatrixVelocity::ToolName()));
            break;
        case MoveResizeVelocity::UNSET:
            break;
    }

    if (!tool) return nullptr;   // shouldn't ever happen
    tool->setRole(Role::OVERLAY);
    m_toolbox->appendDispatchTool(tool, e);

    return tool;
}

void
MatrixMultiTool::handleLeftButtonPress(const MatrixMouseEvent *e)
{
    MATRIX_DEBUG << "MatrixMultiTool::handleLeftButtonPress";

    // Should never happen
    if (!e->element ||
       (e->element && e->element->getSegment() != m_scene->getCurrentSegment()))
        return;

    MatrixTool *tool = overlayTool(e);
    if (!tool) return;   // shouldn't ever happen
    tool->handleLeftButtonPress(e);
}

void
MatrixMultiTool::handleMidButtonPress(const MatrixMouseEvent *e)
{
    // Middle button press draws a new event via MatrixPainter.
    MatrixTool *painter = dynamic_cast<MatrixTool*>(
                          m_toolbox->getTool(MatrixPainter::ToolName()));
    if (!painter) return;    // shouldn't ever happen
    painter->setRole(Role::SINGLE_ACTION);
    m_toolbox->appendDispatchTool(painter, e);

    // Painter might have done appendDispatchTool() of Select or Segment.
    m_toolbox->getActiveTool()->handleLeftButtonPress(e);
}

FollowMode
MatrixMultiTool::handleMouseMove(const MatrixMouseEvent *e)
{
    if (!e) return NO_FOLLOW;
    if (!overlaySelectOrSegment(e)) setContextHelpForPos(e);
    return NO_FOLLOW;
}

bool
MatrixMultiTool::handleKeyPress(const MatrixMouseEvent *e, const int key)
{
    if (!m_isFinished || key != ALTERNATE_TOOL_QT_KEY) return false;
    MatrixTool *painter = dynamic_cast<MatrixTool*>(
                          m_toolbox->getTool(MatrixPainter::ToolName()));
    if (!painter) return false;  // shouldn't ever happen
    painter->setRole(Role::PERSISTENT);
    m_toolbox->appendDispatchTool(painter, e);
    return true;
}

void
MatrixMultiTool::readyAtPos(const MatrixMouseEvent *event)
{
    m_currentOverlay = MoveResizeVelocity::UNSET;
    if (!overlaySelectOrSegment(event)) setContextHelpForPos(event);
}

#if 0   // Never used. MultiTool has no native cursor, always has
        // dispatch tool of MatrixSelect, MatrixSegment, MatrixMover,
        // MatrixResizer, or MatrixVelocity.
void
MatrixMultiTool::setCursor()
{
    if (m_widget) {
        if (m_cursor) m_widget->setCanvasCursor(m_cursor);
        else          m_widget->setCanvasCursor(Qt::OpenHandCursor);
    }
}
#endif

void
MatrixMultiTool::setSelectCursor()
{
    if (m_widget) {
        if (m_selectCursor) m_widget->setCanvasCursor(m_selectCursor);
        else                m_widget->setCanvasCursor(Qt::OpenHandCursor);
    }
}

MatrixMultiTool::MoveResizeVelocity
MatrixMultiTool::moveResizeVelocity(const MatrixMouseEvent *e) const
{
    if (m_widget->isDrumMode() && !m_widget->getShowPercussionDurations()) {
        double y             = e->element->getLayoutY();
        double height        = m_scene->getYResolution();
        double velocityStart = height * 0.2 + y;

        if (e->sceneY < velocityStart) return MoveResizeVelocity::VELOCITY;
        else                           return MoveResizeVelocity::MOVE;
    }
    else {
        double x             = e->element->getLayoutX();
        double width         = e->element->getAccurateWidth();
        double resizeStart   = width * 0.85 + x;
        double velocityStart = width * 0.15 + x;

        if      (e->sceneX >   resizeStart) return MoveResizeVelocity::RESIZE;
        else if (e->sceneX < velocityStart) return MoveResizeVelocity::VELOCITY;
        else                                return MoveResizeVelocity::MOVE;
    }
}

QString
MatrixMultiTool::altToolHelpString()
const
{
    return altToolHelpStringTools(tr("multi"), tr("draw"));
}

void
MatrixMultiTool::setContextHelpForPos(const MatrixMouseEvent *e)
{
    if (!e) {
        setContextHelp(tr("Select/modify tool: Move mouse into matrix "
                          "grid for help."));
        return;
    }

    if (e->buttons != Qt::NoButton) {
        // Can't happen, would be in Select, Segment, Mover,
        // Resizer, or Velocity tool.
        setContextHelp(tr("Release mouse button to finish."));
        return;
    }

    QString help;
    ContextInfo contextInfo(this, e);

    if (e->element) {
        QString moveResizeVelocityText(tr(" drag to"));
        MoveResizeVelocity newOverlay = moveResizeVelocity(e);
        switch (newOverlay) {
            case MoveResizeVelocity::MOVE:
                moveResizeVelocityText += tr(" move (Ctrl to copy)");
                break;
            case MoveResizeVelocity::RESIZE:
                moveResizeVelocityText += tr(" resize");
                break;
            case MoveResizeVelocity::VELOCITY:
                moveResizeVelocityText += tr(" adjust velocity of");
                break;
            case MoveResizeVelocity::UNSET:
                break;
        }

        if (newOverlay != m_currentOverlay) {
            switch (m_currentOverlay = newOverlay) {
                case MoveResizeVelocity::MOVE:
                    getTool(MatrixMover::ToolName())->setCursor();
                    break;
                case MoveResizeVelocity::RESIZE:
                    getTool(MatrixResizer::ToolName())->setCursor();
                    break;
                case MoveResizeVelocity::VELOCITY:
                    getTool(MatrixVelocity::ToolName())->setCursor();
                    break;
                case MoveResizeVelocity::UNSET:
                    break;
            }
        }

        if (contextInfo.haveSelection) {
            if (contextInfo.inSelection) {
                help = tr("Click and") + moveResizeVelocityText;

                if (contextInfo.selectionSize > 1)
                    help += tr(" selected notes");
                else
                    help += tr(" note");

                help += tr(" or shift-click to remove "
                           "note from selection");
            }
            else
                help = tr("Click to select note (or shift-click "
                          "to add to selection), then") +
                       moveResizeVelocityText +
                       tr(" note(s)");
        }
        else
            help = tr("Click and") + moveResizeVelocityText + tr(" note");

        help += tr(", or double-click to delete") +
                (contextInfo.inSelection ? tr(" selected notes")
                                         : tr(" note")) +
                tr(", or right-click (optional add Shift) to "
                   "edit note properties");
    }
    else {
        getTool(MatrixSelect::ToolName())->setCursor();
        help = tr("Click and drag to select notes");

        if (contextInfo.haveSelection)
            help += tr(", hold Shift to add to or "
                       "remove from selection");

        help += tr(", or right-click for tool/undo/segment menu");
    }

    help += m_toolbox->getCurrentTool()->altToolHelpString();

    setContextHelp(help);
}

QString MatrixMultiTool::ToolName() { return "multitool"; }

}
