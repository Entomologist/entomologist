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

#include "TracUI.h"
#include "TracDetails.h"
#include "NewCommentsDialog.h"
#include "ui_tracui.h"
#include "trackers/Backend.h"

#include <SqlBugDelegate.h>
#include <SqlBugModel.h>
#include <QClipboard>
#include <QDesktopServices>
#include "SqlUtilities.h"
#include <QSqlQuery>
#include <QSqlResult>
#include <QModelIndex>
#include <QDebug>
#include <QStringList>
#include <QDateTime>
#include <QSettings>

TracUI::TracUI(const QString &id, Backend *backend, QWidget *parent) :
    BackendUI(id, backend, parent),
    ui(new Ui::TracUI)
{
    ui->setupUi(this);
    QSettings settings("Entomologist");
    ui->tableView->setBugTableName("trac");
    ui->tableView->setTrackerId(id);
    ui->tableView->setDragDropMode(QAbstractItemView::DragOnly);
    ui->tableView->setDragEnabled(true);
    ui->tableView->setSortingEnabled(true);
    ui->tableView->setGridStyle(Qt::PenStyle(Qt::DotLine));

    v = ui->tableView->horizontalHeader();
    v->setContextMenuPolicy(Qt::CustomContextMenu);
    mTableHeaders << ""
                  << tr("Bug ID")
                  << tr("Last Modified")
                  << tr("Severity")
                  << tr("Priority")
                  << tr("Milestone")
                  << tr("Version")
                  << tr("Assigned To")
                  << tr("Status")
                  << tr("Summary");
    connect(ui->tableView, SIGNAL(copyBugURL(QString)),
            this, SLOT(copyBugUrl(QString)));
    connect(ui->tableView, SIGNAL(openBugInBrowser(QString)),
            this, SLOT(openBugInBrowser(QString)));
    connect(ui->tableView, SIGNAL(addBugToToDoList(QString)),
            this, SLOT(addBugToToDoList(QString)));
    connect(v, SIGNAL(customContextMenuRequested(QPoint)),
            this, SLOT(headerContextMenu(QPoint)));
    connect(v, SIGNAL(sortIndicatorChanged(int,Qt::SortOrder)),
            this, SLOT(sortIndicatorChanged(int,Qt::SortOrder)));

    connect(ui->tableView, SIGNAL(doubleClicked(QModelIndex)),
            this, SLOT(itemDoubleClicked(QModelIndex)));
    mBaseQuery = "SELECT trac.id, trac.highlight_type, trac.bug_id, trac.last_modified,"
                   "coalesce(shadow_trac.severity, trac.severity),"
                   "coalesce(shadow_trac.priority, trac.priority),"
                   "coalesce(shadow_trac.milestone, trac.milestone),"
                   "coalesce(shadow_trac.version, trac.version),"
                   "coalesce(shadow_trac.assigned_to, trac.assigned_to),"
                   "coalesce(shadow_trac.status, trac.status),"
                   "coalesce(shadow_trac.summary, trac.summary) "
                   "from trac left outer join shadow_trac ON trac.bug_id = shadow_trac.bug_id";
    mActiveQuery = mBaseQuery;
    mWhereQuery = QString(" trac.tracker_id=%1 ").arg(id);
    mSortQuery = " ORDER BY trac.bug_id ASC";
    loadFields();

    // Set the default sort column & order to be ascending bug ID.
    unsigned int savedSortColumn = settings.value(QString("%1-sort-column").arg(pBackend->name()), 2).toUInt();
    Qt::SortOrder savedSortOrder = static_cast<Qt::SortOrder>(settings.value(QString("%1-sort-order").arg(pBackend->name()), 0).toUInt());
    v->setSortIndicator(savedSortColumn, savedSortOrder);
    setupTable();

    mHiddenColumns = settings.value("hide-trac-columns", QVariantMap()).toMap();
    QMapIterator<QString, QVariant> i(mHiddenColumns);
    while (i.hasNext())
    {
        i.next();
        v->hideSection(i.key().toInt());
    }

}

TracUI::~TracUI()
{
    delete ui;
}


void TracUI::addBugToToDoList(const QString &bugId)
{

}

void
TracUI::headerContextMenu(const QPoint &pos)
{
    int index = v->logicalIndexAt(pos);
    if (index == 1)
        return;
    QString title = mTableHeaders.at(index - 1 );
    hideColumnsMenu(pos, "hide_trac_columns", title);
}

void
TracUI::loadFields()
{
    mSeverities = SqlUtilities::fieldValues(mId, "severity");
    mPriorities = SqlUtilities::fieldValues(mId, "priority");
    mStatuses = SqlUtilities::fieldValues(mId, "status");
    mVersions = SqlUtilities::fieldValues(mId, "version");
    mComponents = SqlUtilities::fieldValues(mId, "component");
    mResolutions = SqlUtilities::fieldValues(mId, "resolution");
}

void
TracUI::commentsDialogClosing(QMap<QString, QString> details, QString newComment)
{
    saveNewShadowItems("shadow_trac", details, newComment);
    reloadFromDatabase();
}

void
TracUI::loadSearchResult(const QString &id)
{
    startSearchProgress();
    pBackend->getSearchedBug(id);
}

void
TracUI::searchResultFinished(QMap<QString, QString> resultMap)
{
    NewCommentsDialog *dialog = new NewCommentsDialog(pBackend, this);
    connect (dialog, SIGNAL(commentsDialogClosing(QMap<QString,QString>,QString)),
             this, SLOT(commentsDialogClosing(QMap<QString,QString>,QString)));
    dialog->setBugInfo(resultMap);
    TracDetails *details = new TracDetails(resultMap["bug_id"]);

    details->setSeverities(resultMap["severity"], mSeverities);
    details->setPriorities(resultMap["priority"], mPriorities);
    details->setStatuses(resultMap["status"], mStatuses);
    details->setVersions(resultMap["version"], mVersions);
    details->setComponents(resultMap["component"], mComponents);
    details->setResolutions(resultMap["resolution"], mResolutions);
    dialog->setDetailsWidget(details);
    dialog->loadComments();
    stopSearchProgress();
    dialog->show();
    stopSearchProgress();
}

void
TracUI::itemDoubleClicked(const QModelIndex &index)
{
    QString rowId = index.sibling(index.row(), 0).data().toString();
    QMap<QString, QString> detailMap = SqlUtilities::tracBugDetail(rowId);
    NewCommentsDialog *dialog = new NewCommentsDialog(pBackend, this);
    connect (dialog, SIGNAL(commentsDialogClosing(QMap<QString,QString>,QString)),
             this, SLOT(commentsDialogClosing(QMap<QString,QString>,QString)));
    dialog->setBugInfo(detailMap);
    TracDetails *details = new TracDetails(detailMap["bug_id"]);

    details->setSeverities(detailMap["severity"], mSeverities);
    details->setPriorities(detailMap["priority"], mPriorities);
    details->setStatuses(detailMap["status"], mStatuses);
    details->setVersions(detailMap["version"], mVersions);
    details->setComponents(detailMap["component"], mComponents);
    details->setResolutions(detailMap["resolution"], mResolutions);
    dialog->setDetailsWidget(details);
    dialog->loadComments();
    dialog->show();
}

void
TracUI::setupTable()
{
    SqlBugDelegate *delegate = new SqlBugDelegate();
    ui->tableView->setItemDelegate(delegate);
    ui->tableView->setModel(pBugModel);
    pBugModel->setHeaderData(1, Qt::Horizontal, tr(""));
    pBugModel->setHeaderData(2, Qt::Horizontal, tr("Bug ID"));
    pBugModel->setHeaderData(3, Qt::Horizontal, tr("Last Modified"));
    pBugModel->setHeaderData(4, Qt::Horizontal, tr("Severity"));
    pBugModel->setHeaderData(5, Qt::Horizontal, tr("Priority"));
    pBugModel->setHeaderData(6, Qt::Horizontal, tr("Milestone"));
    pBugModel->setHeaderData(7, Qt::Horizontal, tr("Version"));
    pBugModel->setHeaderData(8, Qt::Horizontal, tr("Assigned To"));
    pBugModel->setHeaderData(9, Qt::Horizontal, tr("Status"));
    pBugModel->setHeaderData(10, Qt::Horizontal, tr("Summary"));
    ui->tableView->setSortingEnabled(true);
    ui->tableView->setGridStyle(Qt::PenStyle(Qt::DotLine));
    ui->tableView->hideColumn(0); // Hide the internal row id
    // Disable resizing of the highlight icon column
    ui->tableView->horizontalHeader()->setResizeMode(1, QHeaderView::Fixed);
    ui->tableView->verticalHeader()->hide(); // Hide the Row numbers
    ui->tableView->resizeColumnsToContents();
    ui->tableView->resizeRowsToContents();
    ui->tableView->setAlternatingRowColors(true);
}

void
TracUI::sortIndicatorChanged(int logicalIndex, Qt::SortOrder order)
{
    QString sortOrder= "ASC";
    if (order == Qt::DescendingOrder)
        sortOrder = "DESC";

    QString newSortQuery =" ORDER BY %1 " + sortOrder;
    switch(logicalIndex)
    {
    case 1:
        mSortQuery = newSortQuery.arg("trac.highlight_type");
        break;
    case 2:
        mSortQuery = newSortQuery.arg("trac.bug_id");
        break;
    case 3:
        mSortQuery = newSortQuery.arg("trac.last_modified");
        break;
    case 4:
        mSortQuery = newSortQuery.arg("trac.severity");
        break;
    case 5:
        mSortQuery = newSortQuery.arg("trac.priority");
        break;
    case 6:
        mSortQuery = newSortQuery.arg("trac.milestone");
        break;
    case 7:
        mSortQuery = newSortQuery.arg("trac.version");
        break;
    case 8:
        mSortQuery = newSortQuery.arg("trac.assigned_to");
        break;
    case 9:
        mSortQuery = newSortQuery.arg("trac.status");
        break;
    case 10:
        mSortQuery = newSortQuery.arg("trac.summary");
        break;
    default:
        mSortQuery = newSortQuery.arg("trac.bug_id");
        break;
    }
    QSettings settings("Entomologist");
    settings.setValue(QString("%1-sort-column").arg(pBackend->name()), logicalIndex);
    settings.setValue(QString("%1-sort-order").arg(pBackend->name()), order);
    reloadFromDatabase();
}

void
TracUI::reloadFromDatabase()
{
    qDebug() << "TracUI reloading...";
    QString showMy, showRep, showCC, showMonitored;
    if (mShowMyBugs)
        showMy = "Assigned";
    else
        showMy = "XXXAssigned";

    if (mShowMyCCs)
        showCC = "CC";
    else
        showCC = "XXXCC";

    if (mShowMyReports)
        showRep = "Reported";
    else
        showRep = "XXXReported";

    if (mShowMonitored)
        showMonitored = "Monitored";
    else
        showMonitored = "XXXMonitored";

    QString tempQuery = QString("WHERE (trac.bug_type=\'%1\' OR trac.bug_type=\'%2\' OR trac.bug_type=\'%3\' OR trac.bug_type=\'%4')")
                          .arg(showMy)
                          .arg(showRep)
                          .arg(showCC)
                          .arg(showMonitored);

    mActiveQuery = mBaseQuery + " " + tempQuery +" AND" + mWhereQuery + mSortQuery;

    pBugModel->setQuery(mActiveQuery);
    qDebug() << pBugModel->query().lastQuery();
}
