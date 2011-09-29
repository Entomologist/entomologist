/*
 *  Copyright (c) 2011 SUSE Linux Products GmbH
 *  All Rights Reserved.
 *
 *  This file is part of Entomologist.
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
 *  You should have received a copy of the GNU General Public License version 2
 *  along with Foobar.  If not, see <http://www.gnu.org/licenses/>
 *
 *  Author: Matt Barringer <mbarringer@suse.de>
 *
 */

#include "SqlUtilities.h"
#include "ErrorHandler.h"

#include <QSqlQuery>
#include <QStringList>
#include <QSqlError>
#include <QVariant>
#include <QDebug>
#include <QSqlDatabase>
#include <QRegExp>

SqlUtilities::SqlUtilities()
{
    mDatabase = QSqlDatabase::database();
}

SqlUtilities::~SqlUtilities()
{
    mDatabase.close();
}

void
SqlUtilities::openDb(const QString &dbPath)
{
    QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE");
    if (!db.isValid())
    {
        qDebug() << "Couldn't create the database connection";
        ErrorHandler::handleError("Couldn't create the database.", "");
        exit(1);
    }
    db.setDatabaseName(dbPath);

    if (!db.open())
    {
        qDebug() << "Error in openDb(): " << db.lastError().text();
        ErrorHandler::handleError("Error creating database.", db.lastError().text());
        exit(1);
    }
}

void
SqlUtilities::closeDb()
{
    QSqlDatabase db = QSqlDatabase::database();
    db.close();
}


void
SqlUtilities::multiInsert(const QString &tableName,
                         QList< QMap<QString, QString> > list)
{
    qDebug() << "SqlUtilities::multiInsert";
    qDebug() << "multiInsert: " << list;
    if (list.size() == 0)
    {
        emit success();
        return;
    }

    QSqlQuery q(mDatabase);
    QStringList keys = list.at(0).keys();
    QStringList placeholder;
    bool error = false;
    // TODO find a more clever way to do this
    for (int i = 0; i < keys.size(); ++i)
        placeholder << "?";

    QString query = QString("INSERT INTO %1 (%2) VALUES (%3)")
                    .arg(tableName)
                    .arg(keys.join(","))
                    .arg(placeholder.join(","));

    if (!q.prepare(query))
    {
        qDebug() << "Could not prepare " << query << " :" << q.lastError().text();
        emit failure(q.lastError().text());
        return;
    }

    mDatabase.transaction();
    for (int a = 0; a < list.size(); ++a)
    {
        QMap<QString, QString> data = list.at(a);
        for (int i = 0; i < keys.size(); ++i)
        {
            q.bindValue(i, data.value(keys.at(i)));
        }
        if (!q.exec())
        {
            qDebug() << "multiRowInsert failed: " << q.lastError().text();
            qDebug() << q.lastQuery();
            emit failure(q.lastError().text());
            error = true;
            break;
        }
    }

    if (!error)
    {
        mDatabase.commit();
        emit success();
    }
    else
    {
        mDatabase.rollback();
    }

    return;
}


void
SqlUtilities::insertBugs(const QString &tableName,
                         QList< QMap<QString, QString> > list)
{
    QStringList idList;
    if (list.size() == 0)
    {
        emit bugsFinished(idList);
        return;
    }
    QSqlQuery q(mDatabase), bugQuery(mDatabase), commentQuery(mDatabase);
    QStringList keys = list.at(0).keys();
    QStringList placeholder;
    bool error = false;
    // TODO find a more clever way to do this
    for (int i = 0; i < keys.size(); ++i)
        placeholder << "?";

    QString query = QString("INSERT INTO %1 (%2) VALUES (%3)")
                    .arg(tableName)
                    .arg(keys.join(","))
                    .arg(placeholder.join(","));
    QString bugDeleteSql = QString("DELETE FROM %1 WHERE bug_id=:bug_id AND tracker_id=:tracker_id").arg(tableName);
    QString commentDeleteSql = "DELETE FROM comments WHERE bug_id=:bug_id AND tracker_id=:tracker_id";

    if (!q.prepare(query))
    {
        qDebug() << "Could not prepare " << query << " :" << q.lastError().text();
        emit failure(q.lastError().text());
        return;
    }

    if (!bugQuery.prepare(bugDeleteSql))
    {
        qDebug() << "Could not prepare " << bugDeleteSql << " :" << bugQuery.lastError().text();
        emit failure(bugQuery.lastError().text());
        return;
    }

    if (!commentQuery.prepare(commentDeleteSql))
    {
        qDebug() << "Could not prepare " << commentDeleteSql << " :" << commentQuery.lastError().text();
        emit failure(commentQuery.lastError().text());
        return;
    }

    mDatabase.transaction();
    for (int a = 0; a < list.size(); ++a)
    {
        QMap<QString, QString> data = list.at(a);
        bugQuery.bindValue(":bug_id", data["bug_id"]);
        bugQuery.bindValue(":tracker_id", data["tracker_id"]);
        commentQuery.bindValue(":bug_id", data["bug_id"]);
        commentQuery.bindValue(":tracker_id", data["tracker_id"]);

        if (!bugQuery.exec())
        {
            qDebug() << "multiRowInsert bugQuery failed: " << q.lastError().text();
            emit failure(q.lastError().text());
            error = true;
            break;
        }

        if (!commentQuery.exec())
        {
            qDebug() << "multiRowInsert commentQuery failed: " << q.lastError().text();
            emit failure(q.lastError().text());
            error = true;
            break;
        }

        for (int i = 0; i < keys.size(); ++i)
        {
//            qDebug() << "Binding " << i << keys.at(i);
            q.bindValue(i, data.value(keys.at(i)));
        }
        if (!q.exec())
        {
            qDebug() << "multiRowInsert bugs failed: " << q.lastError().text();
            qDebug() << q.lastQuery();
            emit failure(q.lastError().text());
            error = true;
            break;
        }

        idList << data["bug_id"];
    }

    if (!error)
    {
        mDatabase.commit();
        emit bugsFinished(idList);
    }
    else
    {
        mDatabase.rollback();
    }

    return;
}

int
SqlUtilities::simpleInsert(const QString &tableName,
                           QMap<QString, QString> data)
{
    QSqlQuery q;
    QStringList keys = data.keys();
    QStringList placeholder;

    // TODO find a more clever way to do this
    for (int i = 0; i < keys.size(); ++i)
        placeholder << "?";

    QString query = QString("INSERT INTO %1 (%2) VALUES (%3)")
                    .arg(tableName)
                    .arg(keys.join(","))
                    .arg(placeholder.join(","));
    q.prepare(query);
    for (int i = 0; i < keys.size(); ++i)
        q.bindValue(i,data.value(keys.at(i)));

    if (!q.exec())
    {
        qDebug() << "simpleInsert failed: " << q.lastError().text();
        ErrorHandler::handleError("Error inserting data into " + tableName, q.lastError().text());
        return (-1);
    }

    return(q.lastInsertId().toInt());
}

bool
SqlUtilities::simpleUpdate(const QString &tableName, QMap<QString, QString> update, QMap<QString, QString> where)
{
    QSqlQuery q;
    QStringList updateList;
    QStringList whereList;

    QMapIterator<QString, QString> i(update);
    while (i.hasNext())
    {
        i.next();
        QString u = QString("%1=:%1").arg(i.key());
        updateList << u;
    }

    QMapIterator<QString, QString> j(where);
    while (j.hasNext())
    {
        j.next();
        QString u = QString("%1=:%1").arg(j.key());
        whereList << u;
    }

    QString query = QString("UPDATE %1 SET %2 WHERE %3")
                     .arg(tableName)
                     .arg(updateList.join(","))
                     .arg(whereList.join(" AND "));
    qDebug() << query;
    if(!q.prepare(query))
    {
        qDebug() << "Could not prepare simpleUpdate: " << q.lastError().text();
        return false;
    }

    i.toFront();
    while (i.hasNext())
    {
        i.next();
        QString u = QString(":%1").arg(i.key());
        qDebug() << "Binding " << u << " to " << i.value();
        q.bindValue(u, i.value());
    }

    j.toFront();
    while (j.hasNext())
    {
        j.next();
        QString u = QString(":%1").arg(j.key());
        qDebug() << "Binding " << u << " to " << j.value();
        q.bindValue(u, j.value());
    }

    if (!q.exec())
    {
        qDebug() << "Could not execute simpleUpdate: " << q.lastError().text();
        qDebug() << q.lastQuery();
        return false;
    }
        qDebug() << q.lastQuery();

    return true;
}

QStringList
SqlUtilities::fieldValues(const QString &tracker_id, const QString &fieldName)
{
    QStringList ret;
    QSqlQuery q;
    q.prepare("SELECT value FROM fields WHERE tracker_id = :tracker AND field_name = :name");
    q.bindValue(":tracker", tracker_id);
    q.bindValue(":name", fieldName);
    if (!q.exec())
    {
        qDebug() << "fieldValues error: " << q.lastError().text();
        return(ret);
    }

    while (q.next())
    {
        ret << q.value(0).toString();
    }

    return(ret);
}

int
SqlUtilities::dbVersion()
{
    int ret = 0;

    QSqlQuery query;
    query.exec("SELECT db_version FROM entomologist");
    query.next();
    ret = query.value(0).toInt();
    return(ret);
}

bool
SqlUtilities::hasPendingChanges()
{
    QSqlQuery q;
    q.exec("SELECT COUNT(id) FROM shadow_bugs");
    q.next();
    if (q.value(0).toInt() > 0)
        return true;

    q.exec("SELECT COUNT(id) FROM shadow_comments");
    q.next();
    if (q.value(0).toInt() > 0)
        return true;

    return false;
}

// sqlite starts autoincrementing primary keys at 1,
// so it's safe to use this function as both a boolean check and an
// ID lookup
int
SqlUtilities::trackerNameExists(const QString &name)
{
    int ret = 0;
    QSqlQuery q;
    q.prepare("SELECT id FROM trackers WHERE name = :name");
    q.bindValue(":name", name);
    if (q.exec())
    {
        q.next();
        ret = q.value(0).toInt();
    }
    return ret;
}

int
SqlUtilities::hasShadowBug(const QString &tableName,
                           const QString &bugId,
                           const QString &trackerId)
{
    QString sql = QString("SELECT id FROM %1 WHERE bug_id = \'%2\' AND tracker_id=\'%3\'")
                  .arg(tableName)
                  .arg(bugId)
                  .arg(trackerId);
    QSqlQuery q;
    q.exec(sql);
    q.next();
    int val = q.value(0).toInt();
    qDebug() << "Val: " << val;
    return val;
}

QList< QMap<QString, QString> >
SqlUtilities::loadTrackers()
{
    QSqlQuery query;

    QList< QMap<QString, QString> > trackerList;
    query.exec("SELECT type, name, url, username, password,"
               "version, auto_cache_comments, monitored_components,"
               "last_sync FROM trackers");
    while (query.next())
    {
        QMap<QString, QString> tracker;
        tracker["type"] = query.value(0).toString();
        tracker["name"] = query.value(1).toString();
        tracker["url"] = query.value(2).toString();
        tracker["username"] = query.value(3).toString();
        tracker["password"] = query.value(4).toString();
        tracker["version"] = query.value(5).toString();
        tracker["auto_cache_comments"] = query.value(6).toString();
        tracker["monitored_components"] = query.value(7).toString();
        tracker["last_sync"] = query.value(8).toString();
        trackerList << tracker;
    }
    return trackerList;
}

void
SqlUtilities::createTables(int dbVersion)
{
    QString createMetaSql = "CREATE TABLE entomologist(db_version INT)";
    QString createTrackerSql = "CREATE TABLE trackers(id INTEGER PRIMARY KEY,"
                                                      "type TEXT,"
                                                      "name TEXT,"
                                                      "url TEXT,"
                                                      "username TEXT,"
                                                      "password TEXT,"
                                                      "last_sync TEXT,"
                                                      "version TEXT,"
                                                      "monitored_components TEXT,"
                                                      "auto_cache_comments INTEGER)";

    QString createFieldsSql = "CREATE TABLE fields (id INTEGER PRIMARY KEY,"
                                                   "tracker_id INTEGER,"
                                                   "field_name TEXT,"
                                                   "value TEXT)";
    QString createTodoListSql = "CREATE TABLE todolist (id INTEGER PRIMARY KEY,"
                                                       "name TEXT,"
                                                       "rtm_listid TEXT,"
                                                        "google_listid TEXT,"
                                                       "sync_services TEXT)";

    QString createServicesSql = "CREATE TABLE services (id INTEGER PRIMARY KEY,"
                                                        "servicetype TEXT,"
                                                        "name TEXT,"
                                                        "username TEXT,"
                                                        "password TEXT,"
                                                        "url TEXT,"
                                                        "auth_key TEXT,"
                                                        "refresh_token TEXT)";

    QString createTodoListBugsSql = "CREATE TABLE todolistbugs (id INTEGER PRIMARY KEY,"
                                                     "tracker_id INTEGER,"
                                                     "tracker_table TEXT,"
                                                     "bug_id INTEGER,"
                                                     "todolist_id INTEGER,"
                                                     "date TEXT,"
                                                     "completed INTEGER,"
                                                     "item_ids TEXT,"
                                                     "last_modified TEXT)";

    QString createServiceTasksSql = "CREATE TABLE service_tasks (id INTEGER PRIMARY KEY,"
                                                       "task_id TEXT,"
                                                       "service_name TEXT,"
                                                       "item_id TEXT,"
                                                       "misc_id TEXT)";

    QString createSearchSql = "CREATE TABLE search_results (id INTEGER PRIMARY KEY,"
                                                            "tracker_name TEXT,"
                                                            "bug_id TEXT,"
                                                            "summary TEXT)";

    QString createBugzillaSql = "CREATE TABLE %1 (id INTEGER PRIMARY KEY,"
                                              "highlight_type INTEGER,"
                                              "tracker_id INTEGER,"
                                              "bug_id INTEGER,"
                                              "severity TEXT,"
                                              "priority TEXT,"
                                              "assigned_to TEXT,"
                                              "status TEXT,"
                                              "summary TEXT,"
                                              "description TEXT,"
                                              "component TEXT,"
                                              "product TEXT,"
                                              "bug_type TEXT,"
                                              "bug_state TEXT,"
                                              "last_modified TEXT)";

    QString createTracSql = "CREATE TABLE %1 (id INTEGER PRIMARY KEY,"
                                              "highlight_type INTEGER,"
                                              "tracker_id INTEGER,"
                                              "bug_id INTEGER,"
                                              "severity TEXT,"
                                              "priority TEXT,"
                                              "assigned_to TEXT,"
                                              "status TEXT,"
                                              "summary TEXT,"
                                              "component TEXT,"
                                              "milestone TEXT,"
                                              "version TEXT,"
                                              "resolution TEXT,"
                                              "bug_type TEXT,"
                                              "bug_state TEXT,"
                                              "description TEXT,"
                                              "last_modified TEXT)";

    QString createMantisSql = "CREATE TABLE %1 (id INTEGER PRIMARY KEY,"
                                              "highlight_type INTEGER,"
                                              "tracker_id INTEGER,"
                                              "bug_id INTEGER,"
                                              "category TEXT,"
                                              "project TEXT, "
                                              "product_version TEXT,"
                                              "severity TEXT,"
                                              "priority TEXT,"
                                              "assigned_to TEXT,"
                                              "status TEXT,"
                                              "reproducibility TEXT,"
                                              "summary TEXT,"
                                              "description TEXT,"
                                              "os TEXT,"
                                              "os_version TEXT,"
                                              "bug_type TEXT,"
                                              "bug_state TEXT,"
                                              "last_modified TEXT)";

    QString createCommentsSql = "CREATE TABLE %1 (id INTEGER PRIMARY KEY,"
                               "tracker_id INTEGER,"
                               "bug_id INTEGER,"
                               "comment_id INTEGER,"
                               "author TEXT,"
                               "comment TEXT,"
                               "timestamp TEXT,"
                               "private INTEGER)";

    qDebug() << "Creating tables";
    // Create database
    QSqlDatabase db = QSqlDatabase::database();
    directExec(db, createTrackerSql);
    directExec(db, createFieldsSql);
    directExec(db, createSearchSql);
    directExec(db, createMetaSql);
    directExec(db, createTodoListSql);
    directExec(db, createServicesSql);
    directExec(db, createTodoListBugsSql);
    directExec(db, createServiceTasksSql);
    directExec(db, QString(createBugzillaSql).arg("bugzilla"));
    directExec(db, QString(createBugzillaSql).arg("shadow_bugzilla"));
    directExec(db, QString(createTracSql).arg("trac"));
    directExec(db, QString(createTracSql).arg("shadow_trac"));
    directExec(db, QString(createMantisSql).arg("mantis"));
    directExec(db, QString(createMantisSql).arg("shadow_mantis"));
    directExec(db, QString(createCommentsSql).arg("comments"));
    directExec(db, QString(createCommentsSql).arg("shadow_comments"));
    directExec(db, QString("INSERT INTO entomologist (db_version) VALUES (%1)").arg(dbVersion));
}

void
SqlUtilities::clearSearch()
{
    QSqlDatabase db = QSqlDatabase::database();
    directExec(db, "DELETE FROM search_results");
}

void
SqlUtilities::clearRecentBugs(const QString &tableName)
{
    QSqlDatabase db = QSqlDatabase::database();
    QString sql = QString("UPDATE %1 SET highlight_type = 0 WHERE highlight_type = %2")
                  .arg(tableName)
                  .arg(QString::number(HIGHLIGHT_RECENT));
    qDebug() << "clearRecentBugs: " << sql;
    directExec(db, sql);
}

void
SqlUtilities::directExec(QSqlDatabase db, const QString &sql)
{
    QSqlQuery query(db);
    if (!query.exec(sql))
    {
        qDebug() << "Database error: " << db.lastError().text();
        qDebug() << "Failed SQL: " << sql;
        ErrorHandler::handleError("Couldn't execute SQL.", db.lastError().text());
        exit(1);
    }
}

void
SqlUtilities::saveCredentials(int id,
                           const QString &username,
                           const QString &password)
{
    QSqlQuery q;
    q.prepare("UPDATE trackers SET username=:username, password=:password WHERE id=:id");
    q.bindValue(":id", id);
    q.bindValue(":username", username);
    q.bindValue(":password", password);
    if (!q.exec())
    {
        qDebug() << "Couldn't update credentials: " << q.lastError().text();
        emit failure(q.lastError().text());
    }
}
void
SqlUtilities::insertBugComments(QList<QMap<QString, QString> >commentList)
{
    if (commentList.size() == 0)
    {
        emit commentFinished();
        return;
    }

    QSqlQuery q;
    QString sql = "DELETE FROM comments WHERE bug_id=:bug_id AND tracker_id=:tracker_id";
    q.prepare(sql);
    q.bindValue(":bug_id", commentList.at(0).value("bug_id"));
    q.bindValue(":tracker_id", commentList.at(0).value("tracker_id"));
    q.exec();

    sql = "INSERT INTO comments (tracker_id, bug_id, comment_id, author, comment, timestamp, private)"
                  " VALUES (:tracker_id, :bug_id, :comment_id, :author, :comment, :timestamp, :private)";
    q.prepare(sql);
    for (int i = 0; i < commentList.size(); ++i)
    {
        QMap<QString,QString> parameterMap = commentList.at(i);
        q.bindValue(":tracker_id", parameterMap["tracker_id"]);
        q.bindValue(":bug_id", parameterMap["bug_id"]);
        q.bindValue(":comment_id", parameterMap["comment_id"]);
        q.bindValue(":author", parameterMap["author"]);
        q.bindValue(":comment", parameterMap["comment"]);
        q.bindValue(":timestamp", parameterMap["timestamp"]);
        q.bindValue(":private", parameterMap["private"].toInt());
        if (!q.exec())
        {
            emit failure(q.lastError().text());
            break;
        }
    }
    emit commentFinished();

}

void
SqlUtilities::insertComments(QList<QMap<QString, QString> > commentList)
{
    qDebug() << "Going to write " << commentList.count() << " comments";

    QSqlQuery q;
    QString sql = "INSERT INTO comments (tracker_id, bug_id, comment_id, author, comment, timestamp, private)"
                  " VALUES (:tracker_id, :bug_id, :comment_id, :author, :comment, :timestamp, :private)";
    q.prepare(sql);
    for (int i = 0; i < commentList.size(); ++i)
    {
        QMap<QString,QString> parameterMap = commentList.at(i);
        q.bindValue(":tracker_id", parameterMap["tracker_id"]);
        q.bindValue(":bug_id", parameterMap["bug_id"]);
        q.bindValue(":comment_id", parameterMap["comment_id"]);
        q.bindValue(":author", parameterMap["author"]);
        q.bindValue(":comment", parameterMap["comment"]);
        q.bindValue(":timestamp", parameterMap["timestamp"]);
        q.bindValue(":private", parameterMap["private"].toInt());
        if (!q.exec())
        {
            emit failure(q.lastError().text());
            break;
        }
    }
    emit commentFinished();
}

void
SqlUtilities::deleteBugs(const QString &trackerId)
{
    QString sql = QString("DELETE FROM bugs WHERE tracker_id=%1").arg(trackerId);
    QSqlQuery q;
    if (!q.exec())
        emit failure(q.lastError().text());
}

void
SqlUtilities::syncDB(int id, const QString &timestamp)
{
    QString sql = "UPDATE trackers SET last_sync = :last_sync WHERE id = :id";
    QSqlQuery q;
    q.prepare(sql);
    q.bindValue(":last_sync", timestamp);
    q.bindValue(":id", id);
    if (!q.exec())
        emit failure(q.lastError().text());
}

QString
SqlUtilities::getBugDescription(const QString &table, const QString &bugId)
{
    QSqlQuery q;
    QString ret;
    if (!q.prepare(QString("SELECT description FROM %1 WHERE bug_id=:bug").arg(table)))    {
        qDebug() << "getBugDescription: Could not prepare: " << q.lastError().text();
        return "";
    }

    q.bindValue(":table", table);
    q.bindValue(":bug", bugId);
    if (!q.exec())
    {
        qDebug() << "getBugDescription: Could not exec: " << q.lastError().text();
        return "";
    }
    q.next();
    ret = q.value(0).toString();
    return ret;
}

QMap<QString, QString>
SqlUtilities::tracBugDetail(const QString &rowId)
{
    QMap<QString, QString> ret;
    QRegExp removeFont("<[^>]*>");

    QString details = "SELECT trac.tracker_id, trackers.name, trac.bug_id, trac.last_modified,"
                   "coalesce(shadow_trac.severity, trac.severity),"
                   "coalesce(shadow_trac.priority, trac.priority),"
                   "coalesce(shadow_trac.assigned_to, trac.assigned_to),"
                   "coalesce(shadow_trac.status, trac.status),"
                   "coalesce(shadow_trac.summary, trac.summary), "
                   "coalesce(shadow_trac.description, trac.description), "
                   "coalesce(shadow_trac.component, trac.component), "
                   "coalesce(shadow_trac.milestone, trac.milestone), "
                   "coalesce(shadow_trac.version, trac.version), "
                   "coalesce(shadow_trac.resolution, trac.resolution) "
                   "from trac left outer join shadow_trac ON trac.bug_id = shadow_trac.bug_id AND trac.tracker_id = shadow_trac.tracker_id "
                   "join trackers ON trac.tracker_id = trackers.id WHERE trac.id = :id";
    QSqlQuery q;
    if (!q.prepare(details))
    {
        qDebug() << q.lastQuery();
        qDebug() << "Could not prepare tracBugDetails: " << q.lastError().text();
        return ret;
    }

    q.bindValue(":id", rowId);
    if (!q.exec())
    {
        qDebug() << q.lastQuery();
        qDebug() << "Could not exec tracBugDetails: " << q.lastError().text();
        return ret;
    }

    q.next();
    ret["tracker_id"] = q.value(0).toString();
    ret["tracker_name"] = q.value(1).toString();
    ret["bug_id"] = q.value(2).toString();
    ret["last_modified"] = q.value(3).toString();
    ret["severity"] = q.value(4).toString().remove(removeFont);
    ret["priority"] = q.value(5).toString().remove(removeFont);
    ret["assigned_to"] = q.value(6).toString().remove(removeFont);
    ret["status"] = q.value(7).toString().remove(removeFont);
    ret["summary"] = q.value(8).toString().remove(removeFont);
    ret["description"] = q.value(9).toString().remove(removeFont);
    ret["component"] = q.value(10).toString().remove(removeFont);
    ret["milestone"] = q.value(11).toString().remove(removeFont);
    ret["version"] = q.value(12).toString().remove(removeFont);
    ret["resolution"] = q.value(13).toString().remove(removeFont);
    return ret;
}

QMap<QString, QString>
SqlUtilities::bugzillaBugDetail(const QString &rowId)
{
    QMap<QString, QString> ret;
    QRegExp removeFont("<[^>]*>");
    QString details = "SELECT bugzilla.tracker_id, trackers.name, bugzilla.bug_id, bugzilla.last_modified,"
            "coalesce(shadow_bugzilla.severity, bugzilla.severity),"
            "coalesce(shadow_bugzilla.priority, bugzilla.priority),"
            "coalesce(shadow_bugzilla.assigned_to, bugzilla.assigned_to),"
            "coalesce(shadow_bugzilla.status, bugzilla.status),"
            "coalesce(shadow_bugzilla.summary, bugzilla.summary), "
            "coalesce(shadow_bugzilla.product, bugzilla.product), "
            "coalesce(shadow_bugzilla.component, bugzilla.component) "
            "from bugzilla left outer join shadow_bugzilla ON bugzilla.bug_id = shadow_bugzilla.bug_id "
            "join trackers ON bugzilla.tracker_id = trackers.id WHERE bugzilla.id = :id";

    QSqlQuery q;
    if (!q.prepare(details))
    {
        qDebug() << q.lastQuery();
        qDebug() << "Could not prepare bugzillaBugDetails: " << q.lastError().text();
        return ret;
    }

    q.bindValue(":id", rowId);
    if (!q.exec())
    {
        qDebug() << q.lastQuery();
        qDebug() << "Could not exec bugzillaBugDetails: " << q.lastError().text();
        return ret;
    }

    q.next();
    ret["tracker_id"] = q.value(0).toString();
    ret["tracker_name"] = q.value(1).toString();
    ret["bug_id"] = q.value(2).toString();
    ret["last_modified"] = q.value(3).toString();
    ret["severity"] = q.value(4).toString().remove(removeFont);
    ret["priority"] = q.value(5).toString().remove(removeFont);
    ret["assigned_to"] = q.value(6).toString().remove(removeFont);
    ret["status"] = q.value(7).toString().remove(removeFont);
    ret["summary"] = q.value(8).toString().remove(removeFont);
    ret["product"] = q.value(9).toString().remove(removeFont);
    ret["component"] = q.value(10).toString().remove(removeFont);
    return ret;
}

QMap<QString, QString>
SqlUtilities::mantisBugDetail(const QString &rowId)
{
    QMap<QString, QString> ret;
    QRegExp removeFont("<[^>]*>");

    QString details = "SELECT mantis.id, trackers.name, mantis.bug_id, mantis.last_modified,"
                   "coalesce(shadow_mantis.project, mantis.project),"
                   "coalesce(shadow_mantis.category, mantis.category),"
                   "coalesce(shadow_mantis.os, mantis.os),"
                   "coalesce(shadow_mantis.os_version, mantis.os_version),"
                   "coalesce(shadow_mantis.product_version, mantis.product_version),"
                   "coalesce(shadow_mantis.severity, mantis.severity),"
                   "coalesce(shadow_mantis.priority, mantis.priority),"
                   "coalesce(shadow_mantis.reproducibility, mantis.reproducibility),"
                   "coalesce(shadow_mantis.assigned_to, mantis.assigned_to),"
                   "coalesce(shadow_mantis.status, mantis.status),"
                   "coalesce(shadow_mantis.summary, mantis.summary) "
                   "from mantis left outer join shadow_mantis ON mantis.bug_id = shadow_mantis.bug_id "
                   "join trackers ON mantis.tracker_id = trackers.id WHERE mantis.id = :id";

    QSqlQuery q;
    if (!q.prepare(details))
    {
        qDebug() << q.lastQuery();
        qDebug() << "Could not prepare mantisBugDetails: " << q.lastError().text();
        return ret;
    }

    q.bindValue(":id", rowId);
    if (!q.exec())
    {
        qDebug() << q.lastQuery();
        qDebug() << "Could not exec mantisBugDetails: " << q.lastError().text();
        return ret;
    }

    q.next();
    ret["tracker_id"] = q.value(0).toString();
    ret["tracker_name"] = q.value(1).toString();
    ret["bug_id"] = q.value(2).toString();
    ret["last_modified"] = q.value(3).toString();
    ret["project"] = q.value(4).toString().remove(removeFont);
    ret["category"] = q.value(5).toString().remove(removeFont);
    ret["os"] = q.value(6).toString().remove(removeFont);
    ret["os_version"] = q.value(7).toString().remove(removeFont);
    ret["product_version"] = q.value(8).toString().remove(removeFont);
    ret["severity"] = q.value(9).toString().remove(removeFont);
    ret["priority"] = q.value(10).toString().remove(removeFont);
    ret["reproducibility"] = q.value(11).toString().remove(removeFont);
    ret["assigned_to"] = q.value(12).toString().remove(removeFont);
    ret["status"] = q.value(13).toString().remove(removeFont);
    ret["summary"] = q.value(14).toString();

    return ret;
}

QList< QMap<QString, QString> >
SqlUtilities::loadComments(const QString &trackerId,
                           const QString &bugId,
                           bool shadow)
{
    QSqlQuery q;
    QList< QMap<QString, QString> > ret;
    QString prefix = "";
    if (shadow)
        prefix = "shadow_";
    QString sql = QString("SELECT comment_id, author, comment, timestamp, private "
                          "FROM %1comments WHERE tracker_id=:tracker AND bug_id=:bug_id")
                          .arg(prefix);

    if (!q.prepare(sql))
    {
        qDebug() << "Error preparing loadComments: " << q.lastError().text();
        return ret;
    }

    q.bindValue(":tracker", trackerId);
    q.bindValue(":bug_id", bugId);
    if (!q.exec())
    {
        qDebug() << "Error executing loadComments: " << q.lastError().text();
        return ret;
    }

    while (q.next())
    {
        QMap<QString, QString> val;
        val["comment_id"] = q.value(0).toString();
        val["author"] = q.value(1).toString();
        val["comment"] = q.value(2).toString();
        val["timestamp"] = q.value(3).toString();
        val["private"] = q.value(4).toString();
        ret << val;
    }

    return ret;
}
