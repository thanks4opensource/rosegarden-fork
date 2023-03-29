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


#include "MatrixVelocity.h"

#include "base/BaseProperties.h"
#include "base/Event.h"
#include "base/Segment.h"
#include "base/Selection.h"
#include "base/SnapGrid.h"
#include "base/ViewElement.h"
#include "document/CommandHistory.h"
#include "commands/edit/ChangeVelocityCommand.h"
#include "MatrixElement.h"
#include "MatrixViewSegment.h"
#include "MatrixMouseEvent.h"
#include "MatrixTool.h"
#include "MatrixToolBox.h"
#include "MatrixResizer.h"
#include "MatrixScene.h"
#include "MatrixWidget.h"
#include "misc/Debug.h"
#include "gui/rulers/ControlItem.h"
#include "gui/rulers/ControlRulerWidget.h"
#include "gui/rulers/PropertyControlRuler.h"

namespace Rosegarden
{

QCursor *MatrixVelocity::m_cursor(nullptr);
QCursor *MatrixVelocity::m_selectCursor(nullptr);


MatrixVelocity::MatrixVelocity(MatrixWidget *widget, MatrixToolBox *toolbox) :
    MatrixTool("MatrixVelocity", widget, toolbox),
    m_mouseStartY(0.0),
    m_velocityDelta(0),
    m_screenPixelsScale(100.0),
    m_velocityScale(0),
    m_currentElement(nullptr),
    m_event(nullptr),
    m_currentViewSegment(nullptr),
    m_start(false)
{
    createMenu();
    if (!m_cursor      ) m_cursor       = makeCursor("velocity", 4, 10);
    if (!m_selectCursor) m_selectCursor = makeCursor("velocity_select_cursor",
                                                     16, 16);
}

void
MatrixVelocity::handleEventRemoved(Event *event)
{
    // NOTE: Do not use m_currentElement in here as it was freed by
    //       ViewSegment::eventRemoved() before we get here.

    // Is it the event we are holding on to?
    if (m_event == event) {
        m_currentElement = nullptr;
        m_event = nullptr;
    }
}

void
MatrixVelocity::handleLeftButtonPress(const MatrixMouseEvent *e)
{
    m_isFinished = false;

    if (!e->element) return;

    if (e->element->getSegment() !=
        e->element->getScene()->getCurrentSegment()) {
        RG_WARNING << "handleLeftButtonPress(): Will only adjust velocity "
                      "of notes in active segment.";
        return;
    }

    // Mouse position is no more related to pitch
    m_widget->showHighlight(false);

    m_currentViewSegment = e->viewSegment;
    m_currentElement = e->element;
    m_event = m_currentElement->event();

    // Get mouse pointer
    m_mouseStartY = e->sceneY;

    // Add this element and allow velocity change
    EventSelection *selection = m_scene->getSelection();

    if (selection) {
        EventSelection *newSelection;

        if ((e->modifiers & Qt::ShiftModifier) ||
            selection->contains(m_event)) {
            newSelection = new EventSelection(*selection);
        } else {
            newSelection = new EventSelection(m_currentViewSegment->getSegment());
        }

        newSelection->addEvent(m_event);
        m_scene->setSelection(newSelection, true);

    } else {
        m_scene->setSingleSelectedEvent(m_currentViewSegment,
                                        m_currentElement,
                                        true);
    }

    m_start = true;
}

void MatrixVelocity::handleMidButtonPress(const MatrixMouseEvent *event)
{
    MatrixTool *resizer = dynamic_cast<MatrixTool*>(
                            m_toolbox->getTool(MatrixResizer::ToolName()));
    if (!resizer) return;    // shouldn't ever happen
    resizer->setRole(Role::SINGLE_ACTION);
    m_toolbox->appendDispatchTool(resizer, event);
    // Resizer might have done appendDispatchTool() of Select or Segment.
    m_toolbox->getActiveTool()->handleLeftButtonPress(event);
}

bool
MatrixVelocity::handleKeyPress(const MatrixMouseEvent *e, const int key)
{
    if (!m_isFinished || key != ALTERNATE_TOOL_QT_KEY)
        return false;

    if (m_role == Role::OVERLAY) {  // from MultiTool
        m_toolbox->popDispatchTool();
        return m_toolbox->getActiveTool()->handleKeyPress(e, key);
    }

    MatrixTool *resizer = dynamic_cast<MatrixTool*>(
                            m_toolbox->getTool(MatrixResizer::ToolName()));
    if (!resizer) return false;  // shouldn't ever happen
    resizer->setRole(Role::PERSISTENT);
    m_toolbox->appendDispatchTool(resizer, e);
    return true;
}

FollowMode
MatrixVelocity::handleMouseMove(const MatrixMouseEvent *e)
{
    if (!e || (e->buttons == Qt::NoButton && overlaySelectOrSegment(e)))
        return NO_FOLLOW;

    setContextHelpForPos(e);

    if (!m_currentElement || !m_currentViewSegment)
        return NO_FOLLOW;

    // Calculate velocity scale factor
    if ((m_mouseStartY - e->sceneY) > m_screenPixelsScale) {
        m_velocityScale = 1.0;
    } else if ((m_mouseStartY - e->sceneY) < -m_screenPixelsScale) {
        m_velocityScale = -1.0;
    } else {
        m_velocityScale = (m_mouseStartY - e->sceneY) / m_screenPixelsScale;
    }

    m_velocityDelta = 128 * m_velocityScale;

    // Is a velocity ruler visible ?
    ControlRulerWidget * controlRulerWidget =
            m_scene->getMatrixWidget()->getControlsWidget();

    // Assuming velocity is the only one property ruler
    PropertyControlRuler *velocityRuler = controlRulerWidget->getActivePropertyRuler();

    if (velocityRuler) {

        ControlItemList *items = velocityRuler->getSelectedItems();
        for (ControlItemList::iterator it = items->begin(); it != items->end(); ++it) {
            float y = (*it)->y();
            if (m_start) (*it)->setData(velocityRuler->yToValue(y));
            int velocity = (*it)->getData() + m_velocityDelta;
            y = velocityRuler->valueToY(velocity);
            (*it)->setValue(y);
        }
        velocityRuler->update();
    }
    m_start = false;


    // Preview calculated velocity info on element
    // Dupe from MatrixMover
    EventSelection* selection = m_scene->getSelection();

    EventSelection singleSelection(*const_cast<Segment*>(m_currentElement->
                                                         getSegment()));
    if (!selection) {
        if (m_event) singleSelection.addEvent(m_event);
        selection = &singleSelection;
    }

    int maxVelocity = 0;
    int minVelocity = 127;

    for (EventContainer::iterator it =
             selection->getSegmentEvents().begin();
         it != selection->getSegmentEvents().end(); ++it) {

        MatrixElement *element = nullptr;
        ViewElementList::iterator vi = m_currentViewSegment->findEvent(*it);
        if (vi != m_currentViewSegment->getViewElementList()->end()) {
            element = static_cast<MatrixElement *>(*vi);
        }
        if (!element) continue;

        int velocity = 64;
        if (element->event()->has(BaseProperties::VELOCITY)) {
            velocity = element->event()->get<Int>(BaseProperties::VELOCITY);
        }

        velocity += m_velocityDelta;

        element->reconfigure(velocity);

#if 0   // t4os: Not necessary
        element->setSelected(true);
#endif

        if (velocity > 127) velocity = 127;
        if (velocity < 0) velocity = 0;

        if (velocity > maxVelocity) maxVelocity = velocity;
        if (velocity < minVelocity) minVelocity = velocity;
    }


        /** Might be something for the feature
        EventSelection* selection = m_mParentView->getCurrentSelection();
        EventContainer::iterator it = selection->getSegmentEvents().begin();
        MatrixElement *element = 0;
        for (; it != selection->getSegmentEvents().end(); it++) {
            element = m_currentViewSegment->getElement(*it);
            if (element) {
                // Somehow show the calculated velocity for each selected element
                // char label[16];
                // sprintf(label,"%d",(*it->getVelocity())*m_velocityScale);
                // element->label(label) /// DOES NOT EXISTS
            }
        }
        */

    // Preview velocity delta in contexthelp
    if (minVelocity == maxVelocity) {
        setContextHelp(tr("Velocity change: %1   Velocity: %2")
                           .arg(m_velocityDelta).arg(minVelocity));
    } else {
        setContextHelp(tr("Velocity change: %1   Velocity: %2 to %3")
                           .arg(m_velocityDelta).arg(minVelocity).arg(maxVelocity));
    }

    return NO_FOLLOW;
}

void
MatrixVelocity::handleMouseRelease(const MatrixMouseEvent *e)
{
    m_isFinished = true;

    if (!e || !m_currentElement || !m_currentViewSegment) {
        m_mouseStartY = 0.0;
        // Mouse position is again related to pitch
        m_widget->showHighlight(true);
        return;
    }

    EventSelection *selection = m_scene->getSelection();
    if (selection) selection = new EventSelection(*selection);
    else selection = new EventSelection(m_currentViewSegment->getSegment());

    if (selection->getAddedEvents() == 0 || m_velocityDelta == 0) {
        delete selection;
        // Mouse position is again related to pitch
        m_widget->showHighlight(true);
        return;
    } else {
        QString commandLabel = tr("Change Velocity");

        if (selection->getAddedEvents() > 1) {
            commandLabel = tr("Change Velocities");
        }

        m_scene->setSelection(nullptr, false);

        CommandHistory::getInstance()->addCommand
            (new ChangeVelocityCommand(m_velocityDelta, *selection, false));

        m_scene->setSelection(selection, false);
    }

    // Reset the start of mousemove
    m_start = false;
    m_velocityDelta = 0;
    m_mouseStartY = 0.0;
    m_currentElement = nullptr;
    m_event = nullptr;

    MatrixMouseEvent ePrime(*e);
    m_scene->setMouseEventElement(ePrime);  // might have changed
    setContextHelpForPos(&ePrime);

    // Mouse position is again related to pitch
    m_widget->showHighlight(true);
}

void
MatrixVelocity::readyAtPos(const MatrixMouseEvent *event)
{
    setCursor();
    m_start = false;
    m_isFinished = true;
    if (!overlaySelectOrSegment(event)) setContextHelpForPos(event);
}

void
MatrixVelocity::setCursor()
{
    if (m_widget) {
        if (m_cursor) m_widget->setCanvasCursor(m_cursor);
        else          m_widget->setCanvasCursor(Qt::SizeVerCursor);
    }
}
void
MatrixVelocity::setSelectCursor()
{
    if (m_widget) {
        if (m_selectCursor) m_widget->setCanvasCursor(m_selectCursor);
        else                m_widget->setCanvasCursor(Qt::SizeVerCursor);
    }
}

void
MatrixVelocity::stow()
{
    m_start = false;
}

QString
MatrixVelocity::altToolHelpString()
const
{
    return altToolHelpStringTools(tr("velocity"), tr("resize"));
}

void MatrixVelocity::setContextHelpForPos(const MatrixMouseEvent *e)
{
    if (!e) {
        setContextHelp(tr("Velocity tool: Move mouse over note or empty "
                          "area for help."));
        return;
    }

    moveResizeVelocityContextHelp(e,
                                  tr("adjust velocity of"),
                                  tr("adjust velocity of"),
                                  "");
}

QString MatrixVelocity::ToolName() { return "velocity"; }

}
