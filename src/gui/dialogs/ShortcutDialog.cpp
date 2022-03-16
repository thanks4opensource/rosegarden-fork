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

#define RG_MODULE_STRING "[ShortcutDialog]"
//#define RG_NO_DEBUG_PRINT

#include "ShortcutDialog.h"

#include "gui/general/ActionData.h"
#include "misc/ConfigGroups.h"
#include "misc/Debug.h"
#include "gui/dialogs/ShortcutWarnDialog.h"
#include "gui/dialogs/ShortcutDelegate.h"
#include "gui/general/ResourceFinder.h"

#include <QSortFilterProxyModel>
#include <QTreeView>
#include <QLineEdit>
#include <QLabel>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QStandardItemModel>
#include <QSettings>
#include <QItemSelection>
#include <QKeySequenceEdit>
#include <QPushButton>
#include <QComboBox>
#include <QMessageBox>
#include <QDialogButtonBox>
#include <QKeyEvent>

namespace Rosegarden
{

ShortcutDialog::ShortcutDialog(QWidget *parent) :
    QDialog(parent),
    m_editRow(-1),
    m_selectionChanged(false)
{
    setModal(true);
    setWindowTitle(tr("Keyboard Shortcuts"));
    ActionData* adata = ActionData::getInstance();
    adata->resetChanges();
    m_model = adata->getModel();

    connect(m_model,
            SIGNAL(dataChanged(const QModelIndex&, const QModelIndex&)),
            this,
            SLOT(dataChanged(const QModelIndex&, const QModelIndex&)));

    m_proxyModel = new QSortFilterProxyModel(this);
    m_proxyModel->setSourceModel(m_model);
    m_proxyModel->setFilterCaseSensitivity(Qt::CaseInsensitive);
    m_proxyModel->setFilterKeyColumn(-1);

    m_proxyView = new QTreeView;
    m_proxyView->setRootIsDecorated(false);
    m_proxyView->setAlternatingRowColors(true);
    m_proxyView->setModel(m_proxyModel);
    m_proxyView->setSortingEnabled(true);
    m_delegate = new ShortcutDelegate(this);
    m_proxyView->setItemDelegate(m_delegate);

    connect(m_proxyView->selectionModel(),
            SIGNAL(selectionChanged(const QItemSelection&,
                                    const QItemSelection&)),
            this,
            SLOT(selectionChanged(const QItemSelection&,
                                  const QItemSelection&)));

    m_filterPatternLineEdit = new QLineEdit;
    m_filterPatternLabel = new QLabel(tr("Filter pattern:"));

    connect(m_filterPatternLineEdit, SIGNAL(textChanged(const QString&)),
            this, SLOT(filterChanged()));

    QGridLayout *proxyLayout = new QGridLayout;
    proxyLayout->addWidget(m_filterPatternLabel, 0, 0);
    proxyLayout->addWidget(m_filterPatternLineEdit, 0, 1, 1, 3);
    proxyLayout->addWidget(m_proxyView, 1, 0, 1, 4);

    QVBoxLayout *mainLayout = new QVBoxLayout;

    setLayout(mainLayout);

    QLabel* helpLabel = new QLabel;
    helpLabel->setWordWrap(true);
    helpLabel->setText(tr("<p>Select an action in the table below then double click one of the four <b>shortcut</b> fields in the table.  Press the new shortcut key.  A shortcut can be removed by pressing Shift in the <b>edit shortcut</b> field.</p><p><i>Actions marked with a <span style=\"background-color:cyan; color:black\">light blue background</span> are global and valid for all windows.</i></p>"));

    mainLayout->addWidget(helpLabel);

    m_proxyView->sortByColumn(0, Qt::AscendingOrder);

    QSettings settings;
    settings.beginGroup(WindowGeometryConfigGroup);
    this->restoreGeometry(settings.value("Shortcut_Dialog").toByteArray());
    QStringList columnWidths =
        settings.value("Shortcut_Table_Widths").toStringList();
    settings.endGroup();

    // set column widths
    for (int i = 0; i < columnWidths.size(); i++) {
        m_proxyView->setColumnWidth(i, columnWidths[i].toInt());
    }

    QHBoxLayout *hlayout = new QHBoxLayout;
    m_clabel = new QLabel;
    m_alabel = new QLabel;
    QFont font = m_clabel->font();
    font.setPointSize(20);
    font.setBold(true);
    m_clabel->setFont(font);
    m_alabel->setFont(font);
    m_ilabel = new QLabel;
    m_ilabel->setStyleSheet("QLabel { background-color : white }");
    hlayout->addWidget(m_clabel);
    hlayout->addWidget(m_alabel);
    hlayout->addWidget(m_ilabel);

    QHBoxLayout *hlayout2 = new QHBoxLayout;
    m_defPB = new QPushButton(tr("Reset to Defaults"));
    connect(m_defPB, SIGNAL(clicked()),
            this, SLOT(defPBClicked()));
    m_defPB->setEnabled(false);
    m_clearPB = new QPushButton(tr("Remove all shortcuts"));
    connect(m_clearPB, SIGNAL(clicked()),
            this, SLOT(clearPBClicked()));
    m_clearPB->setEnabled(false);

    m_warnLabel = new QLabel(tr("Warnings when:"));
    m_warnSetting = new QComboBox;
    m_warnSetting->addItem(tr("Never"));
    m_warnSetting->addItem(tr("Conflict in same context"));
    m_warnSetting->addItem(tr("Conflict in any context"));
    connect(m_warnSetting, SIGNAL(currentIndexChanged(int)),
            this, SLOT(warnSettingChanged(int)));
    settings.beginGroup(GeneralOptionsConfigGroup);
    m_warnType = (WarningType)(settings.value("shortcut_warnings", 1).toInt());
    m_warnSetting->setCurrentIndex(m_warnType);
    settings.endGroup();

    readKeyboardShortcuts();
    m_keyboardButton = new QPushButton(tr("Apply keyboard layout"));
    connect(m_keyboardButton, SIGNAL(clicked()),
            this, SLOT(kbPBClicked()));
    m_keyboard = new QComboBox;
    int noneIndex = 0;
    int index = 0;
    foreach(auto pair, m_keyboardTranslations) {
        const QString& name = pair.first;
        const KeyboardTranslation& kbtrans = pair.second;
        QString kbText = tr(kbtrans.kbText.toStdString().c_str());
        m_keyboard->addItem(kbText);
        m_indexMap[index] = name;
        if (name == "none") noneIndex = index;
        index++;
    }
    m_keyboard->setCurrentIndex(noneIndex);

    hlayout2->addStretch();
    hlayout2->addWidget(m_defPB);
    hlayout2->addStretch();
    hlayout2->addWidget(m_clearPB);
    hlayout2->addStretch();
    hlayout2->addWidget(m_warnLabel);
    hlayout2->addWidget(m_warnSetting);
    hlayout2->addStretch();
    hlayout2->addWidget(m_keyboardButton);
    hlayout2->addWidget(m_keyboard);
    hlayout2->addStretch();

    QFrame* line = new QFrame();
    line->setFrameShape(QFrame::HLine);
    line->setFrameShadow(QFrame::Sunken);

    QDialogButtonBox *buttonBox =
        new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    QObject::connect(buttonBox, &QDialogButtonBox::accepted,
                     this, &QDialog::accept);
    QObject::connect(buttonBox, &QDialogButtonBox::rejected,
                     this, &ShortcutDialog::reject);

    mainLayout->addLayout(hlayout);
    mainLayout->addLayout(hlayout2);
    mainLayout->addWidget(line);
    mainLayout->addLayout(proxyLayout);
    mainLayout->addWidget(buttonBox);
}

ShortcutDialog::~ShortcutDialog()
{
    QStringList columnWidths;
    // save column widths (except for last one)
    for (int i = 0; i < m_model->columnCount() - 1; i++) {
        columnWidths << QString::number(m_proxyView->columnWidth(i));
    }
    QSettings settings;
    settings.beginGroup(WindowGeometryConfigGroup);
    settings.setValue("Shortcut_Dialog", this->saveGeometry());
    settings.setValue("Shortcut_Table_Widths", columnWidths);
    settings.endGroup();

    // save any changes to settings
    ActionData* adata = ActionData::getInstance();
    adata->saveUserShortcuts();
    if (adata->hasDataChanged()) {
        QMessageBox::information(this,
                                 tr("Shortcuts Changed"),
                                 tr("You must restart Rosegarden for shortcut changes to take effect."));
    }
    delete m_delegate;
}

void ShortcutDialog::setModelData(const QKeySequence ks,
                                  const QModelIndex &index)
{
    // index is an index for the proxy model
    QModelIndex srcIndex = m_proxyModel->mapToSource(index);
    int row = srcIndex.row();
    int column = srcIndex.column();
    ActionData* adata = ActionData::getInstance();
    QString key = adata->getKey(row);
    RG_DEBUG << "setModelData" << key << ks;
    std::set<QKeySequence> ksOld = adata->getShortcuts(key);
    std::set<QKeySequence> ksSet;
    unsigned int editIndex = column - 4;
    RG_DEBUG << "setModelData editIndex:" << editIndex;
    if (editIndex >= ksOld.size() && !ks.isEmpty()) {
        ksSet.insert(ks);
    }
    unsigned int kssIndex = 0;
    QKeySequence toRemove;
    foreach(auto dks, ksOld) {
        if (kssIndex == editIndex) {
            if (! dks.isEmpty()) {
                toRemove = dks;
            }
            if (! ks.isEmpty()) {
                ksSet.insert(ks);
            }
        } else {
            ksSet.insert(dks);
        }
        kssIndex++;
    }
    if (! toRemove.isEmpty()) {
        ksSet.erase(toRemove);
    }
    // debug
    QStringList sl;
    foreach(auto k, ksOld) {
        sl << k.toString();
    }
    RG_DEBUG << "setModelData old:" << sl;
    sl.clear();
    foreach(auto k, ksSet) {
        sl << k.toString();
    }
    RG_DEBUG << "setModelData new:" << sl;
    // debug end
    if (ksSet == ksOld) {
        RG_DEBUG << "setModelData no change";
        return;
    }
    ActionData::DuplicateData duplicates;
    if (m_warnType != None) {
        QString context = "";
        if (m_warnType == SameContext) {
            QStringList klist = m_editKey.split(":");
            context = klist[0];
        }
        adata->getDuplicateShortcuts(m_editKey, ksSet, false,
                                     context, duplicates);
        if (! duplicates.duplicateMap.empty()) {
            // ask the user
            ShortcutWarnDialog warnDialog(duplicates);
            if (warnDialog.exec() != QDialog::Accepted) {
                RG_DEBUG << "setModelData warnDialog rejected";
                // do nothing
                return;
            }
        }
    }

    // set the shortcuts
    adata->setUserShortcuts(m_editKey, ksSet);
    if (m_warnType != None) {
        // remove the duplicates
        foreach(auto pair, duplicates.duplicateMap) {
            const QKeySequence& dks = pair.first;
            const ActionData::KeyDuplicates& kdups = pair.second;
            foreach(auto kdup, kdups) {
                adata->removeUserShortcut(kdup.key, dks);
            }
        }
    }
    // refresh edit data
    editRow();
}

void ShortcutDialog::filterChanged()
{
    m_proxyModel->setFilterFixedString(m_filterPatternLineEdit->text());
}

void ShortcutDialog::selectionChanged(const QItemSelection& selected,
                                      const QItemSelection&)
{
    RG_DEBUG << "selection changed" << selected;
    m_selectionChanged = true;
    QModelIndexList indexes = selected.indexes();
    if (indexes.empty()) {
        m_defPB->setEnabled(false);
        m_clearPB->setEnabled(false);
        m_editRow = -1;
        m_editKey = "";
        editRow(); // to reset the edit data
        return;
    }

    QModelIndex index = indexes.first();
    int row = index.row();
    int column = index.column();
    RG_DEBUG << "row" << row << column << "selected";
    m_editRow = row;
    editRow();
}

void ShortcutDialog::defPBClicked()
{
    RG_DEBUG << "set shortcut to default";
    ActionData* adata = ActionData::getInstance();
    std::set<QKeySequence> ksSet;
    ActionData::DuplicateData duplicates;
    if (m_warnType != None) {
        QString context = "";
        if (m_warnType == SameContext) {
            QStringList klist = m_editKey.split(":");
            context = klist[0];
        }
        adata->getDuplicateShortcuts(m_editKey, ksSet, true,
                                     context, duplicates);
        if (! duplicates.duplicateMap.empty()) {
            // ask the user
            ShortcutWarnDialog warnDialog(duplicates);
            if (warnDialog.exec() != QDialog::Accepted) {
                RG_DEBUG << "setModelData warnDialog rejected";
                // do nothing
                return;
            }
        }
    }
    m_defPB->setEnabled(false);
    m_selectionChanged = false;
    adata->removeUserShortcuts(m_editKey);
    if (m_warnType != None) {
        // remove the duplicates
        foreach(auto pair, duplicates.duplicateMap) {
            const QKeySequence& dks = pair.first;
            const ActionData::KeyDuplicates& kdups = pair.second;
            foreach(auto kdup, kdups) {
                adata->removeUserShortcut(kdup.key, dks);
            }
        }
    }

    // If the selection has not changed - refresh edit data
    if (! m_selectionChanged) editRow();
}

void ShortcutDialog::clearPBClicked()
{
    RG_DEBUG << "remove all shortcuts";
    ActionData* adata = ActionData::getInstance();
    std::set<QKeySequence> ksSet;
    m_selectionChanged = false;
    adata->setUserShortcuts(m_editKey, ksSet);
    if (! m_selectionChanged) editRow();
}

void ShortcutDialog::kbPBClicked()
{
    int index = m_keyboard->currentIndex();
    QString name = m_indexMap[index];
    RG_DEBUG << "kbPBClicked" << name;
    // apply keyboard layout changes
    ActionData* adata = ActionData::getInstance();
    const KeyboardTranslation& trans = m_keyboardTranslations[name];
    foreach(auto pair, trans.translation) {
        const QString& src = pair.first;
        const QString& dest = pair.second;
        QKeySequence ksSrc(src);
        QKeySequence ksDest(dest);
        RG_DEBUG << "keyboard translate" << ksSrc << "->" << ksDest;
        adata->applyTranslation(ksSrc, ksDest);
    }
}

void ShortcutDialog::warnSettingChanged(int index)
{
    RG_DEBUG << "warnSettingChanged" << index;
    m_warnType = static_cast<WarningType>(index);
    QSettings settings;
    settings.beginGroup(GeneralOptionsConfigGroup);
    settings.setValue("shortcut_warnings", m_warnSetting->currentIndex());
    settings.endGroup();
}

void ShortcutDialog::reject()
{
    // if cancel is pressed we must undo all the changes
    ActionData* adata = ActionData::getInstance();
    adata->undoChanges();
    QDialog::reject();
}

void ShortcutDialog::dataChanged(const QModelIndex& topLeft,
                                 const QModelIndex& bottomRight)
{
    RG_DEBUG << "dataChanged" << topLeft.row() << topLeft.column() <<
        bottomRight.row() << bottomRight.column() <<
        (topLeft == bottomRight);
    if (topLeft != bottomRight) {
        RG_DEBUG << "dataChanged region edit";
    }
}

void ShortcutDialog::editRow()
{
    RG_DEBUG << "editRow:" << m_editRow;
    if (m_editRow == -1) {
        m_clabel->setText("");
        m_alabel->setText("");
        QPixmap noPixmap;
        m_ilabel->setPixmap(noPixmap);
        return;
    }
    QModelIndex rindex = m_proxyModel->index(m_editRow, 0);
    QModelIndex srcIndex = m_proxyModel->mapToSource(rindex);
    RG_DEBUG << "src row" << srcIndex.row();
    if (srcIndex.row() == -1) {
        m_clabel->setText("");
        m_alabel->setText("");
        QPixmap noPixmap;
        m_ilabel->setPixmap(noPixmap);
        return;
    }
    ActionData* adata = ActionData::getInstance();
    m_editKey = adata->getKey(srcIndex.row());
    RG_DEBUG << "editing key" << m_editKey;
    QModelIndex i0 = m_proxyModel->index(m_editRow, 0);
    QString ctext = m_proxyModel->data(i0, Qt::DisplayRole).toString();
    m_clabel->setText(ctext);
    QModelIndex i1 = m_proxyModel->index(m_editRow, 1);
    QString atext = m_proxyModel->data(i1, Qt::DisplayRole).toString();
    m_alabel->setText(atext);
    QModelIndex i2 = m_proxyModel->index(m_editRow, 2);
    QVariant imagev = m_proxyModel->data(i2, Qt::DecorationRole);
    QIcon icon = imagev.value<QIcon>();
    if (! icon.isNull()) {
        QPixmap pixmap =
            icon.pixmap(icon.availableSizes().first());
        int w = m_ilabel->width();
        int h = m_ilabel->height();
        m_ilabel->setPixmap(pixmap.scaled(w, h, Qt::KeepAspectRatio));
    } else {
        QPixmap noPixmap;
        m_ilabel->setPixmap(noPixmap);
    }

    std::set<QKeySequence> ksSet = adata->getShortcuts(m_editKey);
    RG_DEBUG << "editRow is default:" << adata->isDefault(m_editKey, ksSet);
    if (adata->isDefault(m_editKey, ksSet)) {
        m_defPB->setEnabled(false);
    } else {
        m_defPB->setEnabled(true);
    }
    m_clearPB->setEnabled(! ksSet.empty());
}

void ShortcutDialog::keyPressEvent(QKeyEvent *event)
{
    // catch enter and return to avoid accepting the dialog with these keys
    if(event->key() == Qt::Key_Enter || event->key() == Qt::Key_Return) {
        return;
    }
    QDialog::keyPressEvent(event);
}

void ShortcutDialog::readKeyboardShortcuts()
{
    QString keyboardsXml =
        ResourceFinder().getResourcePath("locale", "keyboard_shortcuts.xml");
    if (keyboardsXml == "") {
        RG_DEBUG << "keyboard_shortcuts.xml not found";
    }
    RG_DEBUG << "keyboard_shortcuts.xml found" << keyboardsXml;
    QFile infile(keyboardsXml);

    if (infile.open(QIODevice::ReadOnly) ) {
        QXmlStreamReader stream(&infile);

        stream.readNextStartElement();
        if (stream.name().toString() != "rosegarden_keyboards") {
            RG_DEBUG << keyboardsXml << "invalid file";
            return;
        }
        QString kbName;
        QString kbText;
        KeyboardTranslation trans;
        while(!stream.atEnd()) {
            // Read to the next element delimiter
            do {
                stream.readNext();
            } while(!stream.isStartElement() &&
                    !stream.isEndElement() &&
                    !stream.atEnd());

            if (stream.atEnd()) break;

            if (stream.isStartElement()) {
                if (stream.name().toString() == "name") {
                    kbName = stream.readElementText();
                } else if (stream.name().toString() == "text") {
                    kbText = stream.readElementText();
                } else if (stream.name().toString() == "shortcut") {
                    QString src =
                        stream.attributes().value("src").toString();
                    QString dest =
                        stream.attributes().value("dest").toString();
                    trans.translation[src] = dest;
                } else {
                    RG_DEBUG << "start element" << stream.name();
                }
            }

            if (stream.isEndElement()) {
                if (stream.name().toString() == "keyboard") {
                    // Save the keyboard data
                    RG_DEBUG << "save keyboard data for" << kbName;
                    trans.kbText = kbText;
                    m_keyboardTranslations[kbName] = trans;
                    trans.translation.clear();
                } else {
                    RG_DEBUG << "end element" << stream.name();
                }
            }
        }
        infile.close();
    }
}

}
