#ifndef SERVICESBACKEND_H
#define SERVICESBACKEND_H

#include <QObject>
#include <QMetaType>
#include <QTreeWidgetItem>
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
