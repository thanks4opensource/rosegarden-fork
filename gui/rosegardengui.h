// -*- c-basic-offset: 4 -*-

/*
    Rosegarden-4 v0.1
    A sequencer and musical notation editor.

    This program is Copyright 2000-2002
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

#ifndef ROSEGARDENGUI_H
#define ROSEGARDENGUI_H
 

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

// include files for Qt
#include <qstrlist.h>

// include files for KDE 
#include <kapp.h>
#include <kmainwindow.h>
#include <kaccel.h>

#include "rosegardendcop.h"
#include "rosegardenguiiface.h"
#include "rosegardentransportdialog.h"
#include "segmentcanvas.h"

// the sequencer interface
//
#include <MappedEvent.h>
#include "Sound.h"

class KURL;
class KRecentFilesAction;
class KToggleAction;
class KProcess;

// forward declaration of the RosegardenGUI classes
class RosegardenGUIDoc;
class RosegardenGUIView;

namespace Rosegarden { class SequenceManager; }

/**
  * The base class for RosegardenGUI application windows. It sets up the main
  * window and reads the config file as well as providing a menubar, toolbar
  * and statusbar. An instance of RosegardenGUIView creates your center view, which is connected
  * to the window's Doc object.
  * RosegardenGUIApp reimplements the methods that KTMainWindow provides for main window handling and supports
  * full session management as well as keyboard accelerator configuration by using KAccel.
  * @see KTMainWindow
  * @see KApplication
  * @see KConfig
  * @see KAccel
  *
  * @author Source Framework Automatically Generated by KDevelop, (c) The KDevelop Team.
  * @version KDevelop version 0.4 code generation
  */
class RosegardenGUIApp : public KMainWindow, virtual public RosegardenIface
{
  Q_OBJECT

  friend class RosegardenGUIView;

public:

    /**
     * construtor of RosegardenGUIApp, calls all init functions to
     * create the application.
     * @see initMenuBar initToolBar
     */
    RosegardenGUIApp();

    virtual ~RosegardenGUIApp();

    /**
     * opens a file specified by commandline option
     *
     * @return true if file was successfully opened, false otherwise
     */
    bool openDocumentFile(const char *_cmdl=0);

    /**
     * returns a pointer to the current document connected to the
     * KTMainWindow instance and is used by * the View class to access
     * the document object's methods
     */	
    RosegardenGUIDoc *getDocument() const; 	

    /**
     * open a file
     */
    void openFile(const QString& url);

    /**
     * Works like openFile but is able to open remote files
     */
    void openURL(const KURL& url);

    /**
     * imports a Rosegarden 2.1 file
     */
    void importRG21File(const QString &url);

    /**
     * imports a MIDI file
     */
    void importMIDIFile(const QString &url);

    /**
     * export a MIDI file
     */
    void exportMIDIFile(const QString &url);

    /**
     * The Sequencer calls this method to get a MappedCompositon
     * full of MappedEvents for it to play.
     */
    const Rosegarden::MappedComposition&
            getSequencerSlice(long sliceStartSec, long sliceStartUSec,
                              long sliceEndSec, long sliceEndUSec);

    /**
     * The Sequencer sends back a MappedComposition full of
     * any MappedEvents that it's recorded.
     *
     */
    void processRecordedMidi(const Rosegarden::MappedComposition &mC);


    /**
     * Process unexpected MIDI events for the benefit of the GUI
     *
     */
    void processAsynchronousMidi(const Rosegarden::MappedComposition &mC);

    /**
     *
     * Query the sequencer to find out if the sound systems initialised
     * correctly
     *
     */
    bool getSoundSystemStatus();

    /**
     * Equivalents of the GUI slots, for DCOP use
     */
    virtual void fileNew()    { slotFileNew(); }
    virtual void fileSave()   { slotFileSave(); }
    virtual void fileSaveAs() { slotFileSaveAs(); }
    virtual void fileClose()  { slotFileClose(); }
    virtual void quit()       { slotQuit(); }

    /**
     * Set the song position pointer - we use longs so that
     * this method is directly accesible from the sequencer
     * (longs are required over DCOP)
     */
    virtual void setPointerPosition(const long &posSec, const long &posUSec);

    /**
     * Start the sequencer auxiliary process
     * (built in the 'sequencer' directory)
     *
     * @see slotSequencerExited()
     */
    bool launchSequencer();
    
    /**
     * Set the sequencer status - pass through DCOP as an int
     */
    virtual void notifySequencerStatus(const int &status);

    /*
     * Autoload the autoload song for Studio parameters
     *
     */
    bool performAutoload();

protected:

    /**
     * Overridden virtuals for Qt drag 'n drop (XDND)
     */
    virtual void dragEnterEvent(QDragEnterEvent *event);
    virtual void dropEvent(QDropEvent *event);
    virtual void keyPressEvent(QKeyEvent *event);
    virtual void keyReleaseEvent(QKeyEvent *event);


    /**
     * read general Options again and initialize all variables like
     * the recent file list
     */
    void readOptions();

    /**
     * create menus and toolbars
     */
    void setupActions();

    /**
     * sets up the statusbar for the main window by initialzing a
     * statuslabel.
     */
    void initStatusBar();

    /**
     * initializes the document object of the main window that is
     * connected to the view in initView().
     * @see initView();
     */
    void initDocument();

    /**
     * creates the centerwidget of the KTMainWindow instance and sets
     * it as the view
     */
    void initView();

    /**
     * queryClose is called by KTMainWindow on each closeEvent of a
     * window. Against the default implementation (only returns true),
     * this calls saveModified() on the document object to ask if the
     * document shall be saved if Modified; on cancel the closeEvent
     * is rejected.
     *
     * @see KTMainWindow#queryClose
     * @see KTMainWindow#closeEvent
     */
    virtual bool queryClose();

    /**
     * queryExit is called by KTMainWindow when the last window of the
     * application is going to be closed during the closeEvent().
     * Against the default implementation that just returns true, this
     * calls saveOptions() to save the settings of the last window's
     * properties.
     *
     * @see KTMainWindow#queryExit
     * @see KTMainWindow#closeEvent
     */
    virtual bool queryExit();

    /**
     * saves the window properties for each open window during session
     * end to the session config file, including saving the currently
     * opened file by a temporary filename provided by KApplication.
     *
     * @see KTMainWindow#saveProperties
     */
    virtual void saveProperties(KConfig *_cfg);

    /**
     * reads the session config file and restores the application's
     * state including the last opened files and documents by reading
     * the temporary files saved by saveProperties()
     *
     * @see KTMainWindow#readProperties
     */
    virtual void readProperties(KConfig *_cfg);

    /*
     * Send the result of getSequencerSlice (operated by the
     * Sequencer) to the GUI so as to get visual representation
     * of the events/sounds going out
     */
    void showVisuals(Rosegarden::MappedComposition *mC);

    /*
     * place clicktrack events into the global MappedComposition
     *
     */
    void insertMetronomeClicks(Rosegarden::timeT sliceStart,
			       Rosegarden::timeT sliceEnd);

public slots:
    /**
     * open a new application window by creating a new instance of
     * RosegardenGUIApp
     */
    void slotFileNewWindow();

    /**
     * clears the document in the actual view to reuse it as the new
     * document
     */
    void slotFileNew();

    /**
     * open a file and load it into the document
     */
    void slotFileOpen();

    /**
     * opens a file from the recent files menu
     */
    void slotFileOpenRecent(const KURL&);

    /**
     * save a document
     */
    void slotFileSave();

    /**
     * save a document by a new filename
     */
    void slotFileSaveAs();

    /**
     * asks for saving if the file is modified, then closes the actual
     * file and window
     */
    void slotFileClose();

    /**
     * print the actual file
     */
    void slotFilePrint();

    /**
     * Let the user select a MIDI file for import
     */
    void slotImportMIDI();

    /**
     * Let the user select a Rosegarden 2.1 file for import 
     */
    void slotImportRG21();


    /**
     * Let the user enter a MIDI file to export to
     */
    void slotExportMIDI();

    /**
     * closes all open windows by calling close() on each memberList
     * item until the list is empty, then quits the application.  If
     * queryClose() returns false because the user canceled the
     * saveModified() dialog, the closing breaks.
     */
    void slotQuit();
    
    /**
     * put the marked text/object into the clipboard and remove * it
     * from the document
     */
    void slotEditCut();

    /**
     * put the marked text/object into the clipboard
     */
    void slotEditCopy();

    /**
     * paste the clipboard into the document
     */
    void slotEditPaste();

    /**
     * toggles the toolbar
     */
    void slotToggleToolBar();

    /**
     * toggles the transport window
     */
    void slotToggleTransport();

    /**
     * toggles the tracks toolbar
     */
    void slotToggleTracksToolBar();

    /**
     * toggles the statusbar
     */
    void slotToggleStatusBar();

    /**
     * changes the statusbar contents for the standard label
     * permanently, used to indicate current actions.
     *
     * @param text the text that is displayed in the statusbar
     */
    void slotStatusMsg(const QString &text);

    /**
     * changes the status message of the whole statusbar for two
     * seconds, then restores the last status. This is used to display
     * statusbar messages that give information about actions for
     * toolbar icons and menuentries.
     *
     * @param text the text that is displayed in the statusbar
     */
    void slotStatusHelpMsg(const QString &text);

    /**
     * segment select tool
     */
    void slotPointerSelected();

    /**
     * segment eraser tool is selected
     */
    void slotEraseSelected();
    
    /**
     * segment draw tool is selected
     */
    void slotDrawSelected();
    
    /**
     * segment move tool is selected
     */
    void slotMoveSelected();

    /**
     * segment resize tool is selected
     */
    void slotResizeSelected();

    /*
     * Segment join tool
     *
     */
    void slotJoinSelected();

    /*
     * Segment split tool
     *
     */
    void slotSplitSelected();

    /**
     * change the resolution of the segment display
     */
    //void slotChangeTimeResolution();

    /**
     * Add new tracks
     */
    void slotAddTracks();

    /**
     * edit all tracks at once
     */
    void slotEditAllTracks();

    /**
     * Set the song position pointer
     */
    void slotSetPointerPosition(Rosegarden::RealTime time);

    /**
     * timeT version of the same
     */
    void slotSetPointerPosition(Rosegarden::timeT t);

    /**
     * Set the pointer position and start playing (from LoopRuler)
     */
    void slotSetPlayPosition(Rosegarden::timeT position);

    /**
     * Set a loop
     */
    void slotSetLoop(Rosegarden::timeT lhs, Rosegarden::timeT rhs);


    /**
     * Update the transport with the bar, beat and unit times for
     * a given timeT
     */
    void slotDisplayBarTime(Rosegarden::timeT t);


    /**
     * Transport controls
     */
    void slotPlay();
    void slotStop();
    void slotRewind();
    void slotFastforward();
    void slotRecord();
    void slotRewindToBeginning();
    void slotFastForwardToEnd();
    void slotRefreshTimeDisplay();


    /**
     * Called when the sequencer auxiliary process exits
     */
    void slotSequencerExited(KProcess*);

    // When the transport closes 
    //
    void slotCloseTransport();

    /**
     * Put the GUI into a given ToolType edit mode
     */
    void slotActivateTool(SegmentCanvas::ToolType tt);

    /**
     * Toggles either the play or record metronome according
     * to Transport status
     */
    void slotToggleMetronome();

    /*
     * Toggle the solo mode
     */
    void slotToggleSolo();


    /**
     * Set and unset the loop from the transport loop button with
     * these slots.
     */
    void slotSetLoop();
    void slotUnsetLoop();

    /**
     * Toggle the track labels on the TrackEditor
     */
    void slotToggleTrackLabels();

    /**
     * Toggle the segment parameters box on the TrackEditor
     */
    void slotToggleSegmentParameters();

    /**
     * Toggle the instrument parameters box on the TrackEditor
     */
    void slotToggleInstrumentParameters();

    /**
     * Toggle the rulers on the TrackEditor
     * (aka bar buttons)
     */
    void slotToggleRulers();

    /*
     * slotSendMidiController
     **/
    void slotSendMappedEvent(Rosegarden::MappedEvent *mE);

    /*
     * Select Track up or down
     */
    void slotTrackUp();
    void slotTrackDown();

    /**
     * save general Options like all bar positions and status as well
     * as the geometry and the recent file list to the configuration
     * file
     */ 	
    void slotSaveOptions();

    /*
     *
     * Show the configure dialog
     *
     */
    void slotConfigure();

    /*
     * Show the key mappings
     *
     */
    void slotEditKeys();

    /*
     * Do we need this?
     *
     */
    void slotEditToolbars();

    /*
     * Edit the tempo - called from a Transport signal
     */
    void slotEditTempo();
        
private:

    //--------------- Data members ---------------------------------

    /**
     * the configuration object of the application
     */
    KConfig* m_config;

    KRecentFilesAction* m_fileRecent;

    /**
     * view is the main widget which represents your working area. The
     * View class should handle all events of the view widget.  It is
     * kept empty so you can create your view according to your
     * application's needs by changing the view class.
     */
    RosegardenGUIView* m_view;

    /**
     * doc represents your actual document and is created only
     * once. It keeps information such as filename and does the
     * serialization of your files.
     */
    RosegardenGUIDoc* m_doc;

    /**
     * KAction pointers to enable/disable actions
     */
    KRecentFilesAction* m_fileOpenRecent;

    KToggleAction* m_viewToolBar;
    KToggleAction* m_viewTracksToolBar;
    KToggleAction* m_viewStatusBar;
    KToggleAction* m_viewTransport;
    KToggleAction* m_viewTrackLabels;
    KToggleAction* m_viewSegmentParameters;
    KToggleAction* m_viewInstrumentParameters;
    KToggleAction* m_viewRulers;

    KAction *m_playTransport;
    KAction *m_stopTransport;
    KAction *m_rewindTransport;
    KAction *m_ffwdTransport; 
    KAction *m_recordTransport;
    KAction *m_rewindEndTransport;
    KAction *m_ffwdEndTransport;

    KProcess* m_sequencerProcess;

    // SequenceManager
    //
    Rosegarden::SequenceManager *m_seqManager;

    // Transport dialog pointer
    //
    Rosegarden::RosegardenTransportDialog *m_transport;

    bool m_originatingJump;


    // Use these in conjucntion with the loop button to
    // remember where a loop was if we've ever set one.
    Rosegarden::timeT m_storedLoopStart;
    Rosegarden::timeT m_storedLoopEnd;

};
 
#endif // ROSEGARDENGUI_H
