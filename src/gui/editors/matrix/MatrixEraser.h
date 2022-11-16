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

#ifndef RG_MATRIXERASER_H
#define RG_MATRIXERASER_H

#include "MatrixTool.h"

namespace Rosegarden
{

class MatrixEraser : public MatrixTool
{
    Q_OBJECT
    friend class MatrixToolBox;

public:
    void handleLeftButtonPress(const MatrixMouseEvent*)                override;
    void handleMidButtonPress (const MatrixMouseEvent*)                override;
    bool handleKeyPress       (const MatrixMouseEvent*, const int key) override;

    void setCursor() override;
    void setSelectCursor() override;

    static QString ToolName();
    QString toolName() const override { return ToolName();}

    virtual QString altToolHelpString() const override;


protected:
    MatrixEraser(MatrixWidget*, MatrixToolBox*);

    void readyAtPos(const MatrixMouseEvent *e) override;
    void setContextHelpForPos(const MatrixMouseEvent *e) override;

private:
    static QCursor *m_cursor;
    static QCursor *m_selectCursor;
};

}
#endif
