/***************************************************************************
                          rosegardenguidoc.h  -  description
                             -------------------
    begin                : Mon Jun 19 23:41:03 CEST 2000
    copyright            : (C) 2000 by Guillaume Laurent, Chris Cannam, Rich Bown
    email                : glaurent@telegraph-road.org, cannam@all-day-breakfast.com, bownie@bownie.com
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef ROSEGARDENGUIDOC_H
#define ROSEGARDENGUIDOC_H

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif 

// include files for QT
#include <qobject.h>
#include <qstring.h>
#include <qlist.h>
#include <qxml.h>

// Element
#include "Element2.h"

// forward declaration of the RosegardenGUI classes
class RosegardenGUIView;

/**	RosegardenGUIDoc provides a document object for a document-view model.
  *
  * The RosegardenGUIDoc class provides a document object that can be used in conjunction with the classes RosegardenGUIApp and RosegardenGUIView
  * to create a document-view model for standard KDE applications based on KApplication and KTMainWindow. Thereby, the document object
  * is created by the RosegardenGUIApp instance and contains the document structure with the according methods for manipulation of the document
  * data by RosegardenGUIView objects. Also, RosegardenGUIDoc contains the methods for serialization of the document data from and to files.
  *
  * @author Source Framework Automatically Generated by KDevelop, (c) The KDevelop Team. 	
  * @version KDevelop version 0.4 code generation
  */
class RosegardenGUIDoc : public QObject
{
    Q_OBJECT
public:

    /** Constructor for the fileclass of the application */
    RosegardenGUIDoc(QWidget *parent, const char *name=0);
    /** Destructor for the fileclass of the application */
    ~RosegardenGUIDoc();

    /** adds a view to the document which represents the document contents. Usually this is your main view. */
    void addView(RosegardenGUIView *view);

    /** removes a view from the list of currently connected views */
    void removeView(RosegardenGUIView *view);

    /** sets the modified flag for the document after a modifying action on the view connected to the document.*/
    void setModified(bool _m=true){ m_modified=_m; };

    /** returns if the document is modified or not. Use this to determine if your document needs saving by the user on closing.*/
    bool isModified(){ return m_modified; };

    /** "save modified" - asks the user for saving if the document is modified */
    bool saveIfModified();	

    /** deletes the document's contents */
    void deleteContents();

    /** initializes the document generally */
    bool newDocument();

    /** closes the acutal document */
    void closeDocument();

    /** loads the document by filename and format and emits the updateViews() signal */
    bool openDocument(const QString &filename, const char *format=0);

    /** saves the document under filename and format.*/	
    bool saveDocument(const QString &filename, const char *format=0);

    /** sets the path to the file connected with the document */
    void setAbsFilePath(const QString &filename);

    /** returns the pathname of the current document file*/
    const QString &getAbsFilePath() const;

    /** sets the filename of the document */
    void setTitle(const QString &_t);

    /** returns the title of the document */
    const QString &getTitle() const;

    /** deletes the document views */
    void deleteViews();

    EventList&       getEvents()       { return m_events; }
    const EventList& getEvents() const { return m_events; }

public slots:

    /** calls repaint() on all views connected to the document object
     * and is called by the view by which the document has been
     * changed.  As this view normally repaints itself, it is excluded
     * from the paintEvent.
     */
    void slotUpdateAllViews(RosegardenGUIView *sender);

protected:
    bool xmlParse(QFile &file);
    // bool xmlParseElement(const QDomElement &elmnt);
 	
public:	
    /** the list of the views currently connected to the document */
    static QList<RosegardenGUIView> *pViewList;	

private:
    /** the modified flag of the current document */
    bool m_modified;

    /** the title of the current document */
    QString m_title;

    /** absolute file path of the current document */
    QString m_absFilePath;

    /** the document's data : the events (or elements) constituting
     * the document
    */
    EventList m_events;

};

#endif // ROSEGARDENGUIDOC_H
