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
#include <QSslConfiguration>
#include <QStringList>
#include <QNetworkReply>
#include <QNetworkAccessManager>
#include <QNetworkCookieJar>
#include <QDateTime>
#include <QSqlQuery>

#include "SqlWriterThread.h"
class Backend : public QObject
{
    Q_OBJECT
public:
    Backend(const QString &url);
    ~Backend();

    void setId(const QString &id) { mId = id; }
    QString id() { return mId; }

    void setName(const QString &name) { mName = name; }
    QString name() { return mName; }

    void setUrl(const QString &url) { mUrl = url; }
    QString url() { return mUrl; }

    void setUsername(const QString &username) { mUsername = username; }
    QString username() { return mUsername; }

    void setPassword(const QString &password) { mPassword = password; }
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

    void setLastSync(const QString &dateTime);
    // The top level sync call
    virtual void sync() {}

    // This is used to override login methods (like in Novell Bugzilla)
    virtual void login() {}

    // Version check
    virtual void checkVersion() {}

    // These get the valid values
    virtual void checkValidPriorities() {}
    virtual void checkValidSeverities() {}
    virtual void checkValidStatuses() {}

    // Tells the backend to upload everything
    virtual void uploadAll() {}

    // Given a bug ID, create an HTTP url for viewing in a web browser
    // (called by the bug list context menu)
    virtual void buildBugUrl(const QString &id) {}

    // This keeps track of how many bugs were updated in the last sync.
    // It's used to pop up the system tray notification.
    int latestUpdateCount() { return mUpdateCount; }

signals:
    void bugsUpdated();
    void versionChecked(QString version);
    void prioritiesFound(QStringList priorities);
    void severitiesFound(QStringList severities);
    void statusesFound(QStringList statuses);
    void backendError(const QString &message);
    void sqlQuery(QMap<QString, QString> map);

public slots:
    virtual void commentInsertionFinished() {}
    virtual void bugsInsertionFinished(QStringList idList) {}

protected:
    void updateSync();

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
    QNetworkAccessManager *pManager;
    QNetworkCookieJar *pCookieJar;
    bool mLoggedIn;
    int mPendingCommentInsertions;
    int mUpdateCount;
    SqlWriterThread *pSqlWriter;
};



#endif // BACKEND_H
