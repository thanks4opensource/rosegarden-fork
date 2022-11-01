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

#ifndef RG_MATRIXTOOLBOX_H
#define RG_MATRIXTOOLBOX_H

#include "gui/general/BaseToolBox.h"

#include "gui/general/AutoScroller.h"  // For FollowMode


class QString;




namespace Rosegarden
{

class MatrixMouseEvent;
class MatrixScene;
class MatrixTool;
class MatrixWidget;
class Segment;


/**
 * MatrixToolBox : maintains a single instance of each registered tool
 *
 * Tools are fetched by name
 */
class MatrixToolBox : public BaseToolBox
{
    Q_OBJECT

public:
    explicit MatrixToolBox(MatrixWidget *parent);
    ~MatrixToolBox() override;

    void setScene(MatrixScene *scene);

    // Called if segment(s) deleted or label/color change.
    void updateToolsMenus(const Segment *segment);

    // Set currrent tool
    void setDrawTool();
    void setEraseTool();
    void setMultiTool();
    void setMoveTool();
    void setResizeTool();
    void setVelocityTool();

    MatrixTool *getCurrentTool() const { return m_currentTool; }

    // Active dispatch tool, or current tool if none
    MatrixTool *getActiveTool() const;

    // Last tool before getActiveTool()
    MatrixTool *getPenultimateTool() const;

    // If Control or CapsLock changed while mouse outside window
    void checkKeysStateOnEntry();

    // Manipulate dispatch tool chain
    void appendDispatchTool(const QString &name, const MatrixMouseEvent*);
    void appendDispatchTool(MatrixTool*, const MatrixMouseEvent*);
    void popDispatchTool();

    const Segment *segmentWhenClicked() const { return m_segmentWhenClicked; }
    void setSegmentWhenClicked(const Segment *s) { m_segmentWhenClicked = s; }

    void       handleButtonPress     (const MatrixMouseEvent*);
    void       handleMouseDoubleClick(const MatrixMouseEvent*);
    FollowMode handleMouseMove       (const MatrixMouseEvent*);
    void       handleMouseRelease    (const MatrixMouseEvent*);
    void       handleKeyPress        (const MatrixMouseEvent*, const int key);
    void       handleKeyRelease      (const MatrixMouseEvent*, const int key);

    bool hadDoubleClick() const { return m_hadDoubleClick; }
    bool capsLockIsOn()   const { return m_capsLockIsOn; }


protected:
    BaseTool *createTool(QString toolName) override;

    void setTool(QString name);

    bool haveDispatchTools() const { return !m_dispatchTools.empty();}

    // If a keypress or release occurred while a tool was in use
    void checkPendingKeyPressRelease(MatrixTool*, const MatrixMouseEvent*);

    void clearDispatchTools(const MatrixMouseEvent*);

    MatrixWidget    *m_widget;
    MatrixScene     *m_scene;
    MatrixTool      *m_toolWhenClicked;

    // Handle capslock toggle behavior
    bool             m_capsLockIsOn;
    bool             m_capsLockNewlyOn;

    bool             m_controlIsPressed;
    bool             m_hadDoubleClick;
    int              m_pendingKeyPress;
    int              m_pendingKeyRelease;
    MatrixTool      *m_currentTool;
    const Segment   *m_segmentWhenClicked;

    std::vector<MatrixTool*> m_dispatchTools;


private:
    bool getX11CapsLock() const;
    void warnX11CapsLockIsOn();
};

}

#endif
