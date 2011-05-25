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

#include <QtSql>
#include <QSqlDatabase>
#include <QDateTime>
#include <QDebug>

#include "SqlWriter.h"

SqlWriter::SqlWriter(QObject *parent) :
    QObject(parent)
{
     mDatabase = QSqlDatabase::database();
}

SqlWriter::~SqlWriter()
{
    mDatabase.close();
}

void
SqlWriter::saveCredentials(int id,
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
SqlWriter::insertBugs(QList<QMap<QString, QString> > bugList)
{
    qDebug() << "Going to write " << bugList.count() << " bugs";
    bool error = false;
    QString bugDeleteSql;
    QDateTime modifiedTime;
    QString commentDeleteSql;
    QString bugInsertSql;
    QSqlQuery bugDeleteQuery;
    QSqlQuery commentDeleteQuery;
    QSqlQuery bugInsertQuery;
    QStringList idList;
    bugDeleteSql = "DELETE FROM bugs WHERE bug_id=:bug_id AND tracker_id=:tracker_id";
    commentDeleteSql = "DELETE FROM comments WHERE bug_id=:bug_id AND tracker_id=:tracker_id";
    bugInsertSql = "INSERT INTO bugs (tracker_id, bug_id, severity, priority, assigned_to, status, summary, component, product, bug_type, last_modified)"
                   " VALUES (:tracker_id, :bug_id, :severity, :priority, :assigned_to, :status, :summary, :component, :product, :bug_type, :last_modified)";
    bugInsertQuery.prepare(bugInsertSql);
    bugDeleteQuery.prepare(bugDeleteSql);
    commentDeleteQuery.prepare(commentDeleteSql);
    mDatabase.transaction();
    for (int i = 0; i < bugList.size(); ++i)
    {
        QMap<QString,QString> parameterMap = bugList.at(i);

        bugDeleteQuery.bindValue(":bug_id", parameterMap["bug_id"]);
        bugDeleteQuery.bindValue(":tracker_id", parameterMap["tracker_id"]);
        if (!bugDeleteQuery.exec())
        {
            error = true;
            emit failure(bugDeleteQuery.lastError().text());
            break;
        }

        commentDeleteQuery.bindValue(":bug_id", parameterMap["bug_id"]);
        commentDeleteQuery.bindValue(":tracker_id", parameterMap["tracker_id"]);
        if (!commentDeleteQuery.exec())
        {
            error = true;
            emit failure(commentDeleteQuery.lastError().text());
            break;
        }

        if (parameterMap["bug_state"] == "open")
        {
            bugInsertQuery.bindValue(":tracker_id", parameterMap["tracker_id"]);
            bugInsertQuery.bindValue(":bug_id", parameterMap["bug_id"]);
            bugInsertQuery.bindValue(":severity", parameterMap["severity"]);
            bugInsertQuery.bindValue(":priority", parameterMap["priority"]);
            bugInsertQuery.bindValue(":assigned_to", parameterMap["assigned_to"]);
            bugInsertQuery.bindValue(":status", parameterMap["status"]);
            bugInsertQuery.bindValue(":summary", parameterMap["summary"]);
            bugInsertQuery.bindValue(":component", parameterMap["component"]);
            bugInsertQuery.bindValue(":product", parameterMap["product"]);
            bugInsertQuery.bindValue(":bug_type", parameterMap["bug_type"]);
            bugInsertQuery.bindValue(":last_modified", parameterMap["last_modified"]);
            if (!bugInsertQuery.exec())
            {
                error = true;
                emit failure(bugInsertQuery.lastError().text());
                break;
            }

            idList << parameterMap["bug_id"];
        }
    }

    if (!error)
    {
        qDebug() << "Emitting bugsFinished";
        mDatabase.commit();
        emit bugsFinished(idList);
    }
    else
    {
        mDatabase.rollback();
    }

}

void
SqlWriter::insertBugComments(QList<QMap<QString, QString> >commentList)
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
SqlWriter::insertComments(QList<QMap<QString, QString> > commentList)
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
SqlWriter::deleteBugs(const QString &trackerId)
{
    QString sql = QString("DELETE FROM bugs WHERE tracker_id=%1").arg(trackerId);
    QSqlQuery q;
    if (!q.exec())
        emit failure(q.lastError().text());
}

void
SqlWriter::syncDB(int id, const QString &timestamp)
{
    QString sql = "UPDATE trackers SET last_sync = :last_sync WHERE id = :id";
    QSqlQuery q;
    q.prepare(sql);
    q.bindValue(":last_sync", timestamp);
    q.bindValue(":id", id);
    if (!q.exec())
        emit failure(q.lastError().text());
}
