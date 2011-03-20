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
#include <QBuffer>
#include <QDateTime>
#include <QUuid>
#include <QStringList>
#include <QString>
#include <QSqlTableModel>
#include <QMessageBox>
#include <QDesktopServices>
#include <QVariantMap>
#include <qjson/parser.h>
#include <qjson/serializer.h>

#include "Launchpad.h"

Launchpad::Launchpad(const QString &url, QObject *parent) :
    Backend(url)
{
    mConsumerToken = "Entomologist";
    if (!url.startsWith("api"))
        mApiUrl = "https://api." + QUrl(url).host();
    else
        mApiUrl = url;
            pManager->setCookieJar(pCookieJar);
    connect(pManager, SIGNAL(sslErrors(QNetworkReply *, const QList<QSslError> &)),
            this, SLOT(handleSslErrors(QNetworkReply *, const QList<QSslError> &)));
    connect(pSqlWriter, SIGNAL(bugsFinished(QStringList)),
            this, SLOT(bugsInsertionFinished(QStringList)));
    connect(pSqlWriter, SIGNAL(commentFinished()),
            this, SLOT(commentInsertionFinished()));
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
    }
    else
    {
        QStringList list = mPassword.split(" ");
        mToken = list.at(0);
        mSecret = list.at(1);
        getSubscriberBugs();
    }

}

void
Launchpad::login()
{
// No need with OAuth
}

void
Launchpad::getUserBugs()
{
    QString url = QString("%1/1.0/~%2?ws.op=searchTasks&assignee=%1/1.0/~%2&modified_since=%3")
                         .arg(mApiUrl)
                         .arg(mUsername)
                         .arg(mLastSync.toString("yyyy-MM-ddThh:mm:ss"));
    QNetworkRequest req = QNetworkRequest(QUrl(url));
    req.setRawHeader("Authorization", authorizationHeader().toAscii());
    qDebug() << "getUserBugs fetching " << url;
    QNetworkReply *rep = pManager->get(req);
    connect(rep, SIGNAL(finished()),
            this, SLOT(bugListFinished()));
}

void
Launchpad::getSubscriberBugs()
{
    QString url = QString("%1/1.0/~%2?ws.op=searchTasks&bug_subscriber=%1/1.0/~%2&modified_since=%3")
                         .arg(mApiUrl)
                         .arg(mUsername)
                         .arg(mLastSync.toString("yyyy-MM-ddThh:mm:ss"));
    QNetworkRequest req = QNetworkRequest(QUrl(url));
    req.setRawHeader("Authorization", authorizationHeader().toAscii());
    qDebug() << "getSubscriberBugs fetching " << url;
    QNetworkReply *rep = pManager->get(req);
    connect(rep, SIGNAL(finished()),
            this, SLOT(subscriberListFinished()));
}

void
Launchpad::getReporterBugs()
{
    QString url = QString("%1/1.0/~%2?ws.op=searchTasks&bug_reporter=%1/1.0/~%2&modified_since=%3")
                         .arg(mApiUrl)
                         .arg(mUsername)
                         .arg(mLastSync.toString("yyyy-MM-ddThh:mm:ss"));
    QNetworkRequest req = QNetworkRequest(QUrl(url));
    req.setRawHeader("Authorization", authorizationHeader().toAscii());
    qDebug() << "getReporterBugs fetching " << url;
    QNetworkReply *rep = pManager->get(req);
    connect(rep, SIGNAL(finished()),
            this, SLOT(reporterListFinished()));
}

void
Launchpad::subscriberListFinished()
{
    QNetworkReply *reply = qobject_cast<QNetworkReply*>(sender());
    if (reply->error())
    {
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

    handleJSON(bugs, "CC");
    getReporterBugs();
}

void
Launchpad::reporterListFinished()
{
    QNetworkReply *reply = qobject_cast<QNetworkReply*>(sender());
    if (reply->error())
    {
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

    handleJSON(bugs, "Reported");
    getUserBugs();
}

void
Launchpad::bugListFinished()
{
    qDebug() << "Launchpad bug list finished";
    QNetworkReply *reply = qobject_cast<QNetworkReply*>(sender());
    QString response = reply->readAll();
    if (reply->error())
    {
        emit backendError(reply->errorString());
        return;
    }
    reply->deleteLater();
    QJson::Parser parser;
    bool ok;
    QVariant bugs = parser.parse(response.toAscii(), &ok);
    if (!ok)
    {
        emit backendError(tr("A parser error occurred, sorry."));
        return;
    }
    handleJSON(bugs, "Assigned");
    QMapIterator<QString, QVariant> i(mBugs);
    QList< QMap<QString,QString> > insertList;
    while (i.hasNext())
    {
        i.next();
        QVariantMap responseMap = i.value().toMap();
        QMap<QString, QString> newBug;
        newBug["tracker_id"] = mId;
        newBug["bug_id"] = responseMap.value("bug_id").toString();
        newBug["severity"] = responseMap.value("severity").toString();
        newBug["priority"] = responseMap.value("priority").toString();
        newBug["assigned_to"] = responseMap.value("assigned_to").toString();
        newBug["status"] = responseMap.value("status").toString();
        newBug["summary"] = responseMap.value("summary").toString();
        newBug["component"] = responseMap.value("component").toString();
        newBug["product"] = responseMap.value("product").toString();
        newBug["bug_type"] = responseMap.value("bug_type").toString();
        newBug["last_modified"] = responseMap.value("last_modified").toString();
        insertList << newBug;
    }

    pSqlWriter->insertBugs(insertList);
}

void
Launchpad::bugUploadFinished()
{
    qDebug() << "Bug upload finished";
    QNetworkReply *reply = qobject_cast<QNetworkReply*>(sender());
    QString id = reply->request().attribute(QNetworkRequest::User).toString();
    QString response = reply->readAll();

    if (reply->error())
    {
        qDebug() << response;
        emit backendError(reply->errorString());
        return;
    }
    reply->deleteLater();

    QJson::Parser parser;
    bool ok;
    QVariantMap bugs = parser.parse(response.toAscii(), &ok).toMap();

    if (!ok)
    {
        emit backendError(tr("A parser error occurred, sorry."));
        return;
    }

    mUploadList.remove(id);
    QSqlQuery sql;
    sql.exec(QString("DELETE FROM shadow_bugs WHERE bug_id=\'%1\' AND tracker_id=%2").arg(id).arg(mId));
    sql.prepare("UPDATE bugs SET priority=:priority , status=:status WHERE bug_id=:bug_id AND tracker_id=:tracker_id");
    sql.bindValue(":priority", bugs.value("importance").toString());
    sql.bindValue(":status", bugs.value("status").toString());
    sql.bindValue(":bug_id", id);
    sql.bindValue(":tracker_id", mId);
    sql.exec();
    getNextUpload();
}

void
Launchpad::commentUploadFinished()
{
    qDebug() << "Comment upload finished";
    QNetworkReply *reply = qobject_cast<QNetworkReply*>(sender());
    QString response = reply->readAll();
    qDebug() << response;

    if (reply->error())
    {
        emit backendError(reply->errorString());
        return;
    }
    reply->deleteLater();
    QVariantMap map = mCommentUploadList.takeAt(0).toMap();
    QSqlQuery sql;
    sql.exec(QString("DELETE FROM shadow_comments WHERE bug_id=\'%1\' AND tracker_id=%2")
                    .arg(map.value("bug_id").toString())
                     .arg(mId));
    getNextCommentUpload();
}

void
Launchpad::commentFinished()
{
    qDebug() << "Comment finished";
    QNetworkReply *reply = qobject_cast<QNetworkReply*>(sender());
    if (reply->error())
    {
        emit backendError(reply->errorString());
        return;
    }
    reply->deleteLater();
    QString bugId = reply->request().attribute(QNetworkRequest::User).toString();
    QString response = reply->readAll();
    QJson::Parser parser;
    bool ok;
    QVariant comments = parser.parse(response.toAscii(), &ok);
    if (!ok)
    {
        emit backendError(tr("A parser error occurred, sorry."));
        return;
    }

    QList<QMap<QString, QString> > list;
    QVariantList entries = comments.toMap().value("entries").toList();
    for (int i = 0; i < entries.size(); ++i)
    {
        QVariantMap bugMap = entries.at(i).toMap();
        QMap<QString, QString> newComment;
        newComment["tracker_id"] = mId;
        newComment["bug_id"] = bugId;
        QString ownerLink = bugMap.value("owner_link").toString();
        QString author = ownerLink.mid(ownerLink.lastIndexOf("~") + 1);
        newComment["author"] = author;
        newComment["comment_id"] = "";
        newComment["comment"] = bugMap.value("content").toString();
        newComment["timestamp"] = bugMap.value("date_created").toString();
        newComment["private"] = "0";
        list << newComment;
    }

    pSqlWriter->insertBugComments(list);
}

void
Launchpad::handleJSON(QVariant json, const QString &type)
{
    qDebug() << "-----------";
    qDebug() << json;
    qDebug() << "------------";
    QVariantList entries = json.toMap().value("entries").toList();
    for (int i = 0; i < entries.size(); ++i)
    {
        QVariantMap bugMap = entries.at(i).toMap();
        QVariantMap newBug;
        newBug["tracker_id"] = mId;

        // Break off the bug id from the link
        int index = bugMap.value("bug_link").toString().lastIndexOf("/");
        newBug["bug_id"] = bugMap.value("bug_link").toString().mid(index + 1);

        // Launchpad doesn't have severities
        newBug["severity"] = "";
        newBug["priority"] = bugMap.value("importance").toString();

        // Break off the username
        index = bugMap.value("assignee_link").toString().lastIndexOf("~");
        newBug["assigned_to"] = bugMap.value("assignee_link").toString().mid(index + 1);
        newBug["status"] = bugMap.value("status").toString();
        newBug["summary"] = bugMap.value("title").toString();
        newBug["component"] = bugMap.value("bug_target_display_name").toString();
        newBug["product"] = bugMap.value("bug_target_name").toString();
        newBug["bug_type"] = type;

        // Launchpad doesn't have a general last modified value for the bug tasks
        // and I don't really want to be making an additional call for each bug in the list
        newBug["last_modified"] =  friendlyTime(bugMap.value("date_created").toString());
        mBugs[newBug.value("bug_id").toString()] = newBug;
    }
}

void
Launchpad::checkVersion()
{
    // Launchpad doesn't have a call to find this out,
    // so we can only hope for the best.
    emit versionChecked("1.0");
}

void
Launchpad::checkValidPriorities()
{
    // Launchpad seems to hardcode these
    QStringList response;
    response << "Unknown"
             << "Critical"
             << "High"
             << "Medium"
             << "Low"
             << "Wishlist"
             << "Undecided";
    emit prioritiesFound(response);
}

void
Launchpad::checkValidSeverities()
{
    emit severitiesFound(QStringList());
}

void
Launchpad::checkValidStatuses()
{
    // Launchpad seems to hardcode these
    QStringList response;
    response << "New"
            << "Incomplete"
            << "Opinion"
            << "Invalid"
            << "Won't Fix"
            << "Expired"
            << "Confirmed"
            << "Triaged"
            << "In Progress"
            << "Fix Committed"
            << "Fix Released"
            << "Unknown";
    emit statusesFound(response);
}

void
Launchpad::getComments(const QString &bugId)
{
    QString url = QString("%1/1.0/bugs/%2/messages")
                         .arg(mApiUrl)
                         .arg(bugId);
    qDebug() << "Launchpad getComments URL: " << url;
    QNetworkRequest req = QNetworkRequest(QUrl(url));
    req.setAttribute(QNetworkRequest::User, bugId);
    req.setRawHeader("Authorization", authorizationHeader().toAscii());
    QNetworkReply *rep = pManager->get(req);
    connect(rep, SIGNAL(finished()),
            this, SLOT(commentFinished()));
}

void
Launchpad::uploadAll()
{
// Qt < 4.7 don't allow us to make an HTTP PATCH call
#if QT_VERSION < 0x040700
    emit bugsUpdated();
    return;
#endif
    if (!hasPendingChanges())
    {
        emit bugsUpdated();
        return;
    }
    qDebug() << mLastSync.toString("yyyy-MM-ddThh:mm:ss");
    mUploadList.clear();
    mCommentUploadList.clear();
    QString sql = QString("SELECT bug_id, severity, priority, assigned_to, status, summary FROM shadow_bugs where tracker_id=%1")
                       .arg(mId);
    QString commentSql = QString("SELECT bug_id, comment, private FROM shadow_comments WHERE tracker_id=%1")
                         .arg(mId);
    QSqlQuery comment(commentSql);
    while (comment.next())
    {
        QVariantMap map;
        map["bug_id"] = comment.value(0).toString();
        map["comment"] = comment.value(1).toString();
        map["private"] = comment.value(2).toString();
        mCommentUploadList << map;
    }

    QSqlQuery q(sql);
    while (q.next())
    {
        QVariantMap ret;
        if (!q.value(2).isNull())
            ret["importance"] = q.value(2).toString().remove(QRegExp("<[^>]*>"));
        if (!q.value(3).isNull())
            ret["assignee_link"] = QString("%1/1.0/~%2")
                                   .arg(mUrl)
                                   .arg(q.value(3).toString().remove(QRegExp("<[^>]*>")));
        if (!q.value(4).isNull())
            ret["status"] = q.value(4).toString().remove(QRegExp("<[^>]*>"));
        mUploadList[q.value(0).toString()] = ret;
    }
    getNextUpload();
}

void
Launchpad::getNextUpload()
{
#if QT_VERSION >= 0x040700
    qDebug() << "getNextUpload...";
    QString bugId = "";
    QMapIterator<QString, QVariant> i(mUploadList);
    if (!i.hasNext())
    {
        getNextCommentUpload();
        return;
    }
    i.next();

    bugId = i.key();
    QSqlQuery q;
    // We need a project ("target" in Launchpad terms) in order
    // to modify the bug
    q.prepare("SELECT product FROM bugs WHERE bug_id=:bug AND tracker_id=:tracker");
    q.bindValue(":bug", bugId);
    q.bindValue(":tracker_id", mId);
    q.exec();
    q.next();
    if (q.value(0).isNull())
    {
        getNextCommentUpload();
        return;
    }
    QVariantMap values = i.value().toMap();
    QJson::Serializer serializer;
    QByteArray *serialized = new QByteArray(serializer.serialize(values));
    QBuffer *out = new QBuffer(serialized, pManager);
    out->open(QIODevice::ReadOnly);
    QString url = QString("%1/1.0/%2/+bug/%3")
                         .arg(mApiUrl)
                         .arg(q.value(0).toString())
                         .arg(bugId);
    QNetworkRequest req = QNetworkRequest(QUrl(url));
    req.setAttribute(QNetworkRequest::User, bugId);
    qDebug ()<< "Auth header: " << authorizationHeader().toAscii();
    req.setRawHeader("Authorization", authorizationHeader().toAscii());    
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    req.setHeader(QNetworkRequest::ContentLengthHeader, serialized->length());
    req.setRawHeader("Referer", url.toAscii());
    req.setRawHeader("Cookie", "OPENSESAME");
    qDebug() << "sending custom request to " << url;
    QNetworkReply *rep = pManager->sendCustomRequest(req, "PATCH", out);
    connect(rep, SIGNAL(finished()),
            this, SLOT(bugUploadFinished()));
#endif
}

void
Launchpad::getNextCommentUpload()
{
    qDebug() << "getNextCommentUpload...";
    if (mCommentUploadList.size() == 0)
    {
        emit bugsUpdated();
        return;
    }

    qDebug() << "Going to upload a comment...";
    QVariantMap map = mCommentUploadList.at(0).toMap();
    QString bugId = map.value("bug_id").toString();
    QSqlQuery q;
    // We need a project ("target" in Launchpad terms) in order
    // to modify the bug
    q.prepare("SELECT product FROM bugs WHERE bug_id=:bug AND tracker_id=:tracker");
    q.bindValue(":bug", bugId);
    q.bindValue(":tracker_id", mId);
    q.exec();
    q.next();
    if (q.value(0).isNull())
    {
        emit bugsUpdated();
        return;
    }

    QVariantMap commentMap;
    commentMap.insert("content", map.value("comment").toString());
    QJson::Serializer serializer;
    QString url = QString("%1/1.0/bugs/%2")
            .arg(mApiUrl)
            .arg(bugId);
    QNetworkRequest req = QNetworkRequest(QUrl(url));
    req.setAttribute(QNetworkRequest::User, bugId);
    req.setRawHeader("Authorization", authorizationHeader().toAscii());
//    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
//    req.setHeader(QNetworkRequest::ContentLengthHeader, comment.length());
    req.setRawHeader("Referer", url.toAscii());
    req.setRawHeader("Cookie", "OPENSESAME");
    qDebug() << "sending custom request to " << url;
    QString comment = QString("ws.op=newMessage&content=\"%1\"").arg(map.value("comment").toString());
    QNetworkReply *rep = pManager->post(req, comment.toAscii());
    connect(rep, SIGNAL(finished()),
            this, SLOT(commentUploadFinished()));
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
    QString header("OAuth realm=\"%1\","
                   "oauth_consumer_key=\"%2\","
                   "oauth_token=\"%3\","
                   "oauth_signature_method=\"PLAINTEXT\","
                   "oauth_signature=\"&%4\","
                   "oauth_timestamp=\"%5\","
                   "oauth_nonce=\"%6\","
                   "oauth_version=\"1.0\"");

    if (mSecret.isEmpty())
    {
        QStringList list = mPassword.split(" ");
        mToken = list.at(0);
        mSecret = list.at(1);
    }
    QUuid c = QUuid::createUuid();
    QString nonce = c.toString();
    uint timestamp = QDateTime::currentDateTime().toUTC().toTime_t();
    qDebug() << "Authorization header";
    QString url = "https://api." + QUrl(mUrl).host();
    return header.arg(url).arg(mConsumerToken).arg(mToken).arg(mSecret).arg(timestamp).arg(nonce);
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
Launchpad::bugsInsertionFinished(QStringList idList)
{
    updateSync();
    emit bugsUpdated();
}

void
Launchpad::commentInsertionFinished()
{
    emit commentsCached();
}

void
Launchpad::networkError(QNetworkReply::NetworkError e)
{
    qDebug() << "Network error: " << e;
}

void
Launchpad::handleSslErrors(QNetworkReply *reply,
                         const QList<QSslError> &errors)
{
    reply->ignoreSslErrors();
}
