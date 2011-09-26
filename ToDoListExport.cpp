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
#include <QSqlQuery>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include "ToDoListExport.h"
#include "ui_ToDoListExport.h"


ToDoListExport::ToDoListExport(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::ToDoListExport)
{
    ui->setupUi(this);
    populateServices();
    ui->recipientFrame->hide();
    ui->exportButton->setIcon(style()->standardIcon(QStyle::SP_DialogApplyButton));
    ui->cancelButton->setIcon(style()->standardIcon(QStyle::SP_DialogCancelButton));
    ui->exportList->setSelectionMode(QAbstractItemView::MultiSelection);
}

ToDoListExport::~ToDoListExport()
{
    delete ui;
}

void
ToDoListExport::populateServices()
{

    QSqlQuery query("SELECT name,servicetype FROM services");
    query.exec();

    while(query.next())
    {
        QTreeWidgetItem* item = new QTreeWidgetItem;
        item->setText(0,query.value(0).toString());
        item->setText(1,query.value(1).toString());
        ui->exportList->addTopLevelItem(item);
    }
}

void
ToDoListExport::populateRecipients()
{


}

QMap<QString,QString>
ToDoListExport::getData()
{
   QList<QTreeWidgetItem*> items = ui->exportList->selectedItems();

   QMap<QString,QString>itemNames;

   foreach(QTreeWidgetItem* item, items)
   {

       itemNames.insert(item->text(1),item->text(0));
   }

   return itemNames;
}
