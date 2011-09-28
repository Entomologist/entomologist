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
#include <QDrag>
#include <QDropEvent>
#include <QSqlQuery>
#include <QSqlError>
#include <QDebug>
#include <QCheckBox>
#include <QComboBox>
#include <QDesktopServices>
#include <QDir>
#include <QCalendarWidget>
#include <QSettings>
#include <QDateEdit>
#include <QDateTime>
#include <QMenu>
#include <QInputDialog>
#include <QCloseEvent>
#include <QMessageBox>
#include <QShortcut>
#include <QProgressDialog>
#include <QTimer>
#include <QtGlobal>
#include "ToDoListView.h"
#include "BugTreeItemDelegate.h"
#include "ToDoListPreferences.h"
#include "ToDoListExport.h"
#include "ui_ToDoListView.h"
#include "ToDoItem.h"
#include "todolistservices/RememberTheMilk.h"
#include "todolistservices/GoogleTasks.h"
#include "todolistservices/GenericWebDav.h"
#include "ErrorHandler.h"

/*
 * This class is the TODO list class that manages the UI and data of the TODO List.
 * It is essentially a Window with a BugTreeWidget that accepts drops from the
 * TrackerTableView and adds them to BugTreeWidget as QTreeWidgetItems. 2 Database
 * tables are used to store the data, one for 'ToDoLists', which are the TopLevel
 * names for the various ToDo Lists and 'ToDoListBugs' which stores the bug_id and
 * the data for 'completion_date' and 'completed' which are the expected date for
 * a bug to be resolved and the state of its completion respectivley.
 *
 * TODO:
 *
 * - Google Calendar-WebDAV/Pull Data from Remote services if the date/completed status have changed.
 *
 */

ToDoListView::ToDoListView(QWidget *parent) :
    QDialog(parent),

    ui(new Ui::ToDoListView)
{
    ui->setupUi(this);
    QSettings settings("Entomologist");
    restoreGeometry(settings.value("todo-window-geometry", "").toByteArray());

    // Add ToDoLists, if there aren't any, add a DEFAULT todolist
    QSqlQuery query("SELECT name, list_id, id, sync_services FROM todolist");
    query.exec();
    if(!query.next())
    {
        QString newId = toDoListInsert("Default List");
        toDoListAdded("Default List", "", newId, false);
    }
    else
    {
        toDoListAdded(query.value(0).toString(),query.value(1).toString(), query.value(2).toString(), false);
        while(query.next())
            toDoListAdded(query.value(0).toString(),query.value(1).toString(), query.value(2).toString(), false);
    }

    ui->bugTreeWidget->expandAll();
    ui->bugTreeWidget->hideColumn(1); // Hide the 'Data' Column so the internal bug ID isn't shown to the user.

    BugTreeItemDelegate* bug = new BugTreeItemDelegate;
    ui->bugTreeWidget->setItemDelegate(bug);
    dateFormat =  bug->dateFormat;

    connect(bug, SIGNAL(dateFinished()),
            this, SLOT(dateChanged()));
    connect(ui->bugTreeWidget, SIGNAL(customContextMenuRequested(QPoint)),
            this,SLOT(customContextMenuRequested(QPoint)));
    connect(ui->bugTreeWidget, SIGNAL(bugDropped(QString,int)),
            this, SLOT(bugAdded(QString,int)));
    connect(ui->bugTreeWidget, SIGNAL(bugMoved(int,int)),
            this, SLOT(bugMoved(int,int)));
    QShortcut* delKey;
    QShortcut* delBack;

    // This is to catch both forms of "delete" as on Mac OS X there is no delete key (although FN+Backspace is the same key, not many users know this)
    delKey = new QShortcut(QKeySequence(Qt::Key_Delete),this);
    delKey->setContext(Qt::WindowShortcut);
    delBack = new QShortcut(QKeySequence(Qt::Key_Backspace),this);
    delBack->setContext(Qt::WindowShortcut);
    connect(ui->syncAllButton,SIGNAL(clicked()),this,SLOT(syncAll()));
    connect(delKey,SIGNAL(activated()),this,SLOT(deleteItem()));
    connect(delBack,SIGNAL(activated()),this,SLOT(deleteItem()));
    findItems(); // Find the items already added to the ToDo List.
    isLoginOnly = false;
}

ToDoListView::~ToDoListView()
{
    delete ui;
}
void
ToDoListView::findItems()
{

    QSqlQuery query("SELECT bug_id, tracker_id, id, tracker_table, todolist_id, date, completed FROM todolistbugs");
    query.exec();

    while(query.next())
    {
        addItem(query.value(0).toInt(),
                query.value(1).toInt(),
                query.value(2).toInt(),
                query.value(3).toString(),
                query.value(4).toInt() -1,
                query.value(5).toString(),
                query.value(6).toInt(),
                false);
    }
}

void
ToDoListView::toDoListAdded(QString name, QString listID, QString dbId, bool isNew)
{

    QTreeWidgetItem* item = new QTreeWidgetItem(ui->bugTreeWidget->invisibleRootItem(), 1);
    item->setFlags(Qt::ItemIsSelectable | Qt::ItemIsDropEnabled | Qt::ItemIsEnabled);
    item->setText(0, name);
    item->setData(0, Qt::UserRole, dbId);
    ui->bugTreeWidget->addTopLevelItem(item);
    ToDoList* list = new ToDoList(name);

    if(isNew)
    {
        list->setListStatus(ToDoList::NEW);
    }
    else
    {
        list->setListID(listID);
        QStringList services = currentServices(list->listName());
        foreach(QString service,services)
            list->setServices(service);

    }

    lists.insert(name, list);
}

QString
ToDoListView::toDoListInsert(QString name)
{
    QSqlQuery query("INSERT INTO todolist (name) VALUES ( :todolistname )");
    query.bindValue(":todolistname",name);
    query.exec();
    return query.lastInsertId().toString();
}

void
ToDoListView::bugMoved(int newParentId, int bugId)
{
    QSqlQuery update("UPDATE todolistbugs SET todolist_id = :list_id WHERE id = :bug_id");
    update.bindValue(":list_id", newParentId);
    update.bindValue(":bug_id", bugId);

    if (!update.exec())
    {
        qDebug() << update.lastQuery();
        qDebug() << update.lastError();
    }
}

void
ToDoListView::bugAdded(const QString &data,
                       int parent)
{

    QStringList items = data.split(":");
    QString trackerTable = items.at(0);
    int listId =  ui->bugTreeWidget->topLevelItem((parent))->data(0, Qt::UserRole).toInt();
    int trackerID = items.at(1).toInt();
    int bugID = items.at(2).toInt();

    if (checkUniqueBug(bugID, trackerID, listId))
        return;

    qDebug() << "Checked unique";
    QSqlQuery query("INSERT INTO todolistbugs (tracker_id, tracker_table, "
                    "bug_id, todolist_id, date, "
                    "completed, last_modified) "
                    "VALUES (:tracker_id, :tracker_table, :bug_id , :parent , :date , :completed, :modified)");

    int completed = 0;
    QString date = QDate::currentDate().toString(dateFormat);

    query.bindValue(":tracker_id", trackerID);
    query.bindValue(":tracker_table", trackerTable);
    query.bindValue(":bug_id", bugID);
    query.bindValue(":parent",(parent + 1));
    query.bindValue(":date",date);
    query.bindValue(":completed",completed);
    query.bindValue(":modified",QDateTime::currentDateTime().toString());

    if(!query.exec())
    {
        qDebug() << query.lastQuery();
        qDebug() << query.lastError();
        return;
    }
    int id = query.lastInsertId().toInt();
    addItem(bugID, trackerID, id, trackerTable, parent, date, static_cast<bool>(completed), true);

}

void
ToDoListView::addItem(int bugID,
                      int trackerID,
                      int dbID,
                      const QString &trackerTable,
                      int parent,
                      const QString &date,
                      int completed,
                      bool isNew)
{
    int offset = 2; // The offset we need to add to the right columns
    QTreeWidgetItem* item = new QTreeWidgetItem;
    item->setText(1, QString::number(bugID));
    item->setData(0, Qt::UserRole, dbID);
    QStringList bugData = bugDataItems(bugID, trackerID, trackerTable);
    if (bugData.length() == 0)
        return;

    QString iconPath = QString ("%1%2%3%4%5.png")
            .arg(QDesktopServices::storageLocation(QDesktopServices::DataLocation))
            .arg(QDir::separator())
            .arg("entomologist")
            .arg(QDir::separator())
            .arg(bugData.at(1));
    if (!QFile::exists(iconPath))
        iconPath = ":/bug";

    for(int i = 0; i <  bugData.length(); i++)
        item->setText(i+offset, bugData.at(i));

    item->setFlags(Qt::ItemIsDragEnabled |Qt::ItemIsEnabled | Qt::ItemIsSelectable
                   | Qt::ItemIsUserCheckable | Qt::ItemIsEditable);

    item->setText(item->columnCount(), date);
    item->setIcon(0, QIcon(iconPath));
    ui->bugTreeWidget->topLevelItem((parent))->addChild(item);
    item->setCheckState(item->columnCount(),Qt::CheckState(completed));

    setDateColor(item);

    for(int i = 0; i < ui->bugTreeWidget->columnCount();i++)
        ui->bugTreeWidget->resizeColumnToContents(i);

    qDebug() << "Adding bugID: " << bugID;
    ToDoItem* newItem = new ToDoItem(ui->bugTreeWidget->topLevelItem(parent)->text(0),
                                     QString::number(bugID),
                                     bugData.at(0),
                                     bugData.at(2),
                                     date,"",completed);

    if(isNew)
        newItem->setStatus(ToDoItem::NEW);

    items.insert(newItem->listName(),newItem);

    connect(ui->bugTreeWidget, SIGNAL(itemClicked(QTreeWidgetItem*,int)),
            this, SLOT(checkStateChanged(QTreeWidgetItem*,int)));
}

QStringList
ToDoListView::bugDataItems(int bugID,
                           int trackerID,
                           const QString &trackerTable)
{
    QStringList info;
    QString sql =  QString("SELECT %1.bug_id, trackers.name, %1.summary "
                           "FROM %1 INNER JOIN trackers ON trackers.id "
                           "= %2 WHERE %1.bug_id = %3")
            .arg(trackerTable)
            .arg(trackerID)
            .arg(bugID);
    QSqlQuery query(sql);

    if(!query.exec())
    {
        qDebug() << query.lastError();
        qDebug() << query.lastQuery();
    }

    while(query.next())
    {
        info << query.value(0).toString();
        info << query.value(1).toString();
        info << query.value(2).toString();
    }

    return info;
}

void
ToDoListView::checkStateChanged(QTreeWidgetItem* item, int column)
{
    int id = item->data(0, Qt::UserRole).toInt();
    QSqlQuery query("UPDATE todolistbugs SET completed = :completed, last_modified = :modified WHERE "
                    "id = :bugID");

    query.bindValue(":completed",item->checkState(column));
    query.bindValue(":modified",QDateTime::currentDateTime().toString());
    query.bindValue(":bugID", id);
    query.exec();

    QList<ToDoItem*> updatedItems = items.values(findParent(item)->text(0));
    int itemIndex = findItem(item->text(1),updatedItems);

    if (itemIndex != -1)
    {
        updatedItems.at(itemIndex)->setStatus(ToDoItem::COMPLETEDCHANGED);
        updatedItems.at(itemIndex)->setCompleted(item->checkState(column));
    }
}

void
ToDoListView::dateChanged()
{
    QTreeWidgetItem* current = ui->bugTreeWidget->currentItem();
    int id = current->data(0, Qt::UserRole).toInt();
    QSqlQuery query;

    query.prepare("UPDATE todolistbugs SET date = :date, last_modified = :modified "
                  "WHERE id = :bugID");
    query.bindValue(":date",current->text(5));
    query.bindValue(":modified", QDateTime::currentDateTime().toString());
    query.bindValue(":bugID", id);

    if(!query.exec())
    {
        qDebug() << query.lastQuery();
        qDebug() << query.lastError();

    }
    QList<ToDoItem*> updatedItems = items.values(findParent(current)->text(0));

    int itemIndex = findItem(current->text(1),updatedItems);
    if(itemIndex != -1)
    {
        updatedItems.at(itemIndex)->setStatus(ToDoItem::DATECHANGED);
        updatedItems.at(itemIndex)->setDate(current->text(5));
    }
    setDateColor(current);
}

void
ToDoListView::setDateColor(QTreeWidgetItem *current)
{
    if(QDate::fromString(current->text(5),dateFormat).operator < (QDate::currentDate()))
        current->setTextColor(5,Qt::red);
    else
        current->setTextColor(5, Qt::black);
}

int
ToDoListView::checkUniqueBug(int bugID, int trackerID, int listID)
{
    qDebug() << "Checking " << bugID << trackerID << listID;
    int ret = 0;
    QSqlQuery q;
    q.prepare("SELECT id FROM todolistbugs WHERE bug_id = :bugID AND tracker_id = :trackerID AND todolist_id = :listID");
    q.bindValue(":bugID", bugID);
    q.bindValue(":trackerID", trackerID);
    q.bindValue(":listID", listID);
    if (q.exec())
    {
        if (q.next())
            ret = q.value(0).toInt();
    }
    qDebug() << "Returning " << ret;
    return ret;
}

void
ToDoListView::customContextMenuRequested(const QPoint &pos)
{
    QMenu menu(ui->bugTreeWidget);
    QTreeWidgetItem *currentItem = ui->bugTreeWidget->itemAt(pos);
    if (currentItem != NULL)
    {
        int itemIndex = ui->bugTreeWidget->indexOfTopLevelItem(currentItem);

        QString delText;
        if (itemIndex != -1)
            delText = "Delete List";
        else
            delText = "Delete Item";

        QAction* del = menu.addAction(delText);
        connect(del,SIGNAL(triggered()),this,SLOT(deleteItem()));

        if (itemIndex != -1)
        {
            QAction* editToDoListName = menu.addAction("Edit Name");
            connect(editToDoListName,SIGNAL(triggered()),this,SLOT(editTopLevelItem()));
            QAction* exportToCalendar = menu.addAction("Export List to Calendar");
            connect(exportToCalendar,SIGNAL(triggered()),this,SLOT(syncList()));
        }
    }
    else
    {
        QAction* newTLI = menu.addAction("Add New List");
        connect(newTLI,SIGNAL(triggered()),this,SLOT(newTopLevelItem()));
    }

    menu.addSeparator();
    QAction* toDoListConfig = menu.addAction("ToDo List Preferences");
    connect(toDoListConfig,SIGNAL(triggered()),this,SLOT(toDoListPreferences()));
    menu.exec(ui->bugTreeWidget->mapToGlobal(pos));
}

void
ToDoListView::deleteItem()
{
    QSqlQuery query;
    int itemIndex = ui->bugTreeWidget->indexOfTopLevelItem(ui->bugTreeWidget->currentItem());

    QString text;
    if (itemIndex != -1)
        text = "Are you sure you want to delete this list?  All ToDo items will be deleted as well.";
    else
        text = "Are you sure you want to delete this item?";

    QMessageBox box(QMessageBox::Question,
                    "Delete ToDo List Item",
                    text,
                    QMessageBox::No | QMessageBox::Yes);
    if (box.exec() == QMessageBox::No)
        return;

    // List delete
    if(itemIndex != -1)
    {
        QList<QTreeWidgetItem*> children;
        children = ui->bugTreeWidget->topLevelItem(itemIndex)->takeChildren();
        QString listId = ui->bugTreeWidget->topLevelItem(itemIndex)->data(0, Qt::UserRole).toString();
        qDebug() << "ListID: " << listId;
        QList<ToDoItem*> updatedItems = items.values(ui->bugTreeWidget->topLevelItem(itemIndex)->text(0));

        foreach(QTreeWidgetItem* child, children)
        {
            query.prepare("DELETE FROM todolistbugs WHERE bug_id = :bugID AND todolist_id = :list_id");
            query.bindValue(":bugID", child->text(1));
            query.bindValue(":list_id", listId);
            query.exec();

            int itemNo = findItem(child->text(1),updatedItems);
            if(itemNo != -1)
            {
                updatedItems.at(itemNo)->setStatus(ToDoItem::DELETED);
            }
        }

        query.prepare("DELETE FROM todolist WHERE id = :id");
        //query.bindValue(":name",ui->bugTreeWidget->topLevelItem(itemIndex)->text(0));
        query.bindValue(":id", listId);
        query.exec();

        lists.value(ui->bugTreeWidget->topLevelItem(itemIndex)->text(0))->setListStatus(ToDoList::DELETED);
        ui->bugTreeWidget->takeTopLevelItem(itemIndex);
    }
    else
    {
        QTreeWidgetItem* item = ui->bugTreeWidget->currentItem();
        int dbId = item->data(0, Qt::UserRole).toInt();

        if(item != NULL)
        {
            int childIndex = ui->bugTreeWidget->currentIndex().row();
            QTreeWidgetItem* parent = ui->bugTreeWidget->currentItem()->parent();
            query.prepare("DELETE FROM todolistbugs WHERE id = :id");
            query.bindValue(":id", dbId);
            query.exec();

            QList<ToDoItem*> updatedItems = items.values(parent->text(0));

            int itemIndex = findItem(item->text(1),updatedItems);
            if(itemIndex != -1)
            {
                updatedItems.at(itemIndex)->setStatus(ToDoItem::DELETED);
                parent->takeChild(childIndex);
            }
        }

    }

}

int
ToDoListView::findItem(QString targetName,QList<ToDoItem*> list)
{
    int index;
    for(int i = 0; i < list.length();i++)
    {
        if(targetName.compare(list.at(i)->internalID()) == 0)
            return index = i;

    }

    return -1;
}
void
ToDoListView::newTopLevelItem()
{
    bool ok;
    QString itemName = QInputDialog::getText(this,tr("Add a ToDo List"),
                                             tr("New ToDo List Name: "),
                                             QLineEdit::Normal,"",&ok);

    if(ok && !itemName.isEmpty())
    {
        if(checkUniqueList(itemName))
        {
            QString newId = toDoListInsert(itemName);
            toDoListAdded(itemName, "", newId, true);
        }
        else
        {
            QMessageBox box;
            box.setStandardButtons(QMessageBox::Ok);

            box.setText("This Todo List name exists");
            box.setInformativeText(QString("A ToDo List with the name \"%1\" "
                                           "already exists, please "
                                           "select a new name").arg(itemName));
            box.setIcon(QMessageBox::Warning);
            box.exec();
            newTopLevelItem();
        }
    }
}

bool
ToDoListView::checkUniqueList(QString name)
{

    QSqlQuery query;
    query.prepare("SELECT * FROM todolist WHERE name= :name");
    query.bindValue(":name",name);
    query.exec();
    return !query.next();

}

void
ToDoListView::editTopLevelItem()
{

    QTreeWidgetItem* current = ui->bugTreeWidget->currentItem();
    QTreeWidgetItem* topLevelItem;
    QString oldName;
    bool ok;
    topLevelItem = findParent(current);

    QString itemName = QInputDialog::getText(this,tr("Edit ToDo List Name"),
                                             tr("New ToDo List Name: "),
                                             QLineEdit::Normal,
                                             topLevelItem->text(0),&ok);

    if(ok && !itemName.isEmpty() && checkUniqueList(itemName))
    {

        oldName = topLevelItem->text(0);
        topLevelItem->setText(0,itemName);

        QSqlQuery query("UPDATE todolist SET name = :newName WHERE name= :oldName");

        query.bindValue(":newName",itemName);
        query.bindValue(":oldName",oldName);
        query.exec();
        ToDoList* list = lists.value(oldName);
        list->setListName(itemName);
        list->setListStatus(ToDoList::UPDATED);

    }
}

QMap<QString,QString>
ToDoListView::getExports()
{

    ToDoListExport* ex;
    QMap<QString,QString> selectedItems;
    ex = new ToDoListExport(this);
    if(ex->exec() == QDialog::Accepted)
        selectedItems = ex->getData();

    return selectedItems;
}

void
ToDoListView::syncAll()
{
    isLoginOnly = false;
    syncSingle = false;
    syncLists.clear();
    QMap<QString,QString> syncServices = getExports();
    syncLists = lists.values();

    QMapIterator<QString,QString> iter(syncServices);
    while(iter.hasNext())
    {
        iter.next();
        createService(iter.key(),iter.value());
    }

}

void
ToDoListView::syncList()
{
    syncLists.clear();
    isLoginOnly = false;
    syncSingle = true;
    syncLists.append(lists.value(findParent(ui->bugTreeWidget->currentItem())->text(0)));
    QMap<QString,QString> syncServices = getExports();
    QMapIterator<QString,QString> iter(syncServices);

    while(iter.hasNext())
    {
        iter.next();
        createService(iter.key(),iter.value());
    }
}

void
ToDoListView::createService(QString serviceType,QString serviceName)
{
    syncName = serviceName;
    if(serviceType.compare("Remember The Milk") == 0)
    {
        RememberTheMilk* rm = new RememberTheMilk(serviceName,isLoginOnly);
        rm->login();

        connect(rm,SIGNAL(serviceError(QString)),this,SLOT(serviceError(QString)));
        connect(rm,SIGNAL(loginWaiting()),this,SLOT(loginWaiting()));
        connect(rm,SIGNAL(regSuccess()),this,SLOT(regSuccess()));
        connect(rm,SIGNAL(authCompleted()),this,SLOT(authCompleted()));
        connect(rm,SIGNAL(readyToAddItems()),this,SLOT(readyToAddItems()));
    }


    else if(serviceType.compare("Google Tasks") == 0)
    {
        GoogleTasks* gc = new GoogleTasks(serviceName,isLoginOnly);
        connect(gc,SIGNAL(serviceError(QString)),this,SLOT(serviceError(QStirng)));
        connect(gc,SIGNAL(loginWaiting()),this,SLOT(loginWaiting()));
        connect(gc,SIGNAL(authCompleted()),this,SLOT(authCompleted()));
        gc->login();
    }
    else if(serviceType.compare("Generic WebDAV") == 0)
        GenericWebDav* gd = new GenericWebDav();

}

void
ToDoListView::toDoListPreferences()
{

    ToDoListPreferences* t;
    t = new ToDoListPreferences(this);
    t->show();
    t->raise();
    t->activateWindow();
    isLoginOnly = true;
    connect(t,SIGNAL(registerUser(QString,QString)),this,
            SLOT(createService(QString,QString)));
}


QTreeWidgetItem*
ToDoListView::findParent(QTreeWidgetItem *current)
{
    QTreeWidgetItem* topLevelItem;
    int itemIndex = ui->bugTreeWidget->indexOfTopLevelItem(current);

    if(itemIndex == -1)
        topLevelItem = ui->bugTreeWidget->currentItem()->parent();
    else
        topLevelItem = ui->bugTreeWidget->currentItem();

    return topLevelItem;
}


void
ToDoListView::serviceError(const QString &message)
{
    ErrorHandler::handleError("A ToDo List service error has occured: ",message);
}


void
ToDoListView::loginWaiting()
{
    ServicesBackend* obj = static_cast<ServicesBackend*>(sender());


    QMessageBox login(this);
    login.setWindowTitle("Authorisation required");
    login.setText(QString("Authorisation Required for %1.").arg(syncName));
    login.setInformativeText("In order to sync with this service, you need to authorize Entomologist."
                             "A web browser will open, and after you've authenticated, press <b>OK</b>.");
    login.setIcon(QMessageBox::Information);
    login.setDefaultButton(QMessageBox::Ok);
    if (login.exec() == QMessageBox::Ok)
    {
        if(obj->serviceType().compare("Google Tasks") == 0)
            insertAuthToken(static_cast<GoogleTasks*>(obj)); // very nasty.
        else
            obj->newUser();
    }



}

void
ToDoListView::regSuccess()
{
    QMessageBox success(this);
    success.setWindowTitle("Authorisation complete");
    success.setText("Entomologist has been authorised");
    success.setInformativeText(QString("Entomologist has been successfully authorised for %1."
                                       "You can revoke this authorisation at any time by removing it from the list of authorised applications. "
                                       "If this was done as part of an export, Entomologist will now proceed with exporting your tasks.").arg(syncName));

    success.setIcon(QMessageBox::Information);
    success.setDefaultButton(QMessageBox::Ok);
    success.exec();
}

void
ToDoListView::authCompleted()
{
    ServicesBackend *obj;
    if(sender()->inherits("ServicesBackend"))
        obj = static_cast<ServicesBackend*>(sender());
    else
        obj = static_cast<ServicesBackend*>(sender()->parent());

    // We need to get the Access/Refresh Tokens.
    if(obj->serviceType() == "Google Tasks")
        obj->newUser();

    if(!isLoginOnly)
    {

        if(syncLists.isEmpty() == false)
        {
            currentList = syncLists.takeFirst();
            syncItems = items.values(currentList->listName());
            if(hasBeenSynced(currentList) == false)
            {
                currentList->setListStatus(ToDoList::NEW);
            }

            foreach(ToDoItem* item, syncItems)
                if(item->status() == ToDoItem::UNCHANGED)
                {
                    syncItems.removeAt((syncItems.indexOf(item)));
                }

            if(syncItems.length() > 0)
            {

                progress = new QProgressDialog(QString("Syncing %1").arg(currentList->listName()),0,0,syncItems.length(),this);
                numberofItemsToSync = syncItems.length();
                progress->setWindowModality(Qt::WindowModal);
                progress->show();
                obj->setList(currentList);
                obj->setupList();
                currentList->setSyncCount(1);
            }
            else //Can assume that there aren't any items to sync for this list so get a new one.
                authCompleted();

        }
    }
}

// Specific to Google Task as it opens a browser window but doesn't send anything back to us.
void
ToDoListView::insertAuthToken(GoogleTasks* task)
{
    bool ok;
    QString token = QInputDialog::getText(this,tr("Authorization token required"),tr("Auth Token:"),QLineEdit::Normal,0,&ok);

    if(ok && !token.isEmpty())
    {
        task->setAuthToken(token);
        task->newUser();
    }

}

bool
ToDoListView::hasBeenSynced(ToDoList *list)
{
    bool hasBeenSynced = false;
    foreach(QString service,list->services())
        if(service.compare(syncName) == 0)
            hasBeenSynced = true;

    return hasBeenSynced;

}

void
ToDoListView::readyToAddItems()
{
    ServicesBackend* obj = static_cast<ServicesBackend*>(sender());
    timerCount = syncItems.length();
    timer = new QTimer(obj);
    timer->setInterval(1000);
    connect(timer,SIGNAL(timeout()),this,SLOT(syncItem()));
    if(!timer->isActive())
        timer->start();
}

void
ToDoListView::syncItem()
{

    ServicesBackend* obj = static_cast<ServicesBackend*>(sender()->parent());

    bool isNew = !hasBeenSynced(currentList);

    if(timerCount >= 0)
    {

        progress->setValue(qAbs(timerCount - numberofItemsToSync));

        if(!syncItems.isEmpty())
        {
            ToDoItem* t = syncItems.takeFirst();
            if(isNew || (t->status() == ToDoItem::NEW && t->status() != ToDoItem::DELETED))
                obj->addTask(t);
            else
            {
                if(t->status() == ToDoItem::DATECHANGED)
                    obj->updateDate(t);
                else if(t->status() == ToDoItem::COMPLETEDCHANGED)
                    obj->updateCompleted(t);
                else if(t->status() == ToDoItem::DELETED && !isNew)
                {
                    obj->deleteTask(t);

                    if(currentList->syncNeeded() == false)
                    {
                        items.remove(currentList->listName(),t);
                        delete t;

                    }
                }
            }

            if(!currentList->syncNeeded())
                t->setStatus(ToDoItem::UNCHANGED);
        }
        --timerCount;
    }
    else
    {
        timer->stop();
        if(currentList->status() == ToDoList::DELETED)
        {
            obj->deleteList();
            if(currentList->syncNeeded() == false)
                lists.remove(currentList->listName());
        }

        if(!currentList->syncNeeded())
            currentList->setListStatus(ToDoList::UNCHANGED);
        addServiceToList(syncName,currentList);
        authCompleted();
    }
}

QStringList
ToDoListView::currentServices(QString list)
{
    QStringList currentServices;
    QSqlQuery query("SELECT sync_services FROM todolist WHERE name = :name");
    query.bindValue(":name",list);

    if(!query.exec())
    {
        qDebug() << "Error selecting services";
        qDebug() << query.lastQuery() << " Bound Values: " << query.boundValues();
        qDebug() << query.lastError();

    }

    while(query.next())
        currentServices = query.value(0).toString().split(",",QString::SkipEmptyParts);
    return currentServices;
}

void
ToDoListView::addServiceToList(QString serviceName,ToDoList* list)
{

    bool insert = true;
    QSqlQuery query;
    QStringList current = currentServices(list->listName());

    foreach(QString service, current)
        if(service.compare(serviceName) == 0)
            insert = false;

    if(insert)
    {
        current.append(serviceName);
        list->setServices(serviceName);
        query.prepare("UPDATE todolist SET sync_services = :sync WHERE name= :name");
        query.bindValue(":sync",current.join(","));
        query.bindValue(":name",list->listName());

        if(!query.exec())
        {

            qDebug() << "Error adding Service to list";
            qDebug() << query.lastQuery() <<  " Bound Values: " << query.boundValues();
            qDebug() << query.lastError();
        }
    }
}


void
ToDoListView::closeEvent(QCloseEvent *event)
{

    if (isVisible())
    {
        // Save the window geometry and position
        QSettings settings("Entomologist");
        settings.setValue("todo-window-geometry", saveGeometry());
    }
    else
    {
        event->accept();
    }
}
