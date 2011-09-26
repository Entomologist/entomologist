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
#ifndef TODOITEM_H
#define TODOITEM_H

#include <QObject>

class ToDoItem : public QObject
{
    Q_OBJECT
public:
    enum taskStatus { UNCHANGED,NEW, DATECHANGED,COMPLETEDCHANGED, DELETED };
    ToDoItem(QString list,QString internalID,QString bugID,QString summary,
             QString date,QString lastModified, bool completed);

    //getters
    int status();
    QString listName();
    QString bugID();
    QString summary();
    QString name();
    QString date();
    bool isCompleted();
    QString lastModified();
    QString taskID();
    QString taskSeries();
    QString internalID();

    //setters
    void setInternalID(QString internalID);
    void setStatus(taskStatus status);
    void setListName(QString listName);
    void setBugID(QString bugID);
    void setSummary(QString summary);
    void setDate(QString date);
    void setCompleted(bool completed);
    void setTaskID(QString taskID);
    void setTaskSeries(QString taskseries);
signals:

public slots:

private:
    taskStatus status_;
    QString listName_;
    QString internalID_;
    QString bugID_;
    QString summary_;
    QString date_;
    QString lastModified_;
    QString taskID_;
    QString taskseries_;
    bool completed_;

};

#endif // TODOITEM_H
