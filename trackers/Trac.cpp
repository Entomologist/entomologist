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
#include "SqlUtilities.h"
#include "Utilities.hpp"
#include "tracker_uis/TracUI.h"

Trac::Trac(const QString &url,
           const QString &username,
           const QString &password,
           QObject *parent) :
    Backend(url)
{
    mUsername = username;
    mPassword = password;
    QUrl myUrl(url + "/login/xmlrpc");
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
    connect(pSqlWriter, SIGNAL(success()),
            this, SLOT(searchInsertionFinished()));
}

Trac::~Trac()
{
}

BackendUI *
Trac::displayWidget()
{
    if (pDisplayWidget == NULL)
        pDisplayWidget = new TracUI(mId, this);
    return(pDisplayWidget);
}

void
Trac::setUsername(const QString &username)
{
    mUsername = username;
    pClient->setUserName(username);
}

void
Trac::search(const QString &query)
{
    QVariantList args;
    QVariantList filter;
    filter << "ticket";
    args.insert(0, query);
    args.insert(1, filter);
    pClient->call("search.performSearch", args, this, SLOT(searchRpcResponse(QVariant&)), this, SLOT(rpcError(int, const QString &)));
}

void
Trac::setPassword(const QString &password)
{
    mPassword = password;
    pClient->setPassword(password);
}

void
Trac::sync()
{
    qDebug() << "Syncing monitored components...";

    if (mMonitorComponents.empty())
    {
        QStringList none;
        QVariant empty(none);
        qDebug() << "No monitored components for this Trac instance";
        monitoredComponentsRpcResponse(empty);
    }

    SqlUtilities::clearRecentBugs("trac");
    QVariantList args;
    QString closed = "";
    if (mLastSync.date().year() == 1970)
        closed = "status!=closed&";

    QString monitorString;
    for (int i = 0; i < mMonitorComponents.size(); ++i)
    {
        monitorString += QString("component=%1&").arg(mMonitorComponents.at(i));
    }
    QString query = QString("%1%2max=0&modified=%3..")
                    .arg(closed)
                    .arg(monitorString)
                    .arg(mLastSync.toString("yyyy-MM-dd"));
    args << query;
    qDebug() << args;
    pClient->call("ticket.query", args, this, SLOT(monitoredComponentsRpcResponse(QVariant&)), this, SLOT(rpcError(int, const QString &)));
}

void
Trac::getSearchedBug(const QString &bugId)
{
    qDebug() << "getSearchedBug";
    QVariantList args;
    args.append(bugId.toInt());
    pClient->call("ticket.get",
                  args,
                  this,
                  SLOT(searchedTicketResponse(QVariant&)),
                  this,
                  SLOT(rpcError(int,QString)));
}

void
Trac::getComments(const QString &bugId)
{
    QVariantList args;
    args.append(bugId.toInt());
    mActiveCommentId = bugId;
    pClient->call("ticket.changeLog",
                  args,
                  this,
                  SLOT(changelogRpcResponse(QVariant&)),
                  this,
                  SLOT(rpcError(int, const QString &)));
}

void
Trac::login()
{
}

void
Trac::checkVersion()
{
    QUrl url(mUrl + "/login/xmlrpc");
    qDebug() << "Check version for " << url;
    QString concatenated = mUsername + ":" + mPassword;
    QByteArray data = concatenated.toLocal8Bit().toBase64();
    QString headerData = "Basic " + data;

    QNetworkRequest request = QNetworkRequest(url);
    request.setRawHeader("Authorization", headerData.toLocal8Bit());

    QNetworkReply *reply = pManager->head(request);
    connect(reply, SIGNAL(finished()),
            this, SLOT(headFinished()));
}

void
Trac::headFinished()
{
    QNetworkReply *reply = qobject_cast<QNetworkReply*>(sender());
    if (reply->error())
    {
        qDebug() << "headFinished: Not a trac instance: " << reply->errorString();
        reply->deleteLater();
        emit versionChecked("-1");
        return;
    }
    // Mantis seems to report 301 FOUND for any and all URLs.
    // That should be caught when the actual version RPC call is done
    qDebug() << "Found a trac instance?";
    QVariantList args;
    pClient->call("system.getAPIVersion", args, this, SLOT(versionRpcResponse(QVariant&)), this, SLOT(versionRpcError(int, const QString &)));
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
Trac::checkValidComponents()
{
    QVariantList args;
    pClient->call("ticket.component.getAll", args, this, SLOT(componentRpcResponse(QVariant&)), this, SLOT(rpcError(int, const QString &)));
}

void
Trac::checkValidVersions()
{
    QVariantList args;
    pClient->call("ticket.version.getAll", args, this, SLOT(versionsRpcResponse(QVariant&)), this, SLOT(rpcError(int, const QString &)));
}

void
Trac::checkValidResolutions()
{
    QVariantList args;
    pClient->call("ticket.resolution.getAll", args, this, SLOT(resolutionsRpcResponse(QVariant&)), this, SLOT(rpcError(int, const QString &)));
}

void
Trac::checkValidMilestones()
{
    QVariantList args;
    pClient->call("ticket.milestone.getAll", args, this, SLOT(milestonesRpcResponse(QVariant&)), this, SLOT(rpcError(int, const QString &)));

}

void
Trac::checkFields()
{
    QVariantList args;
    pClient->call("ticket.priority.getAll", args, this, SLOT(priorityRpcResponse(QVariant&)), this, SLOT(rpcError(int, const QString &)));
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
        else if (!q.value(4).isNull())
        {
            actionMap["status"] = q.value(4).toString().remove(QRegExp("<[^>]*>"));
            if (actionMap["status"] == "accepted")
            {
                actionMap["action"] = "accept";
            }
            else if (actionMap["status"] == "assigned")
            {
                actionMap.remove("action");
                actionMap["owner"] = mUsername;
            }
        }

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
Trac::searchedTicketResponse(QVariant &arg)
{
   QVariantList bugList = arg.toList();
   QList< QMap<QString, QString> > insertList;
   QString bugId = bugList.at(0).toString();
   QVariantMap bug = bugList.at(3).toMap();
   QMap<QString, QString> newBug;

    newBug["tracker_id"] = mId;
    newBug["bug_id"] = bugId;
    if (!bug.value("severity").isNull())
        newBug["severity"] = bug.value("severity").toString();
    else
        newBug["severity"] = bug.value("type").toString();

    newBug["priority"] = bug.value("priority").toString();
    newBug["version"] = bug.value("version").toString();
    newBug["milestone"] = bug.value("milestone").toString();
    newBug["assigned_to"] = bug.value("owner").toString();
    newBug["status"] = bug.value("status").toString();
    newBug["summary"] = bug.value("summary").toString();
    newBug["description"] = bug.value("description").toString();
    newBug["resolution"] = bug.value("resolution").toString();
    newBug["component"] = bug.value("component").toString();
    newBug["bug_type"] = "Searched";
    newBug["highlight_type"] = QString::number(SqlUtilities::HIGHLIGHT_SEARCH);
    newBug["last_modified"] = bug.value("changetime")
                                 .toDateTime()
                                 .toString("yyyy-MM-dd hh:mm:ss");
    if (bug.value("status").toString() == "closed")
        newBug["bug_state"] = "closed";
    else
        newBug["bug_state"] = "open";
    insertList << newBug;
    pSqlWriter->insertBugs("trac", insertList);
    emit searchResultFinished(newBug);
}

void
Trac::monitoredComponentsRpcResponse(QVariant &arg)
{
    QStringList bugs = arg.toStringList();
    for (int i = 0; i < bugs.size(); ++i)
    {
        mBugMap.insert(bugs.at(i), "Monitored");
    }

    qDebug() << "Syncing CCs";
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
Trac::ccRpcResponse(QVariant &arg)
{
    qDebug() << "ccResponse...";
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
    qDebug() << "Reporter response";
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
    qDebug() << "owner response";
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
Trac::searchRpcResponse(QVariant &arg)
{
    QList< QMap<QString,QString> > insertList;
    QVariantList resultList = arg.toList();
    for (int i = 0; i < resultList.size(); ++i)
    {
        QVariantList result = resultList.at(i).toList();
        QMap<QString, QString> bug;
        QStringList split = result.at(0).toString().split('/');
        qDebug() << "SPLIT: " << result.at(0).toString();
        bug["tracker_name"] = mName;
        bug["bug_id"] = split.last();
        bug["summary"] = result.at(1).toString();
        insertList << bug;
    }
    qDebug() << "multiInsert " << insertList;
    pSqlWriter->multiInsert("search_results", insertList);
}

void
Trac::searchInsertionFinished()
{
   qDebug() << "searchInsertionFinished";
   emit searchFinished();
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
                newBug["version"] = bug.value("version").toString();
                newBug["milestone"] = bug.value("milestone").toString();
                newBug["assigned_to"] = bug.value("owner").toString();
                newBug["status"] = bug.value("status").toString();
                newBug["summary"] = bug.value("summary").toString();
                newBug["description"] = bug.value("description").toString();
                newBug["resolution"] = bug.value("resolution").toString();
                newBug["component"] = bug.value("component").toString();
                newBug["bug_type"] = mBugMap.value(bugId);
                if (mLastSync.date().year() != 1970)
                    newBug["highlight_type"] = QString::number(SqlUtilities::HIGHLIGHT_RECENT);

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

    pSqlWriter->insertBugs("trac", insertList);
}

void
Trac::priorityRpcResponse(QVariant &arg)
{
    QList<QMap<QString, QString> > fieldList;
    QStringList response = arg.toStringList();
    for (int i = 0; i < response.size(); ++i)
    {
        QMap<QString, QString> fieldMap;
        fieldMap["tracker_id"] = mId;
        fieldMap["field_name"] = "priority";
        fieldMap["value"] = response.at(i);
        fieldList << fieldMap;
    }

    pSqlWriter->multiInsert("fields", fieldList);
    checkValidSeverities();
}

void
Trac::statusRpcResponse(QVariant &arg)
{
    QList<QMap<QString, QString> > fieldList;
    QStringList response = arg.toStringList();
    for (int i = 0; i < response.size(); ++i)
    {
        QMap<QString, QString> fieldMap;
        fieldMap["tracker_id"] = mId;
        fieldMap["field_name"] = "status";
        fieldMap["value"] = response.at(i);
        fieldList << fieldMap;
    }

    pSqlWriter->multiInsert("fields", fieldList);
    checkValidMilestones();
}

void
Trac::milestonesRpcResponse(QVariant &arg)
{
    QList<QMap<QString, QString> > fieldList;
    QStringList response = arg.toStringList();
    for (int i = 0; i < response.size(); ++i)
    {
        QMap<QString, QString> fieldMap;
        fieldMap["tracker_id"] = mId;
        fieldMap["field_name"] = "milestone";
        fieldMap["value"] = response.at(i);
        fieldList << fieldMap;
    }

    pSqlWriter->multiInsert("fields", fieldList);
    checkValidResolutions();
}

void
Trac::resolutionsRpcResponse(QVariant &arg)
{
    QList<QMap<QString, QString> > fieldList;
    QStringList response = arg.toStringList();
    for (int i = 0; i < response.size(); ++i)
    {
        QMap<QString, QString> fieldMap;
        fieldMap["tracker_id"] = mId;
        fieldMap["field_name"] = "resolution";
        fieldMap["value"] = response.at(i);
        fieldList << fieldMap;
    }

    pSqlWriter->multiInsert("fields", fieldList);
    checkValidVersions();
}

void
Trac::versionsRpcResponse(QVariant &arg)
{
    QList<QMap<QString, QString> > fieldList;
    QStringList response = arg.toStringList();
    for (int i = 0; i < response.size(); ++i)
    {
        QMap<QString, QString> fieldMap;
        fieldMap["tracker_id"] = mId;
        fieldMap["field_name"] = "version";
        fieldMap["value"] = response.at(i);
        fieldList << fieldMap;
    }

    pSqlWriter->multiInsert("fields", fieldList);

    checkValidComponents();
}

void
Trac::componentRpcResponse(QVariant &arg)
{
    QList<QMap<QString, QString> > fieldList;
    QStringList response = arg.toStringList();
    for (int i = 0; i < response.size(); ++i)
    {
        QMap<QString, QString> fieldMap;
        fieldMap["tracker_id"] = mId;
        fieldMap["field_name"] = "component";
        fieldMap["value"] = response.at(i);
        fieldList << fieldMap;
    }

    pSqlWriter->multiInsert("fields", fieldList);
    if (pDisplayWidget != NULL)
        pDisplayWidget->loadFields();
    emit fieldsFound();
}

void
Trac::severityRpcResponse(QVariant &arg)
{
    // Trac used to just use "severities", but then they renamed it to "type"
    // but users can still use "severities" if they'd like.  We'll check for both and
    // merge them together
    mSeverities = arg.toStringList();
    QList<QMap<QString, QString> > fieldList;
    QStringList response = arg.toStringList();
    for (int i = 0; i < mSeverities.size(); ++i)
    {
        QMap<QString, QString> fieldMap;
        fieldMap["tracker_id"] = mId;
        fieldMap["field_name"] = "severity";
        fieldMap["value"] = response.at(i);
        fieldList << fieldMap;
    }

    pSqlWriter->multiInsert("fields", fieldList);

    QVariantList args;
    pClient->call("ticket.type.getAll", args, this, SLOT(typeRpcResponse(QVariant&)), this, SLOT(rpcError(int, const QString &)));
}

void
Trac::typeRpcResponse(QVariant &arg)
 {
    QList<QMap<QString, QString> > fieldList;
    QStringList response = arg.toStringList();
    for (int i = 0; i < response.size(); ++i)
    {
        QMap<QString, QString> fieldMap;
        fieldMap["tracker_id"] = mId;
        fieldMap["field_name"] = "severity";
        fieldMap["value"] = response.at(i);
        fieldList << fieldMap;
    }

    pSqlWriter->multiInsert("fields", fieldList);
    checkValidStatuses();
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
    qDebug() << list;
    if ((epoch == 0) || (major != 1))
    {
        emit backendError("The version of Trac is too low.  Trac 0.12 or higher is required.");
        return;
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
    qDebug()<< "Trac versionRpcError: " << message;
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
    qDebug() << "Bug insertion is finished";
    updateSync();
    emit bugsUpdated();
}

void
Trac::handleSslErrors(QNetworkReply *reply, const QList<QSslError> &errors)
{
    qDebug() << "Ssl error: " << errors;
    reply->ignoreSslErrors();
}
