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
 *  Author: Matt Barringer <mbarringer@suse.de>
 *
 */

#ifndef BACKEND_H
#define BACKEND_H

#include <QObject>
#include <QMetaType>
#include <QSslConfiguration>
#include <QStringList>
#include <QNetworkReply>
#include <QNetworkAccessManager>
#include <QNetworkCookieJar>
#include <QDateTime>
#include <QSqlQuery>

#include "tracker_uis/BackendUI.h"
#include "SqlWriterThread.h"
class Backend : public QObject
{
    Q_OBJECT
public:
    Backend(const QString &url);
    ~Backend();

    virtual BackendUI *displayWidget();

    void setId(const QString &id) { mId = id; }
    QString id() { return mId; }

    void setName(const QString &name) { mName = name; }
    QString name() { return mName; }

    void setUrl(const QString &url) { mUrl = url; }
    QString url() { return mUrl; }

    virtual void setUsername(const QString &username) { mUsername = username; }
    QString username() { return mUsername; }

    virtual void setPassword(const QString &password) { mPassword = password; }
    QString password() { return mPassword; }

    void setEmail(const QString &email) { mEmail = email; }
    QString email() { return mEmail; }

    void setValidSeverities(QStringList list) { mValidSeverities = list; }
    QStringList validSeverities() { return mValidSeverities; }

    void setValidPriorities(QStringList list) { mValidPriorities = list; }
    QStringList validPriorities() { return mValidPriorities; }

    void setValidStatuses(QStringList list) { mValidStatuses = list; }
    QStringList validStatuses() { return mValidStatuses; }
    void setVersion(const QString &version) { mVersion = version; }
    QString version() { return mVersion; }

    void setMonitorComponents(const QStringList components) { mMonitorComponents = components; }
    QStringList monitorComponents() { return mMonitorComponents; }

    void setLastSync(const QString &dateTime);
    virtual QString type() { return "Unknown"; }
    // The top level sync call
    virtual void sync() {}

    virtual void getComments(const QString &bugId) { Q_UNUSED(bugId); }
    virtual void getSearchedBug(const QString &bugId) { Q_UNUSED(bugId); }

    // This is used to override login methods (like in Novell Bugzilla)
    virtual void login() {}

    // Version check
    virtual void checkVersion() {}

    // These get the valid values
    virtual void checkFields() {}
    virtual void checkValidComponents() {}
    virtual void checkValidComponentsForProducts(const QString &product) { Q_UNUSED(product); }

    // Tells the backend to upload everything
    virtual void uploadAll() {}

    // Given a bug ID, create an HTTP url for viewing in a web browser
    // (called by the bug list context menu)
    virtual QString buildBugUrl(const QString &id) { Q_UNUSED(id); return(""); }

    // If a tracker automatically downloads comments on syncs, it
    // should return "1" here, otherwise "0"
    virtual QString autoCacheComments() { return(""); }

    // This keeps track of how many bugs were updated in the last sync.
    // It's used to pop up the system tray notification.
    int latestUpdateCount() { return mUpdateCount; }

    virtual void search(const QString &query) { Q_UNUSED(query); }

    virtual void deleteData() {}

signals:
    void searchFinished();
    void searchResultFinished(QMap<QString, QString> resultMap);
    void bugsUpdated();
    void commentsCached();
    void versionChecked(QString version);
    void componentsFound(QStringList components);
    void fieldsFound();
    void backendError(const QString &message);
    void sqlQuery(QMap<QString, QString> map);

public slots:
    virtual void commentInsertionFinished() {}
    virtual void bugsInsertionFinished(QStringList idList) { Q_UNUSED(idList);}
    void sqlError(QString message);

protected:
    void updateSync();
    void saveCredentials();
    QString friendlyTime(const QString &time);
    BackendUI *pDisplayWidget;
    QDateTime mLastSync;
    QString mId;
    QString mName;
    QString mUrl;
    QString mUsername;
    QString mPassword;
    QString mEmail;
    QString mVersion;
    QStringList mValidSeverities;
    QStringList mValidPriorities;
    QStringList mValidStatuses;
    QStringList mMonitorComponents;
    QNetworkAccessManager *pManager;
    QNetworkCookieJar *pCookieJar;
    bool mLoggedIn;
    int mPendingCommentInsertions;
    int mUpdateCount;
    SqlWriterThread *pSqlWriter;
};

Q_DECLARE_METATYPE(Backend*)



#endif // BACKEND_H
