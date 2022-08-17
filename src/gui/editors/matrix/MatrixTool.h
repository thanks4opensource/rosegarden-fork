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

#ifndef RG_MATRIXTOOL_H
#define RG_MATRIXTOOL_H

#include "gui/general/BaseTool.h"

#include "gui/general/ActionFileClient.h"
#include "gui/general/AutoScroller.h"  // For FollowMode

#include <map>

class QAction;
class QCursor;
class QLabel;
class QWidgetAction;

namespace Rosegarden
{

class Event;
class MatrixMouseEvent;
class MatrixScene;
class MatrixToolBox;
class MatrixView;
class MatrixWidget;
class Segment;
class SnapGrid;

class MatrixTool : public BaseTool, public ActionFileClient
{
    Q_OBJECT

public:
    ~MatrixTool() override;

    enum class Role {
        FUNDAMENTAL,
        SINGLE_ACTION,
        PERSISTENT,
        OVERLAY,
    };

    void setScene(MatrixScene *scene) { m_scene = scene; }

    Role role() const             { return m_role; }
    void setRole(const Role role) { m_role = role; }

    bool isFinished() const           { return m_isFinished; }
    void setIsFinished(bool finished) { m_isFinished = finished; }

    bool hasMenu() override { return m_menuName != ""; }

    void createMenu() override;
    void ready()      override;
    void stow()       override;

    virtual void readyAtPos(const MatrixMouseEvent*);

    virtual void       handleLeftButtonPress (const MatrixMouseEvent*);
    virtual void       handleMidButtonPress  (const MatrixMouseEvent*);
    virtual void       handleRightButtonPress(const MatrixMouseEvent*);
    virtual void       handleMouseDoubleClick(const MatrixMouseEvent*);
    virtual FollowMode handleMouseMove       (const MatrixMouseEvent*);
    virtual void       handleMouseRelease    (const MatrixMouseEvent*);
    virtual bool       handleKeyPress        (const MatrixMouseEvent*,
                                              const int key) = 0;
    virtual bool       handleKeyRelease      (const MatrixMouseEvent*,
                                              const int key);

    void updateMenuSegmentList(const Segment *segment);

    virtual void setCursor();
    virtual void setSelectCursor();

    virtual QString toolName() const { return "MatrixTool"; }

    virtual QString altToolHelpString() const;

public slots:
    /**
     * Respond to an event being deleted -- it may be one that the
     * tool is remembering as the current event.
     */
    virtual void handleEventRemoved(Event *event);

    void updateMenu();

protected slots:
    // For switching between tools on RMB
    //
    void slotMultiToolSelected();
    void slotMoveSelected();
    void slotEraseSelected();
    void slotResizeSelected();
    void slotVelocityChangeSelected();
    void slotDrawSelected();
    void slotSegmentDeleted(const Segment*);
    void slotUndo();
    void slotRedo();

protected:
    // Keyboard key used to switch to alternate tool.
    // Currently CapsLock (with lots of code to handle toggle behavior).
    // Would rather use Alt or even Tab, but seeming impossible given
    //   inability to work around Qt's special-casing of those.
    // Type should really be something like Qt::Key, but Qt doesn't
    //   class/using/typedef that.
    static const int ALTERNATE_TOOL_QT_KEY;

    MatrixTool(QString menuName, MatrixWidget*, MatrixToolBox*);

    // Helper for subclasses setContextHelpForPos() methods.
    struct ContextInfo {
        ContextInfo(const MatrixTool*, const MatrixMouseEvent*);
        bool     haveSelection,
                 inSelection;
        unsigned selectionSize;
    };

    const SnapGrid *getSnapGrid() const;

    QCursor *makeCursor(QString pixmapName, int  hotSpotX, int  hotSpotY);
    virtual void setContextHelpForPos(const MatrixMouseEvent*);

    MatrixTool *getTool(const QString &toolName);

    void invokeInParentView(QString actionName);
    QAction *findActionInParentView(QString actionName);

    const Segment* segmentIfIsntActive(const MatrixMouseEvent*) const;

    bool overlaySelectOrSegment(const MatrixMouseEvent*);

    void changeActiveSegment(const Segment *segment);

    bool modifySelection(const MatrixMouseEvent*);

    void eraseNotes(const MatrixMouseEvent*) const;

    QString altToolHelpStringTools(const QString &thisTool,
                                   const QString &altTool) const;

    void moveResizeVelocityContextHelp(const MatrixMouseEvent*,
                                       const QString &clickText,
                                       const QString &dragText,
                                       const QString &snapText);

    MatrixWidget *m_widget;
    MatrixToolBox *m_toolbox;
    MatrixScene *m_scene;

    QAction *m_undoAction,
            *m_redoAction;
    Role m_role;
    bool m_isFinished;

private:
    QLabel *generateMenuLabel(const Segment *segment);

    std::map<const Segment*, QWidgetAction*> m_segmentActions;
};

}
#endif
