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
 *			Andrew Wafaa <awafaa@opensuse.org>
 */
#ifndef GITHUB_H
#define GITHUB_H

#include <QObject>
#include "Backend.h"

class Github : public Backend
{
Q_OBJECT
public:
    Github(const QString &url, QObject *parent = 0);
    ~Github();

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

#endif // GITHUB_H
