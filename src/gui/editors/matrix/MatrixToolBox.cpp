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

#define RG_MODULE_STRING "[MatrixToolBox]"

#include "MatrixToolBox.h"
#include "MatrixTool.h"
#include "MatrixWidget.h"
#include "MatrixPainter.h"
#include "MatrixElement.h"
#include "MatrixEraser.h"
#include "MatrixMouseEvent.h"
#include "MatrixMover.h"
#include "MatrixMultiTool.h"
#include "MatrixResizer.h"
#include "MatrixSelect.h"
#include "MatrixSegment.h"
#include "MatrixScene.h"
#include "MatrixVelocity.h"

#include "base/Selection.h"
#include "misc/Debug.h"

#include <QString>
#include <QMessageBox>



namespace Rosegarden
{

MatrixToolBox::MatrixToolBox(MatrixWidget *parent) :
    BaseToolBox(parent),
    m_widget            (parent),
    m_scene             (nullptr),
    m_toolWhenClicked   (nullptr),
    m_capsLockIsOn      (false),
    m_capsLockNewlyOn   (false),
    m_controlIsPressed  (false),
    m_hadDoubleClick    (false),
    m_pendingKeyPress   (0),   // non-existent Qt::Key_None constant
    m_pendingKeyRelease (0),   //  " -   "     " ::   "        "
    m_currentTool       (nullptr),
    m_segmentWhenClicked(nullptr)
{
    // Check and disdplay warning. If is on at start of matrix editor,
    // checkX11CapsLock() called by MatrixScene::focusInEvent()
    // will handle.
    warnX11CapsLockIsOn();
}

MatrixToolBox::~MatrixToolBox()
{
}

BaseTool *
MatrixToolBox::createTool(QString toolName)
{
    MatrixTool *tool = nullptr;

    QString toolNamelc = toolName.toLower();

    if (toolNamelc == MatrixPainter::ToolName())
        tool = new MatrixPainter(m_widget, this);
    else if (toolNamelc == MatrixEraser::ToolName())
        tool = new MatrixEraser(m_widget, this);
    else if (toolNamelc == MatrixMultiTool::ToolName())
        tool = new MatrixMultiTool(m_widget, this);
    else if (toolNamelc == MatrixMover::ToolName())
        tool = new MatrixMover(m_widget, this);
    else if (toolNamelc == MatrixResizer::ToolName())
        tool = new MatrixResizer(m_widget, this);
    else if (toolNamelc == MatrixVelocity::ToolName())
        tool = new MatrixVelocity(m_widget, this);
    else if (toolNamelc == MatrixSegment::ToolName())
        tool = new MatrixSegment(m_widget, this);
    else if (toolNamelc == MatrixSelect::ToolName())
        tool = new MatrixSelect(m_widget, this);
    else {
        QMessageBox::critical(nullptr, tr("Rosegarden"), QString("MatrixToolBox::createTool : unrecognised toolname %1 (%2)")
                           .arg(toolName).arg(toolNamelc));
        return nullptr;
    }

    m_tools.insert(toolName, tool);

    // Subclass handleEventRemoved() methods do little or nothing,
    // and probably can't be called while tool in use (single-threaded
    // code) which is only valid reason for having them. Not removing
    // out of surplus of caution.
    if (m_scene) {
        tool->setScene(m_scene);
        connect(m_scene, &MatrixScene::eventRemoved,
                tool, &MatrixTool::handleEventRemoved);
    }

#if 0   // t4os -- causes harmless but annoying recursion in setTool()
    if (!m_currentTool) setTool(toolName);
#endif

    return tool;
}

void
MatrixToolBox::setScene(MatrixScene *scene)
{
    m_scene = scene;

    // See comment about handleEventRemoved() methods in createTool(), above.
    for (QHash<QString, BaseTool *>::iterator i = m_tools.begin();
         i != m_tools.end(); ++i) {
        MatrixTool *nt = dynamic_cast<MatrixTool *>(*i);
        if (nt) {
            nt->setScene(scene);
            connect(scene, &MatrixScene::eventRemoved,
                    nt, &MatrixTool::handleEventRemoved);
        }
    }
}

// State of CapsLock toggle or Control key pressed/released might have
// changed between when mouse focus left matrix editor window and
// re-entered. MatrixScene::focusInEvent() calls this whe latter happens.
// Possible race condition between X11/Qt keyboard handling and window
// manager focus among windows (Qt and/or non-Qt)? We do the best we
// can within the OS environment and libraries available to us.
void
MatrixToolBox::checkKeysStateOnEntry() {
    MatrixMouseEvent mme;
    m_scene->setupMouseEvent(mme);

    bool capsLock = getX11CapsLock();
    bool control  = mme.modifiers & Qt::ControlModifier;

    if (capsLock == m_capsLockIsOn && control == m_controlIsPressed)
        return;

    if (control) handleKeyPress  (&mme, Qt::Key_Control);
    else         handleKeyRelease(&mme, Qt::Key_Control);

    if (capsLock) handleKeyPress  (&mme, Qt::Key_CapsLock);
    else          handleKeyRelease(&mme, Qt::Key_CapsLock);

}

void
MatrixToolBox::updateToolsMenus(const Segment* /*segment*/)
{
#if 0   // t4os
    // Doesn't work because MatrixTool::updateMenu() changing QLabel
    // for QAction in QWidgetAction doesn't work
    for (auto baseTool : m_tools) {
        MatrixTool *matrixTool = dynamic_cast<MatrixTool*>(baseTool);
        if (matrixTool) matrixTool->updateMenu(segment);
    }
#else
    for (auto baseTool : m_tools) {
        MatrixTool *matrixTool = dynamic_cast<MatrixTool*>(baseTool);
        if (matrixTool && matrixTool->hasMenu())
            matrixTool->createMenu();
    }
#endif
}

void
MatrixToolBox::setTool(QString name)
{
    MatrixTool *tool = dynamic_cast<MatrixTool *>(getTool(name));
    if (!tool) return;

    // Clear all dispatch tools indisciminantly.
    for (auto   it  = m_dispatchTools.rbegin() ;
                it != m_dispatchTools.rend() ;
              ++it)
        (*it)->stow();
    m_dispatchTools.clear();

    if (m_currentTool)
        m_currentTool->stow();

    m_currentTool = tool;
    m_currentTool->ready();

    if (m_capsLockIsOn) {
        MatrixMouseEvent mme;
        m_scene->setupMouseEvent(mme);
        m_currentTool->handleKeyPress(&mme, Qt::Key_CapsLock);
    }

    m_widget->setTool(name);  // for call to Controls Widget slotSetTool()
}

void
MatrixToolBox::setDrawTool()
{
    setTool(MatrixPainter::ToolName());
}

void
MatrixToolBox::setEraseTool()
{
    setTool(MatrixEraser::ToolName());
}

void
MatrixToolBox::setMultiTool()
{
    setTool(MatrixMultiTool::ToolName());
}

void
MatrixToolBox::setMoveTool()
{
    setTool(MatrixMover::ToolName());
}

void
MatrixToolBox::setResizeTool()
{
    setTool(MatrixResizer::ToolName());
}

void
MatrixToolBox::setVelocityTool()
{
    setTool(MatrixVelocity::ToolName());
}

MatrixTool*
MatrixToolBox::getActiveTool()
const
{
#if 0   // Clearer ...
    if (m_dispatchTools.empty()) return m_currentTool;
    return *(m_dispatchTools.end() - 1);
#else  // ... but this slightly more efficient?
    auto end = m_dispatchTools.end();
    if (end == m_dispatchTools.begin()) return m_currentTool;
    return *(end - 1);
#endif
}

MatrixTool*
MatrixToolBox::getPenultimateTool()
const
{
    if (m_dispatchTools.size() < 2) return m_currentTool;
    return *(m_dispatchTools.end() - 2);
}

void
MatrixToolBox::appendDispatchTool(const QString &name,
                                 const MatrixMouseEvent *event)
{
    MatrixTool *tool = dynamic_cast<MatrixTool*>(getTool(name));
    if (!tool) return;  // shouldn't ever happen

    appendDispatchTool(tool, event);
}

void
MatrixToolBox::appendDispatchTool(MatrixTool *tool,
                                  const MatrixMouseEvent *event)
{
    MatrixTool *last = getActiveTool();
    if (last) last->stow();  // should always be non-null

    m_dispatchTools.push_back(tool);
    tool->readyAtPos(event);
}

void
MatrixToolBox::popDispatchTool()
{
    if (!m_dispatchTools.empty()) {
        getActiveTool()->stow();
        m_dispatchTools.pop_back();
    }

#if 0   // t4os
        // Can't do this here.
        // Causes infinite loop when alt-keypress in MultiTool dispatched
        //   to e.g. Select
        // Select::handleKeypress() calls this to remove self, but then
        //   this calls MultiTool::readyAtPos() which reinstalls
        //   Select via appendDispatchTool() before Select::handleKeypress()
        //   gets around to calling MultiTool::handleKeypress().
        // And then Select::handleKeypress() calls handleKeyPress() on
        //   getActiveTool() thinking that's what MultiTool::handleKeyPres()
        //   installed, but is really Select from MultiTool::readyAtPos()
        //   and is infinite loop.
    getActiveTool()->readyAtPos(e);
#endif
}

void
MatrixToolBox::handleButtonPress(const MatrixMouseEvent *e)
{
    // Call correct subclass method depending on which mouse button.

    void (MatrixTool::*buttonPress)(const MatrixMouseEvent*);

    if ((e->buttons & Qt::LeftButton) && (e->buttons & Qt::RightButton))
        buttonPress = &MatrixTool::handleMidButtonPress;
    else if (e->buttons & Qt::LeftButton)
        buttonPress = &MatrixTool::handleLeftButtonPress;
    else if (e->buttons & Qt::MiddleButton)
        buttonPress = &MatrixTool::handleMidButtonPress;
    else if (e->buttons & Qt::RightButton)
        buttonPress = &MatrixTool::handleRightButtonPress;
    else return;

    m_toolWhenClicked = getActiveTool();
    m_segmentWhenClicked = m_scene->getCurrentSegment();

    (getActiveTool()->*buttonPress)(e);
}

void
MatrixToolBox::handleMouseDoubleClick(const MatrixMouseEvent *e)
{
    m_hadDoubleClick = true;

    // Always delete note in all tools

    if (e->buttons != Qt::LeftButton)  // not pseudo-middle via left+right
        return;

     m_toolWhenClicked->handleMouseDoubleClick(e);
}

FollowMode
MatrixToolBox::handleMouseMove(const MatrixMouseEvent *e)
{
    return getActiveTool()->handleMouseMove(e);
}

void
MatrixToolBox::handleMouseRelease(const MatrixMouseEvent *e)
{
    if (m_hadDoubleClick) {
        m_hadDoubleClick = false;
        // MatrixTool::handleMouseDoubleClick() had set m_isFinished to false.
        m_toolWhenClicked->setIsFinished(true);
        getActiveTool()->readyAtPos(e);
        return;
    }

    MatrixTool *tool = getActiveTool();
    tool->handleMouseRelease(e);

    // Tool (dispatch or main) might have run Command which deleted
    // MatrixElement and/or its associated (and back-pointered) QGraphicsItem,
    // e.g. when MatrixMover stops moving temporary notes and creates
    // new permanent ones. MatrixMouseEvent coordinates, buttons, etc.
    // remain unchanged, but MatrixMouseEvent::element might have changed.
    // In particular, checkDispatchToolFinished() can call derived class
    // readyAtPos(MatrixMouseEvent*) (not dispatch tool, but would have
    // same issue) which needs good MatrixMouseEvent::element.
    MatrixMouseEvent ePrime(*e);
    m_scene->setMouseEventElement(ePrime);

    while (tool->role() == MatrixTool::Role::OVERLAY ||
           tool->role() == MatrixTool::Role::SINGLE_ACTION) {
        popDispatchTool();
        tool = getActiveTool();
        if (m_dispatchTools.empty()) break;
    }

    // Not really necessary, but keep coherent in case did
    // overlay to another tool, and then single action back to
    // original one.
    m_currentTool->setRole(MatrixTool::Role::FUNDAMENTAL);

    checkPendingKeyPressRelease(tool, &ePrime);

    // Either/both might have changed
    tool = getActiveTool();
    m_scene->setMouseEventElement(ePrime);

    tool->readyAtPos(&ePrime);
}

void
MatrixToolBox::handleKeyPress(const MatrixMouseEvent *e,
                              const int key)
{
    if (key == Qt::Key_CapsLock) {
        if (m_capsLockIsOn) {
            // Unset if called twice, e.g. once at focusInEvent()
            // and then again to turn off once in window.
            // Other edge cases will probably require punting and
            // calling potentially expensive getX11CapsLock()
            m_capsLockNewlyOn = false;
            return;  // Will handle when caps lock key release
        }
        else if (m_pendingKeyRelease) {
            // Got release and press while busy -- ignore both
            m_pendingKeyPress   = 0;
            m_pendingKeyRelease = 0;
            return;
        }
        else {
            // first press/release toggling capslock on
            m_capsLockIsOn    = true;
            m_capsLockNewlyOn = true;
        }
    }

    MatrixTool *tool = getActiveTool();
    if (tool->handleKeyPress(e, key)) {
        tool = getActiveTool();
        tool->readyAtPos(e);
    }
    else
        m_pendingKeyPress = key;
}

void
MatrixToolBox::handleKeyRelease(const MatrixMouseEvent *e,
                                const int key)
{
    MatrixTool *tool = getActiveTool();

    if (key == Qt::Key_Control) {
        // Only MatrixPainter uses this, all others ignore.
        // Even though is control key release, e->modifiers has
        //   Qt::ControlModifier set.
        // So have to clear it for MatrixPainter::readyAtPos()
        //   not to think Control is on and re-append Select tool.
        if (e) {
            MatrixMouseEvent mme(*e);
            mme.modifiers &= ~Qt::ControlModifier;
            tool->handleKeyRelease(&mme, key);
        }
        else {
            // Tool will only look at Qt::ControlModifier
            MatrixMouseEvent mme;
            tool->handleKeyRelease(&mme, key);
        }
        return;
    }

    if (key == Qt::Key_CapsLock) {
        if (m_capsLockNewlyOn) {
            // Is press/release toggling capslock on.
            m_capsLockNewlyOn = false;
            return;
        }
        else if (m_capsLockIsOn)
            m_capsLockIsOn = false;
        else
            return;  // Will handle on caps lock key press
    }

    if (m_pendingKeyPress) {
        // Got press and release while busy -- ignore both
        m_pendingKeyPress   = 0;
        m_pendingKeyRelease = 0;
        return;
    }

    if (tool->handleKeyRelease(e, key))
        clearDispatchTools(e);
    else
        m_pendingKeyRelease = key;
}

void
MatrixToolBox::checkPendingKeyPressRelease(MatrixTool *tool,
                                           const MatrixMouseEvent *event)
{
    bool handled = false;

    // Can't be both
    if (m_pendingKeyPress) {
        handled = tool->handleKeyPress(event, m_pendingKeyPress);
        m_pendingKeyPress = 0;
    }
    else if (m_pendingKeyRelease) {
        handled = tool->handleKeyRelease(event, m_pendingKeyRelease);
        m_pendingKeyRelease = 0;
    }

    MatrixMouseEvent updated(*event);
    m_scene->setupMouseEvent(updated);

    if (handled) clearDispatchTools(&updated);

    getActiveTool()->readyAtPos(&updated);
}

void
MatrixToolBox::clearDispatchTools(const MatrixMouseEvent *e)
{
    // Clear entire stack
    while (!m_dispatchTools.empty())
        popDispatchTool();
    m_currentTool->setRole(MatrixTool::Role::FUNDAMENTAL);
    m_currentTool->readyAtPos(e);
}

void
MatrixToolBox::warnX11CapsLockIsOn()
{
    static bool dontShow = false;

    if (dontShow) return;

    if (!dontShow && getX11CapsLock()) {
        QMessageBox messageBox;
        QCheckBox *checkBox = new QCheckBox(tr("Don't show this warning"));

        QString message = tr("Warning:\n\n    CapsLock is on\n\n"
                             "Use CapsLock to switch matrix editor tools\n"
                             "to alternate mode.\n\n"
                             "    Tool\t\tAlternate\n"
                             "    -------\t\t---------------\n"
                             "    Multi\t\tDraw\n"
                             "    Draw\t\tMulti\n"
                             "    Erase\t\tDraw\n"
                             "    Move\t\tResize\n"
                             "    Resize\t\tMove\n"
                             "    Velocity\tResize\n");

        messageBox.setText(message);
        messageBox.addButton(QMessageBox::Ok);
        messageBox.setCheckBox(checkBox);

        QObject::connect(checkBox,
                         &QCheckBox::stateChanged,
                         [](int state) {
                            if (static_cast<Qt::CheckState>(state) ==
                                Qt::CheckState::Checked) dontShow = true;
                         });

        messageBox.exec();
    }
}


// The following must be here at the end of the file becasuse
// some X11 include file(s) #define symbols which conflict
// with e.g. Qt::CursorShape

#include <X11/XKBlib.h>

bool
MatrixToolBox::getX11CapsLock()
const
{
    static const unsigned CAPS_LOCK = 0x1;

#if 0   // Requires linking with not-default-installed libQt5X11Extras
    Display *display = QX11Info::display();
#else   // Assume on local X11 server
    Display *display = XOpenDisplay(":0");
#endif

    unsigned     leds;

    XkbGetIndicatorState(display, XkbUseCoreKbd, &leds);

    return static_cast<bool>(leds & CAPS_LOCK);
}

}  // namespace Rosegarden
