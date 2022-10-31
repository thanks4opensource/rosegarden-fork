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

#define RG_MODULE_STRING "[CheckableListDialog]"

#include <algorithm>

#include <QColor>
#include <QDialog>
#include <QDialogButtonBox>
#include <QGroupBox>
#include <QLabel>
#include <QListWidget>
#include <QListWidgetItem>
#include <QPushButton>
#include <QVBoxLayout>

#include "CheckableListDialog.h"


namespace Rosegarden
{

CheckableListDialog::CheckableListDialog(
    QWidget                   *parent,
    const QString             &description,
    const QStringList         &stringList,
    const std::vector<QColor> &foregroundColors,
    const std::vector<QColor> &backgroundColors,
    std::vector<unsigned>     &checkeds)
:   QDialog           (parent),
    m_description     (description),
    m_stringList      (stringList),
    m_foregroundColors(foregroundColors),
    m_backgroundColors(backgroundColors),
    m_checkeds        (checkeds)
{
    setWindowTitle(description);

    createListWidget();
    createOtherWidgets();
    createLayout();
    createConnections();
}

void CheckableListDialog::createListWidget(){
    m_widget = new QListWidget;

    m_widget->addItems(m_stringList);

    QListWidgetItem* item = 0;
    for (unsigned   ndx = 0 ;
                    ndx < static_cast<unsigned>(m_widget->count()) ;
                  ++ndx) {
        item = m_widget->item(ndx);

        if (ndx < m_foregroundColors.size())
            item->setForeground(m_foregroundColors[ndx]);
        if (ndx < m_backgroundColors.size())
            item->setBackground(m_backgroundColors[ndx]);

        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);

        if (   std::find(m_checkeds.begin(), m_checkeds.end(), ndx)
            == m_checkeds.end())
            item->setCheckState(Qt::Unchecked);
        else
            item->setCheckState(Qt::Checked);
    }
}

void CheckableListDialog::createOtherWidgets(){
    m_label     = new QLabel(m_description);
    m_buttonBox = new QDialogButtonBox;

    m_allButton  =   m_buttonBox
                  ->addButton(tr("All"),
                                 QDialogButtonBox::ButtonRole::ActionRole);
    m_noneButton =   m_buttonBox
                   ->addButton(tr("None"),
                               QDialogButtonBox::ButtonRole::ActionRole);

    m_okButton     = m_buttonBox->addButton(QDialogButtonBox::Ok);
    m_cancelButton = m_buttonBox->addButton(QDialogButtonBox::Cancel);
}

void CheckableListDialog::createLayout(){
    QHBoxLayout* horizontalLayout = new QHBoxLayout;
    horizontalLayout->addWidget(m_buttonBox);

    QVBoxLayout* mainLayout = new QVBoxLayout;
    mainLayout->addWidget(m_label);
    mainLayout->addWidget(m_widget);
    mainLayout->addLayout(horizontalLayout);

    setLayout(mainLayout);
}

void CheckableListDialog::createConnections(){
    QObject::connect(m_allButton,    &QPushButton::clicked,
                     this,           &CheckableListDialog::all);
    QObject::connect(m_noneButton,   &QPushButton::clicked,
                     this,           &CheckableListDialog::none);
    QObject::connect(m_okButton,     &QPushButton::clicked,
                     this,           &CheckableListDialog::ok);
    QObject::connect(m_cancelButton, &QPushButton::clicked,
                     this,           &CheckableListDialog::cancel);
}

void CheckableListDialog::all()
{
    m_checkeds.clear();
    for (int i = 0; i < m_widget->count(); ++i) {
        m_widget->item(i)->setCheckState(Qt::Checked);
        m_checkeds.push_back(i);
    }
}

void CheckableListDialog::none()
{
    m_checkeds.clear();
    for (int i = 0; i < m_widget->count(); ++i)
        m_widget->item(i)->setCheckState(Qt::Unchecked);
}

void CheckableListDialog::ok()
{
    m_checkeds.clear();
    QListWidgetItem* item;
    for (int i = 0; i < m_widget->count(); ++i) {
        item = m_widget->item(i);
        if (item->checkState() == Qt::Checked) {
            m_checkeds.push_back(i);
        }
    }
    emit done(QDialog::Accepted);
}

void CheckableListDialog::cancel()
{
    emit done(QDialog::Rejected);
}

}  // namespace Rosegarden
