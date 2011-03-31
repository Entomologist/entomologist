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
    qDebug() << QUrl(mUrl + "/login/xmlrpc");
    pClient->setCookieJar(pCookieJar);
    pCookieJar->setParent(0);

    QSslConfiguration config = pClient->sslConfiguration();
    config.setProtocol(QSsl::AnyProtocol);
    pClient->setSslConfiguration(config);
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
    QString query = QString("cc=%1&max=0&modified=%2..")
                    .arg(mUsername)
                    .arg(mLastSync.toString("yyyy-MM-dd"));
    args << query;
    pClient->call("ticket.query", args, this, SLOT(ccRpcResponse(QVariant&)), this, SLOT(rpcError(int, const QString &)));
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

void
Trac::uploadAll()
{

}

QString
Trac::buildBugUrl(const QString &id)
{
    return QString("%1/ticket/%2").arg(mUrl).arg(id);
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
    QString query = QString("reporter=%1&max=0&modified=%2..")
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
    QString query = QString("owner=%1&max=0&modified=%2..")
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
    QVariantList changelogList = arg.toList();
    for (int i = 0; i < changelogList.size(); ++i)
    {
        //qDebug() << "i: " << i;
        QVariantList changes = changelogList.at(i).toList().at(0).toList();
        for (int j = 0; j < changes.size(); ++j)
        {
            qDebug() << i << ":" << j;
            qDebug() << changes.at(j);
        }
    }
    emit bugsUpdated();
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
    emit versionChecked("-1");
}

void
Trac::rpcError(int error,
               const QString &message)
{
    QString e = QString("Error %1: %2").arg(error).arg(message);
    emit backendError(e);
}

void
Trac::commentInsertionFinished()
{
}

void
Trac::bugsInsertionFinished(QStringList idList)
{
    QVariantList args, methodList;

    for (int i = 0; i < idList.size(); ++i)
    {
        QVariantMap newMethod;
        QVariantList newParams;
        newParams.append(idList.at(i).toInt());
        newMethod.insert("methodName", "ticket.changeLog");
        newMethod.insert("params", newParams);
        methodList.append(newMethod);
    }

    args.insert(0, methodList);;
    pClient->call("system.multicall", args, this, SLOT(changelogRpcResponse(QVariant&)), this, SLOT(rpcError(int, const QString &)));
}

void
Trac::networkError(QNetworkReply::NetworkError e)
{
    qDebug() << "Network error: " << e;
}

void
Trac::handleSslErrors(QNetworkReply *reply, const QList<QSslError> &errors)
{
    reply->ignoreSslErrors();
}
