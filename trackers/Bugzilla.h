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

#ifndef BUGZILLA_H
#define BUGZILLA_H

#include "Backend.h"
#include "libmaia/maiaXmlRpcClient.h"

class QNetworkReply;
class QSslError;

class Bugzilla : public Backend
{
Q_OBJECT
public:

    Bugzilla(const QString &url);
    ~Bugzilla();
    BackendUI *displayWidget();
    void sync();
    void login();
    void checkVersion();
    void getComments(const QString &bugId);
    void getSearchedBug(const QString &bugId);

    void search(const QString &query);

    void checkValidPriorities();
    void checkValidSeverities();
    void checkValidStatuses();
    void checkValidComponents();
    void checkFields();
    void checkValidComponentsForProducts(const QString &product);

    QString type() { return "bugzilla"; }
    void uploadAll();

    QString buildBugUrl(const QString &id);
    QString autoCacheComments() { return "0"; }

public slots:
    void searchCallFinished();
    void individualBugFinished();
    void searchInsertionFinished();
    void idDetailsFinished();
    void itemPostFinished();
    void ccFinished();
    void commentInsertionFinished();
    void bugsInsertionFinished(QStringList idList);
    void commentXMLFinished();
    void reportedBugListFinished();
    void userBugListFinished();

    void versionRpcResponse(QVariant &arg);
    void loginRpcResponse(QVariant &arg);
    void loginSyncRpcResponse(QVariant &arg);
    void emailRpcResponse(QVariant &arg);
    void bugRpcResponse(QVariant &arg);
    void reportedRpcResponse(QVariant &arg);
    void commentRpcResponse(QVariant &arg);
    void rpcError(int error, const QString &message);
    void versionError(int error, const QString &message);
    void priorityResponse(QVariant &arg);
    void severityResponse(QVariant &arg);
    void componentsResponse(QVariant &arg);
    void productsResponse(QVariant &arg);
    void productComponentResponse(QVariant &arg);
    void productNamesResponse(QVariant &arg);
    void addCommentResponse(QVariant &arg);

    void statusResponse(QVariant &arg);
    void handleSslErrors(QNetworkReply *reply, const QList<QSslError> &errors);

protected:
    MaiaXmlRpcClient *pClient;
    void postNewItems(QMap<QString, QString> tokenMap);
    void postItem();
    void postComments();
    void postComment();
    void doUploading();
    void parseBuglistCSV(const QString &csv, const QString &bugType);
    QMap<QString, QString> getShadowValues(const QString &id);
    QVariantMap mProductMap;
    QString mCurrentProduct;
    QVariantMap mBugs;
    QString mBugzillaId;
    QList< QMap<QString, QString> > mPostQueue;
    QList< QMap<QString, QString> > mCommentQueue;
    QString mActiveCommentId;
    bool mUploading;
    void getUserEmail();
    void getUserBugs();
    void getReportedBugs();
    void getCCs();
};

#endif // BUGZILLA_H
