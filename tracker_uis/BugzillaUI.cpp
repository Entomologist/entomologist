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

#include <QDebug>
#include "trackers/Backend.h"
#include "BugzillaUI.h"
#include "BugzillaDetails.h"
#include "SqlUtilities.h"
#include "NewCommentsDialog.h"
#include "ui_bugzillaui.h"

#include <SqlBugDelegate.h>
#include <SqlBugModel.h>
#include <QHeaderView>
#include <QSqlQuery>
#include <QMenu>
#include <QCursor>
#include <QDebug>
#include <QSettings>

BugzillaUI::BugzillaUI(const QString &id,
                       const QString &trackerName,
                       Backend *backend,
                       QWidget *parent) :
    BackendUI(id, trackerName, backend, parent),
    ui(new Ui::BugzillaUI)
{
    QSettings settings("Entomologist");

    ui->setupUi(this);
    ui->tableView->setBugTableName("bugzilla");
    ui->tableView->setTrackerId(id);

    v = ui->tableView->horizontalHeader();
    v->setContextMenuPolicy(Qt::CustomContextMenu);
    restoreHeaderSetting();
    connect(ui->tableView, SIGNAL(copyBugURL(QString)),
            this, SLOT(copyBugUrl(QString)));
    connect(ui->tableView, SIGNAL(openBugInBrowser(QString)),
            this, SLOT(openBugInBrowser(QString)));
    connect(v, SIGNAL(customContextMenuRequested(QPoint)),
            this, SLOT(headerContextMenu(QPoint)));
    connect(v, SIGNAL(sortIndicatorChanged(int,Qt::SortOrder)),
            this, SLOT(sortIndicatorChanged(int,Qt::SortOrder)));
    connect(v, SIGNAL(sectionResized(int,int,int)),
            this, SLOT(saveHeaderSetting(int,int,int)));

    connect(ui->tableView, SIGNAL(doubleClicked(QModelIndex)),
            this, SLOT(itemDoubleClicked(QModelIndex)));
    mTableHeaders << ""
                  << tr("Bug ID")
                  << tr("Last Modified")
                  << tr("Severity")
                  << tr("Priority")
                  << tr("Assignee")
                  << tr("Status")
                  << tr("Summary");

    mBaseQuery = "SELECT bugzilla.id, bugzilla.highlight_type, bugzilla.bug_id, bugzilla.last_modified,"
                   "coalesce(shadow_bugzilla.severity, bugzilla.severity),"
                   "coalesce(shadow_bugzilla.priority, bugzilla.priority),"
                   "coalesce(shadow_bugzilla.assigned_to, bugzilla.assigned_to),"
                   "coalesce(shadow_bugzilla.status, bugzilla.status),"
                   "coalesce(shadow_bugzilla.summary, bugzilla.summary) "
                   "from bugzilla left outer join shadow_bugzilla ON bugzilla.bug_id = shadow_bugzilla.bug_id";
    mActiveQuery = mBaseQuery;
    mWhereQuery = QString(" bugzilla.tracker_id=%1 ").arg(id);
    mSortQuery = " ORDER BY bugzilla.bug_id ASC";
    loadFields();

    unsigned int savedSortColumn = settings.value(QString("%1-sort-column").arg(pBackend->name()), 2).toUInt();
    Qt::SortOrder savedSortOrder = static_cast<Qt::SortOrder>(settings.value(QString("%1-sort-order").arg(pBackend->name()), 0).toUInt());
    v->setSortIndicator(savedSortColumn, savedSortOrder);
    setupTable();

    mHiddenColumns = settings.value("hide-bugzilla-columns", QVariantMap()).toMap();
    QMapIterator<QString, QVariant> i(mHiddenColumns);
    while (i.hasNext())
    {
        i.next();
        v->hideSection(i.key().toInt());
    }
}

BugzillaUI::~BugzillaUI()
{
    delete ui;
}

void
BugzillaUI::loadFields()
{
    mSeverities = SqlUtilities::fieldValues(mId, "severity");
    mPriorities = SqlUtilities::fieldValues(mId, "priority");
    mStatuses = SqlUtilities::fieldValues(mId, "status");
    mResolutions = SqlUtilities::fieldValues(mId, "resolution");
}

void
BugzillaUI::commentsDialogClosing(QMap<QString, QString> details, QString newComment)
{
    saveNewShadowItems("shadow_bugzilla", details, newComment);
    reloadFromDatabase();
    emit bugChanged();
}

void
BugzillaUI::headerContextMenu(const QPoint &pos)
{
    int index = v->logicalIndexAt(pos);
    if (index == 1)
        return;

    QString title = mTableHeaders.at(index - 1);
    hideColumnsMenu(pos, "hide-bugzilla-columns", title);
}

void
BugzillaUI::searchResultFinished(QMap<QString, QString> resultMap)
{
    NewCommentsDialog *dialog = new NewCommentsDialog(pBackend, this);
    connect (dialog, SIGNAL(commentsDialogClosing(QMap<QString,QString>,QString)),
             this, SLOT(commentsDialogClosing(QMap<QString,QString>,QString)));
    dialog->setBugInfo(resultMap);
    BugzillaDetails *details = new BugzillaDetails();
    details->setSeverities(resultMap["severity"], mSeverities);
    details->setPriorities(resultMap["priority"], mPriorities);
    details->setStatuses(resultMap["status"], mStatuses);
    details->setResolutions(resultMap["resolution"], mResolutions);
    details->setComponent(resultMap["component"]);
    details->setProduct(resultMap["product"]);
    dialog->setDetailsWidget(details);
    dialog->loadComments();
    stopSearchProgress();
    dialog->show();
    stopSearchProgress();
}


void
BugzillaUI::itemDoubleClicked(const QModelIndex &index)
{
    QString rowId = index.sibling(index.row(), 0).data().toString();
    QMap<QString, QString> detailMap = SqlUtilities::bugzillaBugDetail(rowId);
    NewCommentsDialog *dialog = new NewCommentsDialog(pBackend, this);
    connect (dialog, SIGNAL(commentsDialogClosing(QMap<QString,QString>,QString)),
             this, SLOT(commentsDialogClosing(QMap<QString,QString>,QString)));

    dialog->setBugInfo(detailMap);
    BugzillaDetails *details = new BugzillaDetails();
    details->setSeverities(detailMap["severity"], mSeverities);
    details->setPriorities(detailMap["priority"], mPriorities);
    details->setStatuses(detailMap["status"], mStatuses);
    details->setResolutions(detailMap["resolution"], mResolutions);
    details->setComponent(detailMap["component"]);
    details->setProduct(detailMap["product"]);
    dialog->setDetailsWidget(details);
    connect(details, SIGNAL(fieldChanged(QString,QString)),
            this, SLOT(fieldChanged(QString,QString)));

    dialog->loadComments();
    dialog->show();
}

void
BugzillaUI::loadSearchResult(const QString &id)
{
    startSearchProgress();
    pBackend->getSearchedBug(id);
}

void
BugzillaUI::setupTable()
{
    SqlBugDelegate *delegate = new SqlBugDelegate();
    ui->tableView->setItemDelegate(delegate);
    ui->tableView->setModel(pBugModel);

    pBugModel->setHeaderData(1, Qt::Horizontal, tr(""));
    pBugModel->setHeaderData(2, Qt::Horizontal, tr("Bug ID"));
    pBugModel->setHeaderData(3, Qt::Horizontal, tr("Last Modified"));
    pBugModel->setHeaderData(4, Qt::Horizontal, tr("Severity"));
    pBugModel->setHeaderData(5, Qt::Horizontal, tr("Priority"));
    pBugModel->setHeaderData(6, Qt::Horizontal, tr("Assignee"));
    pBugModel->setHeaderData(7, Qt::Horizontal, tr("Status"));
    pBugModel->setHeaderData(8, Qt::Horizontal, tr("Summary"));
    ui->tableView->setSortingEnabled(true);
    ui->tableView->setGridStyle(Qt::PenStyle(Qt::DotLine));
    ui->tableView->hideColumn(0); // Hide the internal row id
    ui->tableView->horizontalHeader()->setResizeMode(1, QHeaderView::Fixed);
    ui->tableView->verticalHeader()->hide(); // Hide the Row numbers
    ui->tableView->resizeColumnsToContents();
    ui->tableView->resizeRowsToContents();
    ui->tableView->setAlternatingRowColors(true);
}

void
BugzillaUI::sortIndicatorChanged(int logicalIndex, Qt::SortOrder order)
{
    
    QString sortOrder= "ASC";
    if (order == Qt::DescendingOrder)
        sortOrder = "DESC";

    QString newSortQuery =" ORDER BY %1 " + sortOrder;
    switch(logicalIndex)
    {
    case 1:
        mSortQuery = newSortQuery.arg("bugzilla.highlight_type");
        break;
    case 2:
        mSortQuery = newSortQuery.arg("bugzilla.bug_id");
        break;
    case 3:
        mSortQuery = newSortQuery.arg("bugzilla.last_modified");
        break;
    case 4:
        mSortQuery = newSortQuery.arg("bugzilla.severity");
        break;
    case 5:
        mSortQuery = newSortQuery.arg("bugzilla.priority");
        break;
    case 6:
        mSortQuery = newSortQuery.arg("bugzilla.assigned_to");
        break;
    case 7:
        mSortQuery = newSortQuery.arg("bugzilla.status");
        break;
    case 8:
        mSortQuery = newSortQuery.arg("bugzilla.summary");
        break;
    default:
        mSortQuery = newSortQuery.arg("bugzilla.bug_id");
        break;
    }
    QSettings settings("Entomologist");
    settings.setValue(QString("%1-sort-column").arg(pBackend->name()), logicalIndex);
    settings.setValue(QString("%1-sort-order").arg(pBackend->name()), order);
    reloadFromDatabase();
}
void
BugzillaUI::reloadFromDatabase()
{
    qDebug() << "BugzillaUI reloading...";
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

    QString tempQuery = QString("WHERE (bugzilla.bug_type=\'%1\' OR bugzilla.bug_type=\'%2\' OR bugzilla.bug_type=\'%3\' OR bugzilla.bug_type=\'%4')")
                          .arg(showMy)
                          .arg(showRep)
                          .arg(showCC)
                          .arg(showMonitored);

    mActiveQuery = mBaseQuery + " " + tempQuery +" AND" + mWhereQuery + mSortQuery;

    pBugModel->setQuery(mActiveQuery);
    qDebug() << pBugModel->query().lastQuery();
}
