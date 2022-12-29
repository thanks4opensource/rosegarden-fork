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

#ifndef RG_MATRIXMOVER_H
#define RG_MATRIXMOVER_H

#include "MatrixTool.h"
#include <QString>
#include "base/Event.h"


namespace Rosegarden
{

class ViewElement;
class MatrixViewSegment;
class MatrixElement;
class Event;

class MatrixMover : public MatrixTool
{
    Q_OBJECT

    friend class MatrixToolBox;

public:
    void       handleLeftButtonPress(const MatrixMouseEvent*) override;
    void       handleMidButtonPress (const MatrixMouseEvent*) override;
    FollowMode handleMouseMove      (const MatrixMouseEvent*) override;
    void       handleMouseRelease   (const MatrixMouseEvent*) override;
    bool       handleKeyPress       (const MatrixMouseEvent*,
                                     const int key)           override;

    /**
     * Respond to an event being deleted -- it may be the one the tool
     * is remembering as the current event.
     */
    void handleEventRemoved(Event *event) override;

    void setCursor() override;
    void setSelectCursor() override;

    static QString ToolName();
    QString toolName() const override { return ToolName();}

    virtual QString altToolHelpString() const override;

signals:
    void hoveredOverNoteChanged(int evPitch, bool haveEvent, timeT evTime);

protected slots:
//    void slotMatrixScrolled(int x, int y); //!!! do we need this? probably not

protected:
    MatrixMover(MatrixWidget*, MatrixToolBox*);

    void readyAtPos(const MatrixMouseEvent *e) override;
    void setContextHelpForPos(const MatrixMouseEvent *e) override;

    MatrixElement *m_currentElement;
    /// The Event associated with m_currentElement.
    Event *m_event;
    MatrixViewSegment *m_currentViewSegment;
    timeT m_clickSnappedLeftDeltaTime;

    std::vector<MatrixElement *> m_duplicateElements;
    bool m_quickCopy;

    int m_lastPlayedPitch;

    static const int IMPOSSIBLE_DIFF_PITCH = 128;

    timeT m_crntElementCrntTime;
    int m_prevDiffPitch;

    // This needs to be separate from m_clickSnappedLeftDeltaTime so
    // that just clicking on a current segment note, e.g. to play it,
    // doesn't snap it to grid on mouse release if no intervening
    // mouse movement.
    bool m_mouseMoved;

private:
    static QCursor *m_cursor;
    static QCursor *m_selectCursor;
};

}

#endif
