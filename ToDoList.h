#ifndef TODOLIST_H
#define TODOLIST_H

#include <QString>
#include <QStringList>
class ToDoList
{
public:
    enum listStatus { UNCHANGED, UPDATED, NEW, DELETED};
    ToDoList(QString name);
    QString listName();
    QString listID();
    int status();
    int syncCount();
    QStringList services();
    void setListName(QString name);
    void setListID(QString id);
    void setListStatus(listStatus newStatus);
    void setSyncCount(int sync);
    void setServices(QString service);
    bool syncNeeded();
private:
    QString listName_;
    QString listID_;
    listStatus status_;
    int syncCount_;
    QStringList services_;

};

#endif // TODOLIST_H
