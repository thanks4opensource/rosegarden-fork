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

#ifndef RG_MATRIXSCENE_H
#define RG_MATRIXSCENE_H

#include <QGraphicsScene>

#include "base/Composition.h"
#include "gui/general/SelectionManager.h"

class QGraphicsLineItem;

namespace Rosegarden
{

class MatrixWidget;
class Segment;
class RosegardenDocument;
class Event;
class EventSelection;
class IsotropicRectItem;
class IsotropicDiamondItem;
class IsotropicTextItem;
class MatrixElement;
class MatrixMouseEvent;
class MatrixViewSegment;
class ViewSegment;
class RulerScale;
class ZoomableRulerScale;
class SnapGrid;

/**
 * An instance of this is created and owned by MatrixWidget.  See
 * MatrixWidget::m_scene.
 *
 * Specialised graphics scene for matrix elements.  The note blocks and
 * horizontal and vertical grid lines are all represented by graphics items
 * owned by this scene.  This scene also owns the MatrixViewSegment classes
 * which track segment contents in view objects.
 *
 * The scene works with MatrixViewSegment, MatrixViewElement, MatrixPainter,
 * and MatrixMover to support the new "concert pitch matrix" concept.  All
 * pitches on the grid as well as the key signature pitch highlights are
 * represented in concert pitch, presenting the pitches as they would look if
 * they were struck as notes on a conventional piano.  If the user draws a Bb in
 * a segment in -9 (Eb) transposition, the actual event is created with a pitch
 * +9 higher, and the resulting MatrixElement representing that pitch is then
 * drawn -9, so that it remains at the correct height.  If this note were viewed
 * in notation, it would appear to be a G.
 *
 * The scene can display events from any number of segments spanning any number
 * of tracks, and the concert pitch matrix is necessary in order to avoid the
 * chaos that would ensue when working with segments in different transpositions
 * on the same grid.  Note, however, the pitch highlights may come out
 * incorrectly if the key signatures in various segments do not resolve to the
 * same key after factoring the various segment transpositions into account.
 * This kind of situation would be flagged as an error in the notation view, via
 * the track/staff header warning icon, and it is not a problem that can be
 * resolved.  In this case, the user must intervene to ensure sanity of the
 * results.
 */
class MatrixScene : public QGraphicsScene,
                    public CompositionObserver,
                    public SelectionManager
{
    Q_OBJECT

public:
    MatrixScene();
    ~MatrixScene() override;

    enum HighlightType { HT_BlackKeys, HT_Triads, HT_Key };

    void setMatrixWidget(MatrixWidget *w) { m_widget = w; };
    MatrixWidget *getMatrixWidget() { return m_widget; };

    void setSegments(RosegardenDocument *document,
                     std::vector<Segment *> segments);

    void handleEventAdded(Event *);
    void handleEventRemoved(Event *);

    void setSelection(EventSelection* s, bool preview) override;
    EventSelection *getSelection() const override  { return m_selection; }
    void selectAll();
    int calculatePitchFromY(int y) const;

    void setSingleSelectedEvent(MatrixViewSegment *viewSegment,
                                MatrixElement *e,
                                bool preview);

    void setSingleSelectedEvent(Segment *segment,
                                Event *e,
                                bool preview);

    // Added for MatrixElement::noteColor()
    const RosegardenDocument *getDocument() const { return m_document; }

    Segment *getCurrentSegment();
    void setCurrentSegment(const Segment* const);

    Segment *getPriorSegment();
    Segment *getNextSegment();

    const std::vector<Segment*> &getSegments() const { return m_segments; }


    MatrixViewSegment *getCurrentViewSegment();

    bool segmentsContainNotes() const;

    int getYResolution() const { return m_resolution; }

    const RulerScale *getRulerScale() const { return m_scale; }
    ZoomableRulerScale *getReferenceScale() { return m_referenceScale; }
    const SnapGrid *getSnapGrid() const { return m_snapGrid; }

    void setSnap(timeT);

    bool constrainToSegmentArea(QPointF &scenePos);

    void playNote(Segment &segment, int pitch, int velocity = -1);

    // SegmentObserver method forwarded from MatrixViewSegment
    void segmentEndMarkerTimeChanged(const Segment *s, bool shorten);

    /// Pass on to all Segments
    void setHorizontalZoomFactor(double factor);
    /// Pass on to all Segments
    void setVerticalZoomFactor(double factor);

    /// update all ViewSegments
    void updateAllSegments(bool onlyPercussion=false);

    /// just colors of notes
    void updateAllSegmentsColors();

    /// just in one segment
    void updateCurrentSegmentNames();

    void recreatePitchHighlights();

    // For creating a MatrixMouseEvent as needed, e.g. upon keypress
    // or when editor receives focus. I.e. in situatons other than normal
    // setupMouseEvent(const QGraphicsSceneMouseEvent*, MatrixMouseEvent&)
    // used by MatrixScene overrides of  QGraphicsScene::mousePressEvent(),
    // mouseMoveEvent(), etc.
    bool setupMouseEvent(MatrixMouseEvent&) const;

    // Commands can add/remove/move ViewElements. This resets
    // the MatrixMouseEvent::element to what is currently
    // under the mouse pointer.
    void setMouseEventElement(MatrixMouseEvent&) const;

    // QAbstractGraphicsItems are expensive to construct and destruct,
    // or more specifically to addItem()/removeItem() them to/from the
    // QGraphicsScene. Since MatrixElements are constructed/destructed
    // frequently (possibly excessively so),and with their
    // QAbstractGraphicsItems members are constructed/destructed
    // (with explicit addItem() on construction  and implicit removeItem
    // on destruction) along with them, keeping them in a persistent pool
    // for reuse and calling show() and hide() as appropriate is a win.
    template <typename GraphicsItemType>
    class GraphicsItemPool
    {
      public:
        GraphicsItemPool(MatrixScene *scene) : m_scene(scene) {}

        ~GraphicsItemPool()
        {
            for (auto item : m_items) delete item;
        }

        GraphicsItemType* getFrom()
        {
            if (!m_items.empty()) {
                GraphicsItemType *item = m_items.back();
                m_items.pop_back();
                item->show();
                return item;
            }
            else {
                GraphicsItemType *item = new GraphicsItemType;
                m_scene->addItem(item);
                // don't need to call show(), is by default on construction
                return item;
            }
        }

        void putBack(GraphicsItemType *item)
        {
            // Check to allow client to use like delete (with nullptr)
            if (item) {
                item->hide();
                m_items.push_back(item);
            }
        }


      protected:
        MatrixScene *m_scene;
        std::vector<GraphicsItemType*> m_items;
    };

    GraphicsItemPool<QGraphicsRectItem>     graphicsRectPool;
    GraphicsItemPool<IsotropicTextItem>     graphicsTextPool;
    GraphicsItemPool<IsotropicRectItem>     graphicsIsotropicRectPool;
    GraphicsItemPool<IsotropicDiamondItem>  graphicsIsotropicDiamondPool;

    bool getKeySignaturesChanged() const
        { return m_keySignaturesChanged; }
    void setKeySignaturesChanged(bool changed)
        { m_keySignaturesChanged = changed; }

    void updateNoteLabels();

signals:
#if 0   // SIGNAL_SLOT_ABUSE
    void mousePressed(const MatrixMouseEvent *e);
    void mouseMoved(const MatrixMouseEvent *e);
    void mouseReleased(const MatrixMouseEvent *e);
    void mouseDoubleClicked(const MatrixMouseEvent *e);
#endif

    void eventRemoved(Event *e);

    void currentViewSegmentChanged(ViewSegment *);

    /// Emitted when the user changes the selection.
    /**
     * MatrixWidget::setSegments() connects this to
     * ControlRulerWidget::slotSelectionChanged().
     *
     * This is used to keep the velocity ruler in sync with the selected
     * events.
     */
    void selectionChangedES(EventSelection *s);

    void segmentDeleted(Segment *);
    void sceneDeleted(); // all segments have been removed


public slots:
    void slotNotesTied  (const EventContainer &notes);
    void slotNotesUntied(const EventContainer &notes);


// t4os -- see comment in constructor implemementation
protected slots:
    void slotCommandExecuted();
    void slotKeySignaturesChanged();

protected:
    void mousePressEvent(QGraphicsSceneMouseEvent *) override;
    void mouseMoveEvent(QGraphicsSceneMouseEvent *) override;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *) override;
    void mouseDoubleClickEvent(QGraphicsSceneMouseEvent *) override;

#if 0  // Failed experiments to use Alt or Tab for alternate tool switching
    bool event(QEvent*) override;
#endif
#if 0  // Failed experiments to use Alt or Tab for alternate tool switching
    bool eventFilter(QObject *obj, QEvent *ev) override;
#endif

    void focusInEvent(QFocusEvent*) override;
    void keyPressEvent(QKeyEvent*) override;
    void keyReleaseEvent(QKeyEvent*) override;

    // CompositionObserver notifications
    void segmentRemoved(const Composition *, Segment *) override;
    void trackChanged(const Composition *,Track*) override;
    void segmentTrackChanged(const Composition *, Segment *, TrackId) override;

private:
    MatrixWidget *m_widget; // I do not own this

    RosegardenDocument *m_document; // I do not own this

    // To prevent multiple addObserver() calls to same Document.
    // See comment in setSegments() implementation in .cpp file.
    bool m_observerAdded;

    /// The Segments we are editing.
    std::vector<Segment *> m_segments; // I do not own these
    typedef std::vector<MatrixViewSegment *> ViewSegmentVector;
    /// The MatrixViewSegment for each Segment we are editing.
    ViewSegmentVector m_viewSegments; // I own these
    int findSegmentIndex(const Segment *segment) const;
    /// The Segment that is currently being displayed.
    /**
     * An index into both m_segments and m_viewSegments.
     */
    unsigned m_currentSegmentIndex;

    RulerScale *m_scale; // I own this (it maps between time and scene x)
    ZoomableRulerScale *m_referenceScale; // I own this (it refers to m_scale
                                  // and knows zoom level needed by the loop
                                  // rulers (which refer to m_snapGrid))
    SnapGrid *m_snapGrid; // I own this

    int m_resolution;

    EventSelection *m_selection; // I own this

    HighlightType m_highlightType;

    bool m_keySignaturesChanged;

    // These are the background items -- the grid lines and the shadings
    // used to highlight the first, third and fifth in the current key
    std::vector<QGraphicsLineItem *> m_horizontals;
    std::vector<QGraphicsLineItem *> m_verticals;
    std::vector<QGraphicsRectItem *> m_highlights;

    void setupMouseEvent(const QGraphicsSceneMouseEvent*,
                         MatrixMouseEvent&) const;
    void setupMouseEvent(const QPoint &screenPos,
                         const QPointF &scenePos,
                         const Qt::MouseButtons mouseButtons,
                         const Qt::KeyboardModifiers keyboardModifiers,
                         MatrixMouseEvent &mme) const;
    void setMouseEventElement(const QPointF &scenePos,
                              MatrixMouseEvent &mme) const;
    void recreateLines();
    void recreatePercussionHighlights();
    void recreateKeyHighlights();
    void recreateTriadHighlights();
    void recreateBlackkeyHighlights();
    void updateCurrentSegment(bool isCurrent);
    void setSelectionElementStatus(EventSelection *, bool set);
    void previewSelection(EventSelection *, EventSelection *oldSelection);
    void checkUpdate();
};

}

#endif
