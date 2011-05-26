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

#include "Trac.h"

Trac::Trac(const QString &url,
           const QString &username,
           const QString &password,
           QObject *parent) :
    Backend(url)
{
    QUrl myUrl(mUrl + "/login/xmlrpc");
    myUrl.setUserName(username);
    myUrl.setPassword(password);
    pClient = new MaiaXmlRpcClient(myUrl, "Entomologist");
    QSslConfiguration config = pClient->sslConfiguration();
    config.setProtocol(QSsl::AnyProtocol);
    config.setPeerVerifyMode(QSslSocket::VerifyNone);
    pClient->setSslConfiguration(config);
    //pClient->setCookieJar(pCookieJar);
    //pCookieJar->setParent(0);

    connect(pClient, SIGNAL(sslErrors(QNetworkReply *, const QList<QSslError> &)),
            this, SLOT(handleSslErrors(QNetworkReply *, const QList<QSslError> &)));
    connect(pManager, SIGNAL(sslErrors(QNetworkReply *, const QList<QSslError> &)),
            this, SLOT(handleSslErrors(QNetworkReply *, const QList<QSslError> &)));
    connect(pSqlWriter, SIGNAL(commentFinished()),
            this, SLOT(commentInsertionFinished()));
    connect(pSqlWriter, SIGNAL(bugsFinished(QStringList)),
            this, SLOT(bugsInsertionFinished(QStringList)));
}

void Trac::setUsername(const QString &username)
{
    mUsername = username;
    pClient->setUserName(username);
}

void
Trac::setPassword(const QString &password)
{
    mPassword = password;
    pClient->setPassword(password);
}

Trac::~Trac()
{
}

void
Trac::sync()
{
    qDebug() << "Syncing CCs...";
    QVariantList args;
    QString closed = "";
    if (mLastSync.date().year() == 1970)
        closed = "status!=closed&";
    QString query = QString("%1cc=%2&max=0&modified=%3..")
                    .arg(closed)
                    .arg(mUsername)
                    .arg(mLastSync.toString("yyyy-MM-dd"));
    args << query;
    pClient->call("ticket.query", args, this, SLOT(ccRpcResponse(QVariant&)), this, SLOT(rpcError(int, const QString &)));
}

void
Trac::getComments(const QString &bugId)
{
    QVariantList args;
    args.append(bugId.toInt());
    mActiveCommentId = bugId;
    pClient->call("ticket.changeLog", args, this, SLOT(changelogRpcResponse(QVariant&)), this, SLOT(rpcError(int, const QString &)));
}

void
Trac::login()
{
}

void
Trac::checkVersion()
{
    QVariantList args;
    pClient->call("system.getAPIVersion", args, this, SLOT(versionRpcResponse(QVariant&)), this, SLOT(versionRpcError(int, const QString &)));
}

void
Trac::checkValidPriorities()
{
    QVariantList args;
    pClient->call("ticket.priority.getAll", args, this, SLOT(priorityRpcResponse(QVariant&)), this, SLOT(rpcError(int, const QString &)));
}

void
Trac::checkValidSeverities()
{
    QVariantList args;
    pClient->call("ticket.severity.getAll", args, this, SLOT(severityRpcResponse(QVariant&)), this, SLOT(rpcError(int, const QString &)));
}

void
Trac::checkValidStatuses()
{
    QVariantList args;
    pClient->call("ticket.status.getAll", args, this, SLOT(statusRpcResponse(QVariant&)), this, SLOT(rpcError(int, const QString &)));
}

// This works for trac API 1.1.2, but "in the future" they may require
// that the action map come with a _ts timestamp listing the *last* time the
// bug updated (so we would need to make get calls for each updated bug).
void
Trac::uploadAll()
{
    if (!hasPendingChanges())
    {
        emit bugsUpdated();
        return;
    }

    QVariantList args, methodList;
    QVariantMap commentActionMap;
    commentActionMap["action"] = "leave";

    QString sql = QString("SELECT bug_id, severity, priority, assigned_to, status, summary FROM shadow_bugs where tracker_id=%1")
                       .arg(mId);
    QString commentSql = QString("SELECT bug_id, comment, private FROM shadow_comments WHERE tracker_id=%1")
                         .arg(mId);
    QSqlQuery comment(commentSql);
    while (comment.next())
    {
        QVariantMap newMethod;
        QVariantList newParams;
        newParams.append(comment.value(0).toInt());
        newParams.append(comment.value(1).toString());
        newParams.append(commentActionMap);
        newParams.append(false);
        newParams.append(mUsername);
        newMethod.insert("methodName", "ticket.update");
        newMethod.insert("params", newParams);
        methodList.append(newMethod);
    }

    QSqlQuery q(sql);
    while (q.next())
    {
        QVariantMap actionMap;
        actionMap["action"] = "leave";
        if (!q.value(1).isNull())
            actionMap["type"] = q.value(1).toString().remove(QRegExp("<[^>]*>"));
        if (!q.value(2).isNull())
            actionMap["priority"] = q.value(2).toString().remove(QRegExp("<[^>]*>"));
        if (!q.value(3).isNull())
        {
            actionMap["action"] = "reassign";
            actionMap["owner"] = q.value(3).toString().remove(QRegExp("<[^>]*>"));
        }
        if (!q.value(4).isNull())
            actionMap["status"] = q.value(4).toString().remove(QRegExp("<[^>]*>"));
        if (!q.value(5).isNull())
            actionMap["summary"] = q.value(5).toString().remove(QRegExp("<[^>]*>"));

        QVariantMap newMethod;
        QVariantList newParams;
        newParams.append(q.value(0).toInt());
        newParams.append("");
        newParams.append(actionMap);
        newParams.append(false);
        newParams.append(mUsername);
        newMethod.insert("methodName", "ticket.update");
        newMethod.insert("params", newParams);
        methodList.append(newMethod);
    }

    args.insert(0, methodList);;
    pClient->call("system.multicall", args, this, SLOT(uploadFinished(QVariant&)), this, SLOT(rpcError(int, const QString &)));
}

QString
Trac::buildBugUrl(const QString &id)
{
    return QString("%1/ticket/%2").arg(mUrl).arg(id);
}

void
Trac::uploadFinished(QVariant &arg)
{
    QSqlQuery sql;
    QVariantList bugList = arg.toList();
    for (int i = 0; i < bugList.size(); ++i)
    {
        qDebug() << i;
        QString bugId = bugList.at(i).toList().at(0).toList().at(0).toString();
        sql.exec(QString("DELETE FROM shadow_comments WHERE bug_id=\'%1\' AND tracker_id=%2")
                        .arg(bugId)
                        .arg(mId));
        sql.exec(QString("DELETE FROM shadow_bugs WHERE bug_id=\'%1\' AND tracker_id=%2")
                        .arg(bugId)
                        .arg(mId));
    }

    sync();
}

void
Trac::priorityRpcResponse(QVariant &arg)
{
    QStringList response = arg.toStringList();
    emit prioritiesFound(response);
}

void
Trac::severityRpcResponse(QVariant &arg)
{
    // Trac used to just use "severities", but then they renamed it to "type"
    // but users can still use "severities" if they'd like.  We'll check for both and
    // merge them together
    mSeverities = arg.toStringList();
    QVariantList args;
    pClient->call("ticket.type.getAll", args, this, SLOT(typeRpcResponse(QVariant&)), this, SLOT(rpcError(int, const QString &)));
}

void
Trac::ccRpcResponse(QVariant &arg)
{
    QStringList bugs = arg.toStringList();
    for (int i = 0; i < bugs.size(); ++i)
    {
        mBugMap.insert(bugs.at(i), "CC");
    }

    QVariantList args;
    QString closed = "";
    if (mLastSync.date().year() == 1970)
        closed = "status!=closed&";
    QString query = QString("%1reporter=%2&max=0&modified=%3..")
                    .arg(closed)
                    .arg(mUsername)
                    .arg(mLastSync.toString("yyyy-MM-dd"));
    args << query;
    pClient->call("ticket.query", args, this, SLOT(reporterRpcResponse(QVariant&)), this, SLOT(rpcError(int, const QString &)));
}

void
Trac::reporterRpcResponse(QVariant &arg)
{
    QStringList bugs = arg.toStringList();
    for (int i = 0; i < bugs.size(); ++i)
    {
        mBugMap.insert(bugs.at(i), "Reported");
    }

    QVariantList args;
    QString closed = "";
    if (mLastSync.date().year() == 1970)
        closed = "status!=closed&";
    QString query = QString("%1owner=%2&max=0&modified=%3..")
                    .arg(closed)
                    .arg(mUsername)
                    .arg(mLastSync.toString("yyyy-MM-dd"));
    args << query;
    pClient->call("ticket.query", args, this, SLOT(ownerRpcResponse(QVariant&)), this, SLOT(rpcError(int, const QString &)));
}

void
Trac::ownerRpcResponse(QVariant &arg)
{
    QStringList bugs = arg.toStringList();
    for (int i = 0; i < bugs.size(); ++i)
    {
        mBugMap.insert(bugs.at(i), "Assigned");
    }
    // The search response just gives us a list of bug numbers.
    // In order to get the full details, we bundle a bunch of XMLRPC
    // calls into one array, and ship that off to trac.
    QVariantList args, methodList;

    QMapIterator<QString, QString> i(mBugMap);
    while (i.hasNext())
    {
        i.next();
        QVariantMap newMethod;
        QVariantList newParams;
        newParams.append(i.key().toInt());
        newMethod.insert("methodName", "ticket.get");
        newMethod.insert("params", newParams);
        methodList.append(newMethod);
    }

    args.insert(0, methodList);;
    pClient->call("system.multicall", args, this, SLOT(bugDetailsRpcResponse(QVariant&)), this, SLOT(rpcError(int, const QString &)));
}

void
Trac::changelogRpcResponse(QVariant &arg)
{
    QList<QMap<QString, QString> > list;
    QVariantList changelogList = arg.toList();
    for (int i = 0; i < changelogList.size(); ++i)
    {
        QVariantList changes = changelogList.at(i).toList();
        QDateTime time = changes.at(0).toDateTime();
        QString author = changes.at(1).toString();
        QString field = changes.at(2).toString();
        QString oldValue = changes.at(3).toString();
        QString newValue = changes.at(4).toString();
        if ((field == "comment") && (!newValue.isEmpty()))
        {
            // It's an actual comment
            QMap<QString, QString> newComment;
            newComment["tracker_id"] = mId;
            newComment["bug_id"] = mActiveCommentId;
            newComment["author"] = author;
            newComment["comment_id"] = oldValue;
            newComment["comment"] = newValue;
            newComment["timestamp"] = time.toString("yyyy-MM-dd hh:mm:ss");
            newComment["private"] = "0";
            list << newComment;
        }
    }

    pSqlWriter->insertBugComments(list);
}

void
Trac::bugDetailsRpcResponse(QVariant &arg)
{
    QList<QMap<QString, QString> > insertList;
    QVariantList bugList = arg.toList();
    for (int i = 0; i < bugList.size(); ++i)
    {
        // It's QVariantLists all the way down...
        QVariantList infoList = bugList.at(i).toList().at(0).toList();
        QString bugId = infoList.at(0).toString();
        for (int j = 0; j < infoList.size(); ++j )
        {
            if (infoList.at(j).type() == QVariant::Map)
            {
                QVariantMap bug = infoList.at(j).toMap();
                QMap<QString, QString> newBug;
                newBug["tracker_id"] = mId;
                newBug["bug_id"] = bugId;
                if (!bug.value("severity").isNull())
                    newBug["severity"] = bug.value("severity").toString();
                else
                    newBug["severity"] = bug.value("type").toString();

                newBug["priority"] = bug.value("priority").toString();
                newBug["assigned_to"] = bug.value("owner").toString();
                newBug["status"] = bug.value("status").toString();
                newBug["summary"] = bug.value("summary").toString();
                newBug["component"] = bug.value("component").toString();
                newBug["product"] = "";
                newBug["bug_type"] = mBugMap.value(bugId);
                newBug["last_modified"] = bug.value("changetime")
                                             .toDateTime()
                                             .toString("yyyy-MM-dd hh:mm:ss");
                if (bug.value("status").toString() == "closed")
                    newBug["bug_state"] = "closed";
                else
                    newBug["bug_state"] = "open";
                insertList << newBug;
            }
        }
    }

    pSqlWriter->insertBugs(insertList);
}

void
Trac::statusRpcResponse(QVariant &arg)
{
    QStringList response = arg.toStringList();
    emit statusesFound(response);
}

void
Trac::typeRpcResponse(QVariant &arg)
 {
    mSeverities.append(arg.toStringList());
    emit severitiesFound(mSeverities);
 }

void
Trac::versionRpcResponse(QVariant &arg)
{
    qDebug() << "versionRpcResponse";
    QVariantList list = arg.toList();
    QString version;
    // 0 = Trac 0.10.  1 = Trac 0.11+
    int epoch = list.at(0).toInt();

    // Major should be 1 otherwise the API might be different
    int major = list.at(1).toInt();

    // Minor doesn't matter
    int minor = list.at(2).toInt();
    if ((epoch == 0) || (major != 1))
    {
        version = "-1";
    }
    else
    {
        version = QString("0.%1%2").arg(major).arg(minor);
    }

    emit versionChecked(version);
}

void Trac::versionRpcError(int error,
                           const QString &message)
{
    qDebug()<< "versionRpcError: " << message;
    emit versionChecked("-1");
}

void
Trac::rpcError(int error,
               const QString &message)
{
    qDebug() << "rpcError" << message;
    QString e = QString("Error %1: %2").arg(error).arg(message);
    emit backendError(e);
}

void
Trac::commentInsertionFinished()
{
    emit commentsCached();
}

void
Trac::bugsInsertionFinished(QStringList idList)
{
    updateSync();
    emit bugsUpdated();
}

void
Trac::networkError(QNetworkReply::NetworkError e)
{
    qDebug() << "Network error: " << e;
}

void
Trac::handleSslErrors(QNetworkReply *reply, const QList<QSslError> &errors)
{
    qDebug() << "Ssl error: " << errors;
    reply->ignoreSslErrors();
}
