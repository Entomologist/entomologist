/*
 *  Copyright (c) 2011 Novell, Inc.
 *  All Rights Reserved.
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of version 2 of the GNU General Public License as
 *  published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.   See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, contact Novell, Inc.
 *
 *  To contact Novell about this file by physical or electronic mail,
 *  you may find current contact information at www.novell.com
 *
 *  Author: Matt Barringer <mbarringer@suse.de>
 *
 */

#include <QtSql>
#include <QMessageBox>

#include "ChangelogWindow.h"
#include "SqlUtilities.h"
#include "ChangelogListDelegate.h"
#include "ui_ChangelogWindow.h"

ChangelogWindow::ChangelogWindow(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::ChangelogWindow)
{
    ui->setupUi(this);
    ui->revertButton->setEnabled(false);

    connect(ui->revertButton, SIGNAL(clicked()),
            this, SLOT(revertButtonClicked()));
    connect(ui->listWidget, SIGNAL(itemSelectionChanged()),
            this, SLOT(selectionChanged()));

}

ChangelogWindow::~ChangelogWindow()
{
    delete ui;
}

void
ChangelogWindow::loadChangelog()
{
    ui->listWidget->clear();
    QList< QMap<QString, QString> > commentsChangelist;
    QVariantList bugChangelist;
    QString id, table;
    qDebug() << "Loading changelog";
    QString line;
    QString entry = tr("<b>%1</b> bug <font color=\"blue\"><u>%2</u></font>: Change "
                       "<b>%3</b> from <b>%4</b> to <b>%5</b><br>");
    QString commentEntry = tr("Add a comment to <b>%1</b> bug <font color=\"blue\"><b>%2</u></font>:<br>"
                              "<i>%3</i><br>");

    bugChangelist = SqlUtilities::getTracChangelog();
    bugChangelist << SqlUtilities::getBugzillaChangelog();
    bugChangelist << SqlUtilities::getMantisChangelog();
    for (int i = 0; i < bugChangelist.size(); ++i)
    {
        line = "";
        QVariantList changes = bugChangelist.at(i).toList();
        for (int j = 0; j < changes.size(); ++j)
        {
            QVariantMap newChange = changes.at(j).toMap();
            qDebug() << newChange;
            id = newChange.value("id").toString();
            table = newChange.value("tracker_table").toString();
            line += QString(entry).arg(newChange.value("tracker_name").toString(),
                                      newChange.value("bug_id").toString(),
                                      newChange.value("column_name").toString(),
                                      newChange.value("from").toString(),
                                      newChange.value("to").toString());
        }
        QListWidgetItem *newItem = new QListWidgetItem();
        newItem->setText(line);
        newItem->setData(Qt::UserRole, QString("bug:%1:tracker_table:%2").arg(id).arg(table));
        ui->listWidget->addItem(newItem);
    }

    commentsChangelist = SqlUtilities::getCommentsChangelog();
    for (int i = 0; i < commentsChangelist.size(); ++i)
    {
        QString ret = "";
        QMap<QString, QString> comment = commentsChangelist.at(i);
        QListWidgetItem *newItem = new QListWidgetItem();
        ret += commentEntry.arg(comment["tracker_name"],
                                comment["bug_id"],
                                comment["comment"]);
        newItem->setText(ret);
        newItem->setData(Qt::UserRole, QString("comment:%1").arg(comment["id"]));
        ui->listWidget->addItem(newItem);
    }
    ui->listWidget->setItemDelegate(new ChangelogListDelegate(ui->listWidget));
}

void
ChangelogWindow::revertButtonClicked()
{
    QList<QListWidgetItem *> list = ui->listWidget->selectedItems();
    if (list.count() == 0) return;

    QMessageBox box;
    box.setText(QString("Are you sure you want to revert %1 changes?").arg(list.count()));
    box.setStandardButtons(QMessageBox::Yes|QMessageBox::No);
    if (box.exec() == QMessageBox::Yes)
    {
        for (int i = 0; i < list.count(); ++i)
        {
            QListWidgetItem *item = list.at(i);
            QStringList idList = item->data(Qt::UserRole).toString().split(":");
            if (idList.at(0) == "bug")
            {
                SqlUtilities::simpleDelete(idList.at(1), idList.at(3));
            }
            else
            {
                SqlUtilities::simpleDelete(idList.at(1),"shadow_comments");
            }

            ui->listWidget->takeItem(ui->listWidget->row(item));
            delete item;
        }
    }
}

void
ChangelogWindow::selectionChanged()
{
    if (ui->listWidget->selectedItems().count() >= 1)
        ui->revertButton->setEnabled(true);
    else
        ui->revertButton->setEnabled(false);
}

void ChangelogWindow::changeEvent(QEvent *e)
{
    QDialog::changeEvent(e);
    switch (e->type()) {
    case QEvent::LanguageChange:
        ui->retranslateUi(this);
        break;
    default:
        break;
    }
}
