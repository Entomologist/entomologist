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

#ifndef SQLWRITERTHREAD_H
#define SQLWRITERTHREAD_H

#include <QThread>
#include <QMap>
#include <QStringList>

class SqlUtilities;

class SqlWriterThread : public QThread
{
Q_OBJECT
public:
    SqlWriterThread(QObject *parent = 0);
    ~SqlWriterThread();
    void run();
    void clearBugs(const QString &trackerId);
    void insertBugs(const QString &table, QList<QMap<QString, QString> > list);
    void insertComments(QList<QMap<QString, QString> > commentList);
    void insertBugComments(QList<QMap<QString, QString> > commentList);
    void multiInsert(const QString &table, QList<QMap<QString, QString> > bugList);
    void updateSync(int id, const QString &timestamp);
    void updateCredentials(int id, const QString &username, const QString &password);

signals:
    void success();
    void failure(QString message);
    void commentFinished();
    void bugsFinished(QStringList idList);
    void deleteBugs(const QString &trackerId);
    void bugsInsert(const QString &table, QList<QMap<QString, QString> > bugList);
    void multiRowInsert(const QString &table, QList<QMap<QString, QString> > bugList);
    void newComments(QList<QMap<QString, QString> > commentList);
    void newBugComments(QList<QMap<QString, QString> > commentList);
    void syncDB(int id, const QString &timestamp);
    void saveCredentials(int id, const QString &username, const QString &password);

private:
    SqlUtilities *pWriter;
};

#endif // SQLWRITERTHREAD_H
