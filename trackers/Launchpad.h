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
    void getComments(const QString &bugId);
    void checkValidPriorities();
    void checkValidSeverities();
    void checkValidStatuses();
    QString type() { return "Launchpad"; }

    void uploadAll();
    QString buildBugUrl(const QString &id);
    QString autoCacheComments() {return "0";}

signals:

public slots:
    void handleSslErrors(QNetworkReply *reply,
                         const QList<QSslError> &errors);
    void requestTokenFinished();
    void requestRealTokenFinished();
    void bugListFinished();
    void bugUploadFinished();
    void commentUploadFinished();
    void subscriberListFinished();
    void reporterListFinished();
    void networkError(QNetworkReply::NetworkError e);
    void bugsInsertionFinished(QStringList idList);
    void commentFinished();
    void commentInsertionFinished();

private:
    void getUserBugs();
    void getSubscriberBugs();
    void getReporterBugs();
    void authenticateUser();
    void getRealToken();
    void getNextUpload();
    void getNextCommentUpload();

    void handleJSON(QVariant json, const QString &type);
    QString authorizationHeader();
    QString mApiUrl;
    QString mConsumerToken, mToken, mSecret;
    QVariantMap mUploadList;
    QVariantList mCommentUploadList;
    QVariantMap mBugs;
};

#endif // LAUNCHPAD_H
