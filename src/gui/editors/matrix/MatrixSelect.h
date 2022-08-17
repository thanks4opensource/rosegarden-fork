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

#ifndef RG_MATRIXSELECT_H
#define RG_MATRIXSELECT_H

#include <set>

#include <QGraphicsRectItem>
#include "MatrixTool.h"
#include <QString>
#include <QList>

#include "base/Event.h"


namespace Rosegarden
{

class ViewElement;
class MatrixViewSegment;
class MatrixElement;
class EventSelection;
class Event;


class MatrixSelect : public MatrixTool
{
    Q_OBJECT

    friend class MatrixToolBox;

public:
    void handleLeftButtonPress(const MatrixMouseEvent *) override;
    void handleMidButtonPress(const MatrixMouseEvent *) override;
    FollowMode handleMouseMove(const MatrixMouseEvent *) override;
    void handleMouseRelease(const MatrixMouseEvent *) override;
    bool handleKeyPress(const MatrixMouseEvent*, const int key) override;

    void stow() override;

    void setCursor() override;

    static QString ToolName();
    QString toolName() const override { return ToolName();}


signals:
    void gotSelection(); // inform that we've got a new selection


protected:
    MatrixSelect(MatrixWidget*, MatrixToolBox*);

    void readyAtPos(const MatrixMouseEvent *e) override;
    void setContextHelpForPos(const MatrixMouseEvent *e) override;

    bool getEventsInRectangle(EventSelection *&selection);

    //--------------- Data members ---------------------------------

    QGraphicsRectItem *m_selectionRect;
    QPointF m_selectionOrigin;
    bool m_updateRect;

    MatrixViewSegment *m_currentViewSegment;

    EventSelection *m_selectionToAddTo;
    EventSelection *m_eventsInRectangle;

    QList<QGraphicsItem*> m_previousCollisions;


private:
    void setViewCurrentSelection(bool always);
    static QCursor *m_cursor;
};

}
#endif
