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
#include <QtSql>
#include <QDateTime>

#include "Backend.h"
#include "tracker_uis/BackendUI.h"
#include "SqlWriterThread.h"

Backend::Backend(const QString &url)
    : mUrl(url)
{
    pDisplayWidget = NULL;
    mUpdateCount = 0;
    pManager = new QNetworkAccessManager();
    pCookieJar = new QNetworkCookieJar();
    pManager->setCookieJar(pCookieJar);

    pSqlWriter = new SqlWriterThread();
    connect(pSqlWriter, SIGNAL(failure(QString)),
            this, SIGNAL(backendError(QString)));
    pSqlWriter->start();
}

Backend::~Backend()
{
    delete pSqlWriter;
    delete pManager;
    if (pDisplayWidget != NULL)
        delete pDisplayWidget;
}

BackendUI *
Backend::displayWidget()
{
    return(pDisplayWidget);
}

void
Backend::setLastSync(const QString &dateTime)
{
    mLastSync = QDateTime::fromString(dateTime, "yyyy-MM-ddThh:mm:ss");
}

void
Backend::updateSync()
{
    mLastSync = QDateTime::currentDateTime().toUTC();
    pSqlWriter->updateSync(mId.toInt(), mLastSync.toUTC().toString("yyyy-MM-ddThh:mm:ss"));
}

void
Backend::saveCredentials()
{
    pSqlWriter->updateCredentials(mId.toInt(), mUsername, mPassword);
}

void
Backend::sqlError(QString message)
{
    emit backendError(message);
}

// Utility function to convert date/times to more readable ones
QString
Backend::friendlyTime(const QString &time)
{
    QDateTime newTime = QDateTime::fromString(time, Qt::ISODate);
    if (!newTime.isValid())
    {
        newTime = QDateTime::fromString(time, "yyyy-MM-dd hh:mm:ss");
    }

    if (!newTime.isValid())
        return(time);

    return(newTime.toString("yyyy-MM-dd hh:mm:ss"));
}

bool
Backend::hasPendingChanges()
{
    QSqlQuery q;
    q.exec(QString("SELECT COUNT(id) FROM shadow_bugs WHERE tracker_id=%1").arg(mId));
    if (q.next())
    {
        if (q.value(0).toInt() > 0)
            return true;
    }

    q.exec(QString("SELECT COUNT(id) FROM shadow_comments WHERE tracker_id=%1").arg(mId));
    if (q.next())
    {
        if (q.value(0).toInt() > 0)
            return true;
    }
    return false;
}
