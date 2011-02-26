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

#ifndef SQLWRITER_H
#define SQLWRITER_H

#include <QObject>
#include <QMap>
#include <QSqlDatabase>
#include <QStringList>

class SqlWriter : public QObject
{
Q_OBJECT
public:
    SqlWriter(QObject *parent = 0);
    ~SqlWriter();

signals:
    void success();
    void failure(QString message);
    void commentFinished();
    void bugsFinished(QStringList idList);

public slots:
    void deleteBugs(const QString &trackerId);
    void insertBugs(QList<QMap<QString, QString> > bugList);
    // insertComments inserts comments for a number of different bugs.
    // insertBugComments inserts comments for just one bug
    void insertComments(QList<QMap<QString, QString> > commentList);
    void insertBugComments(QList<QMap<QString, QString> > commentList);

    void syncDB(int id, const QString &timestamp);
    void saveCredentials(int id, const QString &username, const QString &password);

private:
    QSqlDatabase mDatabase;
};

#endif // SQLWRITER_H
