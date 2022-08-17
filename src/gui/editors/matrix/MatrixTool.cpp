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

#define RG_MODULE_STRING "[MatrixTool]"
#define NDEBUG 1

#include "MatrixTool.h"

#include "MatrixEraser.h"
#include "MatrixMover.h"
#include "MatrixMultiTool.h"
#include "MatrixPainter.h"
#include "MatrixResizer.h"
#include "MatrixSegment.h"
#include "MatrixSelect.h"
#include "MatrixVelocity.h"

#include "MatrixElement.h"
#include "MatrixMouseEvent.h"
#include "MatrixToolBox.h"
#include "MatrixViewSegment.h"

#include "base/BaseProperties.h"
#include "base/NotationTypes.h"

#include "commands/edit/EventEditCommand.h"
#include "commands/edit/EraseCommand.h"
#include "commands/matrix/MatrixEraseCommand.h"

#include "document/CommandHistory.h"

#include "gui/application/RosegardenMainWindow.h"
#include "gui/dialogs/EventEditDialog.h"
#include "gui/dialogs/SimpleEventEditDialog.h"

#include "misc/Debug.h"
#include "misc/Strings.h"

#include "gui/general/IconLoader.h"
// #include "gui/general/PixmapFunctions.h"

#include "MatrixWidget.h"
#include "MatrixScene.h"

#include <base/Composition.h>
#include <base/Segment.h>

#include <document/RosegardenDocument.h>

#include <QAction>
#include <QLabel>
#include <QMenu>
#include <QString>
#include <QWidgetAction>


namespace Rosegarden
{

// See comment in .h file.
const int MatrixTool::ALTERNATE_TOOL_QT_KEY = Qt::Key_CapsLock;

MatrixTool::MatrixTool(
QString menuName,
MatrixWidget *widget,
MatrixToolBox *toolbox) :
    BaseTool(menuName, widget),
    m_widget(widget),
    m_toolbox(toolbox),
    m_scene(nullptr),
    m_undoAction(nullptr),
    m_redoAction(nullptr),
    m_role(Role::FUNDAMENTAL),
    m_isFinished(true)
{
    connect(m_widget,
            &MatrixWidget::segmentDeleted,
            this,
            &MatrixTool::slotSegmentDeleted);
}

MatrixTool::~MatrixTool()
{
    // We don't need (or want) to delete the menu; it's owned by the
    // ActionFileMenuWrapper parented to our QObject base class
    MATRIX_DEBUG << "MatrixTool::~MatrixTool()";
}


bool MatrixTool::overlaySelectOrSegment(const MatrixMouseEvent *e)
{
    if (!e) return false;

    if (e->element) {
        if (e->element->getSegment() != m_scene->getCurrentSegment()) {
            m_toolbox->appendDispatchTool(MatrixSegment::ToolName(), e);
            return true;
        }
    }
    else {
        m_toolbox->appendDispatchTool(MatrixSelect::ToolName(), e);
        return true;
    }

    return false;
}

QCursor*
MatrixTool::makeCursor(QString pixmapName, int hotSpotX, int hotSpotY)
{
    QPixmap pixmap = IconLoader::loadPixmap(pixmapName);
    if (!pixmap.isNull()) return new QCursor(pixmap, hotSpotX, hotSpotY);
    RG_WARNING << "makeCursor(): failed to loadPixmap"
               << pixmapName;
    return nullptr;
}

void MatrixTool::setCursor()
{
}

void
MatrixTool::handleLeftButtonPress(const MatrixMouseEvent *)
{
    m_isFinished = false;
}

void
MatrixTool::handleMidButtonPress(const MatrixMouseEvent *)
{
}

void
MatrixTool::handleRightButtonPress(const MatrixMouseEvent *e)
{
    MatrixElement *element = e->element;
    if (element && element->getSegment() == m_scene->getCurrentSegment()) {
        if (element->event()->isa(Note::EventType) &&
            element->event()->has(BaseProperties::TRIGGER_SEGMENT_ID)) {
#if 0  // SIGNAL_SLOT_ABUSE
            emit editTriggerSegment(element->
                                    event()->
                                    get<Int>(BaseProperties::
                                              TRIGGER_SEGMENT_ID));
#else
            RosegardenMainWindow::
            self()->
            getView()->
            slotEditTriggerSegment(element->
                                    event()->
                                    get<Int>(BaseProperties::
                                              TRIGGER_SEGMENT_ID));
#endif
            return;
        }

        MatrixViewSegment *vs = e->viewSegment;
        if (!vs) return;

        if (e->modifiers & Qt::ShiftModifier) { // advanced edit
            EventEditDialog dialog(m_widget, *element->event(), true);

            if (dialog.exec() == QDialog::Accepted && dialog.isModified()) {
                EventEditCommand *command =
                    new EventEditCommand(vs->getSegment(),
                                         element->event(),
                                         dialog.getEvent());
                CommandHistory::getInstance()->addCommand(command);
            }
        }
        else {
            SimpleEventEditDialog dialog(m_widget,
                                         RosegardenDocument::currentDocument,
                                         *element->event(),
                                         false);
            if (dialog.exec() == QDialog::Accepted && dialog.isModified()) {
                EventEditCommand *command =
                    new EventEditCommand(vs->getSegment(),
                                         element->event(),
                                         dialog.getEvent());
                CommandHistory::getInstance()->addCommand(command);
            }
        }
    }
    else {
        MatrixTool *tool = m_toolbox->getCurrentTool();
        tool->updateMenu();
        tool->showMenu();
    }
}

void
MatrixTool::handleMouseDoubleClick(const MatrixMouseEvent *e)
{
    eraseNotes(e);

    setContextHelp(tr("Note(s) deleted. Release mouse button."));

    // Will be truly finished when get mouse button release
    m_isFinished = false;
}

FollowMode
MatrixTool::handleMouseMove(const MatrixMouseEvent *e)
{
    if (!e || overlaySelectOrSegment(e))
        return NO_FOLLOW;

    setContextHelpForPos(e);
    return NO_FOLLOW;
}

void
MatrixTool::handleMouseRelease(const MatrixMouseEvent *)
{
    m_isFinished = true;
}

bool
MatrixTool::handleKeyRelease(const MatrixMouseEvent *e, const int key)
{
    switch (key) {
        case ALTERNATE_TOOL_QT_KEY:
            if (m_isFinished) {
                m_toolbox->popDispatchTool();
                return true;
            }
            else
                return false;

        case Qt::Key_Control:
            {
                MatrixTool *tool = m_toolbox->getCurrentTool();
                if (dynamic_cast<MatrixPainter*>(tool))
                    return tool->handleKeyRelease(e, key);
            }
            break;

        default:
            break;
    }

    return true;
}

void
MatrixTool::handleEventRemoved(Event*)
{
}

void
MatrixTool::ready()
{
    MatrixMouseEvent  matrixMouseEvent;
    MatrixMouseEvent *eventOrNull;

    if (m_scene->setupMouseEvent(matrixMouseEvent))
        eventOrNull = &matrixMouseEvent;
    else
        eventOrNull = nullptr;

    readyAtPos(eventOrNull);

#if 0   // No, this varies among tools
    m_isFinished = false;
#endif
}

void
MatrixTool::readyAtPos(const MatrixMouseEvent*)
{
    m_isFinished = true;
}

void
MatrixTool::stow()
{
#if 0   // Don't do this here, in case class not overriding is calling by
        // self to temporarilly switch to another tool
    m_isFinished = true;
#endif
}

void
MatrixTool::setContextHelpForPos(const MatrixMouseEvent*)
{
}

MatrixTool *MatrixTool::getTool(
const QString &toolName)
{
    return dynamic_cast<MatrixTool*>(m_toolbox->getTool(toolName));
}

void
MatrixTool::slotMultiToolSelected()
{
    invokeInParentView("multitool");
}

void
MatrixTool::slotMoveSelected()
{
    invokeInParentView("move");
}

void
MatrixTool::slotEraseSelected()
{
    invokeInParentView("erase");
}

void
MatrixTool::slotResizeSelected()
{
    invokeInParentView("resize");
}

void
MatrixTool::slotDrawSelected()
{
    invokeInParentView("draw");
}

void
MatrixTool::slotVelocityChangeSelected()
{
    invokeInParentView("velocity");
}

// Only because MatrixMultiTool doesn't implement (because never used).
void MatrixTool::setSelectCursor()
{
}

const SnapGrid *
MatrixTool::getSnapGrid() const
{
    if (!m_scene) return nullptr;
    return m_scene->getSnapGrid();
}

void
MatrixTool::invokeInParentView(QString actionName)
{
    QAction *a = findActionInParentView(actionName);
    if (!a) {
        RG_WARNING << "MatrixTool::invokeInParentView: No action \"" << actionName
                  << "\" found in parent view";
    } else {
        a->trigger();
    }
}

QAction *
MatrixTool::findActionInParentView(QString actionName)
{
    if (!m_widget) return nullptr;
    QWidget *w = m_widget;
    ActionFileClient *c = nullptr;
    while (w && !(c = dynamic_cast<ActionFileClient *>(w))) {
        w = w->parentWidget();
    }
    if (!c) {
        RG_WARNING << "MatrixTool::findActionInParentView: Can't find ActionFileClient in parent widget hierarchy";
        return nullptr;
    }
    QAction *a = c->findAction(actionName);
    return a;
}

MatrixTool::ContextInfo::ContextInfo(
const MatrixTool       *tool,
const MatrixMouseEvent *event)
:   haveSelection(false),
    inSelection(false),
    selectionSize(0)
{
    if (!event) return;

    if (tool->m_scene->getSelection() &&
        (selectionSize = tool->m_scene->getSelection()->getAddedEvents()) > 0) {
        haveSelection = true;
        if (event->element &&
            event->element->event() &&
            tool->m_scene->getSelection() &&
            tool->m_scene->getSelection()->contains(event->element->event()))
            inSelection = true;
    }
}

void
MatrixTool::createMenu()
{
    if (!m_menu) {
        if (toolName() != MatrixMultiTool::ToolName())
            createAction("multitool", SLOT(slotMultiToolSelected()));
        if (toolName() != MatrixPainter::ToolName())
            createAction("draw", SLOT(slotDrawSelected()));
        if (toolName() != MatrixEraser::ToolName())
            createAction("erase", SLOT(slotEraseSelected()));
        if (toolName() != MatrixMover::ToolName())
            createAction("move", SLOT(slotMoveSelected()));
        if (toolName() != MatrixResizer::ToolName())
            createAction("resize", SLOT(slotResizeSelected()));
        if (toolName() != MatrixVelocity::ToolName())
            createAction("velocity", SLOT(slotVelocityChangeSelected()));

        m_undoAction = createAction("undo", SLOT(slotUndo()));
        m_redoAction = createAction("redo", SLOT(slotRedo()));

        if (!createMenusAndToolbars("matrixtool.rc")) return;
        if (!(m_menu = findMenu("MatrixTool"))) return;
    }

    for (auto segmentAction : m_segmentActions) {
        m_menu->removeAction(segmentAction.second);
        delete segmentAction.second;
    }

    for (const Segment *segment : m_widget->getScene()->getSegments()) {
        QLabel *label = generateMenuLabel(segment);
        QWidgetAction *action = new QWidgetAction(this);
        action->setDefaultWidget(label);

        m_menu->addAction(action);
        connect(action,
                &QAction::triggered,
                this,
                [this, segment]{changeActiveSegment(segment);});

        m_segmentActions[segment] = action;
    }

#if 0   // SIGNAL_SLOT abuse: Called explicitly from handleRightButtonPress()
    connect(menu, &QMenu::aboutToShow, this, &MatrixTool::updateMenu);
#endif
}

void
MatrixTool::changeActiveSegment(const Segment *segment)
{
    m_widget->getScene()->setCurrentSegment(segment); // MW::UTCS should do
    m_widget->clearSelection();
    m_widget->updateToCurrentSegment(true, segment);  // true == set instrument
    m_widget->generatePitchRuler(); //in case normal->drum or vice-versa

    MatrixMouseEvent matrixMouseEvent;
    if (m_scene->setupMouseEvent(matrixMouseEvent))
        m_toolbox->getActiveTool()->setContextHelpForPos(&matrixMouseEvent);
    else
        m_toolbox->getActiveTool()->setContextHelpForPos(nullptr);
}

bool
MatrixTool::modifySelection(const MatrixMouseEvent *mouseEvent)
{
    if (!mouseEvent || !mouseEvent->element) return false;

    if (!mouseEvent->viewSegment) return false;

    MatrixElement *element = mouseEvent->element;

    EventSelection *existingSelection = m_scene->getSelection();
    if (!existingSelection) {
        m_scene->setSingleSelectedEvent(mouseEvent->viewSegment, element, true);
        return true;
    }

//  MatrixViewSegment *viewSegment = mouseEvent->viewSegment;
//  if (!viewSegment) return false;

    Event *event = element->event();
    if (!event) return false;

    if (existingSelection->contains(event)) {
        if (mouseEvent->modifiers & Qt::ShiftModifier) {
            existingSelection->removeEvent(event);
            return false;
        }
        else return true;
    }

    if (mouseEvent->modifiers & Qt::ShiftModifier) {
        EventSelection *modifiedSelection = new
                                            EventSelection(*existingSelection);
        modifiedSelection->addEvent(event);
        m_scene->setSelection(modifiedSelection, true);
    }
    else
        m_scene->setSingleSelectedEvent(mouseEvent->viewSegment, element, true);

    return true;
}

void
MatrixTool::eraseNotes(const MatrixMouseEvent *e)
const
{
    MatrixElement *element = e->element;
    if (!element || element->getSegment() != m_scene->getCurrentSegment())
        return;

    Event *event = element->event();
    if (!event || event->getType() != Note::EventType)
        return;

    EventSelection *selection = m_scene->getSelection();

    if (selection && selection->contains(event)) {
        EraseCommand *command = new EraseCommand(selection);
        CommandHistory::getInstance()->addCommand(command);
    }
    else {
        MatrixViewSegment *viewSegment = e->viewSegment;
        if (!viewSegment) return;

        Segment &segment(viewSegment->getSegment());

        MatrixEraseCommand *command = new MatrixEraseCommand(segment, event);
        CommandHistory::getInstance()->addCommand(command);
    }
}

QString
MatrixTool::altToolHelpString()
const
{
    return altToolHelpStringTools("", "");
}

QString
MatrixTool::altToolHelpStringTools(
const QString &thisTool,
const QString &altTool)
const
{
    if (m_toolbox->capsLockIsOn())
        return tr(", or middle button or  turn off CapsLock "
                  "for %1 tool.").arg(thisTool);
    else
        return tr(", or middle button or turn on CapsLock "
                  "for %1 tool.").arg(altTool);
    return "";
}

void
MatrixTool::moveResizeVelocityContextHelp(const MatrixMouseEvent *event,
                                          const QString &clickText,
                                          const QString &dragText,
                                          const QString &snapText)
{
    ContextInfo contextInfo(this, event);

    if (event->buttons != Qt::NoButton) {
        setContextHelp(tr("Drag to %1").arg(dragText) +
                          (  contextInfo.selectionSize > 1
                           ? tr(" selected notes")
                           : tr(" note")) +
                       snapText +
                       tr(", release mouse button to finish"));
        return;
    }

    QString help;

    if (event->element) {
        help = tr("Click and drag to %1").arg(clickText);
        if (contextInfo.haveSelection) {
            if (contextInfo.inSelection) {
                if (contextInfo.selectionSize > 1)
                    help += tr(" selected notes");
                else
                    help += tr(" note");

                help += tr(" or shift-click to remove "
                           "note from selection");
            }
            else
                help = tr("Click to select note (or shift-click "
                          "to add to selection), then drag ") +
                        clickText;
        }
        else
            help = tr("Click and drag to %1 note").arg(clickText);

        help += tr(", or double-click to delete") +
                (contextInfo.inSelection ? tr(" selected notes")
                                         : tr(" note")) +
                tr(", or right-click (optional add Shift) to "
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

void
MatrixTool::slotSegmentDeleted(const Segment *segment)
{
    m_menu->removeAction(m_segmentActions[segment]);
    m_segmentActions.erase(segment);
}

void
MatrixTool::slotUndo()
{
    CommandHistory::getInstance()->undo();
}

void
MatrixTool::slotRedo()
{
    CommandHistory::getInstance()->redo();
}

void
MatrixTool::updateMenuSegmentList(const Segment *segment)
{
    QWidgetAction *action = m_segmentActions[segment];
    QWidget *widget = action->defaultWidget();

    action->releaseWidget(widget);
    action->setDefaultWidget(generateMenuLabel(segment));
}

void
MatrixTool::updateMenu()
{
    m_undoAction->setEnabled(CommandHistory::getInstance()->undoStackSize());
    m_redoAction->setEnabled(CommandHistory::getInstance()->redoStackSize());
}

QLabel
*MatrixTool::generateMenuLabel(const Segment *segment)
{
    QString segmentName(QString::fromUtf8(segment->getLabel().data(),
                                          segment->getLabel().size()));
    QColor segmentColor = m_widget->getDocument()->getComposition().
                          getSegmentColourMap().
                          getColour(segment->getColourIndex());
    QColor textColor = segment->getPreviewColour();
    QColor hoverColor = (qGray(segmentColor.rgb()) < 144 ? Qt::white
                                                         : Qt::black);

    QLabel *label = new QLabel(segmentName, m_menu);
    QString style(QString("QLabel {"
                    "background-color: rgb(%1, %2, %3);"
                    "color: rgb(%4, %5, %6);"
                  "}"
                  "QLabel::hover {"
                    "background-color: rgb(%7, %8, %9);"
                    "color: rgb(%10, %11, %12);"
                  "}").arg(segmentColor.red())
                      .arg(segmentColor.green())
                      .arg(segmentColor.blue())
                      .arg(textColor.red())
                      .arg(textColor.green())
                      .arg(textColor.blue())
                      .arg(hoverColor.red())
                      .arg(hoverColor.green())
                      .arg(hoverColor.blue())
                      .arg(segmentColor.red())
                      .arg(segmentColor.green())
                      .arg(segmentColor.blue()));
    label->setStyleSheet(style);

    return label;
}

}
