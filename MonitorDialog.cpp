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

#include "trackers/Backend.h"
#include "trackers/Bugzilla.h"
#include "trackers/Mantis.h"
#include "trackers/Trac.h"

#include "ErrorHandler.h"
#include "MonitorDialog.h"
#include "ui_MonitorDialog.h"

MonitorDialog::MonitorDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::MonitorDialog)
{
    mRequests = 0;
    ui->setupUi(this);
    pSpinnerMovie = new QMovie(this);
    pSpinnerMovie->setFileName(":/spinner");
    pSpinnerMovie->setScaledSize(QSize(48,48));
    ui->spinnerLabel->setMovie(pSpinnerMovie);
    pSpinnerMovie->start();
    ui->spinnerLabel->show();
    ui->loadingLabel->show();
    ui->treeWidget->setEnabled(false);

    QList <QMap<QString, QString> > trackerList;
    QSqlQuery query;
    query.exec("SELECT type, name, url, username, password, version, id FROM trackers");
    while (query.next())
    {
        QMap<QString, QString> tracker;
        tracker["type"] = query.value(0).toString();
        tracker["name"] = query.value(1).toString();
        tracker["url"] = query.value(2).toString();
        tracker["username"] = query.value(3).toString();
        tracker["password"] = query.value(4).toString();
        tracker["version"] = query.value(5).toString();
        tracker["id"] = query.value(6).toString();
        trackerList << tracker;
    }

    for (int i = 0; i < trackerList.size(); ++i)
    {
        QMap<QString, QString> t;
        t = trackerList.at(i);
        if (t["type"] == "Bugzilla")
        {
            setupBackend(new Bugzilla(t["url"]), t);
        }
        else if (t["type"] == "Trac")
        {
            setupBackend(new Trac(t["url"], t["username"], t["password"], this), t);
        }
        else if (t["type"] == "Mantis")
        {
            setupBackend(new Mantis(t["url"]), t);
        }
    }
}

MonitorDialog::~MonitorDialog()
{
    delete ui;
}
void
MonitorDialog::setupBackend(Backend *b, QMap<QString, QString> tracker)
{
    b->setId(tracker["id"]);
    b->setName(tracker["name"]);
    b->setUrl(tracker["url"]);
    b->setUsername(tracker["username"]);
    b->setPassword(tracker["password"]);
    b->setVersion(tracker["version"]);
    b->setParent(this);
    connect(b, SIGNAL(componentsFound(QStringList)),
            this, SLOT(componentFound(QStringList)));
    connect(b, SIGNAL(backendError(QString)),
            this, SLOT(backendError(QString)));
    mRequests++;
    b->checkValidComponents();
}

void
MonitorDialog::componentFound(QStringList components)
{
    qDebug() << "Components have been found: " << components;
    Backend *backend = qobject_cast<Backend*>(sender());
    QString type = backend->type();
    int i = 0;
    QTreeWidgetItem *trackerItem;
    QTreeWidgetItem *productItem, *componentItem;

    ui->treeWidget->setUpdatesEnabled(false);
    trackerItem = new QTreeWidgetItem(ui->treeWidget);
    trackerItem->setText(0, backend->name());

    if ((type == "Bugzilla") || (type == "Mantis"))
    {
        QMap<QString, QTreeWidgetItem *> productMap;

        // Bugzilla and mantis components come back as Product:Component
        // (for Bugzilla) or Project:Category (for Mantis)
        for (i = 0; i < components.size(); ++i)
        {
            QString product = components.at(i).section(':', 0, 0);
            QString component = components.at(i).section(':', 1,-1);
            productItem = productMap.value(product, NULL);
            if (productItem == NULL)
            {
                productItem = new QTreeWidgetItem(trackerItem);
                productItem->setText(0, product);
                productMap[product] = productItem;
            }
            componentItem = new QTreeWidgetItem(productItem);
            componentItem->setText(0, component);
            componentItem->setFlags(Qt::ItemIsUserCheckable|Qt::ItemIsSelectable|Qt::ItemIsEnabled);
            componentItem->setCheckState(0, Qt::Unchecked);
            componentItem->setData(0, Qt::UserRole, product);
        }
    }
    else
    {
        for (i = 0; i < components.size(); ++i)
        {
            componentItem = new QTreeWidgetItem(trackerItem);
            componentItem->setText(0, components.at(i));
            componentItem->setFlags(Qt::ItemIsUserCheckable|Qt::ItemIsSelectable|Qt::ItemIsEnabled);
            componentItem->setCheckState(0, Qt::Unchecked);
        }
    }
    ui->treeWidget->addTopLevelItem(trackerItem);
    ui->treeWidget->setUpdatesEnabled(true);
    checkRequests();
    backend->deleteLater();
}

void
MonitorDialog::backendError(const QString &msg)
{
    checkRequests();
}

void
MonitorDialog::checkRequests()
{
    mRequests--;
    if (mRequests <= 0)
    {
        pSpinnerMovie->stop();
        ui->spinnerLabel->hide();
        ui->loadingLabel->hide();
        ui->treeWidget->setEnabled(true);
    }
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
