/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*- vi:set ts=8 sts=4 sw=4: */
/*
    Rosegarden
    A sequencer and musical notation editor.
    Copyright 2000-2022 the Rosegarden development team.
    See the AUTHORS file for more details.

    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License as
    published by the Free Software Foundation; either version 2 of the
    License, or (at your option) any later version.  See the file
    COPYING included with this distribution for more information.
*/

#include "SqueezedLabel.h"

#include <QContextMenuEvent>
#include <QAction>
#include <QMenu>
#include <QClipboard>
#include <QApplication>
#include <QMimeData>
#if (QT_VERSION >= QT_VERSION_CHECK(5, 14, 0))
#include <QScreen>
#else
#include <QDesktopWidget>
#endif

namespace Rosegarden
{


/* This code is adapted from version 4.2 of the DE libraries
   Copyright (C) 2000 Ronny Standtke <Ronny.Standtke@gmx.de>
*/

class SqueezedLabelPrivate
{
public:
    SqueezedLabelPrivate() :
        elideMode(Qt::ElideMiddle),
        allowToolTip(false)
    {
    }

    void k_copyFullText()
    {
        QMimeData* data = new QMimeData;
        data->setText(fullText);
        QApplication::clipboard()->setMimeData(data);
    }

    QString fullText;
    Qt::TextElideMode elideMode;
    // Set to true to let the client set the tooltip.
    bool allowToolTip;
};

SqueezedLabel::SqueezedLabel(const QString &text , QWidget *parent)
        : QLabel (parent),
          d(new SqueezedLabelPrivate)
{
    setObjectName("SQUEEZED");
    setSizePolicy(QSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed));
    d->fullText = text;
    squeezeTextToLabel();
}

SqueezedLabel::SqueezedLabel(QWidget *parent)
        : QLabel (parent),
          d(new SqueezedLabelPrivate)
{
    setObjectName("SQUEEZED");
    setSizePolicy(QSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed));
}

SqueezedLabel::~SqueezedLabel()
{
    delete d;
}

void SqueezedLabel::resizeEvent(QResizeEvent *)
{
    squeezeTextToLabel();
}

QSize SqueezedLabel::minimumSizeHint() const
{
    QSize sh = QLabel::minimumSizeHint();
    sh.setWidth(-1);
    return sh;
}

QSize SqueezedLabel::sizeHint() const
{
#if (QT_VERSION >= QT_VERSION_CHECK(5, 14, 0))
    QScreen* screen = this->screen();
    int dw = screen->availableGeometry().width();
#else
    int dw = QApplication::desktop()->availableGeometry(QPoint(0, 0)).width();
#endif
    int maxWidth = dw * 3 / 4;
    QFontMetrics fm(fontMetrics());
    int textWidth = fm.boundingRect(d->fullText).width();
    if (textWidth > maxWidth) {
        textWidth = maxWidth;
    }
    return QSize(textWidth, QLabel::sizeHint().height());
}

void SqueezedLabel::setText(const QString &text)
{
    d->fullText = text;
    squeezeTextToLabel();
}

void SqueezedLabel::clear() {
    d->fullText.clear();
    QLabel::clear();
}

void SqueezedLabel::squeezeTextToLabel() {
    QFontMetrics fm(fontMetrics());
    int labelWidth = size().width();
    QStringList squeezedLines;
    bool squeezed = false;
    Q_FOREACH(const QString& line, d->fullText.split('\n')) {
        int lineWidth = fm.boundingRect(line).width();
        if (lineWidth > labelWidth) {
            squeezed = true;
            squeezedLines << fm.elidedText(line, d->elideMode, labelWidth);
        } else {
            squeezedLines << line;
        }
    }

    if (squeezed) {
        QLabel::setText(squeezedLines.join("\n"));
        if (!d->allowToolTip)
            setToolTip(d->fullText);
    } else {
        QLabel::setText(d->fullText);
        if (!d->allowToolTip)
            setToolTip(QString());
    }
}

void SqueezedLabel::setAlignment(Qt::Alignment alignment)
{
    // save fullText and restore it
    QString tmpFull(d->fullText);
    QLabel::setAlignment(alignment);
    d->fullText = tmpFull;
}

Qt::TextElideMode SqueezedLabel::textElideMode() const
{
    return d->elideMode;
}

void SqueezedLabel::setTextElideMode(Qt::TextElideMode mode)
{
    d->elideMode = mode;
    squeezeTextToLabel();
}

void SqueezedLabel::allowToolTip()
{
    d->allowToolTip = true;
}

void SqueezedLabel::contextMenuEvent(QContextMenuEvent* ev)
{
    // "We" means the KDE team here (comment written by David Faure).
    //
    // We want to reimplement "Copy" to include the elided text.
    // But this means reimplementing the full popup menu, so no more
    // copy-link-address or copy-selection support anymore, since we
    // have no access to the QTextDocument.
    // Maybe we should have a boolean flag in SqueezedLabel itself for
    // whether to show the "Copy Full Text" custom popup?
    // For now I chose to show it when the text is squeezed; when it's not, the
    // standard popup menu can do the job (select all, copy).

    const bool squeezed = text() != d->fullText;
    const bool showCustomPopup = squeezed;
    if (showCustomPopup) {
        QMenu menu(this);

        QAction* act = new QAction(tr("&Copy Full Text"), this);
        connect(act, SIGNAL(triggered()), this, SLOT(k_copyFullText()));
        menu.addAction(act);

        ev->accept();
        menu.exec(ev->globalPos());
    } else {
        QLabel::contextMenuEvent(ev);
    }
}


}

#include "moc_SqueezedLabel.cpp"
