#ifndef TODOLIST_H
#define TODOLIST_H

#include <QString>
#include <QStringList>
class ToDoList
{
public:
    enum listStatus { UNCHANGED, UPDATED, NEW, DELETED};
    ToDoList(const QString &name);
    QString listName() { return mListName; }
    QString rtmListID() { return mRTMListID;}
    QString googleTasksListID() { return mGoogleTasksListID; }
    int status() { return mStatus;}
    int syncCount() { return syncCount();}
    QStringList services() { return mServices;}
    void setListName(const QString &name) { mListName = name; }
    void setmRTMListID(const QString &id) { mRTMListID = id; }
    void setListStatus(listStatus newStatus) { mStatus = newStatus; }
    void setSyncCount(int sync) { mSyncCount = sync; }
    void setServices(const QString &service) { mServices.append(service); }
    void setGoogleTasksListID(const QString &id) { mGoogleTasksListID = id; }
    bool syncNeeded() {  return mSyncCount < mServices.length(); }
private:
    QString mListName;
    QString mRTMListID;
    QString mGoogleTasksListID;
    listStatus mStatus;
    int mSyncCount;
    QStringList mServices;

};

#endif // TODOLIST_H
