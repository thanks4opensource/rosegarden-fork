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

#ifndef RG_MATRIXSEGMENT_H
#define RG_MATRIXSEGMENT_H

#include "MatrixTool.h"

namespace Rosegarden
{

class Segment;

class MatrixSegment : public MatrixTool
{
    Q_OBJECT
    friend class MatrixToolBox;

public:
    void       handleLeftButtonPress (const MatrixMouseEvent*) override;
    void       handleMidButtonPress  (const MatrixMouseEvent*) override;
    void       handleMouseDoubleClick(const MatrixMouseEvent*) override;
    FollowMode handleMouseMove       (const MatrixMouseEvent*) override;
    void       handleMouseRelease    (const MatrixMouseEvent*) override;
    bool       handleKeyPress        (const MatrixMouseEvent*,
                                      const int key)           override;

    void setCursor() override;

    static QString ToolName();
    QString toolName() const override { return ToolName();}


protected:
    MatrixSegment(MatrixWidget*, MatrixToolBox*);

    void readyAtPos(const MatrixMouseEvent *e) override;

    const Segment* segmentIfIsntActive(const MatrixMouseEvent *e) const;

    const Segment *m_segment;

private:
    static QCursor *m_cursor;
};

}

#endif
