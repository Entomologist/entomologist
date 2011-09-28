#ifndef GOOGLECALENDAR_H
#define GOOGLECALENDAR_H
#include <QNetworkAccessManager>
#include "ServicesBackend.h"
class GoogleTasks : public ServicesBackend
{
    Q_OBJECT
public:
    GoogleTasks(QString service,bool state);
    ~GoogleTasks();
    void login();
    void setupList();
    void addTask(ToDoItem* item);
    void deleteTask(ToDoItem* item);
    void newUser();
    void updateCompleted(ToDoItem* item);
    void updateDate(ToDoItem* item);
    void setAuthToken(QString token) { mToken = token; insertKey(mToken,"auth_key"); }

private:
    void insertKey(QString token, QString item);
    QNetworkAccessManager* pManager;
    void reRequestToken();
    QString mApiUrl;
    QString mAccessToken,mRefreshToken, mToken, mSecret;
    QString mRedirectURL,mScope,mClientID;
    QString mUrl,mService;
    bool mState;

private slots:
    void getSwapToken();

};

#endif // GOOGLECALENDAR_H
