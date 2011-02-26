#ifndef MANTIS_H
#define MANTIS_H

#include <QObject>
#include "Backend.h"

class QtSoapHttpTransport;
class QtSoapMessage;

class Mantis : public Backend
{
Q_OBJECT
public:
    Mantis(const QString &url, QObject *parent = 0);
    ~Mantis();

    void sync();
    void login();
    void getComments(const QString &bugId);
    void checkVersion();
    void checkValidPriorities();
    void checkValidSeverities();
    void checkValidStatuses();
    void uploadAll();
    QString buildBugUrl(const QString &id);
    QString autoCacheComments();

public slots:
    void response();
    void loginResponse();
    void viewResponse();
    void assignedResponse();
    void reportedResponse();
    void monitoredResponse();
    void handleSslErrors(QNetworkReply *reply, const QList<QSslError> &errors);
    void bugsInsertionFinished(QStringList idList);
    void commentInsertionFinished();

private:
    enum viewType {
        ASSIGNED = 0,
        REPORTED,
        MONITORED
    };

    void getAssigned();
    void getReported();
    void getMonitored();
    void addNotes();
    void getNextUpload();
    void getNextCommentUpload();
    void handleCSV(const QString& csv, const QString &bugType);
    QVector<QString> parseCSVLine(const QString &line);
    QVariantMap mUploadList;
    QString mCurrentUploadId;
    QVariantList mCommentUploadList;
    void setView();
    viewType mViewType;
    QVariantMap mBugs;
    QtSoapHttpTransport *pMantis;
    bool mUploadingBugs;
};

#endif
