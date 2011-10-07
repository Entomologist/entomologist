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
        qDebug() << "openDb: Couldn't create the database connection";
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
                         QList< QMap<QString, QString> > list,
                          int operation)
{
    if (list.size() == 0)
    {
        emit success(operation);
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
        qDebug() << "multiInsert: Could not prepare " << query << " :" << q.lastError().text();
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
            qDebug() << "multiInsert failed: " << q.lastError().text();
            qDebug() << q.lastQuery();
            emit failure(q.lastError().text());
            error = true;
            break;
        }
    }

    if (!error)
    {
        mDatabase.commit();
        emit success(operation);
    }
    else
    {
        mDatabase.rollback();
    }

    return;
}


void
SqlUtilities::insertBugs(const QString &tableName,
                         QList< QMap<QString, QString> > list,
                         const QString &trackerId)
{
    QStringList idList;
    if ((list.size() == 0) && trackerId == "-1")
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
    QString bugDeleteSql= QString("DELETE FROM %1 WHERE bug_id=:bug_id AND tracker_id=:tracker_id").arg(tableName);
    QString commentDeleteSql = "DELETE FROM comments WHERE bug_id=:bug_id AND tracker_id=:tracker_id";

    if (!q.prepare(query))
    {
        qDebug() << "insertBugs: Could not prepare " << query << " :" << q.lastError().text();
        emit failure(q.lastError().text());
        return;
    }

    if (!bugQuery.prepare(bugDeleteSql))
    {
        qDebug() << "insertBugs: Could not prepare " << bugDeleteSql << " :" << bugQuery.lastError().text();
        emit failure(bugQuery.lastError().text());
        return;
    }

    if (!commentQuery.prepare(commentDeleteSql))
    {
        qDebug() << "insertBugs: Could not prepare " << commentDeleteSql << " :" << commentQuery.lastError().text();
        emit failure(commentQuery.lastError().text());
        return;
    }
    mDatabase.transaction();
    if (trackerId != "-1")
    {
        qDebug() << "insertBugs: Going to delete a bunch of things in " << tableName << " for " << trackerId;
        QSqlQuery rmShadow;

        if (!rmShadow.exec(QString("DELETE FROM %1 WHERE tracker_id = %2").arg(tableName).arg(trackerId)))
        {
            qDebug() << "insertBugs: Couldn't delete mantis bugs: " << rmShadow.lastError().text();
        }
        if (!rmShadow.exec(QString("DELETE FROM shadow_%1 WHERE tracker_id = %2").arg(tableName).arg(trackerId)))
        {
            qDebug() << "insertBugs: Couldn't delete shadow_mantis bugs: " << rmShadow.lastError().text();
        }

        if (!rmShadow.exec(QString("DELETE FROM comments WHERE tracker_id = %1").arg(trackerId)))
        {
            qDebug() << "insertBugs: Couldn't delete comments: " << rmShadow.lastError().text();
        }

        if (!rmShadow.exec(QString("DELETE FROM shadow_comments WHERE tracker_id = %2").arg(trackerId)))
        {
            qDebug() << "insertBugs: Couldn't delete shadow_comments: " << rmShadow.lastError().text();
        }

    }

    for (int a = 0; a < list.size(); ++a)
    {
        QMap<QString, QString> data = list.at(a);
        bugQuery.bindValue(":bug_id", data["bug_id"]);
        bugQuery.bindValue(":tracker_id", data["tracker_id"]);
        commentQuery.bindValue(":bug_id", data["bug_id"]);
        commentQuery.bindValue(":tracker_id", data["tracker_id"]);

        if (trackerId == "-1")
        {
            if (!bugQuery.exec())
            {
                qDebug() << "insertBugs: bugQuery failed: " << q.lastError().text();
                emit failure(q.lastError().text());
                error = true;
                break;
            }

            if (!commentQuery.exec())
            {
                qDebug() << "insertBugs: commentQuery failed: " << q.lastError().text();
                emit failure(q.lastError().text());
                error = true;
                break;
            }
        }
        for (int i = 0; i < keys.size(); ++i)
        {
            q.bindValue(i, data.value(keys.at(i)));
        }
        if (!q.exec())
        {
            qDebug() << "insertBugs failed: " << q.lastError().text();
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
        q.bindValue(u, i.value());
    }

    j.toFront();
    while (j.hasNext())
    {
        j.next();
        QString u = QString(":%1").arg(j.key());
        q.bindValue(u, j.value());
    }

    if (!q.exec())
    {
        qDebug() << "Could not execute simpleUpdate: " << q.lastError().text();
        qDebug() << q.lastQuery();
        return false;
    }

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
void
SqlUtilities::removeFieldValues(const QString &trackerId,
                                const QString &fieldName)
{
    QSqlQuery q;
    q.prepare("DELETE FROM fields WHERE tracker_id = :tracker AND field_name = :name");
    q.bindValue(":tracker", trackerId);
    q.bindValue(":name", fieldName);
    if (!q.exec())
    {
        qDebug() << "removeFieldValues error: " << q.lastError().text();
    }
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
    q.exec("SELECT COUNT(id) FROM shadow_trac");
    q.next();
    if (q.value(0).toInt() > 0)
        return true;

    q.exec("SELECT COUNT(id) FROM shadow_bugzilla");
    q.next();
    if (q.value(0).toInt() > 0)
        return true;

    q.exec("SELECT COUNT(id) FROM shadow_mantis");
    q.next();
    if (q.value(0).toInt() > 0)
        return true;

    q.exec("SELECT COUNT(id) FROM shadow_comments");
    q.next();
    if (q.value(0).toInt() > 0)
        return true;

    return false;
}

bool
SqlUtilities::hasPendingChanges(const QString &shadowTable,
                                const QString &trackerId)
{
    QString shadowQuery = QString("SELECT COUNT(id) FROM %1 WHERE tracker_id = %2").arg(shadowTable).arg(trackerId);
    QSqlQuery q;
    q.exec(shadowQuery);
    if (q.next())
    {
        if (q.value(0).toInt() > 0)
            return true;
    }

    QString commentQuery = QString("SELECT COUNT(id) FROM shadow_comments WHERE tracker_id = %1").arg(trackerId);
    q.exec(commentQuery);
    if (q.next())
    {
        if (q.value(0).toInt() > 0)
            return true;
    }

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
    int val;
    q.exec(sql);
    if (q.next())
        val = q.value(0).toInt();
    else
        val = 0;
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
                                                      "auto_cache_comments INTEGER,"
                                                      "timezone_offset_in_seconds INTEGER DEFAULT 0)";

    QString createFieldsSql = "CREATE TABLE fields (id INTEGER PRIMARY KEY,"
                                                   "tracker_id INTEGER,"
                                                   "field_name TEXT,"
                                                   "value TEXT)";
    QString createTodoListSql = "CREATE TABLE todolist (id INTEGER PRIMARY KEY,"
                                                       "name TEXT,"
                                                       "list_id TEXT,"
                                                       "sync_services TEXT)";

    QString createServicesSql = "CREATE TABLE services (id INTEGER PRIMARY KEY,"
                                                        "servicetype TEXT,"
                                                        "name TEXT,"
                                                        "username TEXT,"
                                                        "password TEXT,"
                                                        "url TEXT,"
                                                        "auth_key TEXT)";

    QString createTodoListBugsSql = "CREATE TABLE todolistbugs (id INTEGER PRIMARY KEY,"
                                                     "tracker_id INTEGER,"
                                                     "tracker_table TEXT,"
                                                     "bug_id INTEGER,"
                                                     "todolist_id INTEGER,"
                                                     "date TEXT,"
                                                     "completed INTEGER,"
                                                     "item_ids TEXT,"
                                                     "last_modified TEXT)";

    QString createServiceTasksSql = "CREATE TABLE service_tasks (id INTERGER PRIMARY KEY,"
                                                       "service_name TEXT,"
                                                       "item_id TEXT,"
                                                       "task_id TEXT)";

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
                                              "resolution TEXT,"
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
                                              "resolution TEXT,"
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
    directExec(db, sql);
}

void
SqlUtilities::directExec(QSqlDatabase db, const QString &sql)
{
    QSqlQuery query(db);
    if (!query.exec(sql))
    {
        qDebug() << "directExec failed: " << db.lastError().text();
        qDebug() << "directExec: Failed SQL: " << sql;
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
        qDebug() << "saveCredentials: Couldn't update credentials: " << q.lastError().text();
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
    qDebug() << "insertComments: Going to write " << commentList.count() << " comments";

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
        qDebug() << "Could not prepare tracBugDetails: " << q.lastError().text();
        qDebug() << q.lastQuery();
        return ret;
    }

    q.bindValue(":id", rowId);
    if (!q.exec())
    {
        qDebug() << "Could not exec tracBugDetails: " << q.lastError().text();
        qDebug() << q.lastQuery();
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
            "coalesce(shadow_bugzilla.component, bugzilla.component), "
            "coalesce(shadow_bugzilla.resolution, bugzilla.resolution) "
            "from bugzilla left outer join shadow_bugzilla ON bugzilla.bug_id = shadow_bugzilla.bug_id "
            "join trackers ON bugzilla.tracker_id = trackers.id WHERE bugzilla.id = :id";

    QSqlQuery q;
    if (!q.prepare(details))
    {
        qDebug() << "Could not prepare bugzillaBugDetails: " << q.lastError().text();
        qDebug() << q.lastQuery();

        return ret;
    }

    q.bindValue(":id", rowId);
    if (!q.exec())
    {
        qDebug() << "Could not exec bugzillaBugDetails: " << q.lastError().text();
        qDebug() << q.lastQuery();

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
    ret["resolution"] = q.value(11).toString().remove(removeFont);
    return ret;
}

QMap<QString, QString>
SqlUtilities::mantisBugDetail(const QString &rowId)
{
    QMap<QString, QString> ret;
    QRegExp removeFont("<[^>]*>");

    QString details = "SELECT mantis.tracker_id, trackers.name, mantis.bug_id, mantis.last_modified,"
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
                   "coalesce(shadow_mantis.summary, mantis.summary), "
                   "coalesce(shadow_mantis.resolution, mantis.resolution) "
                   "from mantis left outer join shadow_mantis ON mantis.bug_id = shadow_mantis.bug_id "
                   "join trackers ON mantis.tracker_id = trackers.id WHERE mantis.id = :id";

    QSqlQuery q;
    if (!q.prepare(details))
    {
        qDebug() << "Could not prepare mantisBugDetails: " << q.lastError().text();
        qDebug() << q.lastQuery();
        return ret;
    }

    q.bindValue(":id", rowId);
    if (!q.exec())
    {
        qDebug() << "Could not exec mantisBugDetails: " << q.lastError().text();
        qDebug() << q.lastQuery();
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
    ret["resolution"] = q.value(15).toString();

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

QList< QMap<QString, QString> >
SqlUtilities::getCommentsChangelog()
{
    QList< QMap<QString, QString> > ret;
    QString commentsQuery = "SELECT trackers.name, "
                            "shadow_comments.bug_id, "
                            "shadow_comments.comment, "
                            "shadow_comments.id, "
                            "shadow_comments.private "
                            "from shadow_comments join trackers ON shadow_comments.tracker_id = trackers.id";
    QSqlQuery q(commentsQuery);
    if (!q.exec())
    {
        qDebug() << "Error loading comments changelog: " << q.lastError().text();
        return ret;
    }
    while(q.next())
    {
        QMap<QString, QString> entry;
        entry["tracker_name"] = q.value(0).toString();
        entry["bug_id"] = q.value(1).toString();
        entry["comment"] = q.value(2).toString();
        entry["id"] = q.value(3).toString();
        entry["private"] = q.value(4).toString();
        ret << entry;
    }

    return ret;
}

QVariantList
SqlUtilities::getTracChangelog()
{
    QVariantList retVal;
    QString bugsQuery = "SELECT trackers.name, "
                   "shadow_trac.bug_id, "
                   "trac.severity, "
                   "shadow_trac.severity, "
                   "trac.priority, "
                   "shadow_trac.priority, "
                   "trac.assigned_to, "
                   "shadow_trac.assigned_to, "
                   "trac.status, "
                   "shadow_trac.status, "
                   "trac.summary, "
                   "shadow_trac.summary, "
                   "trac.component, "
                   "shadow_trac.component,"
                   "trac.milestone,"
                   "shadow_trac.milestone,"
                   "trac.version,"
                   "shadow_trac.version, "
                   "trac.resolution,"
                   "shadow_trac.resolution,"
                   "shadow_trac.id "
                   "from shadow_trac left outer join trac on shadow_trac.bug_id = trac.bug_id AND shadow_trac.tracker_id = trac.tracker_id "
                   "join trackers ON shadow_trac.tracker_id = trackers.id";
    QSqlQuery q(bugsQuery);
    if (!q.exec())
    {
        qDebug() << "getTracChangelog error: " << q.lastError().text();
        qDebug() << "getTracChangelog error: " << q.lastQuery();
        return retVal;
    }

    while (q.next())
    {
        QVariantList ret;
        if (!q.value(3).isNull())
        {
            ret << newChangelogEntry("shadow_trac",
                                     q.value(20).toString(),
                                     q.value(0).toString(),
                                     q.value(1).toString(),
                                     "Severity",
                                     q.value(2).toString(),
                                     q.value(3).toString());
        }

        if (!q.value(5).isNull())
        {
            ret << newChangelogEntry("shadow_trac",
                                     q.value(20).toString(),
                                     q.value(0).toString(),
                                     q.value(1).toString(),
                                     "Priority",
                                     q.value(4).toString(),
                                     q.value(5).toString());
        }

        if (!q.value(7).isNull())
        {
            ret << newChangelogEntry("shadow_trac",
                                     q.value(20).toString(),
                                     q.value(0).toString(),
                                     q.value(1).toString(),
                                     "Assigned To",
                                     q.value(6).toString(),
                                     q.value(7).toString());
        }

        if (!q.value(9).isNull())
        {
            ret << newChangelogEntry("shadow_trac",
                                     q.value(20).toString(),
                                     q.value(0).toString(),
                                     q.value(1).toString(),
                                     "Status",
                                     q.value(8).toString(),
                                     q.value(9).toString());
        }

        if (!q.value(11).isNull())
        {
            ret << newChangelogEntry("shadow_trac",
                                     q.value(20).toString(),
                                     q.value(0).toString(),
                                     q.value(1).toString(),
                                     "Summary",
                                     q.value(10).toString(),
                                     q.value(11).toString());
        }

        if (!q.value(13).isNull())
        {
            ret << newChangelogEntry("shadow_trac",
                                     q.value(20).toString(),
                                     q.value(0).toString(),
                                     q.value(1).toString(),
                                     "Component",
                                     q.value(12).toString(),
                                     q.value(13).toString());
        }

        if (!q.value(15).isNull())
        {
            ret << newChangelogEntry("shadow_trac",
                                     q.value(20).toString(),
                                     q.value(0).toString(),
                                     q.value(1).toString(),
                                     "Milestone",
                                     q.value(14).toString(),
                                     q.value(15).toString());
        }

        if (!q.value(17).isNull())
        {
            ret << newChangelogEntry("shadow_trac",
                                     q.value(20).toString(),
                                     q.value(0).toString(),
                                     q.value(1).toString(),
                                     "Version",
                                     q.value(16).toString(),
                                     q.value(17).toString());
        }

        if (!q.value(19).isNull())
        {
            ret << newChangelogEntry("shadow_trac",
                                     q.value(20).toString(),
                                     q.value(0).toString(),
                                     q.value(1).toString(),
                                     "Resolution",
                                     q.value(18).toString(),
                                     q.value(19).toString());
        }

        if (ret.size() > 0)
            retVal.prepend(ret);
    }

    return(retVal);
}

QVariantList
SqlUtilities::getBugzillaChangelog()
{
    QVariantList retVal;
    QString bugsQuery = "SELECT trackers.name, "
                   "shadow_bugzilla.bug_id, "
                   "bugzilla.severity, "
                   "shadow_bugzilla.severity, "
                   "bugzilla.priority, "
                   "shadow_bugzilla.priority, "
                   "bugzilla.assigned_to, "
                   "shadow_bugzilla.assigned_to, "
                   "bugzilla.status, "
                   "shadow_bugzilla.status, "
                   "bugzilla.summary, "
                   "shadow_bugzilla.summary, "
                   "bugzilla.component, "
                   "shadow_bugzilla.component,"
                   "bugzilla.product,"
                   "shadow_bugzilla.product,"
                   "shadow_bugzilla.id, "
                   "bugzilla.resolution,"
                   "shadow_bugzilla.resolution "
                   "from shadow_bugzilla left outer join bugzilla on shadow_bugzilla.bug_id = bugzilla.bug_id AND shadow_bugzilla.tracker_id = bugzilla.tracker_id "
                   "join trackers ON shadow_bugzilla.tracker_id = trackers.id";
    QSqlQuery q(bugsQuery);
    if (!q.exec())
    {
        qDebug() << "getBugzillaChangelog error: " << q.lastError().text();
        qDebug() << "getBugzillaChangelog error: " << q.lastQuery();
        return retVal;
    }

    while (q.next())
    {
        QVariantList ret;
        if (!q.value(3).isNull())
        {
            ret << newChangelogEntry("shadow_bugzilla",
                                     q.value(16).toString(),
                                     q.value(0).toString(),
                                     q.value(1).toString(),
                                     "Severity",
                                     q.value(2).toString(),
                                     q.value(3).toString());
        }

        if (!q.value(5).isNull())
        {
            ret << newChangelogEntry("shadow_bugzilla",
                                     q.value(16).toString(),
                                     q.value(0).toString(),
                                     q.value(1).toString(),
                                     "Priority",
                                     q.value(4).toString(),
                                     q.value(5).toString());
        }

        if (!q.value(7).isNull())
        {
            ret << newChangelogEntry("shadow_bugzilla",
                                     q.value(16).toString(),
                                     q.value(0).toString(),
                                     q.value(1).toString(),
                                     "Assigned To",
                                     q.value(6).toString(),
                                     q.value(7).toString());
        }

        if (!q.value(9).isNull())
        {
            ret << newChangelogEntry("shadow_bugzilla",
                                     q.value(16).toString(),
                                     q.value(0).toString(),
                                     q.value(1).toString(),
                                     "Status",
                                     q.value(8).toString(),
                                     q.value(9).toString());
        }

        if (!q.value(11).isNull())
        {
            ret << newChangelogEntry("shadow_bugzilla",
                                     q.value(16).toString(),
                                     q.value(0).toString(),
                                     q.value(1).toString(),
                                     "Summary",
                                     q.value(10).toString(),
                                     q.value(11).toString());
        }

        if (!q.value(13).isNull())
        {
            ret << newChangelogEntry("shadow_bugzilla",
                                     q.value(16).toString(),
                                     q.value(0).toString(),
                                     q.value(1).toString(),
                                     "Component",
                                     q.value(12).toString(),
                                     q.value(13).toString());
        }

        if (!q.value(15).isNull())
        {
            ret << newChangelogEntry("shadow_bugzilla",
                                     q.value(16).toString(),
                                     q.value(0).toString(),
                                     q.value(1).toString(),
                                     "Product",
                                     q.value(14).toString(),
                                     q.value(15).toString());
        }

        if (!q.value(18).isNull())
        {
            ret << newChangelogEntry("shadow_bugzilla",
                                     q.value(16).toString(),
                                     q.value(0).toString(),
                                     q.value(1).toString(),
                                     "Resolution",
                                     q.value(17).toString(),
                                     q.value(18).toString());
        }

        if (ret.size() > 0)
            retVal.prepend(ret);

    }

    return(retVal);

}

QVariantList
SqlUtilities::getMantisChangelog()
{
    QVariantList retVal;
    QString bugsQuery = "SELECT trackers.name, "
                   "shadow_mantis.bug_id, "
                   "mantis.severity, "
                   "shadow_mantis.severity, "
                   "mantis.priority, "
                   "shadow_mantis.priority, "
                   "mantis.assigned_to, "
                   "shadow_mantis.assigned_to, "
                   "mantis.status, "
                   "shadow_mantis.status, "
                   "mantis.summary, "
                   "shadow_mantis.summary, "
                   "mantis.category, "
                   "shadow_mantis.category,"
                   "mantis.project,"
                   "shadow_mantis.project,"
                   "mantis.product_version,"
                   "shadow_mantis.product_version,"
                   "mantis.reproducibility,"
                   "shadow_mantis.reproducibility,"
                   "mantis.os,"
                   "shadow_mantis.os, "
                   "mantis.os_version,"
                   "shadow_mantis.os_version,"
                   "shadow_mantis.id, "
                   "mantis.resolution,"
                   "shadow_mantis.resolution "
                   "from shadow_mantis left outer join mantis on shadow_mantis.bug_id = mantis.bug_id AND shadow_mantis.tracker_id = mantis.tracker_id "
                   "join trackers ON shadow_mantis.tracker_id = trackers.id";
    QSqlQuery q(bugsQuery);
    if (!q.exec())
    {
        qDebug() << "getMantisChangelog error: " << q.lastError().text();
        qDebug() << "getMantisChangelog error: " << q.lastQuery();
        return retVal;
    }

    while (q.next())
    {
        QVariantList ret;
        if (!q.value(3).isNull())
        {
            ret << newChangelogEntry("shadow_mantis",
                                     q.value(24).toString(),
                                     q.value(0).toString(),
                                     q.value(1).toString(),
                                     "Severity",
                                     q.value(2).toString(),
                                     q.value(3).toString());
        }

        if (!q.value(5).isNull())
        {
            ret << newChangelogEntry("shadow_mantis",
                                     q.value(24).toString(),
                                     q.value(0).toString(),
                                     q.value(1).toString(),
                                     "Priority",
                                     q.value(4).toString(),
                                     q.value(5).toString());
        }

        if (!q.value(7).isNull())
        {
            ret << newChangelogEntry("shadow_mantis",
                                     q.value(24).toString(),
                                     q.value(0).toString(),
                                     q.value(1).toString(),
                                     "Assigned To",
                                     q.value(6).toString(),
                                     q.value(7).toString());
        }

        if (!q.value(9).isNull())
        {
            ret << newChangelogEntry("shadow_mantis",
                                     q.value(24).toString(),
                                     q.value(0).toString(),
                                     q.value(1).toString(),
                                     "Status",
                                     q.value(8).toString(),
                                     q.value(9).toString());
        }

        if (!q.value(11).isNull())
        {
            ret << newChangelogEntry("shadow_mantis",
                                     q.value(24).toString(),
                                     q.value(0).toString(),
                                     q.value(1).toString(),
                                     "Summary",
                                     q.value(10).toString(),
                                     q.value(11).toString());
        }

        if (!q.value(13).isNull())
        {
            ret << newChangelogEntry("shadow_mantis",
                                     q.value(24).toString(),
                                     q.value(0).toString(),
                                     q.value(1).toString(),
                                     "Category",
                                     q.value(12).toString(),
                                     q.value(13).toString());
        }

        if (!q.value(15).isNull())
        {
            ret << newChangelogEntry("shadow_mantis",
                                     q.value(24).toString(),
                                     q.value(0).toString(),
                                     q.value(1).toString(),
                                     "Project",
                                     q.value(14).toString(),
                                     q.value(15).toString());
        }

        if (!q.value(17).isNull())
        {
            ret << newChangelogEntry("shadow_mantis",
                                     q.value(24).toString(),
                                     q.value(0).toString(),
                                     q.value(1).toString(),
                                     "Product Version",
                                     q.value(16).toString(),
                                     q.value(17).toString());
        }

        if (!q.value(19).isNull())
        {
            ret << newChangelogEntry("shadow_mantis",
                                     q.value(24).toString(),
                                     q.value(0).toString(),
                                     q.value(1).toString(),
                                     "Reproducibility",
                                     q.value(18).toString(),
                                     q.value(19).toString());
        }

        if (!q.value(21).isNull())
        {
            ret << newChangelogEntry("shadow_mantis",
                                     q.value(24).toString(),
                                     q.value(0).toString(),
                                     q.value(1).toString(),
                                     "OS",
                                     q.value(20).toString(),
                                     q.value(21).toString());
        }

        if (!q.value(23).isNull())
        {
            ret << newChangelogEntry("shadow_mantis",
                                     q.value(24).toString(),
                                     q.value(0).toString(),
                                     q.value(1).toString(),
                                     "OS Version",
                                     q.value(22).toString(),
                                     q.value(23).toString());
        }

        if (!q.value(26).isNull())
        {
            ret << newChangelogEntry("shadow_mantis",
                                     q.value(24).toString(),
                                     q.value(0).toString(),
                                     q.value(1).toString(),
                                     "Resolution",
                                     q.value(25).toString(),
                                     q.value(26).toString());
        }

        if (ret.size() > 0)
            retVal.prepend(ret);

    }

    return(retVal);
}

void
SqlUtilities::simpleDelete(const QString &rowId,
                           const QString &tableName)
{
    QSqlQuery q;
    QString query = QString("DELETE FROM %1 WHERE id=%2")
                            .arg(tableName)
                            .arg(rowId);
    q.exec(query);
}

QVariantMap
SqlUtilities::newChangelogEntry(const QString &trackerTable,
                                const QString &id,
                                const QString &trackerName,
                                const QString &bugId,
                                const QString &columnName,
                                const QString &oldValue,
                                const QString &newValue)
{
    QVariantMap ret;
    ret["id"] = id;
    ret["tracker_table"] = trackerTable;
    ret["tracker_name"] = trackerName;
    ret["bug_id"] = bugId;
    ret["column_name"] = columnName;
    ret["from"] = (oldValue.isEmpty()) ? "(none)" : oldValue;
    ret["to"] = newValue;
    return(ret);
}

void
SqlUtilities::removeTracker(const QString &trackerId, const QString &trackerName)
{
    QSqlQuery q;
    q.exec(QString("DELETE FROM trackers WHERE id=%1").arg(trackerId));
    q.exec(QString("DELETE FROM comments WHERE tracker_id=%1").arg(trackerId));
    q.exec(QString("DELETE FROM shadow_comments WHERE tracker_id=%1").arg(trackerId));
    q.exec(QString("DELETE FROM trac WHERE tracker_id=%1").arg(trackerId));
    q.exec(QString("DELETE FROM shadow_trac WHERE tracker_id=%1").arg(trackerId));
    q.exec(QString("DELETE FROM bugzilla WHERE tracker_id=%1").arg(trackerId));
    q.exec(QString("DELETE FROM shadow_bugzilla WHERE tracker_id=%1").arg(trackerId));
    q.exec(QString("DELETE FROM mantis WHERE tracker_id=%1").arg(trackerId));
    q.exec(QString("DELETE FROM shadow_mantis WHERE tracker_id=%1").arg(trackerId));
    q.exec(QString("DELETE FROM search_results WHERE tracker_name=\'%1\'").arg(trackerName));
}

void
SqlUtilities::renameSearchTracker(const QString &oldName,
                                  const QString &newName)
{
    QString query = "UPDATE search_results SET tracker_name = :new_name WHERE tracker_name = :old_name";
    QSqlQuery q(query);
    q.bindValue(":new_name", newName);
    q.bindValue(":old_name", oldName);
    q.exec();
}

bool
SqlUtilities::renameTracker(const QString &id,
                            const QString &name,
                            const QString &username,
                            const QString &password)
{
    QString query = "UPDATE trackers SET name=:name,username=:username,password=:password WHERE id=:id";
    QSqlQuery q;
    q.prepare(query);
    q.bindValue(":name", name);
    q.bindValue(":username", username);
    q.bindValue(":password", password);
    q.bindValue(":id", id);

    if (!q.exec())
    {
        qDebug() << "Error renaming tracker: " << q.lastError().text();
        return false;
    }

    return true;
}

void
SqlUtilities::removeShadowBug(const QString &shadowTable,
                              const QString &bugId,
                              const QString &trackerId)
{
    QString query = QString("DELETE FROM %1 WHERE bug_id = :bug_id AND tracker_id = :tracker_id").arg(shadowTable);
    QSqlQuery q;
    q.prepare(query);
    q.bindValue(":bug_id", bugId);
    q.bindValue(":tracker_id", trackerId);
    if (!q.exec())
    {
        qDebug() << "removeShadowBug failed: " << q.lastError().text();
    }
}

void
SqlUtilities::removeShadowComment(const QString &bugId,
                                const QString &trackerId)
{
    QString query = "DELETE FROM shadow_comments WHERE bug_id = :bug_id AND tracker_id = :tracker_id";
    QSqlQuery q;
    q.prepare(query);
    q.bindValue(":bug_id", bugId);
    q.bindValue(":tracker_id", trackerId);
    if (!q.exec())
    {
        qDebug() << "removeShadowComment failed: " << q.lastError().text();
    }
}

QString
SqlUtilities::getMonitoredComponents(const QString &trackerId)
{
    QString ret;
    QString query = "SELECT monitored_components FROM trackers WHERE id=:id";
    QSqlQuery q;
    q.prepare(query);
    q.bindValue(":id", trackerId);
    if (!q.exec())
    {
        qDebug() << "getMonitoredComponents failed: " << q.lastError().text();
        return(ret);
    }
    if (q.next())
    {
        ret = q.value(0).toString();
    }
    return(ret);
}

void
SqlUtilities::clearBugs(const QString &tableName,
                        const QString &trackerId)
{
    QString query = QString("DELETE FROM %1 WHERE tracker_id = %2").arg(tableName).arg(trackerId);
    QSqlQuery q;
    if (!q.exec(query))
        qDebug() << "SqlUtilities::clearBugs: " << q.lastError().text();
}

QStringList
SqlUtilities::getChangedBugzillaIds(const QString &trackerId)
{
    QStringList ret;
    QString query = QString("SELECT bug_id FROM shadow_bugzilla WHERE tracker_id = %1").arg(trackerId);
    QSqlQuery q;
    if (!q.exec(query))
    {
        qDebug() << "getChangedBugzillaIds SELECT failed: " << q.lastError().text();
        return ret;
    }

    while (q.next())
    {
        ret << q.value(0).toString();
    }
    return ret;
}

int
SqlUtilities::getTimezoneOffset(const QString &trackerId)
{
    QString query = QString("SELECT timezone_offset_in_seconds FROM trackers WHERE id = %1").arg(trackerId);
    QSqlQuery q;
    int ret = 0;
    if (!q.exec(query))
    {
        qDebug() << "SqlUtilities::getTimezoneOffset failed: " << q.lastError().text();
        return(0);
    }

    if (q.next())
    {
        ret = q.value(0).toInt();
    }
    return ret;
}
