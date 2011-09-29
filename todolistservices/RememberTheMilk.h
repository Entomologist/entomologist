#ifndef REMEMBERTHEMILK_H
#define REMEMBERTHEMILK_H

#include <QObject>
#include <QMetaType>
#include <QString>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include "ServicesBackend.h"
#include "ToDoItem.h"
#include "ToDoList.h"

class RememberTheMilk : public ServicesBackend
{

    Q_OBJECT

public:
    RememberTheMilk(QString toDoList,bool loginState);

    void login();
    void newUser();
    void setupList();
    void addTask(ToDoItem* item);
    void deleteTask(ToDoItem* item);
    void updateCompleted(ToDoItem* item);
    void updateDate(ToDoItem* item);
    void updateList();
    void deleteList();
    void getLists();
    void setList(ToDoList* list);
private slots:
    void frobResponse();
    void tokenResponse();
    void checkResponse();
    void timelineResponse();
    void todoListResponse();
    void updateToDoListResponse();
    void addTaskResponse();
    void getTasksResponse();
    void updateCompletedResponse();
    void updateDateResponse();
    void deleteTaskResponse();
    void getListsResponse();
    void deleteListResponse();
private:
    QString serviceName;
    QString apiKey;
    QString secret;
    QString mURL;
    QString validFrob;
    QString authToken;
    QString timeline;
    QList<ToDoItem*> syncTasks;
    QNetworkAccessManager *manager;
    bool loginState;
    void generateToken();
    void createTimeline();
    void regUser();
    void generateFrob();
    void insertKey();
    void createToDoList();
    void checkToken();
    void insertListID(QString listName,QString listID);
    void getTasks();
    QString generateSig(QStringList parameters);
    QStringList getTaskID(QString taskName);
    ToDoList* todoList;
    QList<ToDoList*> remoteLists;
    bool reservedList(QString name);
    bool compareTaskDates(QString task1,QString task2);
    bool listExists(QString name);
    void insertTaskID(ToDoItem* item,QString itemID);
    ToDoItem* currentItem;

};


#endif // REMEMBERTHEMILK_H
