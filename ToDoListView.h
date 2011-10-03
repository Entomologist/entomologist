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

#ifndef TODOLISTVIEW_H
#define TODOLISTVIEW_H

#include <QDialog>
#include <QDate>
#include <QCheckBox>
#include <QDateEdit>
#include <QTreeWidgetItem>
#include <QProgressDialog>
#include "todolistservices/RememberTheMilk.h"
#include "todolistservices/GoogleTasks.h"
#include "ToDoItem.h"
#include "ToDoList.h"

namespace Ui {

    class ToDoListView;

}


class ToDoListView : public QDialog
{
    Q_OBJECT

public:
    explicit ToDoListView(QWidget *parent = 0);
    ~ToDoListView();
    void toDoListAdded(const QString &name,
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
    void syncAll();
    void syncList();
    void createService(QString servicetype,QString serviceName);
    void toDoListPreferences();
    void serviceError(const QString &message);
    void loginWaiting();
    void regSuccess();
    void authCompleted();
    void readyToAddItems();
    void syncItem();

private:
    QMap<QString,QString> getExports();
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
    Ui::ToDoListView *ui;
    int checkUniqueBug(int bugID, int trackerID, int listID);
    bool checkUniqueList(QString name);
    QString dateFormat;
    void setDateColor(QTreeWidgetItem* current);
    QTreeWidgetItem* findParent(QTreeWidgetItem* current);
    bool isLoginOnly;
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
    QTimer* timer;
    QProgressDialog* progress;
    void insertAuthToken(GoogleTasks*);
    QTreeWidgetItem* findTopLevelItemIndex(int itemID);
    bool mIgnoreItemSync;
};

#endif // TODOLISTVIEW_H
