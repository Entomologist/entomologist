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
#include "qtsoap/qtsoap.h"
#include <QSqlTableModel>
#include <QVariantMap>
#include "Mantis.h"

// The Mantis SOAP API for 1.1 and 1.2 doesn't allow us to safely
// list all bugs, so we use the API to get the version and the
// allowed priorities, statuses and severities and then we grab
// CSV values for the bug list.
//
// 1.3 will have full support.
Mantis::Mantis(const QString &url, QObject *parent) :
    Backend(url)
{
    mUploadingBugs = false;
    pManager->setCookieJar(pCookieJar);
    connect(pManager, SIGNAL(sslErrors(QNetworkReply *, const QList<QSslError> &)),
            this, SLOT(handleSslErrors(QNetworkReply *, const QList<QSslError> &)));

    pMantis = new QtSoapHttpTransport(this);
    connect(pMantis, SIGNAL(responseReady()),
            this, SLOT(response()));
    connect(pMantis->networkAccessManager(), SIGNAL(sslErrors(QNetworkReply*,QList<QSslError>)),
            this, SLOT(handleSslErrors(QNetworkReply*,QList<QSslError>)));
    connect(pSqlWriter, SIGNAL(bugsFinished(QStringList)),
            this, SLOT(bugsInsertionFinished(QStringList)));
    connect(pSqlWriter, SIGNAL(commentFinished()),
            this, SLOT(commentInsertionFinished()));

    bool secure = true;
    if (QUrl(mUrl).scheme() == "http")
        secure = false;
    pMantis->setHost(QUrl(mUrl).host(), secure);
}

Mantis::~Mantis()
{
}

void
Mantis::sync()
{
    login();
}

void
Mantis::login()
{
    QString url = mUrl + "/login.php";
    QString query = QString("username=%1&password=%2&").arg(mUsername).arg(mPassword);
    QNetworkRequest req = QNetworkRequest(QUrl(url));
    QNetworkReply *rep = pManager->post(req, query.toAscii());
    connect(rep, SIGNAL(finished()),
            this, SLOT(loginResponse()));
}

void
Mantis::setView()
{
    // First we have to set up the 'view' to get the CSV
    // We set a date in the future to get the end of the date range
    QDateTime future = QDateTime::currentDateTime().addDays(7);
    QString queryType;
    if (mViewType == MONITORED)
        queryType = "user_monitor[]=-1&reporter_id[]=0&handler_id[]=0";
    else if (mViewType == REPORTED)
        queryType = "user_monitor[]=0&reporter_id[]=-1&handler_id[]=0";
    else
        queryType = "user_monitor[]=0&reporter_id[]=0&handler_id[]=-1";
    QString url = mUrl + "/view_all_set.php?f=3";
    // I'm not sure why I missed this in 0.4, but Mantis's date range
    // filter doesn't actually filter on modified times, so we'll
    // have to download all bugs on each sync.
    QString query = QString("type=1&page_number=1&view_type=simple&%1&hide_status[]=80")
            .arg(queryType);
    QNetworkRequest req = QNetworkRequest(QUrl(url));
    QNetworkReply *rep = pManager->post(req, query.toAscii());
    connect(rep, SIGNAL(finished()),
            this, SLOT(viewResponse()));
}

void
Mantis::getAssigned()
{
    QString url = mUrl + "/csv_export.php";
    QNetworkRequest req = QNetworkRequest(QUrl(url));
    QNetworkReply *rep = pManager->get(req);
    connect(rep, SIGNAL(finished()),
            this, SLOT(assignedResponse()));
}

void
Mantis::getReported()
{
    QString url = mUrl + "/csv_export.php";
    QNetworkRequest req = QNetworkRequest(QUrl(url));
    QNetworkReply *rep = pManager->get(req);
    connect(rep, SIGNAL(finished()),
            this, SLOT(reportedResponse()));
}

void
Mantis::getMonitored()
{
    QString url = mUrl + "/csv_export.php";
    QNetworkRequest req = QNetworkRequest(QUrl(url));
    QNetworkReply *rep = pManager->get(req);
    connect(rep, SIGNAL(finished()),
            this, SLOT(monitoredResponse()));
}

void
Mantis::handleCSV(const QString &csv, const QString &bugType)
{
    QVector<QString> bug;

    QString entry;
    QString colEntry;
    int colId = -1,
        colProduct = -1,
        colAssignedTo = -1,
        colPriority = -1,
        colSeverity = -1,
        colComponent = -1,
        colLastModified = -1,
        colSummary = -1,
        colStatus = -1;
    QRegExp reg("^\"|\"$");
    QRegExp removeLeadingZeros("^0+");
    QStringList list = csv.split("\n", QString::SkipEmptyParts);

    // The first line is the column descriptions, so parse them
    // and map them to what we want
    bug = parseCSVLine(list.at(0));
    for (int i = 0; i < bug.size(); ++i)
    {
        colEntry = bug.at(i).toLower().remove(reg);
        if (colEntry == "id")
            colId = i;
        else if (colEntry == "project")
            colProduct = i;
        else if (colEntry == "category")
            colComponent = i;
        else if (colEntry == "assigned to")
            colAssignedTo = i;
        else if (colEntry == "priority")
            colPriority = i;
        else if (colEntry == "severity")
            colSeverity = i;
        else if (colEntry == "updated")
            colLastModified = i;
        else if (colEntry == "summary")
            colSummary = i;
        else if (colEntry == "status")
            colStatus = i;
    }

    for (int i = 1; i < list.size(); ++i)
    {
        bug = parseCSVLine(list.at(i));
        QVariantMap newBug;
        if (colId != -1)
            entry = bug.at(colId);
        else
            break;

        qDebug() << "CSV Found " << entry;
        newBug["id"]  = entry.remove(removeLeadingZeros);

        if (colProduct)
            entry = bug.at(colProduct);
        else
            entry = "";
        newBug["product"] = entry.remove(reg);

        if (colAssignedTo)
            entry = bug.at(colAssignedTo);
        else
            entry = "";
        newBug["assigned_to"] = entry.remove(reg);

        if (colPriority)
            entry = bug.at(colPriority);
        else
            entry = "";
        newBug["priority"] = entry.remove(reg);

        if (colSeverity)
            entry = bug.at(colSeverity);
        else
            entry = "";
        newBug["severity"] = entry.remove(reg);

        if (colComponent)
            entry = bug.at(colComponent);
        else
            entry = "";
        newBug["component"] = entry.remove(reg);

        if (colLastModified)
            entry = bug.at(colLastModified);
        else
            entry = "1970-01-01";
        newBug["last_modified"] = entry.remove(reg);

        if (colSummary)
            entry = bug.at(colSummary);
        else
            entry = "";
        newBug["summary"] = entry.remove(reg);

        if (colStatus)
            entry = bug.at(colStatus);
        else
            entry = "";
        newBug["status"] = entry.remove(reg);

        newBug["bug_type"] = bugType;
        mBugs[bug.at(0)] = newBug;
    }
}

QVector<QString>
Mantis::parseCSVLine(const QString &line)
{
    QVector<QString> columns;
    bool inQuotes = false;
    int parsePosition = 0;
    QString tmp = "";

    while(parsePosition < line.length())
    {
        QChar c = line[parsePosition];
        if (c == '"' && !inQuotes)
        {
                inQuotes = true;
        }
        else if (c== '"' && inQuotes)
        {
            if ((parsePosition + 1 < line.length()) && (line[parsePosition + 1] == '"'))
            {
                    tmp.push_back(c);
                    parsePosition++;
            }
            else
            {
                    inQuotes = false;
            }
        }
        else if (!inQuotes && c == ',')
        {
            columns.push_back(QString(tmp));
            tmp.clear();
        }
        else if (!inQuotes && ( c == '\n' || c == '\r'))
        {
            columns.push_back(QString(tmp));
            tmp.clear();
        } else
        {
            tmp.push_back(c);
        }

        parsePosition++;
    }
    return columns;
}

void
Mantis::response()
{
    const QtSoapMessage &resp = pMantis->getResponse();

    if (resp.isFault())
    {
        emit bugsUpdated();
        emit backendError(QString("%1: %2").arg(resp.faultString().toString()).arg(resp.faultDetail().toString()));
        return;
    }
    const QtSoapType &message = resp.method();
    const QtSoapType &response = resp.returnValue();
    QString messageName = message.name().name();

    if (messageName == "mc_versionResponse")
    {
        // Version might be X.Y.Z, but we just care about the major.minor
        QRegExp ex("^([1-9]+.?[1-9]*).*$");
        double version;
        if (ex.indexIn(response.toString()) == -1)
        {
            qDebug() << "Mantis version response: invalid (" << response.toString() <<")";
            version = -1;
        }
        else
        {
            version = ex.cap(1).toDouble();
            qDebug() << "Mantis version: " << version;
        }

        emit versionChecked(QString("%1").arg(version));
    }
    else if (messageName == "mc_enum_prioritiesResponse")
    {
        QtSoapArray &array = (QtSoapArray &) resp.returnValue();
        QStringList response;

        for (int i = 0; i < array.count(); ++i)
        {
            response  << array.at(i)["name"].toString();
        }

        qDebug() << "Priorities: " << response;
        emit prioritiesFound(response);
    }
    else if (messageName == "mc_enum_statusResponse")
    {
        QtSoapArray &array = (QtSoapArray &) resp.returnValue();
        QStringList response;

        for (int i = 0; i < array.count(); ++i)
        {
            response  << array.at(i)["name"].toString();
        }

        qDebug() << "Statuses: " << response;
        emit statusesFound(response);
    }
    else if (messageName == "mc_enum_severitiesResponse")
    {
        QtSoapArray &array = (QtSoapArray &) resp.returnValue();
        QStringList response;

        for (int i = 0; i < array.count(); ++i)
        {
            response  << array.at(i)["name"].toString();
        }

        qDebug() << "Severities: " << response;
        emit severitiesFound(response);
    }
    else if (messageName == "mc_issue_getResponse")
    {
        if (mUploadingBugs)
        {
            const QtSoapType &issue = resp.returnValue();
            QVariantMap changed = mUploadList.value(issue["id"].toString()).toMap();
            mCurrentUploadId = issue["id"].toString();
            QtSoapMessage request;
            QtSoapStruct *s = new QtSoapStruct(QtSoapQName("issue"));
            if (!changed.value("severity").isNull())
            {
                QtSoapStruct *newSeverity = new QtSoapStruct(QtSoapQName("severity"));
                newSeverity->insert(new QtSoapSimpleType(QtSoapQName("name"), changed.value("severity").toString()));
                s->insert(newSeverity);
            }
            else
                s->insert(new QtSoapStruct((QtSoapStruct&)issue["severity"]));

            if (!changed.value("priority").isNull())
            {
                QtSoapStruct *newPriority = new QtSoapStruct(QtSoapQName("priority"));
                newPriority->insert(new QtSoapSimpleType(QtSoapQName("name"), changed.value("priority").toString()));
                s->insert(newPriority);
            }
            else
                s->insert(new QtSoapStruct((QtSoapStruct&)issue["priority"]));

            if (!changed.value("status").isNull())
            {
                QtSoapStruct *newStatus = new QtSoapStruct(QtSoapQName("status"));
                newStatus->insert(new QtSoapSimpleType(QtSoapQName("name"), changed.value("status").toString()));
                s->insert(newStatus);
            }
            else
                s->insert(new QtSoapStruct((QtSoapStruct&)issue["status"]));

            s->insert(new QtSoapStruct((QtSoapStruct &)issue["handler"]));
            s->insert(new QtSoapStruct((QtSoapStruct &)issue["reproducibility"]));
            s->insert(new QtSoapStruct((QtSoapStruct &)issue["resolution"]));
            s->insert(new QtSoapStruct((QtSoapStruct &)issue["projection"]));
            s->insert(new QtSoapStruct((QtSoapStruct &)issue["eta"]));
            s->insert(new QtSoapStruct((QtSoapStruct &)issue["view_state"]));
            s->insert(new QtSoapStruct((QtSoapStruct &)issue["reporter"]));
            s->insert(new QtSoapSimpleType((QtSoapSimpleType &)issue["summary"]));
            s->insert(new QtSoapSimpleType((QtSoapSimpleType &)issue["description"]));
            s->insert(new QtSoapSimpleType((QtSoapSimpleType &)issue["additional_information"]));
            s->insert(new QtSoapSimpleType((QtSoapSimpleType &)issue["steps_to_reproduce"]));
            s->insert(new QtSoapSimpleType((QtSoapSimpleType &)issue["build"]));
            s->insert(new QtSoapSimpleType((QtSoapSimpleType &)issue["platform"]));
            s->insert(new QtSoapSimpleType((QtSoapSimpleType &)issue["os"]));
            s->insert(new QtSoapSimpleType((QtSoapSimpleType &)issue["os_build"]));
            s->insert(new QtSoapSimpleType((QtSoapSimpleType &)issue["sponsorship_total"]));
            s->insert(new QtSoapSimpleType((QtSoapSimpleType &)issue["category"]));
            s->insert(new QtSoapSimpleType((QtSoapSimpleType &)issue["version"]));
            s->insert(new QtSoapStruct((QtSoapStruct &)issue["project"]));
            request.setMethod(QtSoapQName("mc_issue_update", "http://futureware.biz/mantisconnect"));
            request.addMethodArgument("username", "", mUsername );
            request.addMethodArgument("password", "", mPassword);
            request.addMethodArgument("issueId", "", issue["id"].toInt());
            request.addMethodArgument(s);
            pMantis->submitRequest(request, QUrl(mUrl).path() + "/api/soap/mantisconnect.php");
        }
        else
        {
            QList<QMap<QString, QString> > list;
            QString bugId = resp.returnValue()["id"].toString();
            QtSoapArray &array = (QtSoapArray &) resp.returnValue()["notes"];

            for (int i = 0; i < array.count(); ++i)
            {
                QMap<QString, QString> newComment;
                QtSoapType &note = array.at(i);
                newComment["tracker_id"] = mId;
                newComment["bug_id"] = bugId;
                newComment["author"] = note["reporter"]["name"].toString();
                newComment["comment_id"] = note["id"].toString();
                newComment["comment"] = note["text"].toString();
                newComment["timestamp"] = note["last_modified"].toString();
                newComment["private"] = "0";
                if (note["view_state"]["name"].toString() == "private")
                    newComment["private"] = "1";

                list << newComment;
            }
            pSqlWriter->insertBugComments(list);
        }
    }
    else if (messageName == "mc_issue_updateResponse")
    {
        mUploadList.remove(mCurrentUploadId);
        QSqlQuery sql;
        sql.exec(QString("DELETE FROM shadow_bugs WHERE bug_id=\'%1\' AND tracker_id=%2").arg(mCurrentUploadId).arg(mId));
        getNextUpload();
    }
    else if (messageName == "mc_issue_note_addResponse")
    {
        QVariantMap map = mCommentUploadList.takeAt(0).toMap();
        QSqlQuery sql;
        sql.exec(QString("DELETE FROM shadow_comments WHERE bug_id=\'%1\' AND tracker_id=%2").arg(map.value("bug_id").toString()).arg(mId));
        getNextCommentUpload();
    }
    else
    {
        qDebug() << "Invalid response: " << messageName;
        emit backendError("Something went wrong!");
    }
}

void
Mantis::checkVersion()
{
    qDebug() << "Checking Mantis version";
    QtSoapMessage request;
    request.setMethod(QtSoapQName("mc_version", "http://futureware.biz/mantisconnect"));
    pMantis->submitRequest(request, QUrl(mUrl).path() + "/api/soap/mantisconnect.php");
}

void
Mantis::checkValidPriorities()
{
    qDebug() << "Checking Mantis priorities";
    QtSoapMessage request;
    request.setMethod(QtSoapQName("mc_enum_priorities", "http://futureware.biz/mantisconnect"));
    request.addMethodArgument("username", "", mUsername);
    request.addMethodArgument("password", "", mPassword);
    pMantis->submitRequest(request, QUrl(mUrl).path() + "/api/soap/mantisconnect.php");
}

void
Mantis::checkValidSeverities()
{
    qDebug() << "Checking Mantis severities";
    QtSoapMessage request;
    request.setMethod(QtSoapQName("mc_enum_severities", "http://futureware.biz/mantisconnect"));
    request.addMethodArgument("username", "", mUsername );
    request.addMethodArgument("password", "", mPassword);
    pMantis->submitRequest(request, QUrl(mUrl).path() + "/api/soap/mantisconnect.php");
}

void
Mantis::checkValidStatuses()
{
    qDebug() << "Checking Mantis statuses";
    QtSoapMessage request;
    request.setMethod(QtSoapQName("mc_enum_status", "http://futureware.biz/mantisconnect"));
    request.addMethodArgument("username", "", mUsername );
    request.addMethodArgument("password", "", mPassword);
    pMantis->submitRequest(request, QUrl(mUrl).path() + "/api/soap/mantisconnect.php");
}

void
Mantis::getComments(const QString &bugId)
{
    qDebug() << "Get comments for Mantis bug #" << bugId;
    QtSoapMessage request;
    request.setMethod(QtSoapQName("mc_issue_get", "http://futureware.biz/mantisconnect"));
    request.addMethodArgument("username", "", mUsername );
    request.addMethodArgument("password", "", mPassword);
    request.addMethodArgument("issue_id", "", bugId.toInt());
    pMantis->submitRequest(request, QUrl(mUrl).path() + "/api/soap/mantisconnect.php");
}

void
Mantis::uploadAll()
{
    if (!hasPendingChanges())
    {
        emit bugsUpdated();
        return;
    }

    mUploadList.clear();
    mCommentUploadList.clear();
    mUploadingBugs = true;
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
        if (!q.value(1).isNull())
            ret["severity"] = q.value(1).toString().remove(QRegExp("<[^>]*>"));
        if (!q.value(2).isNull())
            ret["priority"] = q.value(2).toString().remove(QRegExp("<[^>]*>"));
        if (!q.value(3).isNull())
            ret["assignee"] = q.value(3).toString().remove(QRegExp("<[^>]*>"));
        if (!q.value(4).isNull())
            ret["status"] = q.value(4).toString().remove(QRegExp("<[^>]*>"));
        if (!q.value(5).isNull())
            ret["short_desc"] = q.value(5).toString().remove(QRegExp("<[^>]*>"));
        mUploadList[q.value(0).toString()] = ret;
    }

    if ((mCommentUploadList.size() == 0) && (mUploadList.size() == 0))
    {
        emit bugsUpdated();
        return;
    }

    getNextUpload();
}

void
Mantis::getNextUpload()
{
    int issue_id = -1;
    QMapIterator<QString, QVariant> i(mUploadList);
    if (!i.hasNext())
    {
        mCurrentUploadId = "";
        getNextCommentUpload();
        return;
    }
    i.next();

    issue_id = i.key().toInt();

    QtSoapMessage request;
    request.setMethod(QtSoapQName("mc_issue_get", "http://futureware.biz/mantisconnect"));
    request.addMethodArgument("username", "", mUsername );
    request.addMethodArgument("password", "", mPassword);
    request.addMethodArgument("issue_id", "", issue_id);
    pMantis->submitRequest(request, QUrl(mUrl).path() + "/api/soap/mantisconnect.php");
}

void
Mantis::getNextCommentUpload()
{
    if (mCommentUploadList.size() == 0)
    {
        qDebug() << "Done uploading comments";
        mUploadingBugs = false;
        sync();
        return;
    }

    qDebug() << "Going to upload a comment...";
    QVariantMap map = mCommentUploadList.at(0).toMap();
    QtSoapMessage request;
    request.setMethod(QtSoapQName("mc_issue_note_add", "http://futureware.biz/mantisconnect"));
    request.addMethodArgument("username", "", mUsername );
    request.addMethodArgument("password", "", mPassword);
    request.addMethodArgument("issue_id", "", map.value("bug_id").toInt());
    QtSoapStruct *note = new QtSoapStruct(QtSoapQName("note"));
    note->insert(new QtSoapSimpleType(QtSoapQName("text"), map.value("comment").toString()));
    if (map.value("private").toInt() == 1)
    {
        QtSoapStruct *p = new QtSoapStruct(QtSoapQName("view_state"));
        p->insert(new QtSoapSimpleType(QtSoapQName("name"), "private"));
        note->insert(p);
    }
    request.addMethodArgument(note);
    pMantis->submitRequest(request, QUrl(mUrl).path() + "/api/soap/mantisconnect.php");
}

QString
Mantis::buildBugUrl(const QString &id)
{
    QString url = mUrl + "/view.php?id=" + id;
    return(url);
}

QString
Mantis::autoCacheComments()
{
// Mantis 1.3 will maybe support this
//    if (mVersion.toDouble() >= 1.3)
//        return("1");
//    else
        return("0");
}

void Mantis::loginResponse()
{
    QNetworkReply *reply = qobject_cast<QNetworkReply*>(sender());
    if (reply->error())
    {
        qDebug() << "loginResponse: " << reply->errorString();
        emit backendError(reply->errorString());
        reply->close();

        return;
    }
    reply->deleteLater();
    mViewType = MONITORED;
    setView();
}

void Mantis::viewResponse()
{
    qDebug() << "View response";
    QNetworkReply *reply = qobject_cast<QNetworkReply*>(sender());
    if (reply->error())
    {
        qDebug() << "viewResponse: " << reply->errorString();
        emit backendError(reply->errorString());
        reply->close();

        return;
    }

    reply->deleteLater();

    if (mViewType == ASSIGNED)
        getAssigned();
    else if (mViewType == REPORTED)
        getReported();
    else if (mViewType == MONITORED)
        getMonitored();
}

void Mantis::assignedResponse()
{
    qDebug() << "Assigned response";
    QNetworkReply *reply = qobject_cast<QNetworkReply*>(sender());
    if (reply->error())
    {
        qDebug() << "assignedResponse: " << reply->errorString();
        emit backendError(reply->errorString());
        reply->close();

        return;
    }
    QString rep = reply->readAll();
    reply->deleteLater();
    handleCSV(rep, "Assigned");

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
        newBug["last_modified"] = responseMap.value("last_modified").toString();
        insertList << newBug;
    }

    pSqlWriter->insertBugs(insertList);
}

void Mantis::reportedResponse()
{
    qDebug() << "reported response";
    QNetworkReply *reply = qobject_cast<QNetworkReply*>(sender());
    if (reply->error())
    {
        qDebug() << "reportedResponse: " << reply->errorString();
        emit backendError(reply->errorString());
        reply->close();

        return;
    }

    QString rep = reply->readAll();
    reply->deleteLater();
    handleCSV(rep, "Reported");
    mViewType = ASSIGNED;
    setView();
}

void Mantis::monitoredResponse()
{
    QNetworkReply *reply = qobject_cast<QNetworkReply*>(sender());
    if (reply->error())
    {
        qDebug() << "monitoredResponse: " << reply->errorString();
        emit backendError(reply->errorString());
        reply->close();

        return;
    }

    QString rep = reply->readAll();
    reply->deleteLater();
    handleCSV(rep, "CC");
    mViewType = REPORTED;
    setView();
}

void
Mantis::bugsInsertionFinished(QStringList idList)
{
    qDebug() << "Mantis: bugs updated";
    updateSync();
    emit bugsUpdated();
}

void
Mantis::commentInsertionFinished()
{
    emit commentsCached();
}

void
Mantis::handleSslErrors(QNetworkReply *reply,
                         const QList<QSslError> &errors)
{
    reply->ignoreSslErrors();
}
