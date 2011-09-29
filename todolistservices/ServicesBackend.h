#ifndef SERVICESBACKEND_H
#define SERVICESBACKEND_H

#include <QObject>
#include <QMetaType>
#include <QTreeWidgetItem>
#include <QSqlQuery>
#include <QSqlError>
#include <QDebug>
#include "ToDoItem.h"
#include "ToDoList.h"
class ServicesBackend : public QObject
{

    Q_OBJECT
public:
    ServicesBackend();

    virtual void login() {}
    virtual void setupList() {}
    virtual void addTask(ToDoItem* item) { Q_UNUSED(item); }
    virtual void updateCompleted(ToDoItem* item) { Q_UNUSED(item); }
    virtual void updateDate(ToDoItem* item) { Q_UNUSED(item); }
    virtual void deleteTask(ToDoItem* item) { Q_UNUSED(item); }
    virtual void newUser() {}
    virtual void getLists() {}
    virtual void updateList() {}
    virtual void deleteList() {}
    virtual void setList(ToDoList* list) { Q_UNUSED(list); }
    virtual QString serviceType() { return serviceType_; }

    // Probably want to put these in a .cpp file.
    void insertItem(const QString &item, const QString &serviceName) {
        QSqlQuery query("INSERT INTO service_tasks (task_id,service_name) VALUES(:item,:service_name)");
        query.bindValue(":item",item);
        query.bindValue(":service_name",serviceName);
        if(!query.exec())
        {

            qDebug() << "Error adding task to list";
            qDebug() << query.lastQuery() <<  " Bound Values: " << query.boundValues();
            qDebug() << query.lastError();
        } }

    void updateItemID(ToDoItem* item,const QString &serviceName)
    {
        QSqlQuery query("UPDATE service_tasks SET item_id = :id WHERE service_name = :name AND task_id = :taskid");
        query.bindValue(":id",item->RTMTaskID());
        query.bindValue(":name",serviceName);
        query.bindValue(":taskid",item->bugID());

        if(!query.exec())
        {
            qDebug() << "Error on Query";
            qDebug() << query.lastError() << " Bound Values: " << query.boundValues();
            qDebug() << query.lastQuery();

        }


    }

    QString serviceType_;


signals:
    void serviceError(const QString &message);
    void loginWaiting();
    void regSuccess();
    void authCompleted();
    void readyToAddItems();
    void taskIsNewer(ToDoItem* item);
    void taskAdded(ToDoItem* item);

private:


};

Q_DECLARE_METATYPE(ServicesBackend*)

#endif // SERVICESBACKEND_H
