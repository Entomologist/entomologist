#ifndef GOOGLECALENDAR_H
#define GOOGLECALENDAR_H
#include "ServicesBackend.h"

class GoogleTasks : public ServicesBackend
{
    Q_OBJECT
public:
    GoogleTasks(QString service);
    ~GoogleTasks();
    void login();
    void setupList();
    void addTask(ToDoItem* item);
    void deleteTask(ToDoItem* item);
    void newUser();
    void updateCompleted(ToDoItem* item);
    void updateDate(ToDoItem* item);
    void setAuthToken(QString token) { mToken = token; insertKey(mToken,"auth_key"); }
    void getLists();
    void setList(ToDoList* List);
    void deleteList();
    void updateItemID(ToDoItem *item, const QString &serviceName);

private:
    void insertKey(QString token, QString item);
    void reRequestToken();
    QString mAuthURL;
    QString mAccessToken,mRefreshToken, mToken, mSecret;
    QString mRedirectURL,mScope,mClientID;
    QString mUrl,mService;
    QString getTaskIDFromDB(const QString &taskID,const QString &serviceName);
    ToDoList* mTodoList;
    ToDoItem* mCurrentItem;
    QList<ToDoList*> remoteLists;
    void addList();
    bool listExists(QString name);
    void insertListID(QString listName, QString listID);
    QString mRFC3339DateFormat;
    QString mRFC3339TimeFormat;
    QString mDateFormat;
    void getTasksList();
    QMap<QString,QVariant> mTasksList;
    void updateList();

private slots:
    void getSwapToken();
    void getListRepsonse();
    void addListResponse();
    void deleteListResponse();
    void addTaskResponse();
    void deleteTaskResponse();
    void updateDateResponse();
    void getTasksListResponse();
    void updateCompletedResponse();
    void updateListResponse();
};

#endif // GOOGLECALENDAR_H
