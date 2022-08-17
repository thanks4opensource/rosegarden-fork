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

#define RG_MODULE_STRING "[MatrixSelect]"
#define RG_NO_DEBUG_PRINT 1

#include "MatrixSelect.h"

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

QCursor *MatrixSelect::m_cursor(nullptr);

MatrixSelect::MatrixSelect(MatrixWidget *widget, MatrixToolBox *toolbox) :
    MatrixTool("MatrixSelect", widget, toolbox),
    m_selectionRect(nullptr),
    m_selectionOrigin(),
    m_updateRect(false),
    m_currentViewSegment(nullptr),
    m_selectionToAddTo(nullptr),
    m_eventsInRectangle(nullptr),
    m_previousCollisions()
{
    m_role = Role::OVERLAY;  // override default in MatrixTool::MatrixTool

    createMenu();

    if (!m_cursor) m_cursor = makeCursor("rectangle_select_cursor", 17, 11);
}

void
MatrixSelect::handleLeftButtonPress(const MatrixMouseEvent *e)
{
    MATRIX_DEBUG << "MatrixSelect::handleLeftButtonPress";

    m_isFinished = false;
    m_previousCollisions.clear();
    m_currentViewSegment = e->viewSegment;

    if ((e->modifiers & Qt::ShiftModifier) &&
          m_scene->getSelection() &&
         !m_scene->getSelection()->empty())
        m_selectionToAddTo = new EventSelection(*m_scene->getSelection());

    // NOTE: if we ever have axis-independent zoom, this (and similar code
    // elsewhere) will have to be refactored to draw a series of lines using
    // two different widths, based on calculating 200 / axisZoomPercent
    // to solve ((w * axisZoomPercent) / 100) = 2
    //
    // (Not sure how to do this now that we do.  It's obnoxious, but oh
    // well.)
    //
    // thanks4opensrc, 4/2022: Have had independent zoom for a long
    // time. QPen::setCosmetic(), below, solves problem.
    if (!m_selectionRect) {
        m_selectionRect = new QGraphicsRectItem;
        m_scene->addItem(m_selectionRect);
        QColor c = GUIPalette::getColour(GUIPalette::SelectionRectangle);
        QPen p = QPen(c, 2);
        p.setCosmetic(true);
        m_selectionRect->setPen(p);
        c.setAlpha(50);
        m_selectionRect->setBrush(c);
    }

    m_selectionOrigin = QPointF(e->sceneX, e->sceneY);
    m_selectionRect->setRect(QRectF(m_selectionOrigin, QSize()));
    m_selectionRect->hide();
    m_updateRect = true;

    // Clear existing selection if we're not merging
    //
    if (!(e->modifiers & Qt::ShiftModifier))
        m_scene->setSelection(nullptr, false);
}

void
MatrixSelect::handleMidButtonPress(const MatrixMouseEvent *e)
{
    m_toolbox->popDispatchTool();
    m_toolbox->getActiveTool()->handleMidButtonPress(e);
}

FollowMode
MatrixSelect::handleMouseMove(const MatrixMouseEvent *e)
{
    if (!e) return NO_FOLLOW;

    if (e->buttons == Qt::NoButton) {
        if (e->element) {
            m_toolbox->popDispatchTool();
            m_isFinished = true;
            m_toolbox->getActiveTool()->readyAtPos(e);
            return NO_FOLLOW;
        }
        setContextHelpForPos(e);
        return NO_FOLLOW;
    }

    if (m_updateRect && m_selectionRect) {
        QPointF p0(m_selectionOrigin);
        QPointF p1(e->sceneX, e->sceneY);
        QRectF r = QRectF(p0, p1).normalized();

        m_selectionRect->setRect(r.x() + 0.5,
                                 r.y() + 0.5,
                                 r.width(),
                                 r.height());
        m_selectionRect->show();

        setViewCurrentSelection(false);
    }

    setContextHelpForPos(e);

    if (m_updateRect) return FOLLOW_HORIZONTAL | FOLLOW_VERTICAL;
    else              return NO_FOLLOW;
}

void
MatrixSelect::handleMouseRelease(const MatrixMouseEvent *e)
{
    MATRIX_DEBUG << "MatrixSelect::handleMouseRelease";

    m_updateRect = false;

    if (m_selectionRect) {
        setViewCurrentSelection(true);
        m_previousCollisions.clear();
        m_selectionRect->hide();

        if ( m_selectionToAddTo &&
            !m_selectionToAddTo->empty() &&
             m_eventsInRectangle &&
            !m_eventsInRectangle->empty()) {
            bool allAlreadyInSelection = true;
            for (Event *event : m_eventsInRectangle->getSegmentEvents())
                if (!m_selectionToAddTo->contains(event)) {
                    allAlreadyInSelection = false;
                    break;
                }

            if (allAlreadyInSelection) {
                for (Event *event : m_eventsInRectangle->getSegmentEvents())
                    m_selectionToAddTo->removeEvent(event);
                m_scene->setSelection(m_selectionToAddTo, false);
            }
        }
     // m_widget->canvas()->update();
    }

    if (m_eventsInRectangle && m_eventsInRectangle != m_scene->getSelection())
        delete m_eventsInRectangle;
    m_eventsInRectangle = nullptr;

    if (m_selectionToAddTo && m_selectionToAddTo != m_scene->getSelection())
        delete m_selectionToAddTo;
    m_selectionToAddTo = nullptr;

    // Tell anyone who's interested that the selection has changed
    emit gotSelection();

    setContextHelpForPos(e);

    if (!e || e->element) m_isFinished = true;
    else                  m_isFinished = false;
}

bool
MatrixSelect::handleKeyPress(const MatrixMouseEvent *e, const int key)
{
    if (key != ALTERNATE_TOOL_QT_KEY || !m_isFinished) return false;
    m_toolbox->popDispatchTool();
    return m_toolbox->getActiveTool()->handleKeyPress(e, key);
}

void
MatrixSelect::readyAtPos(const MatrixMouseEvent *event)
{
    setCursor();
    m_isFinished = true;
    setContextHelpForPos(event);
}

void
MatrixSelect::setCursor()
{
    m_toolbox->getPenultimateTool()->setSelectCursor();
}

void
MatrixSelect::stow()
{
    m_selectionRect = nullptr;
    m_updateRect = false;

    if (m_selectionRect) {
        delete m_selectionRect;
        m_selectionRect = nullptr;
    }
}

void
MatrixSelect::setViewCurrentSelection(bool always)
{
    if (always) m_previousCollisions.clear();

    Segment &currentSegment(m_currentViewSegment->getSegment());
    if (m_eventsInRectangle && m_eventsInRectangle != m_scene->getSelection())
        delete m_eventsInRectangle;
    m_eventsInRectangle = new EventSelection(currentSegment);
    if (getEventsInRectangle(m_eventsInRectangle)) {
        if (m_selectionToAddTo) {
            EventSelection *allEvents = new
                                        EventSelection(*m_selectionToAddTo);
            allEvents->addFromSelection(m_eventsInRectangle);
            m_scene->setSelection(allEvents, true);
            if (allEvents != m_scene->getSelection())   // never true
                delete allEvents;                       // never happens
        }
        else
            m_scene->setSelection(m_eventsInRectangle, true);
    }
}

bool
MatrixSelect::getEventsInRectangle(EventSelection *&selection)
{
    if (!m_selectionRect || !m_selectionRect->isVisible()) return 0;

    // get the selections
    //
    QList<QGraphicsItem *> l = m_selectionRect->collidingItems(
                                                Qt::IntersectsItemShape);

    // This is a nasty optimisation, just to avoid re-creating the
    // selection if the items we span are unchanged.  It's not very
    // effective, either, because the colliding items returned
    // includes things like the horizontal and vertical background
    // lines -- and so it changes often: every time we cross a line.
    // More thought needed.

    // It might be better to use the event properties (i.e. time and
    // pitch) to calculate this "from first principles" rather than
    // doing it graphically.  That might also be helpful to avoid us
    // dragging off the logical edges of the scene.

    // (Come to think of it, though, that would be troublesome just
    // because of the requirement to use all events that have any part
    // inside the selection.  Quickly finding all events that start
    // within a time range is trivial, finding all events that
    // intersect one is more of a pain.)
    if (l == m_previousCollisions) return false;
    m_previousCollisions = l;

    if (!l.empty()) {
        for (int i = 0; i < l.size(); ++i) {
            QGraphicsItem *item = l[i];
            MatrixElement *element = MatrixElement::getMatrixElement(item);
            if (element && element->getSegment() ==
                           element->getScene()->getCurrentSegment()) {
                //!!! NB. In principle, this element might not come
                //!!! from the right segment (in practice we only have
                //!!! one segment, but that may change)
                selection->addEvent(element->event());
            }
        }
    }

    return true;
}

void
MatrixSelect::setContextHelpForPos(const MatrixMouseEvent *e)
{
    if (!e) {
        setContextHelp(tr("Select/modify tool: Move mouse into matrix "
                          "grid for help."));
        return;
    }

    if (e->buttons != Qt::NoButton) {
        // Must be rectangle drag because would be dispatched to another
        // tool if not.
        setContextHelp(tr("Release mouse button to finish selection."));
        return;
    }

    QString help;
    ContextInfo contextInfo(this, e);

        help = tr("Click and drag to select notes");

        if (contextInfo.haveSelection)
            help += tr(", hold Shift to add to or "
                       "remove from selection");

        help += tr(", or right-click for tool/undo/segment menu");

    help += m_toolbox->getCurrentTool()->altToolHelpString();

    setContextHelp(help);
}

QString MatrixSelect::ToolName() { return "select"; }

}
