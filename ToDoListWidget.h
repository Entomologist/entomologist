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

#ifndef TODOLISTWIDGET_H
#define TODOLISTWIDGET_H

#include <QWidget>
#include <QDate>
#include <QCheckBox>
#include <QDateEdit>
#include <QTreeWidgetItem>
#include <QProgressDialog>
#include <QStack>
#include "todolistservices/RememberTheMilk.h"
#include "todolistservices/GoogleTasks.h"
#include "ToDoItem.h"
#include "ToDoList.h"

namespace Ui {

    class ToDoListWidget;

}


class ToDoListWidget : public QWidget
{
    Q_OBJECT


public:

    // POD for services
    typedef struct serviceData {
           QString serviceName;
           QString serviceType;
    } serviceData_t;

    explicit ToDoListWidget(QWidget *parent = 0);
    ~ToDoListWidget();
    int toDoListAdded(const QString &name,
                       const QString &rtmID,
                       const QString &googleID,
                       const QString &dbId,
                       bool isNew);
    QString toDoListInsert(QString name);
    void closeEvent(QCloseEvent *event);

public slots:
    void bugAdded(const QString &data, int parent);
    void bugMoved(int newParentId, int bugId);
    void checkStateChanged(QTreeWidgetItem*,int);
    void dateChanged();
    void customContextMenuRequested(const QPoint &pos);
    void deleteItem();
    void newTopLevelItem();
    void editTopLevelItem();
    void syncAll(bool callback);
    void syncList(bool callback);
    void createService(serviceData_t service);
    void toDoListPreferences();
    void serviceError(const QString &message);
    void loginWaiting();
    void regSuccess();
    void authCompleted();
    void readyToAddItems();
    void syncItem();
    void startSync();


private:
    QStack<serviceData_t> getExports();
    QStringList bugDataItems(int bugID,
                             int trackerID,
                             const QString &trackerTable);
    void addItem(int bugID,
                 int trackerID,
                 int dbID,
                 const QString &trackerTable,
                 int parent,
                 const QString &date,
                 int completed,
                 bool isNew);
    void findItems();
    Ui::ToDoListWidget *ui;
    int checkUniqueBug(int bugID, int trackerID, int listID);
    bool checkUniqueList(QString name);
    QString dateFormat;
    void setDateColor(QTreeWidgetItem* current);
    QTreeWidgetItem* findParent(QTreeWidgetItem* current);
    void addServiceToList(QString serviceName,ToDoList* list);
    QString syncName;
    QMap<QString,ToDoList*> lists;
    QMultiMap<QString,ToDoItem*> items;
    QList<ToDoList*> syncLists;
    QList<ToDoItem*> syncItems;
    ToDoList* currentList;
    QStringList currentServices(QString list);
    bool hasBeenSynced(ToDoList * list);
    bool syncSingle;
    int findItem(QString targetName,QList<ToDoItem*> list);
    int timerCount;
    int numberofItemsToSync;
    QStack<serviceData_t> mSyncServices;
    QTimer* timer;
    bool mNeedExports;
    QProgressDialog* progress;
    void insertAuthToken(GoogleTasks*);
    QTreeWidgetItem* findTopLevelItemIndex(int itemID);
    bool mNewSync;
    bool mIgnoreItemSync;
};

#endif // TODOLISTWIDGET_H
