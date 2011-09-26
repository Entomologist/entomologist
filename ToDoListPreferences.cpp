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
 *  Author: David Williams <redache@googlemail.com>
 *
 */

#include <QMenu>
#include <QSqlQuery>
#include <QSqlError>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QDebug>

#include "ToDoListPreferences.h"
#include "ToDoListServiceAdd.h"
#include "ui_ToDoListPreferences.h"
#include "todolistservices/GenericWebDav.h"
#include "todolistservices/RememberTheMilk.h"
#include "todolistservices/GoogleCalendar.h"
#include "todolistservices/ServicesBackend.h"

/* This class displays configuration options for ToDo Lists. The user can add new export
 * services and view the currently configured services. They can also choose to add a prompt for
 * recipients to be added for a given ToDo List.
 *
 */
ToDoListPreferences::ToDoListPreferences(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::ToDoListPreferences)
{

    ui->setupUi(this);
    ui->addServiceButton->setIcon(style()->standardIcon(QStyle::SP_DialogApplyButton));
    ui->cancelButton->setIcon(style()->standardIcon(QStyle::SP_DialogCancelButton));

    connect(ui->serviceList, SIGNAL(customContextMenuRequested(QPoint)),
            this, SLOT(customContextMenuRequested(QPoint)));
    connect(ui->addServiceButton, SIGNAL(clicked()),
            this ,SLOT(addService()));
    connect(ui->cancelButton, SIGNAL(clicked()),
            this, SLOT(reject()));
    ui->serviceList->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->serviceList->setSelectionMode(QAbstractItemView::SingleSelection);
    populateServices();
}

ToDoListPreferences::~ToDoListPreferences()
{
    delete ui;
}

void
ToDoListPreferences::populateServices()
{
    QSqlQuery query("SELECT name,servicetype,username FROM services");
    query.exec();

    while(query.next())
    {

        QTreeWidgetItem* item = new QTreeWidgetItem;
        item->setText(0,query.value(0).toString());
        item->setText(1,query.value(1).toString());
        item->setText(2,query.value(2).toString());
        ui->serviceList->addTopLevelItem(item);

    }
}

void
ToDoListPreferences::addService()
{
    ToDoListServiceAdd* a;
    a = new ToDoListServiceAdd(this);
    QStringList services;
    services<< "Generic Web Dav" << "Google Calendar" << "Remember The Milk";
    a->setServiceCombo(services);
    if(a->exec() == QDialog::Accepted)
        insertData(a);
}

void
ToDoListPreferences::customContextMenuRequested(const QPoint &pos)
{
    QMenu menu(ui->serviceList);
    QAction* editService = menu.addAction("Edit Service details");
    QAction* deleteService = menu.addAction("Delete Service");

    connect(editService,SIGNAL(triggered()),this,SLOT(editService()));
    connect(deleteService,SIGNAL(triggered()),this,SLOT(deleteService()));
    menu.exec(ui->serviceList->mapToGlobal(pos));
}

void
ToDoListPreferences::insertData(ToDoListServiceAdd* a)
{
    QStringList data;
    QSqlQuery query;

    data = a->getData();
    QString queryString = "INSERT INTO services (servicetype,name,username,password,url) "
            "VALUES(:service,:name,:username,:password,:url)";
    query.prepare(queryString);
    query.bindValue(":service",data.at(0));
    query.bindValue(":name",data.at(1));
    query.bindValue(":username",data.at(2));
    query.bindValue(":password",data.at(3));
    if(data.length() < 5)
        query.bindValue(":url","NULL");
    else
        query.bindValue(":url",data.at(4));

    query.exec();
    ui->serviceList->clear();
    populateServices();
    //emit registerUser(data.at(0), data.at(1));
}



void
ToDoListPreferences::editService()
{
    QString name = ui->serviceList->currentItem()->text(0);
    QString servicetype,username,password,url;
    int id;
    QSqlQuery query;
    query.prepare("SELECT id,servicetype,username,password,url FROM services WHERE name=:name");
    query.bindValue(":name",name);
    query.exec();

    while(query.next())
    {
        id = query.value(0).toInt();
        servicetype = query.value(1).toString();
        username = query.value(2).toString();
        password = query.value(3).toString();
        url = query.value(4).toString();
    }

    ToDoListServiceAdd* a;
    a = new ToDoListServiceAdd(this);
    QStringList services;
    services<< "Generic Web Dav" << "Google Calendar" << "Remember The Milk";
    a->setServiceCombo(services);
    a->setData(servicetype,name,username,password,url);
    a->disableUser();
    if(a->exec() == QDialog::Accepted)
        updateData(a,id);
}

void
ToDoListPreferences::updateData(ToDoListServiceAdd *service,int serviceID)
{
    QStringList data;
    QSqlQuery query;

    data = service->getData();
    QString queryString = "UPDATE services SET servicetype = :service,name = :name,"
            "username = :username,password= :password,url = :url "
            "WHERE id = :id ";

    query.prepare(queryString);
    query.bindValue(":id",serviceID);
    query.bindValue(":service",data.at(0));
    query.bindValue(":name",data.at(1));
    query.bindValue(":username",data.at(2));
    query.bindValue(":password",data.at(3));

    if(data.length() < 5)
        query.bindValue(":url","NULL");
    else
        query.bindValue(":url",data.at(4));

    query.exec();
    ui->serviceList->clear();
    populateServices();

}

void
ToDoListPreferences::deleteService()
{

    QString name = ui->serviceList->currentItem()->text(0);
    QSqlQuery query("DELETE FROM services WHERE name = :name");
    query.bindValue(":name",name);
    query.exec();
    ui->serviceList->clear();
    populateServices();

}

