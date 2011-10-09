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
    BackendUI *displayWidget();

    void sync();
    void login();
    void getComments(const QString &bugId);
    void getSearchedBug(const QString &bugId);

    void checkVersion();
    void checkFields();
    void search(const QString &query);

    void checkValidPriorities();
    void checkValidSeverities();
    void checkValidStatuses();
    void checkValidResolutions();
    void checkValidReproducibilities();

    void checkValidComponents();

    QString type() { return "mantis"; }

    void uploadAll();
    QString buildBugUrl(const QString &id);
    QString autoCacheComments();
    void deleteData();

public slots:
    void headFinished();
    void response();
    void loginResponse();
    void loginSyncResponse();
    void viewResponse();
    void assignedResponse();
    void reportedResponse();
    void ccResponse();
    void monitoredResponse();
    void searchResponse();
    void searchedBugResponse();
    void multiInsertSuccess(int operation);
    void handleSslErrors(QNetworkReply *reply, const QList<QSslError> &errors);
    void bugsInsertionFinished(QStringList idList);
    void commentInsertionFinished();

private:
    enum viewType {
        ASSIGNED = 0,
        REPORTED,
        CC,
        MONITORED,
        SEARCHED
    };

    void getAssigned();
    void validFieldCall(const QString &request);
    void getReported();
    void getCC();
    void getMonitored();
    void getSearched();
    void getCategories();
    void addNotes();
    void getNextUpload();
    void getNextCommentUpload();
    void handleCSV(const QString& csv, const QString &bugType);
    QVector<QString> parseCSVLine(const QString &line);
    QVariantMap mUploadList;
    QString mCurrentUploadId;
    QVariantList mCommentUploadList;
    void setView(const QString &search = "");
    viewType mViewType;
    QVariantMap mBugs;
    QMap<QString, QString> mSearchedBugResult;
    QStringList mCategoriesList;
    QStringList mProjectList;
    QtSoapHttpTransport *pMantis;
    bool mUploadingBugs;
};

#endif
