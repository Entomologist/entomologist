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
 */
#ifndef TRAC_H
#define TRAC_H

#include <QObject>
#include "Backend.h"
#include "libmaia/maiaXmlRpcClient.h"

class Trac : public Backend
{
Q_OBJECT
public:
    Trac(const QString &url, const QString &username, const QString &password, QObject *parent = 0);
    ~Trac();
    BackendUI *displayWidget();

    void sync();
    void login();
    void checkVersion();
    void getComments(const QString &bugId);
    void getSearchedBug(const QString &bugId);

    void checkFields();
    void checkValidComponents();
    void search(const QString &query);

    QString type() { return "trac"; }

    void uploadAll();
    void setUsername(const QString &username);
    void setPassword(const QString &password);

    QString buildBugUrl(const QString &id);
    QString autoCacheComments() {return "0";}
    void deleteData();

public slots:
    void commentInsertionFinished();
    void bugsInsertionFinished(QStringList idList);
    void searchedTicketResponse(QVariant &arg);
    void uploadFinished(QVariant &arg);
    void versionRpcResponse(QVariant &arg);
    void priorityRpcResponse(QVariant &arg);
    void severityRpcResponse(QVariant &arg);
    void versionsRpcResponse(QVariant &arg);
    void resolutionsRpcResponse(QVariant &arg);
    void componentRpcResponse(QVariant &arg);
    void milestonesRpcResponse(QVariant &arg);
    void searchRpcResponse(QVariant &arg);
    void searchInsertionFinished();
    void statusRpcResponse(QVariant &arg);
    void typeRpcResponse(QVariant &arg);
    void ccRpcResponse(QVariant &arg);
    void reporterRpcResponse(QVariant &arg);
    void ownerRpcResponse(QVariant &arg);
    void monitoredComponentsRpcResponse(QVariant &arg);
    void bugDetailsRpcResponse(QVariant &arg);
    void changelogRpcResponse(QVariant &arg);
    void rpcError(int error, const QString &message);
    void versionRpcError(int error, const QString &message);
    void headFinished();
    void handleSslErrors(QNetworkReply *reply, const QList<QSslError> &errors);

private:
    void checkValidSeverities();
    void checkValidStatuses();
    void checkValidVersions();
    void checkValidResolutions();
    void checkValidMilestones();

    MaiaXmlRpcClient *pClient;
    QMap<QString, QString> mBugMap;
    QStringList mSeverities;
    QString mActiveCommentId;
};

#endif // TRAC_H
