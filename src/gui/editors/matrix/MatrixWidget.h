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

#ifndef RG_MATRIX_WIDGET_H
#define RG_MATRIX_WIDGET_H

#include "base/Event.h"             // for timeT
#include "MatrixView.h"
#include "base/MidiTypes.h"         // for MidiByte
#include "gui/general/AutoScroller.h"
#include "gui/general/SelectionManager.h"

#include <vector>

#include <QCheckBox>
#include <QSharedPointer>
#include <QTimer>
#include <QWidget>

class QGraphicsScene;
class QGridLayout;
class QLabel;
class QPushButton;

namespace Rosegarden
{

class RosegardenDocument;
class Segment;
class MatrixScene;
class MatrixToolBox;
class MatrixMouseEvent;
class SnapGrid;
class ZoomableRulerScale;
class Panner;
class Panned;
class EventSelection;
class PitchRuler;
class PianoKeyboard;
class PercussionPitchRuler;
class MidiKeyMapping;
class ControlRulerWidget;
class StandardRuler;
class TempoRuler;
class ChordNameRuler;
class Device;
class Instrument;
class Thumbwheel;


/// QWidget that fills the Matrix Editor's (MatrixView) client area.
/**
 * The main Matrix Editor window, MatrixView, owns the only instance
 * of this class.  See MatrixView::m_matrixWidget.
 *
 * MatrixWidget contains the matrix itself (m_view and m_scene).
 * MatrixWidget also contains all the other parts of the Matrix Editor
 * that appear around the matrix.  From left to right, top to bottom:
 *
 *   - The chord name ruler
 *   - The tempo ruler
 *   - The top standard ruler
 *   - The pitch ruler, m_pianoView (to the left of the matrix)
 *   - The matrix itself, m_panned (Panned/QGraphicsView) and m_scene
 *         (MatrixScene)
 *   - The bottom standard ruler, m_bottomStandardRuler
 *   - The controls widget, m_controlsWidget (optional)
 *   - The Segment label, m_segmentLabel
 *   - The Segment changer, m_segmentChanger (knob in the bottom left corner)
 *   - The panner, m_hpanner (navigation area)
 *   - The zoom area, m_HVzoom et al. (knobs in the bottom right corner)
 *
 * This class also owns the editing tools.
 * t4os: No, moved to MatrixToolBox
 */
class MatrixWidget : public QWidget,
                     public SelectionManager
{
    Q_OBJECT

public:
    MatrixWidget(MatrixView *matrixView);
    virtual ~MatrixWidget() override;

    enum class NoteColorType {
        VELOCITY,
        SEGMENT,
        SEGMENT_AND_VELOCITY
    };

    enum class NoteNameType {
        OFF,
        CONCERT,
        FIXED_DO,
        INTEGER_ABSOLUTE,
        RAW_MIDI,
        DEGREE,
        MOVABLE_DO,
        INTEGER_KEY,
    };

    enum class ChordSpellingType {
        OFF,
        DEGREE,
        INTEGER,
    };

    RosegardenDocument *getDocument() { return m_document; }
    Device *getCurrentDevice();
    MatrixScene *getScene()  { return m_scene; }
#if 0   // t4osDEBUG
    Panner *getPanner() { return m_panner; }  // unused
#else
    Panned *getPanned() { return m_panned; }  // unused
#endif

    /**
     * Show the pointer.  Used by MatrixView upon construction, this ensures
     * the pointer is visible initially.
     */
    void showInitialPointer();

    /// Set the Segment(s) to display.
    /**
     * ??? Only one caller.  Might want to fold this into the ctor.
     */
    void setSegments(RosegardenDocument *document,
                     std::vector<Segment *> segments);
    /// MatrixScene::getCurrentSegment()
    Segment *getCurrentSegment();

    /// Updates the wheel background, segment label text, and
    /// MIDI out instrument (whether step recording or not) if
    /// setInstrumentOverride == true
    /// Optional Segment* argument if caller already has that,
    /// otherwise will get from MatrixScene::getCurrentSegment()
    void updateToCurrentSegment(bool setInstrumentOverride,
                                const Segment *segment = nullptr);

    /// MatrixScene::segmentsContainNotes()
    bool segmentsContainNotes() const;

    /// One or more segments are percussion
    bool hasPercussionSegments() const { return m_hasPercussionSegments; }

    ControlRulerWidget *getControlsWidget()  { return m_controlsWidget; }

    /// MatrixScene::getSnapGrid()
    const SnapGrid *getSnapGrid() const;
    /// MatrixScene::setSnap()
    void setSnap(timeT);

    bool chordNameRulerIsVisible() const;
    ChordNameRuler *getChordNameRuler() const { return m_chordNameRuler; }
    void setChordNameRulerVisible(bool visible);
    void setTempoRulerVisible(bool visible);

    /// Show the highlight on the piano/percussion rulers.
    void showHighlight(bool visible);

    /// (Re)generate the pitch ruler (useful when key mapping changed)
    //  Needs to be public so MatrixMover::handleLeftButtonPress() can
    //  invoke when changing active segement (in case normal->percussion
    //  or vice-versa).
    void generatePitchRuler();

    // SelectionManager interface.

    // These delegate to MatrixScene, which possesses the selection
    /// MatrixScene::getSelection()
    EventSelection *getSelection() const override;
    /// MatrixScene::setSelection()
    void setSelection(EventSelection *s, bool preview) override;
    EventSelection *getRulerSelection() const;


    // Tools

    MatrixToolBox *getToolBox()       const { return m_toolBox; }

    /// Used by the tools to set an appropriate mouse cursor.
    void setCanvasCursor(const QCursor*);
    void setCanvasCursor(const Qt::CursorShape);

    // Used by updateSegmentChangerBackground() for main segment
    // label bar, and MatrixTool subclasses for help text when
    // switching active segment.
    QString segmentTrackInstrumentLabel(const QString formatString,
                                        const Segment* constsegment);

    // Used by MatrixScene::trackChanged()
    void setSegmentTrackInstrumentLabel(const Segment* const segment);

    // Used by MatrixViewSegment::colourChanged()
    void setSegmentLabelAndChangerColor(const Segment* const segment,
                                        const QColor *segmentColor = nullptr);


    bool getShowNoteNames() const          { return m_showNoteNames; }
    void setShowNoteNames(const bool show) { m_showNoteNames = show; }

    bool isDrumMode() const { return m_drumMode; }

    bool getShowPercussionDurations() const
         { return m_showPercussionDurations; }
    void setShowPercussionDurations(const bool show)
         { m_showPercussionDurations = show; }

    NoteColorType getNoteColorType() const
                  { return m_noteColorType; }
    void          setNoteColorType(const NoteColorType colorType)
                  { m_noteColorType = colorType; }

    bool getNoteColorAllSegments() const
         { return m_noteColorAllSegments; }
    void setNoteColorAllSegments(const bool all)
         { m_noteColorAllSegments = all; }

    NoteNameType getNoteNameType() const
                 { return m_noteNameType; }
    void         setNoteNameType(const NoteNameType nameType)
                 { m_noteNameType = nameType; }

    ChordSpellingType getChordSpellingType() const
                 { return m_chordSpellingType; }
    void         setChordSpellingType(const ChordSpellingType nameType)
                 { m_chordSpellingType = nameType; }

    bool getNoteNamesCmajFlats() const
         { return m_noteNamesCmajFlats; }
    void setNoteNamesCmajFlats(const bool flats)
         { m_noteNamesCmajFlats = flats; }

    bool getNoteNamesOffsetMinors() const
         { return m_noteNamesOffsetMinors; }
    void setNoteNamesOffsetMinors(const bool offset)
         { m_noteNamesOffsetMinors = offset; }

    bool getNoteNamesAlternateMinors() const
         { return m_noteNamesAlternateMinors; }
    void setNoteNamesAlternateMinors(const bool alternate)
         { m_noteNamesAlternateMinors = alternate; }

    /// Velocity for new notes (and moved notes, too).
    int getCurrentVelocity() const { return m_currentVelocity; }

    // Interface for MatrixView menu commands

    /// Edit > Select All
    void selectAll();
    /// Edit > Clear Selection
    void clearSelection();
    /// Move > Previous Segment
    void previousSegment();
    /// Move > Next Segment
    void nextSegment();
    void setTool(QString name);
    /// Move > Scroll to Follow Playback
    void setScrollToFollowPlayback(bool);
    /// View > Rulers > Show Velocity Ruler
    void showVelocityRuler();
    /// View > Rulers > Show Pitch Bend Ruler
    void showPitchBendRuler();
    /// View > Rulers > Add Control Ruler
    void addControlRuler(QAction *);

    // MatrixScene Interface
    // SIGNAL_SLOT_ABUSE
    // Were "private" but were called externally via signal, so not really
    // t4os: No longer used as Qt slots, should be renamed to
    //       dispatchMouse...()
    void slotDispatchMousePress(const MatrixMouseEvent *);
    void slotDispatchMouseMove(const MatrixMouseEvent *);
    void slotDispatchMouseRelease(const MatrixMouseEvent *);
    void slotDispatchMouseDoubleClick(const MatrixMouseEvent *);

signals:
#if 0   // SIGNAL_SLOT_ABUSE
    /**
     * Emitted when the user double-clicks on a note that triggers a
     * segment.
     *
     * RosegardenMainViewWidget::slotEditTriggerSegment() launches the event
     * editor on the triggered segment in response to this.
     */
    void editTriggerSegment(int);
#endif

    /// Forwarded from MatrixScene::segmentDeleted().
    void segmentDeleted(Segment *);
    /// Forwarded from MatrixScene::sceneDeleted().
    void sceneDeleted();
    /// Forwarded from MatrixScene::selectionChanged()
    void selectionChanged();
    /// Forwarded from ControlRulerWidget::childRulerSelectionChanged().
    /*
     * Connected to MatrixView::slotUpdateMenuStates().
     */
    void rulerSelectionChanged();
    /// Special case for velocity ruler.
    /**
     * See the emitter, ControlRuler::updateSelection(), for details.
     */
    void rulerSelectionUpdate();

    void showContextHelp(const QString &);

public slots:
    /// Velocity combo box.
    void slotSetCurrentVelocity(int velocity)  { m_currentVelocity = velocity; }

    /// Plays the preview note when using the computer keyboard to enter notes.
    void slotPlayPreviewNote(Segment *segment, int pitch);

protected:
    // QWidget Override
    /// Make sure the rulers are in sync when we are shown.
    void showEvent(QShowEvent *event) override;

#if 0   // Now done in void MatrixScene::focusInEvent() so always happens
        // wnen entering matrix editor window, not just widget area.
        // Was:
        // QWidget override
        // For doing RosegardenSequencer::setTrackInstrumentOverride()
    void enterEvent(QEvent *event) override;
#endif

private slots:
    /// Called when the document is modified in some way.
    void slotDocumentModified(bool);

    /// Called when segment(??) colors change
    void slotDocColoursChanged();

    /// Connected to Panned::zoomIn() for ctrl+wheel.
    void slotZoomIn();
    /// Connected to Panned::zoomOut() for ctrl+wheel.
    void slotZoomOut();

    /// Scroll rulers to sync up with view.
    void slotScrollRulers();

#if 0   // SIGNAL_SLOT_ABUSE
    // "private" but called externally via signal, so not really
    // MatrixScene Interface
    void slotDispatchMousePress(const MatrixMouseEvent *);
    void slotDispatchMouseMove(const MatrixMouseEvent *);
    void slotDispatchMouseRelease(const MatrixMouseEvent *);
    void slotDispatchMouseDoubleClick(const MatrixMouseEvent *);
#endif

    /// Display the playback position pointer.
    void slotPointerPositionChanged(timeT t);
    void slotStandardRulerDrag(timeT t);
    /// Handle StandardRuler startMouseMove()
    void slotSRStartMouseMove();
    /// Handle StandardRuler stopMouseMove()
    void slotSRStopMouseMove();

    /// Handle ControlRulerWidget::mousePress().
    void slotCRWMousePress();
    /// Handle ControlRulerWidget::mouseMove().
    void slotCRWMouseMove(FollowMode followMode);
    /// Handle ControlRulerWidget::mouseRelease().
    void slotCRWMouseRelease();

    /// Handle TempoRuler::mousePress().
    void slotTRMousePress();
    /// Handle TempoRuler::mouseRelease().
    void slotTRMouseRelease();

    /// Hide the horizontal scrollbar when not needed.
    /**
     * ??? Why do we need to manage this?  We turn off the horizontal
     *     scrollbar in the ctor with Qt::ScrollBarAlwaysOff.
     */
    void slotHScrollBarRangeChanged(int min, int max);

    // PitchRuler slots
    /// Draw the highlight as we move from one pitch to the next.
    void slotHoveredOverKeyChanged(unsigned int);
    void slotKeyPressed(unsigned int, bool);
    void slotKeySelected(unsigned int, bool);
    void slotKeyReleased(unsigned int, bool);

    /// The horizontal zoom thumbwheel moved
    void slotHorizontalThumbwheelMoved(int);

    /// The vertical zoom thumbwheel moved
    void slotVerticalThumbwheelMoved(int);

    /// The primary (combined axes) thumbwheel moved
    void slotPrimaryThumbwheelMoved(int);

    /// Reset the zoom to 100% and reset the zoomy wheels
    void slotResetZoomClicked();

    /// Trap a zoom in from the panner and sync it to the primary thumb wheel
    void slotSyncPannerZoomIn();

    /// Trap a zoom out from the panner and sync it to the primary thumb wheel
    void slotSyncPannerZoomOut();

    /// The Segment control thumbwheel moved, display a different Segment.
    void slotSegmentChangerMoved(int);

    /// The mouse has left the view, hide the highlight note.
    void slotMouseLeavesView();

    /// Instrument is being destroyed
    void slotInstrumentGone();

    // Command has executed, update ruler and possibly note labels
    void slotUpdateChordNameRuler();

    // User has changed chord name ruler segments (to analzyze)
    void slotChordNameRulerSegmentsUpdated();

private:
    MatrixView* const m_view;

    // ??? Instead of storing the document, which can change, get the
    //     document as needed via RosegardenDocument::currentDocument.
    RosegardenDocument *m_document; // I do not own this

    QGridLayout *m_layout; // I own this


    // View

    /// QGraphicsScene holding the note Events.
    MatrixScene *m_scene; // I own this

    /// The main view of the MatrixScene (m_scene).
    Panned *m_panned; // I own this

    /// Whether the view will scroll along with the playback position pointer.
    bool m_playTracking;

    /// View horizontal zoom factor.
    double m_hZoomFactor;
    void setHorizontalZoomFactor(double factor);

    /// View vertical zoom factor.
    double m_vZoomFactor;
    void setVerticalZoomFactor(double factor);


    /// Cursor
    bool m_isCursorShape;
    const QCursor *m_cursor;
    Qt::CursorShape m_cursorShape;

    // Pointer
    void updatePointer(timeT t);


    // Panner (Navigation Area below the matrix)

    /// Navigation area under the main view.
    Panner *m_panner; // I own this
    void zoomInFromPanner();
    void zoomOutFromPanner();

    QWidget *m_changerWidget;
    Thumbwheel *m_segmentChanger;
    int m_lastSegmentChangerValue;
    QLabel *m_segmentLabel;


    // Pitch Ruler

    // This can be nullptr.  It tracks what pitchruler corresponds to.
    Instrument *m_instrument; // Studio owns this (TBC)
    /// Key mapping from the Instrument.
    QSharedPointer<MidiKeyMapping> m_localMapping;

    /// Either a PercussionPitchRuler or a PianoKeyboard object.
    PitchRuler *m_pitchRuler; // Points to ...
    PianoKeyboard *m_pianoPitchRuler; // ... one of these ...
    PercussionPitchRuler *m_percussionPitchRuler;  // ... two (I own both)

    // For Qt bug workaround. See comments in generatePitchRuler() in .cxx file
    QGraphicsProxyWidget *m_pitchRulerProxy;

    /// Contains m_pitchRuler.
    QGraphicsScene *m_pianoScene; // I own this

    /// Contains m_pianoScene.
    Panned *m_pianoView; // I own this

    /// All Segments only have key mappings.  Use a PercussionPitchRuler.
    bool m_onlyKeyMapping;

    /// One or more segments are percussion
    bool m_hasPercussionSegments;
    /// Percussion matrix editor?
    /**
     * For the Percussion matrix editor, we ignore key release events from
     * the PitchRuler.
     */
    bool m_drumMode;

    /// First note selected when doing a run up/down the keyboard.
    /**
     * Used to allow selection of a range of pitches by shift-clicking a
     * note on the pitch ruler then dragging up or down to another note.
     */
    MidiByte m_firstNote;

    /// Last note selected when doing a run up/down the keyboard.
    /**
     * Used to prevent redundant note on/offs when doing a run up/down the
     * pitch ruler.
     */
    MidiByte m_lastNote;

    /// Hide pitch ruler highlight when mouse move is not related to a pitch change.
    bool m_highlightVisible;


    // These need to be accessed by MatrixElement::reconfigure() each
    // time a percussion segment note moves/etc., and getting from
    // the QSettings persistent database is too slow.
    // Will be set from MatrixView constructor when read from database,
    // and updated each time changed via user GUI interaction as
    // signaled to MatrixView::slotPercussionDurations(),
    // MatrixView::slotNoteColors() and
    // MatrixView::slotNoteColorsAllSegments().
    bool m_showNoteNames;
    bool m_showPercussionDurations;
    NoteColorType m_noteColorType;
    bool m_noteColorAllSegments;
    NoteNameType m_noteNameType;
    ChordSpellingType m_chordSpellingType;
    bool m_noteNamesCmajFlats;
    bool m_noteNamesOffsetMinors;
    bool m_noteNamesAlternateMinors;

    // Tools
    MatrixToolBox *m_toolBox; // I own this
    /// Used by the MatrixMover and MatrixPainter tools for preview notes.
    int m_currentVelocity;

    // Zoom Area (to the right of the Panner)

    /// The big zoom wheel.
    Thumbwheel *m_HVzoom;
    /// Used to compute how far the big zoom wheel has moved.
    int m_lastHVzoomValue;
    /// Which zoom factor to use.  For the pitch ruler.
    bool m_lastZoomWasHV;
    /// Thin horizontal zoom wheel under the big zoom wheel.
    Thumbwheel *m_Hzoom;
    /// Used to compute how far the horizontal zoom wheel has moved.
    int m_lastH;
    /// Thin vertical zoom wheel to the right of the big zoom wheel.
    Thumbwheel *m_Vzoom;
    /// Used to compute how far the vertical zoom wheel has moved.
    int m_lastV;
    /// Small reset button to the lower right of the big zoom wheel.
    QPushButton *m_reset;


    // Rulers

    ChordNameRuler *m_chordNameRuler; // I own this
    TempoRuler *m_tempoRuler; // I own this
    StandardRuler *m_topStandardRuler; // I own this
    StandardRuler *m_bottomStandardRuler; // I own this
    // ??? Rename: m_controlRuler
    ControlRulerWidget *m_controlsWidget; // I own this

    /// Used by all rulers to make sure they are all zoomed to the same scale.
    /**
     * See MatrixScene::getReferenceScale().
     */
    ZoomableRulerScale *m_referenceScale;  // Owned by MatrixScene


    // Auto-scroll

    AutoScroller m_autoScroller;

};

}

#endif
