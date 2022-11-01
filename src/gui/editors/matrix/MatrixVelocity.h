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

#ifndef RG_MATRIXVELOCITY_H
#define RG_MATRIXVELOCITY_H

#include "MatrixTool.h"

#include "base/Event.h"

#include <QString>

namespace Rosegarden
{

class ViewElement;
class MatrixViewSegment;
class MatrixElement;
class Event;


class MatrixVelocity : public MatrixTool
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

    void setCursor() override;
    void setSelectCursor() override;

    static QString ToolName();

    virtual QString altToolHelpString() const override;
    QString toolName() const override { return ToolName();}

    /**
     * Respond to an event being deleted -- it may be the one the tool
     * is remembering as the current event.
     */
    void handleEventRemoved(Event *event) override;

    void stow() override;


protected:
    MatrixVelocity(MatrixWidget*, MatrixToolBox*);

    void readyAtPos(const MatrixMouseEvent *e) override;
    void setContextHelpForPos(const MatrixMouseEvent *e) override;

    double m_mouseStartY;
    int m_velocityDelta;
    double m_screenPixelsScale;  // Amount of screen pixels used for
                                 // scale +-127 1:1 scale ratio
    double m_velocityScale;

    explicit MatrixVelocity(MatrixWidget *);

    MatrixElement *m_currentElement;
    /// The Event associated with m_currentElement.
    Event *m_event;
    MatrixViewSegment *m_currentViewSegment;

    bool m_start;   // Indicator of the start of a velocity change sequence

private:
    static QCursor *m_cursor;
    static QCursor *m_selectCursor;
};


}

#endif
