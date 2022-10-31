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

#define RG_MODULE_STRING "[EditViewBase]"
#define RG_NO_DEBUG_PRINT

#include "EditViewBase.h"

#include "document/RosegardenDocument.h"
#include "document/CommandHistory.h"
#include "gui/dialogs/ConfigureDialog.h"
#include "gui/dialogs/TimeDialog.h"
#include "base/Clipboard.h"
#include "commands/segment/SegmentReconfigureCommand.h"

#include "gui/widgets/TmpStatusMsg.h"

#include "misc/Debug.h"

#include <QTabWidget>
#include <QAction>
#include <QStatusBar>
#include <QTabWidget>
#include <QToolBar>

namespace Rosegarden
{

EditViewBase::EditViewBase(const std::vector<Segment *>& segments,
                           QWidget * /* parent */) :
    // QMainWindow(parent),   // See following comments
    QMainWindow(nullptr),
    m_segments(segments),
    m_configDialogPageIndex(0)
{
    setAttribute(Qt::WA_DeleteOnClose);
    // Address #1508:  Show the edit windows without activating them, so either
    // they or the main window can continue to have focus in Qt5.
    //
    // On my system (yg) and with parent passed to QMainWindow:
    //   - With Qt5 Qt::WA_ShowWithoutActivating has no effect: the main
    //     windows may always have focus but is always under the editors.
    //   - With Qt4 and WA_ShowWithoutActivating the editors are always
    //     opened under the main window.
    //
    // It seems that a Qt5 window is always under its child.
    // So when passing 0 as parent to QMainWindow the editors are no more child
    // of the main window and the problem is fixed.
    //
    // setAttribute(Qt::WA_ShowWithoutActivating);

    // Store so that we attach and detach from the same document.
    m_doc = RosegardenDocument::currentDocument;
    RosegardenDocument::currentDocument->attachEditView(this);

    connect(CommandHistory::getInstance(), &CommandHistory::commandExecuted,
            this, &EditViewBase::slotTestClipboard);
}

EditViewBase::~EditViewBase()
{
    // Use m_doc to make sure we detach from the same document we attached to.
    m_doc->detachEditView(this);
    slotSaveOptions();
}

Clipboard *EditViewBase::getClipboard()
{
    return Clipboard::mainClipboard();
}

void EditViewBase::slotSaveOptions()
{
}

void EditViewBase::readOptions()
{
    QAction *a = findAction("options_show_statusbar");
    if (a) a->setChecked( ! statusBar()->isHidden() );

//    a = findAction("options_show_toolbar");
//    if (a) a->setChecked( ! m_toolBar->isHidden());
}

void EditViewBase::setCheckBoxState(QString actionName, QString toolbarName)
{
  // Use !isHidden() for visibility since ancestors may not be visible
  // since this is called during the Matrixview constructor.
  bool view = !findToolbar(toolbarName)->isHidden();
  findAction(actionName)->setChecked(view);
}


void EditViewBase::setupBaseActions(bool haveClipboard)
{
    // Actions all edit views will have

//    createAction("options_show_toolbar", SLOT(slotToggleToolBar()));
    createAction("options_show_statusbar", SLOT(slotToggleStatusBar()));
    createAction("options_configure", SLOT(slotConfigure()));

    createAction("file_save", SIGNAL(saveFile()));
    createAction("file_close", SLOT(slotCloseWindow()));

    if (haveClipboard) {
        createAction("edit_cut", SLOT(slotEditCut()));
        createAction("edit_copy", SLOT(slotEditCopy()));
        createAction("edit_paste", SLOT(slotEditPaste()));
    }

    createAction("open_in_matrix", SLOT(slotOpenInMatrix()));
    createAction("open_in_percussion_matrix", SLOT(slotOpenInPercussionMatrix()));
    createAction("open_in_notation", SLOT(slotOpenInNotation()));
    createAction("open_in_event_list", SLOT(slotOpenInEventList()));
    createAction("open_in_pitch_tracker", SLOT(slotOpenInPitchTracker()));
    createAction("set_segment_start", SLOT(slotSetSegmentStartTime()));
    createAction("set_segment_duration", SLOT(slotSetSegmentDuration()));
}

void EditViewBase::slotConfigure()
{
    ConfigureDialog *configDlg =
        new ConfigureDialog(RosegardenDocument::currentDocument, this);

    configDlg->show();
}

void
EditViewBase::slotOpenInNotation()
{
    emit openInNotation(m_segments);
}

void
EditViewBase::slotOpenInMatrix()
{
    emit openInMatrix(m_segments);
}

void
EditViewBase::slotOpenInPercussionMatrix()
{
    emit openInPercussionMatrix(m_segments);
}

void
EditViewBase::slotOpenInEventList()
{
    emit openInEventList(m_segments);
}

void
EditViewBase::slotOpenInPitchTracker()
{
    emit slotOpenInPitchTracker(m_segments);
}

void EditViewBase::closeEvent(QCloseEvent* /* e */)
{
    RG_DEBUG << "EditViewBase::closeEvent()\n";
/*!!!
    if (isInCtor()) {
        RG_DEBUG << "EditViewBase::closeEvent() : is in ctor, ignoring close event\n";
        e->ignore();
    } else {
//         KMainWindow::closeEvent(e);
		close(e);
    }
*/
}

void EditViewBase::slotCloseWindow()
{
    close();
}
/*
void EditViewBase::slotToggleToolBar()
{
    TmpStatusMsg msg(tr("Toggle the toolbar..."), this);

    if (m_toolBar->isVisible())
		m_toolBar->hide();
    else
		m_toolBar->show();
}
*/
void EditViewBase::slotToggleStatusBar()
{
    TmpStatusMsg msg(tr("Toggle the statusbar..."), this);

    if (statusBar()->isVisible())
        statusBar()->hide();
    else
        statusBar()->show();
}

void EditViewBase::slotStatusMsg(const QString &text)
{
    ///////////////////////////////////////////////////////////////////
    // change status message permanently
    statusBar()->clearMessage();
    statusBar()->showMessage(text);	//, ID_STATUS_MSG);
}

void EditViewBase::slotStatusHelpMsg(const QString &text)
{
    ///////////////////////////////////////////////////////////////////
    // change status message of whole statusbar temporary (text, msec)
    statusBar()->showMessage(text, 2000);
}

void
EditViewBase::slotTestClipboard()
{
    if (getClipboard()->isEmpty()) {
        RG_DEBUG << "EditViewBase::slotTestClipboard(): empty";
        leaveActionState("have_clipboard");
        leaveActionState("have_clipboard_single_segment");
    } else {
        RG_DEBUG << "EditViewBase::slotTestClipboard(): not empty";
        enterActionState("have_clipboard");
        if (getClipboard()->isSingleSegment()) {
            enterActionState("have_clipboard_single_segment");
        } else {
            leaveActionState("have_clipboard_single_segment");
        }
    }
}

void
EditViewBase::slotToggleSolo()
{
    // Select the track for this segment.
    RosegardenDocument::currentDocument->getComposition().setSelectedTrack(
            getCurrentSegment()->getTrack());
    RosegardenDocument::currentDocument->getComposition().notifyTrackSelectionChanged(
            getCurrentSegment()->getTrack());
    // Old notification mechanism.
    emit selectTrack(getCurrentSegment()->getTrack());

    // Toggle solo on the selected track.
    // The "false" is ignored.  It was used for the checked state.
    emit toggleSolo(false);
}

void
EditViewBase::slotSetSegmentStartTime()
{
    Segment *s = getCurrentSegment();
    if (!s)
        return ;

    TimeDialog dialog(this, tr("Segment Start Time"),
                      &RosegardenDocument::currentDocument->getComposition(),
                      s->getStartTime(), false);

    if (dialog.exec() == QDialog::Accepted) {

        SegmentReconfigureCommand *command =
            new SegmentReconfigureCommand(tr("Set Segment Start Time"),
                    &RosegardenDocument::currentDocument->getComposition());

        command->addSegment
        (s, dialog.getTime(),
         s->getEndMarkerTime() - s->getStartTime() + dialog.getTime(),
         s->getTrack());

        CommandHistory::getInstance()->addCommand(command);
    }
}

void
EditViewBase::slotSetSegmentDuration()
{
    Segment *s = getCurrentSegment();
    if (!s)
        return ;

    TimeDialog dialog(this, tr("Segment Duration"),
                      &RosegardenDocument::currentDocument->getComposition(),
                      s->getStartTime(),
                      s->getEndMarkerTime() - s->getStartTime(),
                      Note(Note::Shortest).getDuration(), false);

    if (dialog.exec() == QDialog::Accepted) {

        SegmentReconfigureCommand *command =
            new SegmentReconfigureCommand(tr("Set Segment Duration"),
                    &RosegardenDocument::currentDocument->getComposition());

        command->addSegment
        (s, s->getStartTime(),
         s->getStartTime() + dialog.getTime(),
         s->getTrack());

        CommandHistory::getInstance()->addCommand(command);
    }
}

void
EditViewBase::slotCompositionStateUpdate()
{
    // update the window caption
    updateViewCaption();
}

void
EditViewBase::handleEventRemoved(Event */* event */)
{
}

QString
EditViewBase::getTitle(const QString& view)
{
    QString title;
    QString indicator = (RosegardenDocument::currentDocument->isModified() ? "*" : "");
    int nsegs = m_segments.size();
    RG_DEBUG << "getTitle segs:" << nsegs;
    if (nsegs == 1) {

        TrackId trackId = m_segments[0]->getTrack();
        Track *track =
            m_segments[0]->getComposition()->getTrackById(trackId);

        int trackPosition = -1;
        if (track)
            trackPosition = track->getPosition();

        QString segLabel = strtoqstr(m_segments[0]->getLabel());
        if (segLabel.isEmpty()) {
            segLabel = " ";
        } else {
            segLabel = QString(" \"%1\" ").arg(segLabel);
        }

        QString trkLabel = strtoqstr(track->getLabel());
        if (trkLabel.isEmpty() || trkLabel == tr("<untitled>")) {
            trkLabel = " ";
        } else {
            trkLabel = QString(" \"%1\" ").arg(trkLabel);
        }
        title = tr("%1%2 - Segment%3Track%4#%5 - %6")
            .arg(indicator)
            .arg(RosegardenDocument::currentDocument->getTitle())
            .arg(segLabel)
            .arg(trkLabel)
            .arg(trackPosition + 1)
            .arg(view);
    } else if (m_segments.size() ==
               RosegardenDocument::currentDocument->getComposition().getNbSegments()) {
        title = tr("%1%2 - All Segments - %3")
            .arg(indicator)
            .arg(RosegardenDocument::currentDocument->getTitle())
            .arg(view);
    } else {
        title = tr("%1%2 - %3 Segment(s) - %4")
            .arg(indicator)
            .arg(RosegardenDocument::currentDocument->getTitle())
            .arg(m_segments.size())
            .arg(view);
    }

    return title;
}

}
