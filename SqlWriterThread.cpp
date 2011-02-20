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

#include <QMetaType>
#include "SqlWriterThread.h"
#include "SqlWriter.h"


SqlWriterThread::SqlWriterThread(QObject *parent) :
    QThread(parent)
{
}

SqlWriterThread::~SqlWriterThread()
{
     QThread::quit();
     QThread::wait();
}

// In order to have asynchronous writing to the DB (which is important,
// otherwise the UI freezes when inserting new bugs) we have to redirect
// the signals
void
SqlWriterThread::run()
{
    pWriter = new SqlWriter();

    qRegisterMetaType< QMap<QString,QString> >("QMap<QString,QString>");
    qRegisterMetaType< QList<QMap<QString,QString> > >("QList<QMap<QString,QString> >");
    connect(this, SIGNAL(newBugs(QList<QMap<QString,QString> >)),
            pWriter, SLOT(insertBugs(QList<QMap<QString,QString> >)));
    connect(this, SIGNAL(newComments(QList<QMap<QString,QString> >)),
            pWriter, SLOT(insertComments(QList<QMap<QString,QString> >)));
    connect(this, SIGNAL(syncDB(int, QString)),
            pWriter, SLOT(syncDB(int, QString)));
    connect(this, SIGNAL(deleteBugs(QString)),
            pWriter, SLOT(deleteBugs(QString)));
    connect(pWriter, SIGNAL(failure(QString)),
            this, SIGNAL(failure(QString)));
    connect(pWriter, SIGNAL(success()),
            this, SIGNAL(success()));
    connect(pWriter, SIGNAL(commentFinished()),
            this, SIGNAL(commentFinished()));
    connect(pWriter, SIGNAL(bugsFinished(QStringList)),
            this, SIGNAL(bugsFinished(QStringList)));
    exec();
}

// Utility functions to keep the backends from caring about the
// signal/slot implementation
void
SqlWriterThread::insertBugs(QList<QMap<QString, QString> > bugList)
{
    emit newBugs(bugList);
}

void
SqlWriterThread::insertComments(QList<QMap<QString, QString> > commentList)
{
    emit newComments(commentList);
}

void
SqlWriterThread::updateSync(int id, const QString &timestamp)
{
    emit syncDB(id, timestamp);
}

void
SqlWriterThread::clearBugs(const QString &trackerId)
{
    emit deleteBugs(trackerId);
}

