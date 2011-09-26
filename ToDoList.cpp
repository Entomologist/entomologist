
#include <QStringList>
#include "ToDoList.h"

ToDoList::ToDoList(QString listName) : listName_(listName)
{
    status_ = ToDoList::UNCHANGED;
    syncCount_ = 0;

}

QString
ToDoList::listName()
{

    return listName_;
}

QString
ToDoList::listID()
{
    return listID_;

}

int
ToDoList::status()
{

    return status_;
}

int
ToDoList::syncCount()
{
    return syncCount_;
}

QStringList
ToDoList::services()
{
    return services_;
}

void
ToDoList::setServices(QString service)
{
    services_.append(service);

}

void
ToDoList::setSyncCount(int sync)
{
    syncCount_ += sync;

}

void
ToDoList::setListID(QString id)
{
    listID_ = id;
}
void
ToDoList::setListName(QString name)
{
    listName_ = name;

}

void
ToDoList::setListStatus(listStatus newStatus)
{

    status_ = newStatus;
}

bool
ToDoList::syncNeeded()
{

    return syncCount_ < services_.length();
}

