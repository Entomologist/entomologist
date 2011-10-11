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
#include "SqlUtilities.h"
#include "tracker_uis/MantisUI.h"
#include "Translator.h"

// The Mantis SOAP API for 1.1 and 1.2 doesn't allow us to safely
// list all bugs, so we use the API to get the version and the
// allowed priorities, statuses and severities and then we grab
// CSV values for the bug list.
//
// 1.3 will have full support.
Mantis::Mantis(const QString &url, QObject *parent) :
    Backend(url)
{
    Q_UNUSED(parent);
    mUploadingBugs = false;
    mVersion = "-1";
    pManager->setCookieJar(pCookieJar);
    connect(pManager, SIGNAL(sslErrors(QNetworkReply *, const QList<QSslError> &)),
            this, SLOT(handleSslErrors(QNetworkReply *, const QList<QSslError> &)));

    pMantis = new QtSoapHttpTransport(this);
    connect(pMantis, SIGNAL(responseReady()),
            this, SLOT(response()));
    connect(pMantis->networkAccessManager(), SIGNAL(sslErrors(QNetworkReply*,QList<QSslError>)),
            this, SLOT(handleSslErrors(QNetworkReply*,QList<QSslError>)));
    connect(pSqlWriter, SIGNAL(bugsFinished(QStringList, int)),
            this, SLOT(bugsInsertionFinished(QStringList, int)));
    connect(pSqlWriter, SIGNAL(commentFinished()),
            this, SLOT(commentInsertionFinished()));
    connect(pSqlWriter, SIGNAL(success(int)),
            this, SLOT(multiInsertSuccess(int)));

    bool secure = true;
    if (QUrl(mUrl).scheme() == "http")
        secure = false;
    pMantis->setHost(QUrl(mUrl).host(), secure);
}

Mantis::~Mantis()
{
}

BackendUI *
Mantis::displayWidget()
{
    if (pDisplayWidget == NULL)
        pDisplayWidget = new MantisUI(mId, mName, this);
    return(pDisplayWidget);
}

void
Mantis::deleteData()
{

}

void
Mantis::sync()
{
    mBugs.clear();
    QString url = mUrl + "/login.php";
    QString query = QString("username=%1&password=%2&").arg(mUsername).arg(mPassword);
    QNetworkRequest req = QNetworkRequest(QUrl(url));
    QNetworkReply *rep = pManager->post(req, query.toAscii());
    connect(rep, SIGNAL(finished()),
            this, SLOT(loginSyncResponse()));
}

void
Mantis::search(const QString &query)
{
    mBugs.clear();
    mViewType = SEARCHED;
    setView(query);
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
Mantis::setView(const QString &search)
{
    // First we have to set up the 'view' to get the CSV
    QString queryType;
    if (mViewType == CC) // In mantis, this is a "monitor", but called CC here to not clash with our "monitors"
    {
        queryType = "user_monitor[]=-1&reporter_id[]=0&handler_id[]=0&project_id[]=0";
    }
    else if (mViewType == MONITORED)
    {
        if (mMonitorComponents.isEmpty())
        {
            mViewType = CC;
            setView();
            return;
        }
        QString componentQuery;
        for (int i = 0; i < mMonitorComponents.size(); ++i)
        {
            QString item = mMonitorComponents.at(i);
            // The components will be in the form of ID:Project Name:Component
            QString id = item.section(':', 0, 0);
            QString component = item.section(':', 2);
            componentQuery += QString("&project_id[]=%1&show_category[]=%2").arg(id,component);
        }

        queryType = QString("user_monitor[]=0&reporter_id[]=0&handler_id[]=0%1").arg(componentQuery);
    }
    else if (mViewType == REPORTED)
    {
        queryType = "user_monitor[]=0&reporter_id[]=-1&handler_id[]=0&project_id[]=0";
    }
    else if (mViewType == SEARCHED)
    {
        queryType = QString("user_monitor[]=0&reporter_id[]=0&handler_id[]=0&project_id[]=0&search=%1").arg(search);
    }
    else
    {
        queryType = "user_monitor[]=0&reporter_id[]=0&handler_id[]=-1&project_id[]=0";
    }
    QString url = mUrl + "/view_all_set.php?f=3";

    // Mantis's date range filter doesn't actually filter on modified times, so we'll
    // have to download all bugs open on each sync.
    QString query = QString("type=1&page_number=1&per_page=64000&view_type=simple&%1&hide_status[]=80&show_resolution[]=10&show_resolution[]=30")
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
Mantis::getCC()
{
    QString url = mUrl + "/csv_export.php";
    QNetworkRequest req = QNetworkRequest(QUrl(url));
    QNetworkReply *rep = pManager->get(req);
    connect(rep, SIGNAL(finished()),
            this, SLOT(ccResponse()));
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
Mantis::getSearched()
{
    qDebug() << "getSearched()";
    QString url = mUrl + "/csv_export.php";
    QNetworkRequest req = QNetworkRequest(QUrl(url));
    QNetworkReply *rep = pManager->get(req);
    connect(rep, SIGNAL(finished()),
            this, SLOT(searchResponse()));
}

void
Mantis::getSearchedBug(const QString &bugId)
{
    qDebug() << "getSearchedBug: " << bugId;
    int possibleBug = SqlUtilities::hasShadowBug("mantis", bugId, mId);
    if (possibleBug)
    {
        qDebug() << "Bug already exists...";
        QMap<QString, QString> bug = SqlUtilities::mantisBugDetail(QString::number(possibleBug));
        emit searchResultFinished(bug);
        return;
    }

    QtSoapHttpTransport *searchTransport = new QtSoapHttpTransport(this);
    bool secure = true;
    if (QUrl(mUrl).scheme() == "http")
        secure = false;

    searchTransport->setHost(QUrl(mUrl).host(), secure);
    connect(searchTransport, SIGNAL(responseReady()),
            this, SLOT(searchedBugResponse()));
    connect(searchTransport->networkAccessManager(), SIGNAL(sslErrors(QNetworkReply*,QList<QSslError>)),
            this, SLOT(handleSslErrors(QNetworkReply*,QList<QSslError>)));
    QtSoapMessage request;
    request.setMethod(QtSoapQName("mc_issue_get", "http://futureware.biz/mantisconnect"));
    request.addMethodArgument("username", "", mUsername );
    request.addMethodArgument("password", "", mPassword);
    request.addMethodArgument("issue_id", "", bugId.toInt());
    searchTransport->submitRequest(request, QUrl(mUrl).path() + "/api/soap/mantisconnect.php");
}

void
Mantis::handleCSV(const QString &csv, const QString &bugType)
{
    QVector<QString> bug;
    QString entry;
    QString tmpBugId;
    QString colEntry;

    int colId = -1,
        colProject = -1,
        colAssignedTo = -1,
        colProductVersion = -1,
        colPriority = -1,
        colOS = -1,
        colOSVersion = -1,
        colSeverity = -1,
        colCategory = -1,
        colReproducibility = -1,
        colLastModified = -1,
        colSummary = -1,
        colStatus = -1;
    QRegExp reg("^\"|\"$");
    QString translatedEntry = "";
    QRegExp removeLeadingZeros("^0+");
    QStringList list = csv.split("\n", QString::SkipEmptyParts);

    // The first line is the column descriptions, so parse them
    // and map them to what we want
    Translator t;
    t.openDatabase();
    qDebug() << list.at(0);
    bug = parseCSVLine(list.at(0));
    for (int i = 0; i < bug.size(); ++i)
    {
        colEntry = bug.at(i);
        colEntry = colEntry.remove(reg);
        translatedEntry = t.translate(colEntry);
        if (translatedEntry == "id")
            colId = i;
        if (translatedEntry == "product_version")
            colProductVersion = i;
        else if (translatedEntry == "project")
            colProject = i;
        else if (translatedEntry == "os")
            colOS = i;
        else if (translatedEntry == "os_version")
            colOSVersion =i;
        else if (translatedEntry == "category")
            colCategory = i;
        else if (translatedEntry == "assigned_to")
            colAssignedTo = i;
        else if (translatedEntry == "reproducibility")
            colReproducibility = i;
        else if (translatedEntry == "priority")
            colPriority = i;
        else if (translatedEntry == "severity")
            colSeverity = i;
        else if (translatedEntry == "updated")
            colLastModified = i;
        else if (translatedEntry == "summary")
            colSummary = i;
        else if (translatedEntry == "status")
            colStatus = i;
    }
    t.closeDatabase();

    if ((colProject == -1)
        || (colCategory == -1)
        || (colAssignedTo == -1)
        || (colPriority == -1)
        || (colSeverity == -1)
        || (colLastModified  == -1)
        || (colSummary == -1)
        || (colStatus == -1)
        || (colReproducibility == -1)
        || (colOSVersion == -1)
        || (colOS == -1)
        || (colProductVersion == -1))
    {
        qDebug() << "Missing a column somewhere...:";
        qDebug() << QString("%1, %2, %3, %4, %5, %6, %7, %8, %9")
                    .arg(colId)
                    .arg(colProject)
                    .arg(colAssignedTo)
                    .arg(colPriority)
                    .arg(colSeverity)
                    .arg(colCategory)
                    .arg(colLastModified)
                    .arg(colStatus)
                    .arg(colReproducibility);
        return;
    }

    for (int i = 1; i < list.size(); ++i)
    {
        bug = parseCSVLine(list.at(i));
        QVariantMap newBug;
        if (colId != -1)
            entry = bug.at(colId);
        else
            break;

        newBug["id"]  = entry.remove(removeLeadingZeros);
        tmpBugId = entry;

        if (colProject)
            entry = bug.at(colProject);
        else
            entry = "";
        newBug["project"] = entry.remove(reg);

        if (colProductVersion)
            entry = bug.at(colProductVersion);
        else
            entry = "";
        newBug["product_version"] = entry.remove(reg);

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

        if (colCategory)
            entry = bug.at(colCategory);
        else
            entry = "";
        newBug["category"] = entry.remove(reg);

        if (colLastModified)
            entry = bug.at(colLastModified);
        else
            entry = "1970-01-01";
        newBug["last_modified"] = entry.remove(reg);

        if (colReproducibility)
            entry = bug.at(colReproducibility);
        else
            entry = "";
        newBug["reproducibility"] = entry.remove(reg);

        qDebug() << "col OS";
        if (colOS)
            entry = bug.at(colOS);
        else
            entry = "";
        newBug["os"] = entry.remove(reg);

        qDebug() << "col os version";
        if (colOSVersion)
            entry = bug.at(colOSVersion);
        else
            entry = "";
        newBug["os_version"] = entry.remove(reg);

        qDebug() <<  "summary";
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
        mBugs[tmpBugId] = newBug;
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
        qDebug() << "SOAP fault: " << resp.faultString().toString();
        qDebug() << resp.faultDetail().toString();
        if (mVersion == "-1")
        {
            emit versionChecked("-1", resp.faultString().toString());
        }
        else
        {
            emit bugsUpdated();
            emit backendError(QString("%1: %2").arg(resp.faultString().toString()).arg(resp.faultDetail().toString()));
        }
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

        emit versionChecked(QString("%1").arg(version), "");
    }
    else if (messageName == "mc_enum_access_levelsResponse")
    {
        QtSoapMessage request;
        request.setMethod(QtSoapQName("mc_version", "http://futureware.biz/mantisconnect"));
        pMantis->submitRequest(request, QUrl(mUrl).path() + "/api/soap/mantisconnect.php");
    }
    else if (messageName == "mc_enum_prioritiesResponse")
    {
        QtSoapArray &array = (QtSoapArray &) resp.returnValue();
        QList<QMap<QString, QString> > fieldList;
        for (int i = 0; i < array.count(); ++i)
        {
            QMap<QString, QString> fieldMap;
            fieldMap["tracker_id"] = mId;
            fieldMap["field_name"] = "priority";
            fieldMap["value"] = array.at(i)["name"].toString();
            fieldList << fieldMap;
        }

        pSqlWriter->multiInsert("fields", fieldList);
        checkValidSeverities();
    }
    else if (messageName == "mc_enum_statusResponse")
    {
        QtSoapArray &array = (QtSoapArray &) resp.returnValue();
        QList<QMap<QString, QString> > fieldList;
        for (int i = 0; i < array.count(); ++i)
        {
            QMap<QString, QString> fieldMap;
            fieldMap["tracker_id"] = mId;
            fieldMap["field_name"] = "status";
            fieldMap["value"] = array.at(i)["name"].toString();
            fieldList << fieldMap;
        }

        pSqlWriter->multiInsert("fields", fieldList);
        emit fieldsFound();
    }
    else if (messageName == "mc_enum_severitiesResponse")
    {
        QtSoapArray &array = (QtSoapArray &) resp.returnValue();
        QList<QMap<QString, QString> > fieldList;
        for (int i = 0; i < array.count(); ++i)
        {
            QMap<QString, QString> fieldMap;
            fieldMap["tracker_id"] = mId;
            fieldMap["field_name"] = "severity";
            fieldMap["value"] = array.at(i)["name"].toString();
            fieldList << fieldMap;
        }

        pSqlWriter->multiInsert("fields", fieldList);
        checkValidResolutions();
    }
    else if (messageName == "mc_enum_resolutionsResponse")
    {
        QtSoapArray &array = (QtSoapArray &) resp.returnValue();
        QList<QMap<QString, QString> > fieldList;
        for (int i = 0; i < array.count(); ++i)
        {
            QMap<QString, QString> fieldMap;
            fieldMap["tracker_id"] = mId;
            fieldMap["field_name"] = "resolution";
            fieldMap["value"] = array.at(i)["name"].toString();
            fieldList << fieldMap;
        }

        pSqlWriter->multiInsert("fields", fieldList);
        checkValidReproducibilities();
    }
    else if (messageName == "mc_enum_reproducibilitiesResponse")
    {
        QtSoapArray &array = (QtSoapArray &) resp.returnValue();
        QList<QMap<QString, QString> > fieldList;
        for (int i = 0; i < array.count(); ++i)
        {
            QMap<QString, QString> fieldMap;
            fieldMap["tracker_id"] = mId;
            fieldMap["field_name"] = "reproducibility";
            fieldMap["value"] = array.at(i)["name"].toString();
            fieldList << fieldMap;
        }

        pSqlWriter->multiInsert("fields", fieldList);
        checkValidStatuses();
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
#if 0
            else if (column == "Category")
                uploadBug["category"] = newChange.value("to").toString().remove(QRegExp("<[^>]*>"));
#endif
            if (!changed.value("Severity").isNull())
            {
                QtSoapStruct *newSeverity = new QtSoapStruct(QtSoapQName("severity"));
                newSeverity->insert(new QtSoapSimpleType(QtSoapQName("name"), changed.value("Severity").toString()));
                s->insert(newSeverity);
            }
            else
                s->insert(new QtSoapStruct((QtSoapStruct&)issue["severity"]));

            if (!changed.value("Priority").isNull())
            {
                QtSoapStruct *newPriority = new QtSoapStruct(QtSoapQName("priority"));
                newPriority->insert(new QtSoapSimpleType(QtSoapQName("name"), changed.value("Priority").toString()));
                s->insert(newPriority);
            }
            else
                s->insert(new QtSoapStruct((QtSoapStruct&)issue["priority"]));

            if (!changed.value("Status").isNull())
            {
                QtSoapStruct *newStatus = new QtSoapStruct(QtSoapQName("status"));
                newStatus->insert(new QtSoapSimpleType(QtSoapQName("name"), changed.value("Status").toString()));
                s->insert(newStatus);
            }
            else
                s->insert(new QtSoapStruct((QtSoapStruct&)issue["status"]));

            if (!changed.value("Assigned To").isNull())
            {
                QtSoapStruct *newStatus = new QtSoapStruct(QtSoapQName("handler"));
                newStatus->insert(new QtSoapSimpleType(QtSoapQName("name"), changed.value("Assigned To").toString()));
                s->insert(newStatus);
            }
            else
            {
                if (issue["handler"].isValid())
                {
                    s->insert(new QtSoapStruct((QtSoapStruct&)issue["handler"]));
                }
                else
                {
                    QtSoapStruct *newStatus = new QtSoapStruct(QtSoapQName("handler"));
                    newStatus->insert(new QtSoapSimpleType(QtSoapQName("name"), ""));
                    s->insert(newStatus);
                }

            }
            if (!changed.value("Reproducibility").isNull())
            {
                QtSoapStruct *newStatus = new QtSoapStruct(QtSoapQName("reproducibility"));
                newStatus->insert(new QtSoapSimpleType(QtSoapQName("name"), changed.value("Reproducibility").toString()));
                s->insert(newStatus);
            }
            else
                s->insert(new QtSoapStruct((QtSoapStruct&)issue["reproducibility"]));

            if (!changed.value("Summary").isNull())
            {
                s->insert(new QtSoapSimpleType(changed.value("Summary").toString()));
            }
            else
                s->insert(new QtSoapSimpleType((QtSoapSimpleType &)issue["summary"]));

            if (!changed.value("Category").isNull())
            {
                s->insert(new QtSoapSimpleType(changed.value("Category").toString()));
            }
            else
                s->insert(new QtSoapSimpleType((QtSoapSimpleType &)issue["category"]));

            if (!changed.value("Resolution").isNull())
            {
                QtSoapStruct *newStatus = new QtSoapStruct(QtSoapQName("resolution"));
                newStatus->insert(new QtSoapSimpleType(QtSoapQName("name"), changed.value("Resolution").toString()));
                s->insert(newStatus);
            }
            else
                s->insert(new QtSoapStruct((QtSoapStruct&)issue["resolution"]));

            s->insert(new QtSoapStruct((QtSoapStruct &)issue["projection"]));
            s->insert(new QtSoapStruct((QtSoapStruct &)issue["eta"]));
            s->insert(new QtSoapStruct((QtSoapStruct &)issue["view_state"]));
            s->insert(new QtSoapStruct((QtSoapStruct &)issue["reporter"]));
            s->insert(new QtSoapSimpleType((QtSoapSimpleType &)issue["description"]));
            s->insert(new QtSoapSimpleType((QtSoapSimpleType &)issue["additional_information"]));
            s->insert(new QtSoapSimpleType((QtSoapSimpleType &)issue["steps_to_reproduce"]));
            s->insert(new QtSoapSimpleType((QtSoapSimpleType &)issue["build"]));
            s->insert(new QtSoapSimpleType((QtSoapSimpleType &)issue["platform"]));
            s->insert(new QtSoapSimpleType((QtSoapSimpleType &)issue["os"]));
            s->insert(new QtSoapSimpleType((QtSoapSimpleType &)issue["os_build"]));
            s->insert(new QtSoapSimpleType((QtSoapSimpleType &)issue["sponsorship_total"]));
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

            // First get the bug description and store it separately
            QMap<QString, QString> val;
            QMap<QString, QString> params;
            val["description"] = resp.returnValue()["description"].toString();
            params["tracker_id"] = mId;
            params["bug_id"] = bugId;
            SqlUtilities::simpleUpdate("mantis", val, params);

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
        sql.exec(QString("DELETE FROM shadow_mantis WHERE bug_id=\'%1\' AND tracker_id=%2").arg(mCurrentUploadId).arg(mId));
        getNextUpload();
    }
    else if (messageName == "mc_issue_note_addResponse")
    {
        QVariantMap map = mCommentUploadList.takeAt(0).toMap();
        QSqlQuery sql;
        sql.exec(QString("DELETE FROM shadow_comments WHERE bug_id=\'%1\' AND tracker_id=%2").arg(map.value("bug_id").toString()).arg(mId));
        getNextCommentUpload();
    }
    else if (messageName == "mc_projects_get_user_accessibleResponse")
    {
        QtSoapArray &array = (QtSoapArray &) resp.returnValue();
        QString name, id;

        for (int i = 0; i < array.count(); ++i)
        {
            name = array.at(i)["name"].toString();
            id = array.at(i)["id"].toString();
            mProjectList  << QString("%1:%2").arg(id).arg(name);
        }
        getCategories();
    }
    else if (messageName == "mc_project_get_categoriesResponse")
    {
        QtSoapArray &array = (QtSoapArray &) resp.returnValue();
        QString currentProject = mProjectList.takeAt(0);
        QString id = currentProject.section(':', 0,0);
        QString name = currentProject.section(':', 1);
        QString item = "";
        qDebug() << "Current project:" << currentProject;
        qDebug() << "Project: " << name;
        for (int i = 0; i < array.count(); ++i)
        {
            if (array.at(i).isValid())
            {
                item = array.at(i).toString();
                if (!item.isEmpty())
                    mCategoriesList  << QString("%1:%2:%3")
                                        .arg(id)
                                        .arg(name)
                                        .arg(item);
            }
        }
        getCategories();
    }
    else
    {
        qDebug() << "Invalid response: " << messageName;
        emit backendError("Something went wrong!");
    }
}

void
Mantis::searchedBugResponse()
{
    QtSoapHttpTransport *transport = qobject_cast<QtSoapHttpTransport*>(sender());
    const QtSoapMessage &resp = transport->getResponse();
    qDebug() << "searchedBugResponse";
    if (resp.isFault())
    {
        qDebug() << "SOAP fault: " << resp.faultString().toString();
        qDebug() << resp.faultDetail().toString();
        transport->deleteLater();
        emit bugsUpdated();
        emit backendError(QString("%1: %2").arg(resp.faultString().toString()).arg(resp.faultDetail().toString()));
        return;
    }

    const QtSoapType &message = resp.method();
    const QtSoapType &response = resp.returnValue();
    QString messageName = message.name().name();

    if (messageName == "mc_issue_getResponse")
    {
        QList<QMap<QString, QString> > list;
        QMap<QString, QString> params;
        params["description"] = response["description"].toString();
        params["tracker_id"] = mId;
        params["bug_id"] = response["id"].toString();;
        params["severity"] = response["severity"]["name"].toString();
        params["priority"] = response["priority"]["name"].toString();
        params["project"] = response["project"]["name"].toString();
        params["category"] = response["category"].toString();
        params["reproducibility"] = response["reproducibility"]["name"].toString();
        params["os"] = response["os"].toString();
        params["os_version"] = response["os_build"].toString();
        params["assigned_to"] = response["handler"]["name"].toString();
        params["status"] = response["status"]["name"].toString();
        params["summary"] = response["summary"].toString();
        params["product_version"] = response["product_version"].toString();
        params["bug_type"] = "SearchedTemp";
        params["highlight_type"] = QString::number(SqlUtilities::HIGHLIGHT_SEARCH);
        params["last_modified"] = response["last_updated"].toString();
        list << params;
        pSqlWriter->multiInsert("mantis", list);
        emit searchResultFinished(params);
    }
    transport->deleteLater();
}

void
Mantis::checkVersion()
{
    QUrl url(mUrl + "/api/soap/mantisconnect.php");
    qDebug() << "Checking Mantis version at " << url;

    QNetworkRequest request = QNetworkRequest(url);
    QNetworkReply *reply = pManager->head(request);
    connect(reply, SIGNAL(finished()),
            this, SLOT(headFinished()));
}

void
Mantis::headFinished()
{
    QNetworkReply *reply = qobject_cast<QNetworkReply*>(sender());
    if (reply->error())
    {
        qDebug() << "Mantis::headFinished: " << reply->errorString();
        qDebug() << "Mantis: Error value: " << reply->error();
        if (reply->error() == QNetworkReply::AuthenticationRequiredError)
        {
            emit versionChecked("-1", "Invalid username or password.");
        }
        else if (reply->error() == QNetworkReply::HostNotFoundError)
        {
            emit versionChecked("-1", "Host not found");
        }
        else if (reply->error() == QNetworkReply::ConnectionRefusedError)
        {
            emit versionChecked("-1", "Connection refused");
        }
        else
        {
            emit versionChecked("-1", "Not a Mantis instance!");
        }

        reply->deleteLater();
        return;
    }

    // Mantis doesn't have a "login" call, really (the php doesn't return an HTTP error on failure)
    // so this is a stupid hack to test if the username and password are correct during version detection
    QtSoapMessage request;
    request.setMethod(QtSoapQName("mc_enum_access_levels", "http://futureware.biz/mantisconnect"));
    request.addMethodArgument("username", "", mUsername);
    request.addMethodArgument("password", "", mPassword);
    pMantis->submitRequest(request, QUrl(mUrl).path() + "/api/soap/mantisconnect.php");

}

void
Mantis::checkFields()
{
    checkValidPriorities();
}

void
Mantis::validFieldCall(const QString &request)
{
    qDebug() << "Calling " << request;
    QtSoapMessage msg;
    msg.setMethod(QtSoapQName(request, "http://futureware.biz/mantisconnect"));
    msg.addMethodArgument("username", "", mUsername);
    msg.addMethodArgument("password", "", mPassword);
    pMantis->submitRequest(msg, QUrl(mUrl).path() + "/api/soap/mantisconnect.php");
}

void
Mantis::checkValidPriorities()
{
    validFieldCall("mc_enum_priorities");
}

void
Mantis::checkValidSeverities()
{
    validFieldCall("mc_enum_severities");
}

void
Mantis::checkValidComponents()
{
    validFieldCall("mc_projects_get_user_accessible");
}

void
Mantis::checkValidResolutions()
{
    validFieldCall("mc_enum_resolutions");
}

void
Mantis::checkValidReproducibilities()
{
    validFieldCall("mc_enum_reproducibilities");
}

void
Mantis::getCategories()
{
    if (mProjectList.isEmpty())
    {
        qDebug() << "Finished with the project list: " << mCategoriesList;
        QList<QMap<QString, QString> > fieldList;
        for (int i = 0; i < mCategoriesList.count(); ++i)
        {
            QMap<QString, QString> fieldMap;
            fieldMap["tracker_id"] = mId;
            fieldMap["field_name"] = "component";
            fieldMap["value"] = mCategoriesList.at(i);
            fieldList << fieldMap;
        }

        SqlUtilities::removeFieldValues(mId, "component");
        pSqlWriter->multiInsert("fields", fieldList, SqlUtilities::MULTI_INSERT_COMPONENTS);
        return;
    }

    QString project = mProjectList.at(0);
    QString id = project.section(':', 0,0);
    qDebug() << "Getting mantis categories for " << id;
    QtSoapMessage request;
    request.setMethod(QtSoapQName("mc_project_get_categories", "http://futureware.biz/mantisconnect"));
    request.addMethodArgument("username", "", mUsername );
    request.addMethodArgument("password", "", mPassword);
    request.addMethodArgument("project_id","", id.toInt());
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
    if (!SqlUtilities::hasPendingChanges("shadow_mantis", mId))
    {
        qDebug() << "No pending changes";
        emit bugsUpdated();
        return;
    }

    mUploadList.clear();
    mCommentUploadList.clear();
    mUploadingBugs = true;
    int i = 0;
    int j = 0;
    QVariantList changeList = SqlUtilities::getMantisChangelog();
    QList< QMap<QString, QString> > commentList = SqlUtilities::getCommentsChangelog();

    for (i = 0; i < commentList.size(); ++i)
    {
        QMap<QString, QString> comment = commentList.at(i);
        if (comment["tracker_name"] != mName)
            continue;

        QVariantMap map;
        map["bug_id"] = comment["bug_id"];
        map["comment"] = comment["comment"];
        map["private"] = comment["private"];
        map["id"] = comment["id"];
        mCommentUploadList << map;
    }

    for (i = 0; i < changeList.size(); ++i)
    {
        QVariantList changes = changeList.at(i).toList();
        QVariantMap uploadBug;
        for (j = 0; j < changes.size(); ++j)
        {
            QVariantMap newChange = changes.at(j).toMap();
            if (newChange.value("tracker_name").toString() != mName)
                break;

            uploadBug["bug_id"] = newChange.value("bug_id").toString();
            QString column = newChange.value("column_name").toString();
            uploadBug[column] = newChange.value("to").toString().remove(QRegExp("<[^>]*>"));
        }

        if (uploadBug.size() > 0)
            mUploadList[uploadBug.value("bug_id").toString()] = uploadBug;
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
}


void Mantis::loginSyncResponse()
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
    else if (mViewType == CC)
        getCC();
    else if (mViewType == MONITORED)
        getMonitored();
    else if (mViewType == SEARCHED)
        getSearched();
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
    QString rep = QString::fromUtf8(reply->readAll());
    reply->deleteLater();
    handleCSV(QString(rep), "Assigned");

    QList< QMap<QString,QString> > insertList;
    QVariantMap responseMap;
    QMapIterator<QString, QVariant> i(mBugs);
    while (i.hasNext())
    {
        i.next();
        responseMap = i.value().toMap();
        QMap<QString, QString> newBug;
        newBug["tracker_id"] = mId;
        newBug["bug_id"] = responseMap.value("id").toString();
        newBug["severity"] = responseMap.value("severity").toString();
        newBug["priority"] = responseMap.value("priority").toString();
        newBug["project"] = responseMap.value("project").toString();
        newBug["category"] = responseMap.value("category").toString();
        newBug["reproducibility"] = responseMap.value("reproducibility").toString();
        newBug["os"] = responseMap.value("os").toString();
        newBug["os_version"] = responseMap.value("os_version").toString();
        newBug["assigned_to"] = responseMap.value("assigned_to").toString();
        newBug["status"] = responseMap.value("status").toString();
        newBug["summary"] = responseMap.value("summary").toString();
        newBug["product_version"] = responseMap.value("product_version").toString();
        newBug["bug_type"] = responseMap.value("bug_type").toString();
        newBug["last_modified"] = responseMap.value("last_modified").toString();
        qDebug() << "Last modified is now: " << newBug["last_modified"];

        insertList << newBug;
    }

    pSqlWriter->insertBugs("mantis", insertList, mId);
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

    QString rep = QString::fromUtf8(reply->readAll());
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

    QString rep = QString::fromUtf8(reply->readAll());
    reply->deleteLater();
    handleCSV(rep, "Monitored");
    mViewType = CC;
    setView();
}
void Mantis::ccResponse()
{
    QNetworkReply *reply = qobject_cast<QNetworkReply*>(sender());
    if (reply->error())
    {
        qDebug() << "ccResponse: " << reply->errorString();
        emit backendError(reply->errorString());
        reply->close();

        return;
    }

    QString rep = QString::fromUtf8(reply->readAll());
    reply->deleteLater();
    handleCSV(rep, "CC");
    mViewType = REPORTED;
    setView();
}

void
Mantis::searchResponse()
{
    QNetworkReply *reply = qobject_cast<QNetworkReply*>(sender());
    if (reply->error())
    {
        emit backendError(reply->errorString());
        reply->close();
        return;
    }

    QString rep = QString::fromUtf8(reply->readAll());
    reply->deleteLater();
    handleCSV(rep, "Searched");

    QList< QMap<QString,QString> > insertList;
    QVariantMap responseMap;
    QMapIterator<QString, QVariant> i(mBugs);
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

    pSqlWriter->multiInsert("search_results", insertList, SqlUtilities::MULTI_INSERT_SEARCH);
}

void
Mantis::multiInsertSuccess(int operation)
{
    if (operation == SqlUtilities::MULTI_INSERT_SEARCH)
        emit searchFinished();
    else if (operation == SqlUtilities::MULTI_INSERT_COMPONENTS)
        emit fieldsFound();
}

void
Mantis::bugsInsertionFinished(QStringList idList, int operation)
{
    Q_UNUSED(idList);
    qDebug() << "Mantis: bugs updated";
    if (operation != SqlUtilities::BUGS_INSERT_SEARCH)
    {
        updateSync();
        emit bugsUpdated();
    }
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
    Q_UNUSED(errors);
    reply->ignoreSslErrors();
}
