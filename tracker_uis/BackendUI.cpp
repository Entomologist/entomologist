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

#include "BackendUI.h"
#include "trackers/Backend.h"
#include <SqlBugDelegate.h>
#include <QClipboard>
#include <QDesktopServices>
#include <QApplication>
#include <SqlBugModel.h>
#include <QMenu>
#include <QProgressDialog>
#include <QSettings>
#include "SqlUtilities.h"

BackendUI::BackendUI(const QString &id, Backend *backend, QWidget *parent) :
    QWidget(parent), mId(id)
{
    pBackend = backend;
    connect(pBackend, SIGNAL(searchResultFinished(QMap<QString,QString>)),
            this, SLOT(searchResultFinished(QMap<QString,QString>)));
    pSearchProgress = NULL;
    mShowMyBugs = true;
    mShowMyReports = true;
    mShowMyCCs = true;
    mShowMonitored = true;
    pBugModel = new SqlBugModel();
    pBugModel->setParent(this);
    mWhereQuery = "";
}

BackendUI::~BackendUI()
{
}

void
BackendUI::startSearchProgress()
{
    if (pSearchProgress == NULL)
    {
        pSearchProgress = new QProgressDialog(tr("Loading search result..."), QString(), 0, 0);
        pSearchProgress->setWindowModality(Qt::ApplicationModal);
        pSearchProgress->setMinimumDuration(0);
    }

    pSearchProgress->setValue(1);
}

void
BackendUI::stopSearchProgress()
{
    if (pSearchProgress != NULL)
    {
        pSearchProgress->reset();
        pSearchProgress->hide();
    }
}

void
BackendUI::copyBugUrl(const QString &bugId)
{
    QString url = pBackend->buildBugUrl(bugId);
    QApplication::clipboard()->setText(url);
}

void BackendUI::openBugInBrowser(const QString &bugId)
{
    QString url = pBackend->buildBugUrl(bugId);
#ifdef Q_OS_ANDROID
        Utilities::openAndroidUrl(QUrl(url));
#else
        QDesktopServices::openUrl(QUrl(url));
#endif
}

void
BackendUI::hideColumnsMenu(const QPoint &pos, const QString &settingName, const QString &columnName)
{
    QMenu contextMenu(this);
    QAction *editAction = new QAction(QString("Hide %1").arg(columnName), this);
    editAction->setData(QVariant::fromValue(31337));
    contextMenu.addAction(editAction);
    contextMenu.addSeparator();

    QMapIterator<QString, QVariant> i(mHiddenColumns);
    while (i.hasNext())
    {
        i.next();
        QString text = QString("Unhide %1")
                       .arg(i.value().toString());
        QAction *newAction = new QAction(text, this);
        newAction->setData(i.key());
        contextMenu.addAction(newAction);
    }

    QAction *a = contextMenu.exec(QCursor::pos());
    if (!a)
        return;

    if (a->data().toInt() == 31337)
    {
        int index = v->logicalIndexAt(pos);
        QString key = QString::number(index);
        QString title = mTableHeaders.at(index - 1);
        mHiddenColumns[key] = title;
        v->hideSection(index);
    }
    else
    {
        int index = a->data().toInt();
        mHiddenColumns.remove(QString::number(index));
        v->showSection(index);
    }

    QSettings settings("Entomologist");
    if (mHiddenColumns.size() > 0)
        settings.setValue(settingName, mHiddenColumns);
    else
        settings.remove(settingName);
}


void
BackendUI::setBackend(Backend *backend)
{
    pBackend = backend;
}

void
BackendUI::saveNewShadowItems(const QString &tableName,
                              QMap<QString, QString> shadowBug,
                              const QString &newComment)
{
    qDebug() << "I'll save " << shadowBug;
    int shadow_id;
    if (shadowBug.count() > 2)
    {
        if ((shadow_id = SqlUtilities::hasShadowBug(tableName, shadowBug["bug_id"], shadowBug["tracker_id"])) >= 1)
        {
            QMap<QString, QString> where;
            where["id"] = QString::number(shadow_id);
            SqlUtilities::simpleUpdate(tableName, shadowBug, where);
        }
        else
        {
            SqlUtilities::simpleInsert(tableName, shadowBug);
        }
    }

    if (!newComment.isEmpty())
    {
        QMap<QString, QString> comment;
        comment["tracker_id"] = shadowBug["tracker_id"];
        comment["bug_id"] = shadowBug["bug_id"];
        comment["comment"] = newComment;
        comment["timestamp"] = QDateTime::currentDateTime().toString("yyyy-MM-ddThh:mm:ss");
        SqlUtilities::simpleInsert("shadow_comments", comment);
    }
}

void BackendUI::setShowOptions(bool showMyBugs,
                    bool showMyReports,
                    bool showMyCCs,
                    bool showMonitored)
{
    mShowMyBugs = showMyBugs;
    mShowMyReports = showMyReports;
    mShowMyCCs = showMyCCs;
    mShowMonitored = showMonitored;
    reloadFromDatabase();
}
