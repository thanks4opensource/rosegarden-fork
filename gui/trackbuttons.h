
// -*- c-basic-offset: 4 -*-

/*
    Rosegarden-4
    A sequencer and musical notation editor.

    This program is Copyright 2000-2004
        Guillaume Laurent   <glaurent@telegraph-road.org>,
        Chris Cannam        <cannam@all-day-breakfast.com>,
        Richard Bown        <bownie@bownie.com>

    The moral right of the authors to claim authorship of this work
    has been asserted.

    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License as
    published by the Free Software Foundation; either version 2 of the
    License, or (at your option) any later version.  See the file
    COPYING included with this distribution for more information.
*/


#ifndef _TRACKBUTTONS_H_
#define _TRACKBUTTONS_H_

#include <vector>

#include <qframe.h>
#include <qpopupmenu.h>

#include "kled.h"

#include "Instrument.h"
#include "Track.h"

#include "tracklabel.h"
#include "vumeter.h"

class QVBoxLayout;
class QButtonGroup;
class TrackVUMeter;
class RosegardenGUIDoc;
class InstrumentLabel;
class QSignalMapper;


/**
 * @author Stefan Schimanski
 * Taken from KMix code,
 * Copyright (C) 2000 Stefan Schimanski <1Stein@gmx.de>
 */
class KLedButton : public KLed  {
   Q_OBJECT
  public: 
   KLedButton(const QColor &col=Qt::green, QWidget *parent=0, const char *name=0);
   KLedButton(const QColor& col, KLed::State st, KLed::Look look, KLed::Shape shape,
	      QWidget *parent=0, const char *name=0);
   ~KLedButton();	

  signals:
   void stateChanged( bool newState );

  protected:	
   void mousePressEvent ( QMouseEvent *e );
};


// This class creates a list of mute and record buttons
// based on the rosegarden document and a specialisation
// of the Vertical Box widget.
//
//
// 

class TrackVUMeter: public VUMeter
{
public:
     TrackVUMeter(QWidget *parent = 0,
                  VUMeterType type = Plain,
                  int width = 0,
                  int height = 0,
                  int position = 0,
                  const char *name = 0);

    int getPosition() const { return m_position; }

protected:
    virtual void meterStart();
    virtual void meterStop();

private:
    int m_position;
    int m_textHeight;

};

class TrackButtons : public QFrame
{
    Q_OBJECT
public:

    TrackButtons(RosegardenGUIDoc* doc,
                 unsigned int trackCellHeight,
                 unsigned int trackLabelWidth,
                 bool showTrackLabels,
                 int overallHeight,
                 QWidget* parent = 0,
                 const char* name = 0,
                 WFlags f=0);

    ~TrackButtons();

    /// Return the track selected for recording
    int selectedRecordTrack();

    /// Return a vector of muted tracks
    std::vector<int> mutedTracks();

    /// Return a vector of highlighted tracks
    std::vector<int> getHighlightedTracks();

    void changeTrackInstrumentLabels(TrackLabel::InstrumentTrackLabels label);

    /**
     * Change the instrument label to something else like
     * an actual program name rather than a meaningless
     * device number and midi channel
     */
    void changeInstrumentLabel(Rosegarden::InstrumentId id, QString label);

    void changeTrackLabel(Rosegarden::TrackId id, QString label);

    // Select a label from outside this class by position
    //
    void selectLabel(int trackId);

    /*
     * Set the mute button down or up
     */
    void setMuteButton(Rosegarden::TrackId track, bool value);

    /*
     * Make this available so that others can set record buttons down
     */
    void setRecordTrack(int position);

    /**
     * Precalculate the Instrument popup so we don't have to every
     * time it appears
     * not protected because also used by the RosegardenGUIApp
     *
     * @see RosegardenGUIApp#slotPopulateTrackInstrumentPopup()
     */
    void populateInstrumentPopup(Rosegarden::Instrument *thisTrackInstr, QPopupMenu* instrumentPopup);

signals:
    // to emit what Track has been selected
    //
    void trackSelected(int);
    void instrumentSelected(int);

    void widthChanged();

    // to tell the notation canvas &c when a name changes
    //
    void nameChanged();

    // document modified (mute button)
    //
    void modified();

    // New record button set - if we're setting to an audio track
    // we need to tell the sequencer for live monitoring purposes.
    //
    void newRecordButton();

    // A mute button has been pressed
    //
    void muteButton(Rosegarden::TrackId track, bool state);

public slots:

    void slotSetRecordTrack(int position);
    void slotToggleMutedTrack(int mutedTrack);
    void slotUpdateTracks();
    void slotRenameTrack(QString newName, Rosegarden::TrackId trackId);
    void slotSetTrackMeter(double value, int position);
    void slotSetMetersByInstrument(double value, Rosegarden::InstrumentId id);

    void slotInstrumentSelection(int);
    void slotInstrumentPopupActivated(int);

    // ensure track buttons match the Composition
    //
    void slotSynchroniseWithComposition();

    // Convert a positional selection into a track selection and re-emit
    //
    void slotLabelSelected(int position);

protected:

    /**
     * Populate the track buttons themselves with Instrument information
     */
    void populateButtons();

    /**
     * Remove buttons and clear iterators for a position
     */
    void removeButtons(unsigned int position);

    /**
     * Set record button down - graphically only
     */
    void setRecordButtonDown(int position);

    /**
     *  buttons, starting at the specified index
     */
    void makeButtons();

    QFrame* makeButton(Rosegarden::TrackId trackId);
    QString getPresentationName(Rosegarden::Instrument *);

    void setButtonMapping(QObject*, Rosegarden::TrackId);

    //--------------- Data members ---------------------------------

    RosegardenGUIDoc                 *m_doc;

    QVBoxLayout                      *m_layout;

    std::vector<KLedButton *>         m_muteLeds;
    std::vector<KLedButton *>         m_recordLeds;
    std::vector<TrackLabel *>         m_trackLabels;
    std::vector<TrackVUMeter *>       m_trackMeters;
    std::vector<QFrame *>             m_trackHBoxes;

    QSignalMapper                    *m_recordSigMapper;
    QSignalMapper                    *m_muteSigMapper;
    QSignalMapper                    *m_clickedSigMapper;
    QSignalMapper                    *m_instListSigMapper;

    // Number of tracks on our view
    //
    unsigned int                      m_tracks;

    // The pixel offset from the top - just to overcome
    // the borders
    int                               m_offset;

    // The height of the cells
    //
    int                               m_cellSize;

    // gaps between elements
    //
    int                               m_borderGap;

    int                               m_lastID;
    int                               m_trackLabelWidth;
    int                               m_popupItem;

    TrackLabel::InstrumentTrackLabels             m_trackInstrumentLabels;
    int m_lastSelected;
};


#endif // _TRACKBUTTONS_H_


