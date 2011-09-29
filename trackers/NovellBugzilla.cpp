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

#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkCookieJar>

#include "NovellBugzilla.h"

NovellBugzilla::NovellBugzilla(const QString &url)
    : Bugzilla(url)
{
    checkingVersion = false;
}

NovellBugzilla::~NovellBugzilla()
{
}

void
NovellBugzilla::checkVersion()
{
    checkingVersion = true;
    login();
}

void
NovellBugzilla::sync()
{
    checkingVersion = false;
    qDebug() << "NovellBugzilla::login";
    QString ichainLogin = "https://bugzilla.novell.com/ICSLogin/auth-up";
    QByteArray username(QString("username=%1&password=%2").arg(mUsername).arg(mPassword).toLocal8Bit());

    QNetworkRequest request = QNetworkRequest(QUrl(ichainLogin));
    QNetworkReply *reply = pManager->post(request, username);
    connect(reply, SIGNAL(finished()),
            this, SLOT(syncFinished()));
}

// Novell bugzilla uses a proprietary login system called "ichain".  We
// log into ichain by mimicing a POST form, save the cookie they give us, and
// then we can use the XMLRPC calls

void
NovellBugzilla::login()
{
    qDebug() << "NovellBugzilla::login";
    QString ichainLogin = "https://bugzilla.novell.com/ICSLogin/auth-up";
    QByteArray username(QString("username=%1&password=%2").arg(mUsername).arg(mPassword).toLocal8Bit());

    QNetworkRequest request = QNetworkRequest(QUrl(ichainLogin));
    QNetworkReply *reply = pManager->post(request, username);
    connect(reply, SIGNAL(finished()),
            this, SLOT(finished()));
}

void
NovellBugzilla::finished()
{
    QNetworkReply *reply = qobject_cast<QNetworkReply*>(sender());
    if (reply->error())
    {
        qDebug() << "ERROR: " << reply->errorString();
        emit backendError(reply->errorString());
        return;
    }
    reply->deleteLater();

    if (checkingVersion)
        Bugzilla::checkVersion();
    else
        Bugzilla::login();
}

void
NovellBugzilla::syncFinished()
{
    QNetworkReply *reply = qobject_cast<QNetworkReply*>(sender());
    if (reply->error())
    {
        qDebug() << "ERROR: " << reply->errorString();
        emit backendError(reply->errorString());
        return;
    }
    reply->deleteLater();

    if (checkingVersion)
        Bugzilla::checkVersion();
    else
        Bugzilla::sync();
}
