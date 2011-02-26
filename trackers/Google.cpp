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
#include <QStringList>
#include <QXmlStreamReader>
#include <QString>
#include <QRegExp>
#include <QDesktopServices>

#include "Google.h"

// The Google Code issue API doesn't allow for searching by CC or reported,
// so we just support 'assigned'.

Google::Google(const QString &url, QObject *parent) :
    Backend(url)
{
    mToken = "";
    mProject = "";
    QString tmp = QUrl(url).path();
    // We've directed the user to use the code.google.com/p/PROJECT_NAME format, hopefully they listened...
    QRegExp rx("/p/([^/]*)/");
    if (rx.indexIn(tmp, 0) != -1)
        mProject = rx.cap(1);
}
Google::~Google()
{
}

void
Google::sync()
{
    if (mProject == "")
    {
        emit backendError("Invalid Google Code Hosting project");
        return;
    }
    if (mToken.isEmpty())
        login();
}

void
Google::login()
{
    QString tokenUrl = "https://www.google.com/accounts/ClientLogin";
    QNetworkRequest req = QNetworkRequest(QUrl(tokenUrl));
    req.setHeader(QNetworkRequest::ContentTypeHeader,"application/x-www-form-urlencoded");
    QString fields = QString("accountType=GOOGLE&Email=%1&Passwd=%2&service=code&source=entomologist-%3")
                             .arg(mUsername)
                             .arg(mPassword)
                             .arg(APP_VERSION);
    QNetworkReply *rep = pManager->post(req, fields.toAscii());
    connect(rep, SIGNAL(finished()),
            this, SLOT(tokenFinished()));
    connect(rep, SIGNAL(error(QNetworkReply::NetworkError)),
            this, SLOT(networkError(QNetworkReply::NetworkError)));
}

void
Google::tokenFinished()
{
    QNetworkReply *reply = qobject_cast<QNetworkReply*>(sender());
    if (reply->error())
    {
        emit backendError(reply->errorString());
        reply->close();
        return;
    }

    QString response = reply->readAll();
    QStringList list = response.split('\n');
    for (int i = 0; i < list.size(); ++i)
    {
        QString e = list.at(i);
        QStringList entry = e.split('=');
        if (entry.at(0) == "Auth")
            mToken = entry.at(1);
    }


    if (mToken.isEmpty())
    {
        qDebug() << "Couldn't find the token!";
        emit backendError("Google responded with something I can't understand.");
    }
    else
    {
        qDebug() << "Auth token: " << mToken;
        fetchBugs();
    }
}

void
Google::fetchBugs()
{
    QStringList u = mUsername.split("@");
    QString user = u.at(0);
    QString url = QString("https://code.google.com/feeds/issues/p/%1/issues/full?updated-min=%2&owner=%3")
                  .arg(mProject)
                  .arg(mLastSync.toUTC().toString("yyyy-MM-ddThh:mm:ss"))
                  .arg(user);
    QString auth = QString("GoogleLogin auth=%1")
                   .arg(mToken);
    QNetworkRequest req = QNetworkRequest(QUrl(url));
    req.setHeader(QNetworkRequest::ContentTypeHeader,"application/x-www-form-urlencoded");
    req.setRawHeader("Authorization", auth.toAscii());
    QNetworkReply *rep = pManager->get(req);
    connect(rep, SIGNAL(finished()),
            this, SLOT(bugsFinished()));
    connect(rep, SIGNAL(error(QNetworkReply::NetworkError)),
            this, SLOT(networkError(QNetworkReply::NetworkError)));

}
void
Google::bugsFinished()
{
    bool inEntry;
    QString text, name, qualifiedName;
    QList< QMap<QString,QString> > insertList;
    QMap<QString, QString> bugMap;
    QNetworkReply *reply = qobject_cast<QNetworkReply*>(sender());
    if (reply->error())
    {
        emit backendError(reply->errorString());
        reply->close();
        return;
    }
#if 0
    newBug["severity"] = responseMap.value("severity").toString();
    newBug["priority"] = responseMap.value("priority").toString();
    newBug["component"] = responseMap.value("component").toString();
    newBug["product"] = responseMap.value("product").toString();

#endif

    QString response = reply->readAll();
    qDebug() << "---------RESPONSE";
    qDebug() << response;
    qDebug() << "----------";
    QXmlStreamReader xmlReader(response);
    while (!xmlReader.atEnd())
    {
        xmlReader.readNext();
        name = xmlReader.name().toString();
        qualifiedName = xmlReader.qualifiedName().toString();
        if (xmlReader.isStartElement())
        {
            if (name == "entry")
            {
                inEntry = true;
                bugMap.clear();
                bugMap["tracker_id"] = mId;
                bugMap["bug_type"] = "Assigned";
            }
        }
        else if (xmlReader.isEndElement())
        {
            if (name == "entry" && inEntry)
            {
                insertList << bugMap;
                inEntry = false;
            }
            else if (qualifiedName == "issues:id" && inEntry)
            {
                bugMap["bug_id"] = text.mid(text.lastIndexOf("/") + 1);
                qDebug() << "New bug id: " << bugMap["bug_id"];
            }
            else if (name == "updated" && inEntry)
            {
                bugMap["last_modified"] = text;
            }
            else if (name == "title" && inEntry)
            {
                bugMap["summary"] = text;
            }
            else if (qualifiedName == "issues:state" && inEntry)
            {
                bugMap["status"] = text;
            }
            else if (qualifiedName == "issues:username" && inEntry)
            {
                bugMap["assigned_to"] = text;
            }

        }
        else if (xmlReader.isCharacters() && !xmlReader.isWhitespace())
        {
            text = xmlReader.text().toString();
        }
    }

    reply->close();

    emit bugsUpdated();
}

void
Google::checkVersion()
{

}

void
Google::checkValidPriorities()
{

}

void
Google::checkValidSeverities()
{

}

void
Google::checkValidStatuses()
{

}

void
Google::uploadAll()
{

}

QString
Google::buildBugUrl(const QString &id)
{
    return("");
}

void
Google::networkError(QNetworkReply::NetworkError e)
{
    qDebug() << "Network error: " << e;
}
