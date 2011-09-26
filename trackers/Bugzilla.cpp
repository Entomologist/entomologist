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

#include <QString>
#include <QStringList>
#include <QtSql>
#include <QDebug>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QNetworkCookie>
#include <QXmlStreamReader>

#include "Bugzilla.h"
#include "SqlUtilities.h"
#include "tracker_uis/BugzillaUI.h"

// Bugzilla is not fun to work with.  This uses a mix of XMLRPC and POST calls:
// - XMLRPC to login
// - XMLRPC to get the bug list for 3.4+, HTTP for 3.2
// - In order to update bug statuses for bugzilla < 4.0, we first have to request bug information
//   in XML format, find the token in it, and then we can go ahead and POST the modified fields.
// - CC lists can only be found, in any version, by calling buglist.cgi with a ctype of 'csv'
// - Comments are added for all versions via xmlrpc
// - TODO: Consider the first comment to be the description

Bugzilla::Bugzilla(const QString &url)
    : Backend(url)
{
    pClient = new MaiaXmlRpcClient(QUrl(mUrl + "/xmlrpc.cgi"), "Entomologist/0.1");
    // To keep the CSV output easy to parse, we specify the specific columns we're interested in
    QNetworkCookie columnCookie("COLUMNLIST", "changeddate%20bug_severity%20priority%20assigned_to%20bug_status%20product%20component%20short_short_desc");
    QList<QNetworkCookie> list;
    list << columnCookie;
    pCookieJar->setCookiesFromUrl(list, QUrl(url));
    pClient->setCookieJar(pCookieJar);
    //pCookieJar->setParent(0);

    QSslConfiguration config = pClient->sslConfiguration();
    config.setProtocol(QSsl::AnyProtocol);
    config.setPeerVerifyMode(QSslSocket::VerifyNone);
    pClient->setSslConfiguration(config);

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
    mLoggedIn = false;
}

Bugzilla::~Bugzilla()
{
}

BackendUI *
Bugzilla::displayWidget()
{
    if (pDisplayWidget == NULL)
        pDisplayWidget = new BugzillaUI(mId, this);
    return(pDisplayWidget);
}

void
Bugzilla::login()
{
    qDebug() << "Logging into " << mUrl + "/xmlrpc.cgi";
    QVariantList args;
    QVariantMap params;
    params["login"] = QVariant(mUsername);
    params["password"] = QVariant(mPassword);
    args << params;
    pClient->call("User.login", args, this, SLOT(loginRpcResponse(QVariant&)), this, SLOT(rpcError(int, const QString &)));
}

void
Bugzilla::sync()
{
    mUpdateCount = 0;
    mUploading = false;
    qDebug() << "Bugzilla::sync for " << name() << " at " << mLastSync;
    qDebug() << "Logging into " << mUrl + "/xmlrpc.cgi";
    QVariantList args;
    QVariantMap params;
    params["login"] = QVariant(mUsername);
    params["password"] = QVariant(mPassword);
    args << params;
    pClient->call("User.login", args, this, SLOT(loginSyncRpcResponse(QVariant&)), this, SLOT(rpcError(int, const QString &)));

}

void
Bugzilla::getSearchedBug(const QString &bugId)
{
    QString url = mUrl + QString("/show_bug.cgi?id=%1&ctype=xml")
                                .arg(bugId);
    qDebug() << "Fetching " << bugId;
    QNetworkRequest req = QNetworkRequest(QUrl(url));
    QNetworkReply *rep = pManager->get(req);
    connect(rep, SIGNAL(finished()),
            this, SLOT(individualBugFinished()));
}

void
Bugzilla::checkVersion()
{
    qDebug() << "Checking version";
    QVariantList args;
    pClient->call("Bugzilla.version", args, this, SLOT(versionRpcResponse(QVariant&)), this, SLOT(versionError(int, const QString &)));
}

void
Bugzilla::search(const QString &query)
{
    QString url = mUrl + QString("/buglist.cgi?query_format=advanced"
                                 "&bug_status=NEW&bug_status=ASSIGNED"
                                 "&bug_status=REOPENED&bug_status=NEEDINFO&bug_status=UNCONFIRMED"
                                 "&field0-0-0=short_desc"
                                 "&type0-0-0=substring"
                                 "&field0-0-1=longdesc"
                                 "&type0-0-1=substring"
                                 "&query_format=advanced"
                                 "&value0-0-0=%1&value0-0-1=%1"
                                 "&ctype=csv")
                                .arg(query);
    qDebug() << "Searching " << url;
    QNetworkRequest req = QNetworkRequest(QUrl(url));
    QNetworkReply *rep = pManager->get(req);
    connect(rep, SIGNAL(finished()),
            this, SLOT(searchCallFinished()));
}

// Upload all of the bug changes
void
Bugzilla::uploadAll()
{
    qDebug() << "Upload all for " << name() << mLastSync;
    if (!hasPendingChanges())
    {
        emit bugsUpdated();
        return;
    }
    mUploading = true;
    login();
}

void Bugzilla::doUploading()
{
    qDebug() << "doUploading";
    // First, iterate through the shadow_bugs table
    QString ids;
    QSqlTableModel model;
    model.setTable("shadow_bugs");
    model.setFilter(QString("tracker_id=%1").arg(mId));
    model.select();

    if (model.rowCount() == 0)
    {
        postComments();
        return;
    }

    for (int i = 0; i < model.rowCount(); ++i)
    {
        QSqlRecord record = model.record(i);
        ids += "id=" + record.value(2).toString() + "&";
    }

    // We're assuming that the login cookie is still valid here
    // TODO: It might not be
    QString url = mUrl + "/show_bug.cgi?" + ids + "ctype=xml";
    qDebug() << "Fetching " << url;
    QNetworkRequest req = QNetworkRequest(QUrl(url));
    QNetworkReply *rep = pManager->get(req);
    connect(rep, SIGNAL(finished()),
            this, SLOT(idDetailsFinished()));

}

// 3.2 doesn't support the User.get call, so we'll have to just assume
// that their login is the same as their email address for bug search
// purposes.
//
// TODO: Is there a better way to do it?
void
Bugzilla::getUserEmail()
{
    if (mVersion == "3.2")
    {
        mEmail = mUsername;
        getCCs();
    }
    else
    {
        QVariantList args;
        QVariantList array;
        QVariantMap params;
        if (mBugzillaId.isEmpty())
        {
            array << mUsername;
            params["names"] = array;
        }
        else
        {
            array << mBugzillaId.toInt();
            params["ids"] = array;
        }
        args << params;
        pClient->call("User.get", args, this, SLOT(emailRpcResponse(QVariant&)), this, SLOT(rpcError(int,QString)));
    }
}

void
Bugzilla::getUserBugs()
{
    if (mVersion == "3.2")
    {
        QString closed = "";
        if (mLastSync.date().year() != 1970)
            closed = "&bug_status=CLOSED&bug_status=RESOLVED";

        QString url = mUrl + QString("/buglist.cgi?query_format=advanced"
                                     "&bug_status=NEW&bug_status=ASSIGNED"
                                     "%1"
                                     "&bug_status=REOPENED&bug_status=NEEDINFO&bug_status=UNCONFIRMED"
                                     "&chfieldfrom=%2"
                                     "&emailassigned_to1=1"
                                     "&emailtype1=substring&email1=%3&ctype=csv")
                                    .arg(closed)
                                    .arg(mLastSync.toString("yyyy-MM-dd"))
                                    .arg(mEmail);
        QNetworkRequest req = QNetworkRequest(QUrl(url));
        QNetworkReply *rep = pManager->get(req);
        connect(rep, SIGNAL(finished()),
                this, SLOT(userBugListFinished()));
    }
    else
    {
        QVariantList args, usernameArgs;
        QVariantMap params;
        usernameArgs << mUsername << mEmail;
        params["assigned_to"] = usernameArgs;
        if (mLastSync.date().year() == 1970)
            params["resolution"] = ""; // Only show open bugs
        params["last_change_time"] = mLastSync.addDays(-1).toString("yyyy-MM-ddThh:mm:ss");
        args << params;
        qDebug() << params;
        pClient->call("Bug.search", args, this, SLOT(bugRpcResponse(QVariant&)), this, SLOT(rpcError(int,QString)));
    }
}

void
Bugzilla::getReportedBugs()
{
    if (mVersion == "3.2")
    {
        QString closed = "";
        if (mLastSync.date().year() != 1970)
            closed = "&bug_status=CLOSED&bug_status=RESOLVED";

        QString url = mUrl + QString("/buglist.cgi?query_format=advanced"
                                     "&bug_status=NEW&bug_status=ASSIGNED"
                                     "%1"
                                     "&bug_status=REOPENED&bug_status=NEEDINFO&bug_status=UNCONFIRMED"
                                     "&chfieldfrom=%2"
                                     "&emailreporter1=1"
                                     "&emailtype1=substring&email1=%3&ctype=csv")
                                    .arg(closed)
                                    .arg(mLastSync.toString("yyyy-MM-dd"))
                                    .arg(mEmail);
        QNetworkRequest req = QNetworkRequest(QUrl(url));
        QNetworkReply *rep = pManager->get(req);
        connect(rep, SIGNAL(finished()),
                this, SLOT(reportedBugListFinished()));
    }
    else
    {
        QVariantList args, usernameArgs;
        QVariantMap params;
        usernameArgs << mUsername << mEmail;
        if ((mVersion == "3.4") || (mVersion == "3.6"))
            params["reporter"] = usernameArgs;
        else
            params["creator"] = usernameArgs;
        if (mLastSync.date().year() == 1970)
            params["resolution"] = ""; // Only show open bugs
        params["last_change_time"] = mLastSync.addDays(-1).toString("yyyy-MM-ddThh:mm:ss");
        args << params;
        pClient->call("Bug.search", args, this, SLOT(reportedRpcResponse(QVariant&)), this, SLOT(rpcError(int,QString)));
    }
}

void
Bugzilla::getCCs()
{
    QString closed = "";
    if (mLastSync.date().year() != 1970)
        closed = "&bug_status=CLOSED&bug_status=RESOLVED";

    QString url = mUrl + QString("/buglist.cgi?emailcc1=1&emailtype1=substring"
                                 "%1"
                                 "&query_format=advanced&bug_status=NEW&bug_status=ASSIGNED&bug_status=NEEDINFO&bug_status=REOPENED&bug_status=UNCONFIRMED"
                                 "&chfieldfrom=%2"
                                 "&email1=%3&ctype=csv")
                          .arg(closed)
                         .arg(mLastSync.toString("yyyy-MM-dd"))
                         .arg(mEmail);
    qDebug() << url;
    QNetworkRequest req = QNetworkRequest(QUrl(url));
    QNetworkReply *rep = pManager->get(req);
    connect(rep, SIGNAL(finished()),
            this, SLOT(ccFinished()));
}

void
Bugzilla::postNewItems(QMap<QString, QString> tokenMap)
{
    QMap<QString, QString> vals;
    QMapIterator<QString, QString> i(tokenMap);
    while (i.hasNext())
    {
        i.next();
        vals = getShadowValues(i.key());
        vals["id"] = i.key();
        vals["token"] = i.value();
        mPostQueue << vals;
    }

    postItem();
}

// This function posts the first item in the list.  When the
// itemPostFinished slot is done with the response, it removes
// the first item from the list, and calls postItem() again
// to handle the next.
void
Bugzilla::postItem()
{
    if (mPostQueue.size() == 0)
    {
        postComments();
        return;
    }

    QMap<QString, QString> map = mPostQueue.at(0);
    QString url = mUrl + "/process_bug.cgi";
    QString query = QString("id=%1&token=%2&").arg(map["id"]).arg(map["token"]);
    QString key, val;
    QMapIterator<QString, QString> i(map);
    while (i.hasNext())
    {
        i.next();
        key = i.key();
        val = i.value();
        if (key != "id" && key != "token")
        {
            val.remove(QRegExp("<[^>]*>"));
            query += QString("%1=%2&").arg(key).arg(val);
        }
    }

    QNetworkRequest req = QNetworkRequest(QUrl(url));
    req.setAttribute(QNetworkRequest::User, map["id"]);
    qDebug() << "Posting " << query;
    qDebug() << "To " << url;
    QNetworkReply *rep = pManager->post(req, query.toAscii());
    connect(rep, SIGNAL(finished()),
            this, SLOT(itemPostFinished()));
}

QMap<QString, QString>
Bugzilla::getShadowValues(const QString &id)
{
    QMap<QString, QString> ret;
    QString sql = QString("SELECT severity, priority, assigned_to, status, summary FROM shadow_bugs where bug_id=%1 AND tracker_id=%2")
                       .arg(id)
                       .arg(mId);
    QSqlQuery q(sql);
    q.next();
    if (!q.value(0).isNull())
        ret["bug_severity"] = q.value(0).toString();
    if (!q.value(1).isNull())
        ret["priority"] = q.value(1).toString();
    if (!q.value(2).isNull())
        ret["assignee"] = q.value(2).toString();
    if (!q.value(3).isNull())
        ret["bug_status"] = q.value(3).toString();
    if (!q.value(4).isNull())
        ret["short_desc"] = q.value(4).toString();
    return ret;
}

void
Bugzilla::postComments()
{
    // Read the shadow_comments table, build up a queue
    mCommentQueue.clear();
    QSqlTableModel model;
    model.setTable("shadow_comments");
    model.setFilter(QString("tracker_id=%1").arg(mId));
    model.select();
    if (model.rowCount() == 0)
    {
        mUploading = false;
        qDebug() << "Calling sync from postComments.";
        sync();
        return;
    }
    for (int i = 0; i < model.rowCount(); ++i)
    {
        QSqlRecord record = model.record(i);
        QMap<QString, QString> commentMap;
        commentMap["comment_id"] = record.value(0).toString();
        commentMap["id"] = record.value(2).toString();
        commentMap["comment"] = record.value(5).toString();
        commentMap["is_private"] = record.value(7).toBool();
        mCommentQueue << commentMap;
    }
    postComment();
}

void Bugzilla::postComment()
{
    if (mCommentQueue.size() == 0)
    {
        // We're done posting the changes, so resync with the remote server
        mUploading = false;
        qDebug() << "Calling sync from postcomment";
        sync();
        return;
    }

    QMap<QString, QString> map = mCommentQueue.at(0);
    QVariantList args;
    QVariantMap request;
    mActiveCommentId = map["comment_id"];
    request["id"] = map["id"];
    request["comment"] = map["comment"];
    request["is_private"] = map["is_private"];
    args << request;
    pClient->call("Bug.add_comment", args, this, SLOT(addCommentResponse(QVariant&)), this, SLOT(rpcError(int,QString)));
}

void Bugzilla::addCommentResponse(QVariant &arg)
{
    Q_UNUSED(arg);
    QString sql = "DELETE FROM shadow_comments WHERE id=" + mActiveCommentId;
    QSqlQuery q(sql);
    if (!q.exec())
        qDebug () << "Could not delete comment id " << mActiveCommentId;
    mCommentQueue.takeAt(0);
    postComment();
}

void
Bugzilla::getComments(const QString &bugId)
{
    if (mVersion == "3.2")
    {
        qDebug() << "Get comments for 3.2, " << bugId;

        QString url = mUrl + "/show_bug.cgi?id=" + bugId+ "ctype=xml";

        QNetworkRequest req = QNetworkRequest(QUrl(url));
        QNetworkReply *rep = pManager->get(req);
        connect(rep, SIGNAL(finished()),
                this, SLOT(commentXMLFinished()));
    }
    else
    {
        qDebug() << "Get comments";
        QVariantList args;
        QStringList ids;
        ids << bugId;
        QVariantMap params;
        QVariant v(ids);
        params["ids"] = v.toList();
        qDebug() << "Finding comments for " << params;
        args << params;
        pClient->call("Bug.comments", args, this, SLOT(commentRpcResponse(QVariant&)), this, SLOT(rpcError(int,QString)));
    }
}

void
Bugzilla::checkFields()
{
    checkValidPriorities();
}

// The next three calls are valid for Bugzilla 3.0, 3.2, 3.4
// and deprecated in 3.6 & 4.0
// TODO: refactor this into one call
void
Bugzilla::checkValidPriorities()
{
    QVariantList args;
    QVariantMap params;
    params["field"] = "priority";
    args << params;
    pClient->call("Bug.legal_values", args, this, SLOT(priorityResponse(QVariant&)), this, SLOT(rpcError(int,QString)));
}

void
Bugzilla::checkValidSeverities()
{
    QVariantList args;
    QVariantMap params;
    params["field"] = "severity";
    args << params;
    pClient->call("Bug.legal_values", args, this, SLOT(severityResponse(QVariant&)), this, SLOT(rpcError(int,QString)));
}

void
Bugzilla::checkValidStatuses()
{
    QVariantList args;
    QVariantMap params;
    params["field"] = "status";
    args << params;
    pClient->call("Bug.legal_values", args, this, SLOT(statusResponse(QVariant&)), this, SLOT(rpcError(int,QString)));
}

void
Bugzilla::checkValidComponents()
{
    // For Bugzilla 3.2 and 3.4, we'll need to
    // parse the output of query.cgi?format=advanced,
    // looking for prods[] and cpts[] arrays.
    // For 3.6+ we can use the fields call with the
    // "names: component" parameter.
    if ((mVersion == "3.2") || (mVersion == "3.4"))
    {
        QVariantList args;
        pClient->call("Product.get_selectable_products", args, this, SLOT(productsResponse(QVariant&)), this, SLOT(rpcError(int,QString)));
    }
    else
    {
        QVariantList args;
        QVariantList statusArgs;
        statusArgs << "component";
        QVariantMap params;
        params["names"] = statusArgs;
        args << params;
        pClient->call("Bug.fields", args, this, SLOT(componentsResponse(QVariant&)), this, SLOT(rpcError(int,QString)));
    }
}

void
Bugzilla::checkValidComponentsForProducts(const QString &product)
{
    int id = mProductMap.value(product, -1).toInt();
    if (id)
    {
        QVariantList args;
        mCurrentProduct = product;
        QVariantMap params;
        params["field"] = "component";
        params["product_id"] = id;
        args << params;
        pClient->call("Bug.legal_values", args, this, SLOT(productComponentResponse(QVariant&)), this, SLOT(rpcError(int,QString)));
    }
}

////////////////////////////////////////
// XMLRPC result handlers
///////////////////////////////////////

// The catch-all error handler for XMLRPC calls
void
Bugzilla::rpcError(int error, const QString &message)
{
    QString e = QString("Error %1: %2").arg(error).arg(message);
    emit backendError(e);
}

void
Bugzilla::versionError(int error, const QString &message)
{
    Q_UNUSED(error);
    Q_UNUSED(message);
    emit versionChecked("-1");
}

void
Bugzilla::versionRpcResponse(QVariant &arg)
{
    QString version = arg.toMap().value("version").toString();
    qDebug() << "Bugzilla version response: " << version;
    if (version.count('.') > 1)
        version = version.remove(version.lastIndexOf('.'), version.length() + 1);
    if (version.toFloat() < 3.2)
    {
        qDebug() << "Version is too low: " << version.toFloat();
        version = "-1";
    }
    qDebug() << "Emitting versionChecked";
    mVersion = version;
    emit versionChecked(version);
}

void Bugzilla::loginRpcResponse(QVariant &arg)
{
    QVariantMap map = arg.toMap();
    if (!map.isEmpty())
    {
        mBugzillaId = map.value("id").toString();
    }
}

void Bugzilla::loginSyncRpcResponse(QVariant &arg)
{
    qDebug() << "RPC response LOGIN";
    QVariantMap map = arg.toMap();
    if (!map.isEmpty())
    {
        mBugzillaId = map.value("id").toString();
    }

    if (mUploading)
        doUploading();
    else
        getUserEmail();
}

void Bugzilla::emailRpcResponse(QVariant &arg)
{
    qDebug() << "RPC response USER_EMAIL: " << arg;
    QVariantMap userMap = arg.toMap();
    if (userMap.isEmpty())
    {
        qDebug() << "usermap is empty!";
        mEmail = mUsername;
        getCCs();
        return;
    }

    QVariantList userList = userMap.value("users").toList();
    if (userList.isEmpty())
    {
        mEmail = mUsername;
        getCCs();
        return;
    }

    mEmail = userList.at(0).toMap().value("email").toString();
    getCCs();
}

void Bugzilla::reportedRpcResponse(QVariant &arg)
{
    qDebug() << "REPORTED_BUGS";
    QVariantList bugList = arg.toMap().value("bugs").toList();
    for (int i = 0; i < bugList.size(); ++i)
    {
        QVariantMap responseMap = bugList.at(i).toMap();
        responseMap["bug_type"] = "Reported";
        mBugs[responseMap.value("id").toString()] = responseMap;
    }
    getUserBugs();
}

void Bugzilla::bugRpcResponse(QVariant &arg)
{
    qDebug() << "USER_BUGS";
    QVariantList bugList = arg.toMap().value("bugs").toList();
    QVariantList idList;
    QVariantMap responseMap;
    mUpdateCount = bugList.size();
    for (int i = 0; i < bugList.size(); ++i)
    {
        responseMap = bugList.at(i).toMap();
        responseMap["bug_type"] = "Assigned";
        mBugs[responseMap.value("id").toString()] = responseMap;
    }

    QMapIterator<QString, QVariant> i(mBugs);
    QList< QMap<QString,QString> > insertList;

    while (i.hasNext())
    {
        i.next();
        responseMap = i.value().toMap();
        QMap<QString, QString> newBug;
        newBug["tracker_id"] = mId;
        newBug["bug_id"] = responseMap.value("id").toString();
        newBug["severity"] = responseMap.value("severity").toString();
        newBug["priority"] = responseMap.value("priority").toString();
        newBug["assigned_to"] = responseMap.value("assigned_to").toString();
        newBug["status"] = responseMap.value("status").toString();
        newBug["summary"] = responseMap.value("summary").toString();
        newBug["component"] = responseMap.value("component").toString();
        newBug["product"] = responseMap.value("product").toString();
        newBug["bug_type"] = responseMap.value("bug_type").toString();
        newBug["description"] = responseMap.value("description").toString();
        if (responseMap.value("resolution").toString() != "")
            newBug["bug_state"] = "closed";
        else
            newBug["bug_state"] = "open";
        // Bugs from RPC come in in ISO format (YYYY-MM-DDTHH:MM:SS) so convert
        // to an easier to read format
        newBug["last_modified"] = friendlyTime(responseMap.value("last_change_time").toString());
        insertList << newBug;
    }

    pSqlWriter->insertBugs("bugzilla", insertList);
}

void Bugzilla::commentRpcResponse(QVariant &arg)
{
    qDebug() << "USER_COMMENTS";
    QVariantMap commentHash = arg.toMap().value("bugs").toMap();
    QVariantList commentList;
    QVariantMap comment;
    QList<QMap<QString, QString> > commentInsertionList;

    QMapIterator<QString, QVariant> j(commentHash);
    while (j.hasNext())
    {
        j.next();
        commentList = j.value().toMap().value("comments").toList();
        QListIterator<QVariant> c(commentList);
        // The first comment is the description
        if (c.hasNext())
        {
            comment = c.next().toMap();

            QMap<QString, QString> val;
            QMap<QString, QString> params;
            val["description"] = comment.value("text").toString();
            params["tracker_id"] = mId;
            params["bug_id"] = j.key();
            SqlUtilities::simpleUpdate("bugzilla", val, params);
        }

        while (c.hasNext())
        {
            comment = c.next().toMap();
            QMap<QString, QString> newComment;
            newComment["tracker_id"] = mId;
            newComment["bug_id"] = j.key();
            newComment["comment_id"] = comment.value("id").toString();
            newComment["author"] = comment.value("author").toString();
            newComment["comment"] = comment.value("text").toString();
            newComment["timestamp"] = comment.value("time").toDateTime().toString("yyyy-MM-ddThh:mm:ss");
            newComment["private"] = comment.value("is_private").toInt();
            commentInsertionList << newComment;
        }
    }

    pSqlWriter->insertBugComments(commentInsertionList);
}

void
Bugzilla::priorityResponse(QVariant &arg)
{
    QStringList response;
    response = arg.toMap().value("values").toStringList();
    QList<QMap<QString, QString> > fieldList;
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
Bugzilla::statusResponse(QVariant &arg)
{
    QStringList response;
    response = arg.toMap().value("values").toStringList();
    QList<QMap<QString, QString> > fieldList;
    for (int i = 0; i < response.size(); ++i)
    {
        QMap<QString, QString> fieldMap;
        fieldMap["tracker_id"] = mId;
        fieldMap["field_name"] = "status";
        fieldMap["value"] = response.at(i);
        fieldList << fieldMap;
    }

    pSqlWriter->multiInsert("fields", fieldList);
    checkValidComponents();
}

void
Bugzilla::severityResponse(QVariant &arg)
{
    QList<QMap<QString, QString> > fieldList;
    QStringList response = arg.toMap().value("values").toStringList();
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
Bugzilla::componentsResponse(QVariant &arg)
{
    QString component;
    QStringList response;

    qDebug() << "componentsResponse: ";
    QVariantList v = arg.toMap().value("fields").toList();
    QVariantList values = v.at(0).toMap().value("values").toList();
    for (int i = 0; i < values.size(); ++i)
    {
        QVariantMap val = values.at(i).toMap();
        component = val.value("name").toString();
        QVariantList products = val.value("visibility_values").toList();
        for (int j = 0; j < products.size(); ++j)
        {
            response << QString("%1:%2").arg(products.at(j).toString()).arg(component);
        }
    }

    QList<QMap<QString, QString> > fieldList;
    for (int i = 0; i < response.size(); ++i)
    {
        QMap<QString, QString> fieldMap;
        fieldMap["tracker_id"] = mId;
        fieldMap["field_name"] = "component";
        fieldMap["value"] = response.at(i);
        fieldList << fieldMap;
    }

    pSqlWriter->multiInsert("fields", fieldList);
    emit fieldsFound();
}

void
Bugzilla::productsResponse(QVariant &arg)
{
    QVariantList idList = arg.toMap().value("ids").toList();
    QVariantList args;
    QVariantMap params;
    params["ids"] = idList;
    args << params;
    pClient->call("Product.get", args, this, SLOT(productNamesResponse(QVariant&)), this, SLOT(rpcError(int,QString)));
}

void
Bugzilla::productNamesResponse(QVariant &arg)
{
    QStringList response;
    QVariantList productList = arg.toMap().value("products").toList();
    for (int i = 0; i < productList.size(); ++i)
    {
        QVariantMap m = productList.at(i).toMap();
        QString name = m.value("name").toString();
        mProductMap[name] = m.value("id").toString();
        response << QString("%1:No data cached").arg(name);
    }
    emit componentsFound(response);
    emit fieldsFound();
}

void
Bugzilla::productComponentResponse(QVariant &arg)
{
    QVariantList vals = arg.toMap().value("values").toList();
    QStringList response;
    for (int i = 0; i < vals.size(); ++i)
    {
        response << QString("%1:%2").arg(mCurrentProduct).arg(vals.at(i).toString());
    }
    emit componentsFound(response);
}

///////////////////////////////////////////
// Bugzilla 3.2 CSV call handlers
///////////////////////////////////////////
void
Bugzilla::reportedBugListFinished()
{
    QNetworkReply *reply = qobject_cast<QNetworkReply*>(sender());
    if (reply->error())
    {
        emit backendError(reply->errorString());
        reply->close();
        return;
    }
    qDebug() << "reportedBugListFinished";
    QString csv = reply->readAll();
    parseBuglistCSV(csv, "Reported");
    reply->close();
    getUserBugs();
}

void
Bugzilla::searchCallFinished()
{
    qDebug() << "searchFinished";
    QNetworkReply *reply = qobject_cast<QNetworkReply*>(sender());
    if (reply->error())
    {
        emit backendError(reply->errorString());
        reply->close();
        return;
    }

    QString csv = reply->readAll();
    parseBuglistCSV(csv, "Search");
    reply->close();

    QList< QMap<QString,QString> > insertList;
    QVariantMap responseMap;
    QMapIterator<QString, QVariant> i(mBugs);
    mUpdateCount = mBugs.count();
    while (i.hasNext())
    {
        i.next();
        responseMap = i.value().toMap();
        QMap<QString, QString> newBug;
        newBug["tracker_name"] = mName;
        newBug["bug_id"] = responseMap.value("id").toString();
        newBug["summary"] = responseMap.value("summary").toString();
        insertList << newBug;
    }

    pSqlWriter->multiInsert("search_results", insertList);
}

void
Bugzilla::searchInsertionFinished()
{
    qDebug() << "searchInsertionFinished";
   emit searchFinished();
}

void
Bugzilla::userBugListFinished()
{
    qDebug() << "userBugListFinished";
    QNetworkReply *reply = qobject_cast<QNetworkReply*>(sender());
    if (reply->error())
    {
        emit backendError(reply->errorString());
        reply->close();

        return;
    }
    QString csv = reply->readAll();
    parseBuglistCSV(csv, "Assigned");
    reply->close();

    QList< QMap<QString,QString> > insertList;
    QVariantMap responseMap;
    QMapIterator<QString, QVariant> i(mBugs);
    mUpdateCount = mBugs.count();
    while (i.hasNext())
    {
        i.next();
        responseMap = i.value().toMap();
        QMap<QString, QString> newBug;
        newBug["tracker_id"] = mId;
        newBug["bug_id"] = responseMap.value("id").toString();
        newBug["severity"] = responseMap.value("severity").toString();
        newBug["priority"] = responseMap.value("priority").toString();
        newBug["assigned_to"] = responseMap.value("assigned_to").toString();
        newBug["status"] = responseMap.value("status").toString();
        newBug["summary"] = responseMap.value("summary").toString();
        newBug["component"] = responseMap.value("component").toString();
        newBug["product"] = responseMap.value("product").toString();
        newBug["bug_type"] = responseMap.value("bug_type").toString();
        newBug["last_modified"] = responseMap.value("last_change_time").toString();
        if ((newBug["status"].toUpper() == "RESOLVED")
            ||(newBug["status"].toUpper() == "CLOSED"))
                newBug["bug_state"] = "closed";
        insertList << newBug;
    }

    pSqlWriter->insertBugs("bugzilla", insertList);
}

void
Bugzilla::bugsInsertionFinished(QStringList idList)
{
    Q_UNUSED(idList);
    qDebug() << "bugInsertionFinished";
    updateSync();
    emit bugsUpdated();

}

void
Bugzilla::ccFinished()
{
    qDebug() << "CCs are done";
    QNetworkReply *reply = qobject_cast<QNetworkReply*>(sender());
    int wasRedirected = reply->request().attribute(QNetworkRequest::User).toInt();
    QVariant redirect = reply->attribute(QNetworkRequest::RedirectionTargetAttribute);
    if (reply->error())
    {
        emit backendError(reply->errorString());
        reply->close();
        return;
    }

    if (!redirect.toUrl().isEmpty() && !wasRedirected)
    {
        qDebug() << "Was redirected to " << redirect.toUrl();
        reply->deleteLater();
        QNetworkRequest req = QNetworkRequest(redirect.toUrl());
        req.setAttribute(QNetworkRequest::User, QVariant(1));
        QNetworkReply *rep = pManager->get(req);
        connect(rep, SIGNAL(finished()),
                this, SLOT(ccFinished()));
        return;
    }

    mBugs.clear();
    QString csv = reply->readAll();
    parseBuglistCSV(csv, "CC");
    reply->close();
    getReportedBugs();
}

// Parse buglist.cgi?ctype=csv output and convert to a map
void
Bugzilla::parseBuglistCSV(const QString &csv,
                          const QString &bugType)
{
    QString entry;
    QRegExp reg("^\"|\"$");
    QStringList list = csv.split("\n", QString::SkipEmptyParts);
    for (int i = 1; i < list.size(); ++i) // the first line is the column descriptions
    {
         QStringList bug = list.at(i).split(",");
         QVariantMap newBug;
         newBug["id"]  = bug.at(0);
         entry = bug.at(1);
         newBug["last_change_time"] = entry.remove(reg);
         entry = bug.at(2);
         newBug["severity"] = entry.remove(reg);
         entry = bug.at(3);
         newBug["priority"] = entry.remove(reg);
         entry = bug.at(4);
         newBug["assigned_to"] = entry.remove(reg);
         entry = bug.at(5);
         newBug["status"] = entry.remove(reg);
         entry = bug.at(6);
         newBug["product"] = entry.remove(reg);
         entry = bug.at(7);
         newBug["component"] = entry.remove(reg);
         entry = list.at(i).section(',', 8);
         newBug["summary"] = entry.remove(reg);
         newBug["bug_type"] = bugType;
         mBugs[bug.at(0)] = newBug;
    }
}

// This is the response slot called after show_bug.cgi?id=X&id=Y&id=Z is called.
// It just cares about figuring out what tokens match with what bug IDs.
void
Bugzilla::idDetailsFinished()
{
    QString id, token, text;
    QMap<QString, QString> tokenMap;
    QNetworkReply *reply = qobject_cast<QNetworkReply*>(sender());
    if (reply->error())
    {
        emit backendError(reply->errorString());
        reply->close();
        return;
    }

    QString xml = QString::fromUtf8((const char*) reply->readAll());

    QXmlStreamReader xmlReader(xml);
    while (!xmlReader.atEnd())
    {
        xmlReader.readNext();
        if (xmlReader.isEndElement())
        {
            if (xmlReader.name().toString() == "bug_id")
            {
                id = text;
            }
            else if (xmlReader.name().toString() == "token")
            {
                token = text;
            }
            else if (xmlReader.name().toString() == "bug")
            {
                if (id != "" && token != "")
                {
                    qDebug() << "Will push for " << id << " token " << token;
                    tokenMap[id] = token;
                }

                id = "";
                token = "";
            }
        }
        else if (xmlReader.isCharacters() && !xmlReader.isWhitespace())
        {
            text = xmlReader.text().toString();
        }
    }

    // Now that we've found the tokens, post all the relevent information
    postNewItems(tokenMap);
    reply->close();
}


void
Bugzilla::itemPostFinished()
{
    int i;
    QNetworkReply *reply = qobject_cast<QNetworkReply*>(sender());
    QString id = reply->request().attribute(QNetworkRequest::User).toString();
    if (reply->error())
    {
        if (reply->error() != QNetworkReply::OperationCanceledError)
        {
            emit backendError(reply->errorString());
        }
        reply->close();
        return;
    }

    QMap<QString, QString> map;
    for (i = 0; i < mPostQueue.size(); ++i)
    {
        map = mPostQueue.at(i);
        if (map["id"] == id)
        {
            mPostQueue.takeAt(i);
            QSqlQuery sql;
            sql.exec(QString("DELETE FROM shadow_bugs WHERE bug_id=\'%1\' AND tracker_id=%2").arg(id).arg(mId));
            break;
        }
    }

    postItem();
    reply->close();
}

void
Bugzilla::commentXMLFinished()
{
    QNetworkReply *reply = qobject_cast<QNetworkReply*>(sender());
    if (reply->error())
    {
        emit backendError(reply->errorString());
        reply->close();
        return;
    }

    // The first comment in the XML is really the description of the bug.
    // For consistancy with the other trackers, we store it separately.
    bool foundDescription = false;
    QString xml = QString::fromUtf8((const char*) reply->readAll());
    QString id, name, text;
    QList<QMap<QString, QString> > commentList;
    QMap<QString, QString> commentMap;
    QXmlStreamReader xmlReader(xml);
    mPendingCommentInsertions = 0;
    bool insertedComment = false;
    while (!xmlReader.atEnd())
    {
        xmlReader.readNext();
        name = xmlReader.name().toString();
        if (xmlReader.isStartElement())
        {
            if (name == "long_desc")
            {
                commentMap.clear();
                commentMap["bug_id"] = id;
                commentMap["comment_id"] = "";
                commentMap["tracker_id"] = mId;
                commentMap["private"] = xmlReader.attributes().value("isprivate").toString();
            }
            else if (name == "who")
            {
                QString n = xmlReader.attributes().value("name").toString();
                if (!n.isEmpty())
                    commentMap["author"] = n;
            }
        }
        else if (xmlReader.isEndElement())
        {
            if (name == "bug_id")
            {
                id = text;
            }
            else if (name == "long_desc")
            {
                if (!foundDescription)
                {
                    foundDescription = true;
                    QMap<QString, QString> val;
                    QMap<QString, QString> params;
                    val["description"] = commentMap["comment"];
                    params["tracker_id"] = mId;
                    params["bug_id"] = commentMap["bug_id"];
                    SqlUtilities::simpleUpdate("bugzilla", val, params);
                }
                else
                {
                    commentList << commentMap;
                }
            }
            else if (name == "who")
            {
                if (commentMap["author"].isEmpty())
                    commentMap["author"] = text;
            }
            else if (name == "bug_when")
            {
                commentMap["timestamp"] = text;
            }
            else if (name == "thetext")
            {
                commentMap["comment"] = text;
            }
            else if (name == "bug")
            {
                if (id != "" && commentList.size() > 0)
                {
                    // Save the comments
                        mPendingCommentInsertions++;
                        insertedComment = true;
                        pSqlWriter->insertComments(commentList);
                }

                id = "";
                commentList.clear();
            }
        }
        else if (xmlReader.isCharacters() && !xmlReader.isWhitespace())
        {
            text = xmlReader.text().toString();
        }
    }

    // If we reach here, that means we didn't have anything to do
    if (!insertedComment)
    {
        updateSync();
        emit bugsUpdated();
    }
}

void
Bugzilla::individualBugFinished()
{
    qDebug() << "individualBugFinished";
    QNetworkReply *reply = qobject_cast<QNetworkReply*>(sender());
    if (reply->error())
    {
        emit backendError(reply->errorString());
        reply->close();
        return;
    }

    bool foundDescription = false;
    QString xml = QString::fromUtf8((const char*) reply->readAll());
    QString id, name, text;
    QList<QMap<QString, QString> > commentList;
    QList<QMap<QString, QString> > bugInsertList;
    QMap<QString, QString> newBug;
    QMap<QString, QString> commentMap;
    QXmlStreamReader xmlReader(xml);
    mPendingCommentInsertions = 0;
    while (!xmlReader.atEnd())
    {
        xmlReader.readNext();
        name = xmlReader.name().toString();
        if (xmlReader.isStartElement())
        {
            if (name == "long_desc")
            {
                commentMap.clear();
                commentMap["bug_id"] = id;
                commentMap["comment_id"] = "";
                commentMap["tracker_id"] = mId;
                commentMap["private"] = xmlReader.attributes().value("isprivate").toString();
            }
            else if (name == "who")
            {
                QString n = xmlReader.attributes().value("name").toString();
                if (!n.isEmpty())
                    commentMap["author"] = n;
            }
            else if (name == "bug")
            {
                newBug["highlight_type"] = QString::number(SqlUtilities::HIGHLIGHT_SEARCH);
                newBug["tracker_id"] = mId;
                newBug["bug_type"] = "Searched";
            }
        }
        else if (xmlReader.isEndElement())
        {
            if (name == "bug_id")
            {
                id = text;
                newBug["bug_id"] = text;
            }
            else if (name == "short_desc")
            {
                newBug["summary"] = text;
            }
            else if (name == "bug_severity")
            {
                newBug["severity"] = text;
            }
            else if (name == "priority")
            {
                newBug["priority"] = text;
            }
            else if (name == "assigned_to")
            {
                newBug["assigned_to"] = text;
            }
            else if (name == "component")
            {
                newBug["component"] = text;
            }
            else if (name == "product")
            {
                newBug["product"] = text;
            }
            else if (name == "bug_status")
            {
                newBug["status"] = text;
                if ((text.toLower() != "resolved") && (text.toLower() != "closed"))
                    newBug["bug_state"] = "open";
                else
                    newBug["bug_state"] = "closed";
            }
            else if (name == "delta_ts")
            {
                newBug["last_modified"] = text;
            }
            else if (name == "long_desc")
            {
                if (!foundDescription)
                {
                    foundDescription = true;
                    newBug["description"] = commentMap["comment"];
                }
                else
                {
                    commentList << commentMap;
                }
            }
            else if (name == "who")
            {
                if (commentMap["author"].isEmpty())
                    commentMap["author"] = text;
            }
            else if (name == "bug_when")
            {
                commentMap["timestamp"] = text;
            }
            else if (name == "thetext")
            {
                commentMap["comment"] = text;
            }
        }
        else if (xmlReader.isCharacters() && !xmlReader.isWhitespace())
        {
            text = xmlReader.text().toString();
        }
    }

    if ((id == "") || (newBug.size() <= 1))
    {
        emit backendError("Got some weird XML back, sorry");
        return;
    }
    bugInsertList << newBug;
    pSqlWriter->insertBugs("bugzilla", bugInsertList);
    if (commentList.size() > 0)
    {
        mPendingCommentInsertions++;
        pSqlWriter->insertBugComments(commentList);
    }

    emit searchResultFinished(newBug);
}

void
Bugzilla::commentInsertionFinished()
{
    emit commentsCached();
}

QString
Bugzilla::buildBugUrl(const QString &id)
{
    QString url = mUrl + "/show_bug.cgi?id=" + id;
    return(url);
}

// TODO implement this?
void
Bugzilla::handleSslErrors(QNetworkReply *reply,
                         const QList<QSslError> &errors)
{
    Q_UNUSED(errors);
    reply->ignoreSslErrors();
}


