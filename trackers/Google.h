#ifndef GOOGLE_H
#define GOOGLE_H

#include <QObject>
#include "Backend.h"

class Google : public Backend
{
Q_OBJECT
public:
    Google(const QString &url, QObject *parent = 0);
    ~Google();

    void sync();
    void login();
    void checkVersion();
    void checkValidPriorities();
    void checkValidSeverities();
    void checkValidStatuses();
    void uploadAll();
    QString buildBugUrl(const QString &id);
    QString autoCacheComments() { return "0"; }

signals:

public slots:
    void networkError(QNetworkReply::NetworkError e);
    void tokenFinished();
    void bugsFinished();

private:
    void fetchBugs();
    QString mToken, mProject;
};

#endif // GOOGLE_H
