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

#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkCookieJar>

#include "libmaia/maiaXmlRpcClient.h"
#include "NovellBugzilla.h"
#include "Autodetector.h"

Autodetector::Autodetector()
    : QObject()
{
    novellBugzilla = NULL;
    genericBugzilla = NULL;
}

void
Autodetector::detect(QMap<QString, QString> data)
{
    qDebug() << "Detecting...";
    mData = data;
    QString temp = mData["url"];

    if (!temp.startsWith("http://") &&
        !temp.startsWith("https://"))
    {
        temp = temp.prepend("https://");
    }
    mData["url"]  = temp;
    mUrl = QUrl(temp);
    if (mUrl.host() == "bugzilla.novell.com")
    {
        mDetectionState = NOVELL_CHECK;
        novellBugzilla = new NovellBugzilla(temp);
        checkVersion(novellBugzilla);
    }
    else
    {
        mDetectionState = BUGZILLA_CHECK;
        genericBugzilla = new Bugzilla(temp);
        checkVersion(genericBugzilla);
    }

}

void
Autodetector::checkVersion(Backend *b)
{
    qDebug() << "Check version";
    connect(b, SIGNAL(versionChecked(QString)),
            this, SLOT(versionChecked(QString)));
    connect(b, SIGNAL(severitiesFound(QStringList)),
            this, SLOT(severitiesFound(QStringList)));
    connect(b, SIGNAL(prioritiesFound(QStringList)),
            this, SLOT(prioritiesFound(QStringList)));
    connect(b, SIGNAL(statusesFound(QStringList)),
            this, SLOT(statusesFound(QStringList)));
    b->checkVersion();
}

void
Autodetector::versionChecked(QString version)
{
    if (version == "-1")
    {
        mData["type"] = "Unknown";
        // Here is where it would check Launchpad, etc.
        emit finishedDetecting(mData);
        return;
    }

    mData["version"] = version;
    if (mDetectionState == NOVELL_CHECK)
    {
        mData["type"] = "NovellBugzilla";
    }
    else if (mDetectionState == BUGZILLA_CHECK)
    {
        mData["type"] = "Bugzilla";
    }

    getValidValues();
}

void
Autodetector::prioritiesFound(QStringList priorities)
{
    if (priorities.size() > 0)
        mData["valid_priorities"] = priorities.join(",");

    if (novellBugzilla != NULL)
        novellBugzilla->checkValidStatuses();
    else if (genericBugzilla != NULL)
        genericBugzilla->checkValidStatuses();
}

void
Autodetector::statusesFound(QStringList statuses)
{
    if (statuses.size() > 0)
        mData["valid_statuses"] = statuses.join(",");

    if (novellBugzilla != NULL)
        novellBugzilla->checkValidSeverities();
    else if (genericBugzilla != NULL)
        genericBugzilla->checkValidSeverities();
}

void
Autodetector::severitiesFound(QStringList severities)
{
    if (severities.size() > 0)
        mData["valid_severities"] = severities.join(",");

    if (novellBugzilla != NULL)
        delete novellBugzilla;
    else if (genericBugzilla != NULL)
        delete genericBugzilla;

    emit finishedDetecting(mData);
}

void
Autodetector::getValidValues()
{
    if (novellBugzilla != NULL)
        novellBugzilla->checkValidPriorities();
    else if (genericBugzilla != NULL)
        genericBugzilla->checkValidPriorities();
}
