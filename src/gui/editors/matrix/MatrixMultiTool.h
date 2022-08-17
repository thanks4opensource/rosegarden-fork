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

#ifndef RG_MATRIXMULTITOOL_H
#define RG_MATRIXMULTITOOL_H

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


class MatrixMultiTool : public MatrixTool
{
    Q_OBJECT

    friend class MatrixToolBox;

public:
    void handleLeftButtonPress(const MatrixMouseEvent *) override;
    void handleMidButtonPress(const MatrixMouseEvent *) override;
    FollowMode handleMouseMove(const MatrixMouseEvent *) override;
    bool handleKeyPress(const MatrixMouseEvent*, const int key) override;

    /**
     * Create the selection rect
     *
     * We need this because MatrixScene deletes all scene items along
     * with it. This happens before the MatrixMultiTool is deleted, so
     * we can't delete the selection rect in ~MatrixMultiTool because
     * that leads to double deletion.
     */

    void setSelectCursor() override;

    static QString ToolName();
    QString toolName() const override { return ToolName();}

    virtual QString altToolHelpString() const override;

protected:
    enum class MoveResizeVelocity {
        UNSET,
        MOVE,
        RESIZE,
        VELOCITY
    };
    MoveResizeVelocity moveResizeVelocity(const MatrixMouseEvent*) const;

    MatrixMultiTool(MatrixWidget*, MatrixToolBox*);

    void readyAtPos(const MatrixMouseEvent *e) override;
    void setContextHelpForPos(const MatrixMouseEvent *e) override;

    //--------------- Data members ---------------------------------
private:
    MatrixTool *overlayTool(const MatrixMouseEvent*);
    MoveResizeVelocity m_currentOverlay;
    static QCursor *m_selectCursor;
};

}
#endif
