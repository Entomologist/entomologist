/*
 *  Copyright (c) 2011 SUSE Linux Products GmbH
 *  All Rights Reserved.
 *
 *  This file is part of Entomologist.
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
 *  You should have received a copy of the GNU General Public License version 2
 *  along with Foobar.  If not, see <http://www.gnu.org/licenses/>
 *
 *  Author: Matt Barringer <mbarringer@suse.de>
 *
 */

#include <QMovie>
#include <QSqlQuery>
#include <QListWidgetItem>
#include <QDebug>

#include "ErrorHandler.h"
#include "MonitorDialog.h"
#include "ui_MonitorDialog.h"

MonitorDialog::MonitorDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::MonitorDialog)
{
    QList<int> splitterSizes;
    splitterSizes << 100;
    splitterSizes << 400;

    ui->setupUi(this);
    ui->splitter->setSizes(splitterSizes);
    pSpinnerMovie = new QMovie(this);
    pSpinnerMovie->setFileName(":/spinner");
    pSpinnerMovie->setScaledSize(QSize(48,48));
    ui->spinnerLabel->setMovie(pSpinnerMovie);
    pSpinnerMovie->stop();
    ui->spinnerLabel->hide();
    ui->loadingLabel->hide();

    QSqlQuery query;
    query.exec("SELECT type, name, url, username, password, version FROM trackers");
    while (query.next())
    {
        QMap<QString, QString> tracker;
        tracker["type"] = query.value(0).toString();
        tracker["name"] = query.value(1).toString();
        tracker["url"] = query.value(2).toString();
        tracker["username"] = query.value(3).toString();
        tracker["password"] = query.value(4).toString();
        tracker["version"] = query.value(5).toString();
        mTrackerMap[tracker["name"]] = tracker;
        ui->trackerListWidget->addItem(tracker["name"]);
    }

    connect(ui->trackerListWidget, SIGNAL(itemClicked(QListWidgetItem*)),
            this, SLOT(trackerActivated(QListWidgetItem*)));
}

void
MonitorDialog::trackerActivated (QListWidgetItem *item)
{
    QMap<QString, QString> tracker = mTrackerMap[item->text()];
    qDebug() << "Activating " << item->text();
    qDebug() << "With url: " << tracker["url"];
}

MonitorDialog::~MonitorDialog()
{
    delete ui;
}

void MonitorDialog::changeEvent(QEvent *e)
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
