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
#include <QDateTime>
#include <QUuid>
#include <QStringList>
#include <QString>
#include <QMessageBox>
#include <QDesktopServices>
#include <qjson/parser.h>

#include "Launchpad.h"

Launchpad::Launchpad(const QString &url, QObject *parent) :
    Backend(url)
{
    mConsumerToken = "Entomologist";
    if (!url.startsWith("api"))
        mApiUrl = "https://api." + QUrl(url).host();
    else
        mApiUrl = url;
}

Launchpad::~Launchpad()
{
}

void
Launchpad::sync()
{
    if (mPassword.isEmpty())
    {
        // We need to initiate the OAuth authorization
        QString url = mUrl + "/+request-token";
        QString query = QString("oauth_consumer_key=%1&oauth_signature_method=PLAINTEXT&oauth_signature=%26").arg(mConsumerToken);
        qDebug() << "Fetching " << url;
        QNetworkRequest req = QNetworkRequest(QUrl(url));
        QNetworkReply *rep = pManager->post(req, query.toAscii());
        connect(rep, SIGNAL(finished()),
                this, SLOT(requestTokenFinished()));
        qDebug() << "Synced this thang";
    }
    else
    {
        QStringList list = mPassword.split(" ");
        mToken = list.at(0);
        mSecret = list.at(1);
        getUserBugs();
    }

}

void
Launchpad::login()
{
}

void
Launchpad::getUserBugs()
{
    QString url = QString("%1/1.0/~%2?ws.op=searchTasks&modified_since=%3")
                         .arg(mApiUrl)
                         .arg(mUsername)
                         .arg(mLastSync.toString("yyyy-MM-dd"));
    QNetworkRequest req = QNetworkRequest(QUrl(url));
    req.setRawHeader("Authentication", authorizationHeader().toAscii());
    qDebug() << "getUserBugs fetching " << url;
    QNetworkReply *rep = pManager->get(req);
    connect(rep, SIGNAL(finished()),
            this, SLOT(bugListFinished()));
}

void
Launchpad::bugListFinished()
{
    qDebug() << "Bug list finished";
    QNetworkReply *reply = qobject_cast<QNetworkReply*>(sender());
    if (reply->error())
    {
        qDebug() << "ERROR: " << reply->errorString();
        emit backendError(reply->errorString());
        return;
    }
    reply->deleteLater();
    QString response = reply->readAll();
    QJson::Parser parser;

    bool ok;
    QVariant bugs = parser.parse(response.toAscii(), &ok);
    if (!ok)
    {
        emit backendError(tr("A parser error occurred, sorry."));
        return;
    }
    QVariantList entries = bugs.toMap().value("entries").toList();
    for (int i = 0; i < entries.size(); ++i)
    {
        QVariantMap bugMap = entries.at(i).toMap();
        QMapIterator<QString, QVariant> j(bugMap);
        while (j.hasNext())
        {
            j.next();
            qDebug() << j.key() << ": " << j.value() << endl;
        }
    }
    emit bugsUpdated();
}

void
Launchpad::checkVersion()
{

}

void
Launchpad::checkValidPriorities()
{

}

void
Launchpad::checkValidSeverities()
{

}

void
Launchpad::checkValidStatuses()
{

}

void
Launchpad::uploadAll()
{

}

QString
Launchpad::buildBugUrl(const QString &id)
{
    return("");
}

// OAuth related functions

QString
Launchpad::authorizationHeader()
{
    qDebug() << "Authorization header";
    QString header("OAuth realm=\"%1\","
                   "oauth_consumer_key=\"%2\","
                   "oauth_token=\"%3\","
                   "oauth_signature_method=\"PLAINTEXT\","
                   "oauth_signature=\"%4\","
                   "oauth_timestamp=\"%5\","
                   "oauth_nonce=\"%6\","
                   "oauth_version=\"1.0\"");

    QUuid c = QUuid::createUuid();
    QString nonce = c.toString();
    uint timestamp = QDateTime::currentDateTime().toUTC().toTime_t();
    qDebug() << "Authorization header";
    return header.arg(mUrl).arg(mConsumerToken).arg(mToken).arg(mSecret).arg(timestamp).arg(nonce);
}

void
Launchpad::requestTokenFinished()
{
    qDebug() << "requestTokenFinished";
    QNetworkReply *reply = qobject_cast<QNetworkReply*>(sender());
    if (reply->error())
    {
        qDebug() << "ERROR: " << reply->errorString();
        emit backendError(reply->errorString());
        return;
    }
    reply->deleteLater();
    QString response = reply->readAll();
    QMap<QString, QString> responseMap;
    qDebug() << "Response: " << response;
    QStringList list = response.split("&", QString::SkipEmptyParts);

    for (int i = 0; i < list.size(); ++i)
    {
        QStringList parts = list.at(i).split("=");
        qDebug() << "Parts 0: " << parts.at(0) << " Parts 1: " << parts.at(1);
        responseMap[parts.at(0)] = parts.at(1);
    }

    mToken = responseMap["oauth_token"];
    mSecret = responseMap["oauth_token_secret"];
    qDebug() << "requestTokenFinished: authenticate user";
    authenticateUser();
}

void
Launchpad::requestRealTokenFinished()
{
    qDebug() << "Real token finished";
    QNetworkReply *reply = qobject_cast<QNetworkReply*>(sender());
    if (reply->error())
    {
        qDebug() << "ERROR: " << reply->errorString();
        emit backendError(reply->errorString());
        return;
    }
    reply->deleteLater();
    QString response = reply->readAll();
    QMap<QString, QString> responseMap;
    qDebug() << "Response: " << response;
    QStringList list = response.split("&", QString::SkipEmptyParts);

    for (int i = 0; i < list.size(); ++i)
    {
        QStringList parts = list.at(i).split("=");
        qDebug() << "Parts 0: " << parts.at(0) << " Parts 1: " << parts.at(1);
        responseMap[parts.at(0)] = parts.at(1);
    }

    mToken = responseMap["oauth_token"];
    mSecret = responseMap["oauth_token_secret"];
    mPassword = mToken + " " + mSecret;
    saveCredentials();
    qDebug() << "REAL TOKEN FINISHED";
    getUserBugs();
}

void
Launchpad::authenticateUser()
{
    QString authUrl = mUrl + QString("/+authorize-token?oauth_token=%1").arg(mToken);
    QString authString("In order to retrieve Launchpad bugs, you need to authorize Entomologist. "
                       "When you press the <b>OK</b> button, your web browser will take you to <tt>Launchpad</tt>.  "
                       "Authorize Entomologist to <b>Change Anything</b> and then press the next dialog's <b>OK</b> button");
    QMessageBox box;
    box.setText(authString);
    if (box.exec() == QMessageBox::Ok)
    {
        QDesktopServices::openUrl(authUrl);
        QMessageBox newBox;
        newBox.setText("You've authorized the access?");
        newBox.setStandardButtons(QMessageBox::Cancel|QMessageBox::Ok);
        if (newBox.exec() == QMessageBox::Ok)
        {
            getRealToken();
        }
        else
        {
            qDebug() << "OAuth cancelled";
            emit backendError("You cancelled the Launchpad OAuth process.");
        }
    }
}

void
Launchpad::getRealToken()
{
    qDebug() << "getRealToken";
    QString url = mUrl + "/+access-token";
    QString query = QString("oauth_signature=\%26%1&oauth_consumer_key=%2&oauth_token=%3&oauth_signature_method=PLAINTEXT")
                    .arg(mSecret)
                    .arg(mConsumerToken)
                    .arg(mToken);
    qDebug() << "getRealToken fetching " << url;
    qDebug() << "getRealToken Query: " << query;
    QNetworkRequest req = QNetworkRequest(QUrl(url));
    QNetworkReply *rep = pManager->post(req, query.toAscii());
    connect(rep, SIGNAL(finished()),
            this, SLOT(requestRealTokenFinished()));
    connect(rep, SIGNAL(error(QNetworkReply::NetworkError)),
            this, SLOT(networkError(QNetworkReply::NetworkError)));
    qDebug() << "getRealToken is finished";
}

void
Launchpad::networkError(QNetworkReply::NetworkError e)
{
    qDebug() << "Network error: " << e;
}
