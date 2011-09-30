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
#include "trackers/NovellBugzilla.h"
//#include "trackers/Google.h"
#include "trackers/Trac.h"
#include "trackers/Mantis.h"
#include "Autodetector.h"

Autodetector::Autodetector()
    : QObject()
{
    novellBugzilla = NULL;
    genericBugzilla = NULL;
    mantis = NULL;
    trac = NULL;
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
    if (mUrl.host().toLower() == "bugzilla.novell.com")
    {
        mDetectionState = NOVELL_CHECK;
        novellBugzilla = new NovellBugzilla(temp);
        checkVersion(novellBugzilla);
    }
    else if (mUrl.host().toLower() == "code.google.com")
    {
        mData["type"] = "Google";
        emit finishedDetecting(mData);
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
    //connect(b, SIGNAL(fieldsFound()),
    //        this, SLOT(fieldsFound()));
    connect(b, SIGNAL(backendError(QString)),
            this, SLOT(handleError(QString)));
    b->checkVersion();
}

void
Autodetector::handleError(QString msg)
{
    qDebug() << "Autodetector::Backend Error: " << msg;
    mData["type"] = "Unknown";
    emit backendError(msg);
}

void
Autodetector::versionChecked(QString version)
{
    if (version == "-1")
    {
        mData["type"] = "Unknown";
        if (mDetectionState == BUGZILLA_CHECK)
        {
            genericBugzilla->deleteLater();
            genericBugzilla = NULL;
            qDebug() << "Checking trac";
            mDetectionState = TRAC_CHECK;
            trac = new Trac(mData["url"], mData["username"], mData["password"]);
            checkVersion(trac);
            return;
        }
        else if (mDetectionState == TRAC_CHECK)
        {
            trac->deleteLater();
            trac = NULL;
            qDebug() << "Checking mantis";
            mDetectionState = MANTIS_CHECK;
            mantis = new Mantis(mData["url"]);
            checkVersion(mantis);
            return;

        }
        else if (mDetectionState == MANTIS_CHECK)
        {
            qDebug() << "Mantis said no, too";
            mantis->deleteLater();
            mantis = NULL;
        }

        qDebug() << "Done detecting, no trackers found.";
        emit finishedDetecting(mData);
        return;
    }

    mData["version"] = version;
    if (mDetectionState == NOVELL_CHECK)
    {
        mData["type"] = "NovellBugzilla";
        qDebug() << "Found: Novell Bugzilla";
    }
    else if (mDetectionState == BUGZILLA_CHECK)
    {
        mData["type"] = "Bugzilla";
        qDebug() << "Found: Bugzilla";

    }
    else if (mDetectionState == MANTIS_CHECK)
    {
        qDebug() << "Found: Mantis";
        mData["type"] = "Mantis";
        mantis->setUsername(mData["username"]);
        mantis->setPassword(mData["password"]);
    }
    else if (mDetectionState == LAUNCHPAD_CHECK)
    {
        mData["type"] = "Launchpad";
        qDebug() << "Found: Launchpad";

    }
    else if (mDetectionState == TRAC_CHECK)
    {
        qDebug() << "Found: Trac";
        mData["type"] = "Trac";
        trac->setUsername(mData["username"]);
        trac->setPassword(mData["password"]);
    }

    emit finishedDetecting(mData);
}
