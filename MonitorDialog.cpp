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
#include <QSqlError>
#include <QListWidgetItem>
#include <QDebug>
#include <QMessageBox>

#include "trackers/Backend.h"
#include "trackers/Bugzilla.h"
#include "trackers/Mantis.h"
#include "trackers/Trac.h"
#include "SqlUtilities.h"
#include "ErrorHandler.h"
#include "MonitorDialog.h"
#include "ui_MonitorDialog.h"

MonitorDialog::MonitorDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::MonitorDialog)
{
    mRequests = 0;
    mComponentCount = 0;
    ui->setupUi(this);
    ui->okButton->setIcon(style()->standardIcon(QStyle::SP_DialogOkButton));
    ui->cancelButton->setIcon(style()->standardIcon(QStyle::SP_DialogCancelButton));
    pSpinnerMovie = new QMovie(this);
    pSpinnerMovie->setFileName(":/spinner");
    pSpinnerMovie->setScaledSize(QSize(48,48));
    ui->spinnerLabel->setMovie(pSpinnerMovie);
    startSync();

    QList <QMap<QString, QString> > trackerList;
    QSqlQuery query;
    query.exec("SELECT type, name, url, username, password, version, id, monitored_components FROM trackers");
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
        QString monitorList = query.value(7).toString();
        QStringList componentList = monitorList.split(",");
        mComponentMap[tracker["name"]] = componentList;
        trackerList << tracker;
    }

    for (int i = 0; i < trackerList.size(); ++i)
    {
        QMap<QString, QString> t;
        t = trackerList.at(i);
        if (t["type"] == "bugzilla")
        {
            setupBackend(new Bugzilla(t["url"]), t);
        }
        else if (t["type"] == "trac")
        {
            setupBackend(new Trac(t["url"], t["username"], t["password"], this), t);
        }
        else if (t["type"] == "mantis")
        {
            setupBackend(new Mantis(t["url"]), t);
        }
    }

    connect(ui->treeWidget, SIGNAL(itemExpanded(QTreeWidgetItem*)),
            this, SLOT(itemExpanded(QTreeWidgetItem*)));
    connect(ui->okButton, SIGNAL(clicked()),
            this, SLOT(okClicked()));
    connect(ui->cancelButton, SIGNAL(clicked()),
            this, SLOT(reject()));
}

MonitorDialog::~MonitorDialog()
{
    delete ui;
}

void
MonitorDialog::okClicked()
{
    mComponentMap.clear();
    findCheckedChildren(ui->treeWidget->invisibleRootItem());
    QString text;
    if (mComponentCount > 0)
        text = QString(tr("Are you sure you want to monitor %n component(s)?", "", mComponentCount));
    else
        text = QString(tr("Are you sure you don't want to monitor any components?"));

    QMessageBox msgBox;
    msgBox.setIcon(QMessageBox::Warning);
    msgBox.setText(text);
    if (mComponentCount > 0)
        msgBox.setInformativeText("This may result in a <b>very</b> large list of bugs.");
    msgBox.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
    msgBox.setDefaultButton(QMessageBox::No);
    if (msgBox.exec() == QMessageBox::Yes)
    {
        QMapIterator<QString, QStringList> i(mComponentMap);
        while (i.hasNext())
        {
            i.next();
            insertMonitorList(i.key(), i.value());
        }
        accept();
    }
    else
    {
        mComponentCount = 0;
    }
}

void
MonitorDialog::insertMonitorList(const QString &trackerName,
                                 const QStringList &components)
{
    QString componentList = components.join(",");
    QSqlQuery q;
    q.prepare("UPDATE trackers SET monitored_components = :componentList WHERE name = :name");
    q.bindValue(":componentList", componentList);
    q.bindValue(":name", trackerName);
    if (!q.exec())
    {
        qDebug() << "insertMonitorList failed: " << q.lastError().text();
        ErrorHandler::handleError("Could not update the monitored components list!", q.lastError().text());
    }
}

// Iterate through the tree and see if any of the items are checked off
void
MonitorDialog::findCheckedChildren(QTreeWidgetItem *item)
{
    if (item->checkState(0) == Qt::Checked)
    {
        QStringList q;
        Backend *b = item->data(0, MONITOR_NODE_TRACKER).value<Backend*>();
        if (mComponentMap.contains(b->name()))
        {
                q = mComponentMap.value(b->name());
        }

        mComponentCount++;
        if (!item->data(0, MONITOR_NODE_PARENT).isNull())
            q << QString("%1:%2")
                         .arg(item->data(0, MONITOR_NODE_PARENT).toString())
                         .arg(item->text(0));
        else
            q << item->text(0);
        mComponentMap.insert(b->name(), q);
    }
    else
    {
        for (int i = 0; i < item->childCount(); ++i)
        {
            findCheckedChildren(item->child(i));
        }
    }
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
    connect(b, SIGNAL(fieldsFound()),
            this, SLOT(componentsFound()));
    connect(b, SIGNAL(backendError(QString)),
            this, SLOT(backendError(QString)));
    mRequests++;
    qDebug() << "Checking components for " << b->name() << "...";
    b->checkValidComponents();
}

void
MonitorDialog::componentsFound()
{
    Backend *backend = qobject_cast<Backend*>(sender());
    QStringList components = SqlUtilities::fieldValues(backend->id(), "component");
    QVariant backendVariant;
    backendVariant.setValue(backend);
    QString type = backend->type();
    int i = 0;
    QTreeWidgetItem *trackerItem = NULL;
    QTreeWidgetItem *productItem, *componentItem;

    ui->treeWidget->setUpdatesEnabled(false);
    QList<QTreeWidgetItem *> list = ui->treeWidget->findItems(backend->name(), Qt::MatchExactly);
    if (!list.empty())
        trackerItem = list.at(0);

    if (trackerItem == NULL)
    {
        trackerItem = new QTreeWidgetItem(ui->treeWidget);
        trackerItem->setText(0, backend->name());
        trackerItem->setData(0, MONITOR_NODE_TRACKER, backendVariant);
        trackerItem->setData(0, MONITOR_NODE_IS_PRODUCT, 0);
    }

    if ((type == "bugzilla") || (type == "mantis"))
    {
        // Bugzilla and mantis components come back as Product:Component
        // (for Bugzilla) or Project:Category (for Mantis)
        QString version = backend->version();

        for (i = 0; i < components.size(); ++i)
        {
            QString product = components.at(i).section(':', 0, 0);
            QString component = components.at(i).section(':', 1,-1);
            productItem = mProductMap.value(QString("%1:%2").arg(backend->name()).arg(product), NULL);
            if (productItem == NULL)
            {
                productItem = new QTreeWidgetItem(trackerItem);
                productItem->setData(0, MONITOR_NODE_TRACKER, backendVariant);
                productItem->setData(0, MONITOR_NODE_IS_PRODUCT, 1);
                productItem->setText(0, product);
                mProductMap[QString("%1:%2").arg(backend->name()).arg(product)] = productItem;
            }

            productItem->setData(0, MONITOR_NODE_IS_CACHED, 1);

            // 3.2 and 3.4 don't support grabbing all of the Product:Component
            // values in one call, so we hack around that by first getting the product list,
            // and then making the XMLRPC call to get the components when the product
            // node is expanded.
            if (type == "bugzilla" && ((version == "3.2") || (version == "3.4")))
                if (component == "No data cached")
                    productItem->setData(0, MONITOR_NODE_IS_CACHED, 0);

            componentItem = new QTreeWidgetItem(productItem);
            componentItem->setText(0, component);
            componentItem->setFlags(Qt::ItemIsUserCheckable|Qt::ItemIsSelectable|Qt::ItemIsEnabled);
            componentItem->setCheckState(0, Qt::Unchecked);
            componentItem->setData(0, MONITOR_NODE_PARENT, product);
            componentItem->setData(0, MONITOR_NODE_TRACKER, backendVariant);
            componentItem->setData(0, MONITOR_NODE_IS_PRODUCT, 0);
            if (mComponentMap.value(backend->name()).contains(components.at(i)))
                componentItem->setCheckState(0, Qt::Checked);

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
            componentItem->setData(0, MONITOR_NODE_TRACKER, backendVariant);
            componentItem->setData(0, MONITOR_NODE_IS_PRODUCT, 0);
            if (mComponentMap.value(backend->name()).contains(components.at(i)))
                componentItem->setCheckState(0, Qt::Checked);

        }
    }

    ui->treeWidget->addTopLevelItem(trackerItem);
    ui->treeWidget->setUpdatesEnabled(true);
    checkRequests();
}

void
MonitorDialog::backendError(const QString &msg)
{
    Q_UNUSED(msg);
    checkRequests();
}

void
MonitorDialog::itemExpanded(QTreeWidgetItem *item)
{
    int product = item->data(0, MONITOR_NODE_IS_PRODUCT).toInt();
    int cached = item->data(0, MONITOR_NODE_IS_CACHED).toInt();
    Backend *b = item->data(0, MONITOR_NODE_TRACKER).value<Backend*>();
    if (product && !cached)
    {
        startSync();
        qDeleteAll(item->takeChildren());
        mRequests++;
        b->checkValidComponentsForProducts(item->text(0));
    }
}

void
MonitorDialog::startSync()
{
    pSpinnerMovie->start();
    ui->spinnerLabel->show();
    ui->loadingLabel->show();
    ui->treeWidget->setEnabled(false);
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
