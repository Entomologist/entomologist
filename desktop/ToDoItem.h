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

    int status() { return mStatus;}
    QString listName() { return mListName;}
    QString bugID() { return mBugID; }
    QString summary() { return mSummary; }
    QString name() { return mBugID + " : " + mSummary; }
    QString date() { return mDate;}
    bool isCompleted() { return mCompleted;}
    QString lastModified() { return mLastModified;}
    QString RTMTaskID() { return mRTMTaskID;}
    QString RTMTaskSeriesID() { return mRTMTaskSeriesID; }
    QString internalID() { return mInternalID;}
    QString googleTaskID() { return mGoogleTaskID; }

    void setInternalID(const QString &internalID) { mInternalID = internalID; }
    void setStatus(taskStatus status) { mStatus = status; }
    void setListName(const QString &listName) { mListName = listName; }
    void setBugID(const QString &bugID) { mBugID = bugID; }
    void setSummary(const QString &summary) { mSummary = summary;}
    void setDate(const QString &date) { mDate = date;}
    void setCompleted(bool completed) { mCompleted = completed; }
    void setRTMTaskID(const QString &taskID) { mRTMTaskID = taskID; }
    void setRTMTaskSeriesID(const QString &taskseries) { mRTMTaskSeriesID = taskseries; }
    void setGoogleTasksID(const QString &id) { mGoogleTaskID = id;}

private:
    taskStatus mStatus;
    QString mListName;
    QString mInternalID;
    QString mBugID;
    QString mSummary;
    QString mDate;
    QString mLastModified;
    QString mRTMTaskID;
    QString mRTMTaskSeriesID;
    QString mGoogleTaskID;
    bool mCompleted;


};

#endif // TODOITEM_H
