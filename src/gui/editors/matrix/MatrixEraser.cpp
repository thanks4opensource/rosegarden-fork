/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*- vi:set ts=8 sts=4 sw=4: */

/*
    Rosegarden
    A MIDI and audio sequencer and musical notation editor.
    Copyright 2000-2022 the Rosegarden development team.

    Other copyrights also apply to some parts of this work.  Please
    see the AUTHORS file and individual file headers for details.

    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License as
    published by the Free Software Foundation; either version 2 of the
    License, or (at your option) any later version.  See the file
    COPYING included with this distribution for more information.
*/

#include "MatrixEraser.h"

#include "MatrixPainter.h"

#include "MatrixMouseEvent.h"
#include "MatrixViewSegment.h"
#include "MatrixElement.h"
#include "MatrixScene.h"
#include "MatrixToolBox.h"
#include "MatrixWidget.h"

#include "base/ViewElement.h"
#include "base/Selection.h"
#include "commands/matrix/MatrixEraseCommand.h"
#include "document/CommandHistory.h"
#include "misc/Debug.h"

#include <Qt>

namespace Rosegarden
{

QCursor *MatrixEraser::m_cursor(nullptr);
QCursor *MatrixEraser::m_selectCursor(nullptr);

MatrixEraser::MatrixEraser(MatrixWidget *parent, MatrixToolBox *toolbox) :
    MatrixTool("MatrixEraser", parent, toolbox)
{
    createMenu();
    if (!m_cursor      ) m_cursor       = makeCursor("eraser", 6, 13);
    if (!m_selectCursor) m_selectCursor = makeCursor("eraser_select_cursor",
                                                     10, 18);
}

void MatrixEraser::handleLeftButtonPress(const MatrixMouseEvent *e)
{
    m_isFinished = true;
    if (!e->element || !e->viewSegment) return; // nothing to erase
    eraseNotes(e);
}


void MatrixEraser::handleMidButtonPress(const MatrixMouseEvent *e)
{
    MatrixTool *painter = dynamic_cast<MatrixTool*>(
                          m_toolbox->getTool(MatrixPainter::ToolName()));
    if (!painter) return;    // shouldn't ever happen
    painter->setRole(Role::SINGLE_ACTION);
    m_toolbox->appendDispatchTool(painter, e);
    // Painter might have done appendDispatchTool() of Select or Segment.
    m_toolbox->getActiveTool()->handleLeftButtonPress(e);
}

bool
MatrixEraser::handleKeyPress(const MatrixMouseEvent *e, const int key)
{
    if (key != ALTERNATE_TOOL_QT_KEY) return false;
    m_toolbox->appendDispatchTool(MatrixPainter::ToolName(), e);
    return true;
}

void MatrixEraser::readyAtPos(const MatrixMouseEvent *event)
{
    setCursor();
    m_isFinished = true;
    if (!overlaySelectOrSegment(event)) setContextHelpForPos(event);
}

void
MatrixEraser::setCursor()
{
    if (m_widget) {
        if (m_cursor) m_widget->setCanvasCursor(m_cursor);
        else          m_widget->setCanvasCursor(Qt::PointingHandCursor);
    }
}
void
MatrixEraser::setSelectCursor()
{
    if (m_widget) {
        if (m_selectCursor) m_widget->setCanvasCursor(m_selectCursor);
        else                m_widget->setCanvasCursor(Qt::PointingHandCursor);
    }
}

QString
MatrixEraser::altToolHelpString()
const
{
    return altToolHelpStringTools(tr("erase"), tr("draw"));
}

void
MatrixEraser::setContextHelpForPos(const MatrixMouseEvent *e)
{
    if (!e) {
        setContextHelp(tr("Erase tool: Move mouse over note or empty "
                          "area for help."));
        return;
    }

    if (e->buttons != Qt::NoButton) {
        setContextHelp(tr("Release mouse button."));
        return;
    }

    QString help;
    ContextInfo contextInfo(this, e);

    if (e->element) {
        help = tr("Click to delete");

        if (contextInfo.haveSelection) {
            if (contextInfo.inSelection)
                help += tr(" selected notes");
            else
                help += tr(" note");
        }
        help += tr(", or right-click (optional add Shift) to "
                   "edit note properties");
    }
    else {
        help = tr("Click and drag to select notes");

        if (contextInfo.haveSelection)
            help += tr(", hold Shift to add to or "
                       "remove from selection");

        help += tr(", or right-click for tool/undo/segment menu");
    }

    help += m_toolbox->getCurrentTool()->altToolHelpString();

    setContextHelp(help);
}

QString MatrixEraser::ToolName() { return "eraser"; }

}
