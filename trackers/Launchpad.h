#ifndef LAUNCHPAD_H
#define LAUNCHPAD_H

#include <QObject>
#include "Backend.h"

class Launchpad : public Backend
{
Q_OBJECT
public:
    Launchpad(const QString &url, QObject *parent = 0);
    ~Launchpad();

    void sync();
    void login();
    void checkVersion();
    void checkValidPriorities();
    void checkValidSeverities();
    void checkValidStatuses();
    void uploadAll();
    QString buildBugUrl(const QString &id);
    QString autoCacheComments() {return "0";}

signals:

public slots:
    void requestTokenFinished();
    void requestRealTokenFinished();
    void bugListFinished();
    void networkError(QNetworkReply::NetworkError e);
private:
    void getUserBugs();
    void authenticateUser();
    void getRealToken();
    QString authorizationHeader();
    QString mApiUrl;
    QString mConsumerToken, mToken, mSecret;
};

#endif // LAUNCHPAD_H
