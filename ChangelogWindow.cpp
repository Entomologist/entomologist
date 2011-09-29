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
    qDebug() << "Loading changelog";
    QSqlQueryModel model;
    QString line;
    QString bugsQuery = "SELECT trackers.name, "
                   "shadow_bugs.bug_id, "
                   "bugs.severity, "
                   "shadow_bugs.severity, "
                   "bugs.priority, "
                   "shadow_bugs.priority, "
                   "bugs.assigned_to, "
                   "shadow_bugs.assigned_to, "
                   "bugs.status, "
                   "shadow_bugs.status, "
                   "bugs.summary, "
                   "shadow_bugs.summary, "
                   "shadow_bugs.id "
                   "from shadow_bugs left outer join bugs on shadow_bugs.bug_id = bugs.bug_id AND shadow_bugs.tracker_id = bugs.tracker_id "
                   "join trackers ON shadow_bugs.tracker_id = trackers.id";
    QString commentsQuery = "SELECT trackers.name, "
                            "shadow_comments.bug_id, "
                            "shadow_comments.comment, "
                            "shadow_comments.id "
                            "from shadow_comments join trackers ON shadow_comments.tracker_id = trackers.id";
    QString entry = tr("<b>%1</b> bug <font color=\"blue\"><u>%2</u></font>: Change "
                       "<b>%3</b> from <b>%4</b> to <b>%5</b><br>");
    QString commentEntry = tr("Add a comment to <b>%1</b> bug <font color=\"blue\"><b>%2</u></font>:<br>"
                              "<i>%3</i><br>");
    model.setQuery(bugsQuery);
    for (int i = 0; i < model.rowCount(); ++i)
    {
        QSqlRecord record = model.record(i);
        if (!record.value(3).isNull())
        {
            line = QString(entry).arg(record.value(0).toString(),
                             record.value(1).toString(),
                             tr("Severity"),
                             record.value(2).toString(),
                             record.value(3).toString());
            QListWidgetItem *newItem = new QListWidgetItem();
            newItem->setText(line);
            newItem->setData(Qt::UserRole, QString("bug:%1").arg(record.value(12).toString()));
            ui->listWidget->addItem(newItem);
        }

        if (!record.value(5).isNull())
        {
            line = QString(entry).arg(record.value(0).toString(),
                             record.value(1).toString(),
                             tr("Priority"),
                             record.value(4).toString(),
                             record.value(5).toString());

            QListWidgetItem *newItem = new QListWidgetItem();
            newItem->setText(line);
            newItem->setData(Qt::UserRole, QString("bug:%1").arg(record.value(12).toString()));
            ui->listWidget->addItem(newItem);
        }

        if (!record.value(7).isNull())
        {
            line = QString(entry).arg(record.value(0).toString(),
                             record.value(1).toString(),
                             tr("Assigned To"),
                             record.value(6).toString(),
                             record.value(7).toString());
            QListWidgetItem *newItem = new QListWidgetItem();
            newItem->setText(line);
            newItem->setData(Qt::UserRole, QString("bug:%1").arg(record.value(12).toString()));
            ui->listWidget->addItem(newItem);
        }
        if (!record.value(9).isNull())
        {
            line = QString(entry).arg(record.value(0).toString(),
                             record.value(1).toString(),
                             tr("Status"),
                             record.value(8).toString(),
                             record.value(9).toString());
            QListWidgetItem *newItem = new QListWidgetItem();
            newItem->setText(line);
            newItem->setData(Qt::UserRole, QString("bug:%1").arg(record.value(12).toString()));
            ui->listWidget->addItem(newItem);
        }
        if (!record.value(11).isNull())
        {
            line = QString(entry).arg(record.value(0).toString(),
                             record.value(1).toString(),
                             tr("Summary"),
                             record.value(10).toString(),
                             record.value(11).toString());
            QListWidgetItem *newItem = new QListWidgetItem();
            newItem->setText(line);
            newItem->setData(Qt::UserRole, QString("bug:%1").arg(record.value(12).toString()));
            ui->listWidget->addItem(newItem);
        }
    }

    model.setQuery(commentsQuery);
    for (int i = 0; i < model.rowCount(); ++i)
    {
        QString ret = "";
        QListWidgetItem *newItem = new QListWidgetItem();
        QSqlRecord record = model.record(i);
        ret += commentEntry.arg(record.value(0).toString(),
                                record.value(1).toString(),
                                record.value(2).toString());
        newItem->setText(ret);
        newItem->setData(Qt::UserRole, QString("comment:%1").arg(record.value(3).toString()));
        ui->listWidget->addItem(newItem);
    }

    ui->listWidget->setItemDelegate(new ChangelogListDelegate(ui->listWidget));
}

void
ChangelogWindow::revertButtonClicked()
{
    QSqlQuery q;

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
                q.exec(QString("DELETE FROM shadow_bugs WHERE id=%1").arg(idList.at(1)));
            }
            else
            {
                q.exec(QString("DELETE FROM shadow_comments WHERE id=%1").arg(idList.at(1)));
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
