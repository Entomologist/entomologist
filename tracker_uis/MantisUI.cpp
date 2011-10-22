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

#include "MantisUI.h"
#include "MantisDetails.h"
#include "NewCommentsDialog.h"
#include "SqlUtilities.h"
#include "ui_mantisui.h"
#include "trackers/Backend.h"

#include <SqlBugDelegate.h>
#include <SqlBugModel.h>
#include <QHeaderView>
#include <QSqlQuery>
#include <QMenu>
#include <QCursor>
#include <QDebug>
#include <QSettings>

MantisUI::MantisUI(const QString &id,
                   const QString &trackerName,
                   Backend *backend,
                   QWidget *parent) :
    BackendUI(id, trackerName, backend, parent),
    ui(new Ui::MantisUI)
{
    ui->setupUi(this);
    QSettings settings("Entomologist");
    ui->tableView->setBugTableName("mantis");
    ui->tableView->setTrackerId(id);

    v = ui->tableView->horizontalHeader();
    v->setContextMenuPolicy(Qt::CustomContextMenu);

    connect(ui->tableView, SIGNAL(copyBugURL(QString)),
            this, SLOT(copyBugUrl(QString)));
    connect(ui->tableView, SIGNAL(openBugInBrowser(QString)),
            this, SLOT(openBugInBrowser(QString)));
    connect(ui->tableView, SIGNAL(removeSearchedBug(int)),
            this, SLOT(removeSearchedBug(int)));

    connect(v, SIGNAL(customContextMenuRequested(QPoint)),
            this, SLOT(headerContextMenu(QPoint)));
    connect(v, SIGNAL(sortIndicatorChanged(int,Qt::SortOrder)),
            this, SLOT(sortIndicatorChanged(int,Qt::SortOrder)));
    connect(ui->tableView, SIGNAL(doubleClicked(QModelIndex)),
            this, SLOT(itemDoubleClicked(QModelIndex)));
    connect(v, SIGNAL(sectionResized(int,int,int)),
            this, SLOT(saveHeaderSetting(int,int,int)));

    mTableHeaders << ""
                    << tr("Bug ID")
                    << tr("Last Modified")
                    << tr("Project")
                    << tr("Category")
                    << tr("OS")
                    << tr("OS Version")
                    << tr("Product Version")
                    << tr("Severity")
                    << tr("Priority")
                    << tr("Reproducibility")
                    << tr("Assigned To")
                    << tr("Status")
                    << tr("Summary");

    mBaseQuery = "SELECT mantis.id, mantis.highlight_type, mantis.bug_id, mantis.last_modified,"
                   "coalesce(shadow_mantis.project, mantis.project),"
                   "coalesce(shadow_mantis.category, mantis.category),"
                   "coalesce(shadow_mantis.os, mantis.os),"
                   "coalesce(shadow_mantis.os_version, mantis.os_version),"
                   "coalesce(shadow_mantis.product_version, mantis.product_version),"
                   "coalesce(shadow_mantis.severity, mantis.severity),"
                   "coalesce(shadow_mantis.priority, mantis.priority),"
                   "coalesce(shadow_mantis.reproducibility, mantis.reproducibility),"
                   "coalesce(shadow_mantis.assigned_to, mantis.assigned_to),"
                   "coalesce(shadow_mantis.status, mantis.status),"
                   "coalesce(shadow_mantis.summary, mantis.summary) "
                   "from mantis left outer join shadow_mantis ON mantis.bug_id = shadow_mantis.bug_id";
    mActiveQuery = mBaseQuery;
    mWhereQuery = QString(" mantis.tracker_id=%1 ").arg(id);
    mSortQuery = " ORDER BY mantis.bug_id ASC";
    loadFields();

    unsigned int savedSortColumn = settings.value(QString("%1-sort-column").arg(pBackend->name()), 2).toUInt();
    Qt::SortOrder savedSortOrder = static_cast<Qt::SortOrder>(settings.value(QString("%1-sort-order").arg(pBackend->name()), 0).toUInt());
    v->setSortIndicator(savedSortColumn, savedSortOrder);
    setupTable();

    mHiddenColumns = settings.value("hide-mantis-columns", QVariantMap()).toMap();
    QMapIterator<QString, QVariant> i(mHiddenColumns);
    while (i.hasNext())
    {
        i.next();
        v->hideSection(i.key().toInt());
    }

    restoreHeaderSetting();
}

MantisUI::~MantisUI()
{
    delete ui;
}

void
MantisUI::headerContextMenu(const QPoint &pos)
{
    int index = v->logicalIndexAt(pos);
    if (index == 1)
        return;

    QString title = mTableHeaders.at(index - 1);
    hideColumnsMenu(pos, "hide-mantis-columns", title);
}

void
MantisUI::loadFields()
{
    mSeverities = SqlUtilities::fieldValues(mId, "severity");
    mPriorities = SqlUtilities::fieldValues(mId, "priority");
    mStatuses = SqlUtilities::fieldValues(mId, "status");
    mResolutions = SqlUtilities::fieldValues(mId, "resolution");
    mRepro = SqlUtilities::fieldValues(mId, "reproducibility");
}
void
MantisUI::loadSearchResult(const QString &id)
{
    startSearchProgress();
    pBackend->getSearchedBug(id);
}
void
MantisUI::searchResultFinished(QMap<QString, QString> resultMap)
{
    NewCommentsDialog *dialog = new NewCommentsDialog(pBackend, this);
    connect (dialog, SIGNAL(commentsDialogClosing(QMap<QString,QString>,QString)),
             this, SLOT(searchCommentsDialogClosing(QMap<QString,QString>,QString)));
    connect(dialog, SIGNAL(commentsDialogCanceled(QString,QString)),
            this, SLOT(commentsDialogCanceled(QString,QString)));

    dialog->setBugInfo(resultMap);

    MantisDetails *details = new MantisDetails(resultMap["bug_id"]);
    details->setProject(resultMap["project"]);
    details->setVersion(resultMap["product_version"]);
    details->setCategory(resultMap["category"]);
    details->setSeverities(resultMap["severity"], mSeverities);
    details->setPriorities(resultMap["priority"], mPriorities);
    details->setStatuses(resultMap["status"], mStatuses);
    details->setResolutions(resultMap["resolution"], mResolutions);
    details->setReproducibility(resultMap["reproducibility"], mRepro);
    QStringList assignedVals = SqlUtilities::assignedToValues("mantis", mId);
    details->setAssigneds(resultMap["assigned_to"], assignedVals);
    dialog->setDetailsWidget(details);
    dialog->loadComments();
    stopSearchProgress();
    dialog->show();
    stopSearchProgress();
}

void
MantisUI::itemDoubleClicked(const QModelIndex &index)
{
    QString rowId = index.sibling(index.row(), 0).data().toString();
    QMap<QString, QString> detailMap = SqlUtilities::mantisBugDetail(rowId);
    NewCommentsDialog *dialog = new NewCommentsDialog(pBackend, this);
    connect (dialog, SIGNAL(commentsDialogClosing(QMap<QString,QString>,QString)),
             this, SLOT(commentsDialogClosing(QMap<QString,QString>,QString)));

    dialog->setBugInfo(detailMap);
    MantisDetails *details = new MantisDetails(detailMap["bug_id"]);
    details->setProject(detailMap["project"]);
    details->setVersion(detailMap["product_version"]);
    details->setCategory(detailMap["category"]);
    details->setSeverities(detailMap["severity"], mSeverities);
    details->setPriorities(detailMap["priority"], mPriorities);
    details->setStatuses(detailMap["status"], mStatuses);
    details->setResolutions(detailMap["resolution"], mResolutions);
    details->setReproducibility(detailMap["reproducibility"], mRepro);
    QStringList assignedVals = SqlUtilities::assignedToValues("mantis", mId);
    details->setAssigneds(detailMap["assigned_to"], assignedVals);
    dialog->setDetailsWidget(details);
    dialog->loadComments();
    dialog->show();
}

void
MantisUI::setupTable()
{
    SqlBugDelegate *delegate = new SqlBugDelegate();
    ui->tableView->setItemDelegate(delegate);
    ui->tableView->setModel(pBugModel);
    pBugModel->setHeaderData(1, Qt::Horizontal, "");
    pBugModel->setHeaderData(2, Qt::Horizontal, tr("Bug ID"));
    pBugModel->setHeaderData(3, Qt::Horizontal, tr("Last Modified"));
    pBugModel->setHeaderData(4, Qt::Horizontal, tr("Project"));
    pBugModel->setHeaderData(5, Qt::Horizontal, tr("Category"));
    pBugModel->setHeaderData(6, Qt::Horizontal, tr("OS"));
    pBugModel->setHeaderData(7, Qt::Horizontal, tr("OS Version"));
    pBugModel->setHeaderData(8, Qt::Horizontal, tr("Product Version"));
    pBugModel->setHeaderData(9, Qt::Horizontal, tr("Severity"));
    pBugModel->setHeaderData(10, Qt::Horizontal, tr("Priority"));
    pBugModel->setHeaderData(11, Qt::Horizontal, tr("Reproducibility"));
    pBugModel->setHeaderData(12, Qt::Horizontal, tr("Assigned To"));
    pBugModel->setHeaderData(13, Qt::Horizontal, tr("Status"));
    pBugModel->setHeaderData(14, Qt::Horizontal, tr("Summary"));

    ui->tableView->setSortingEnabled(true);
    ui->tableView->setGridStyle(Qt::PenStyle(Qt::DotLine));
    ui->tableView->hideColumn(0); // Hide the internal row id
    ui->tableView->verticalHeader()->hide(); // Hide the Row numbers
    ui->tableView->horizontalHeader()->setResizeMode(1, QHeaderView::Fixed);
    ui->tableView->resizeColumnsToContents();
    ui->tableView->resizeRowsToContents();
    ui->tableView->setAlternatingRowColors(true);
}

void
MantisUI::sortIndicatorChanged(int logicalIndex, Qt::SortOrder order)
{

    QString sortOrder= "ASC";
    if (order == Qt::DescendingOrder)
        sortOrder = "DESC";

    QString newSortQuery =" ORDER BY %1 " + sortOrder;
    switch(logicalIndex)
    {
    case 1:
        mSortQuery = newSortQuery.arg("mantis.highlight_type");
        break;
    case 2:
        mSortQuery = newSortQuery.arg("mantis.bug_id");
        break;
    case 3:
        mSortQuery = newSortQuery.arg("mantis.last_modified");
        break;
    case 4:
        mSortQuery = newSortQuery.arg("mantis.project");
        break;
    case 5:
        mSortQuery = newSortQuery.arg("mantis.category");
        break;
    case 6:
        mSortQuery = newSortQuery.arg("mantis.os");
        break;
    case 7:
        mSortQuery = newSortQuery.arg("mantis.os_version");
        break;
    case 8:
        mSortQuery = newSortQuery.arg("mantis.product_version");
        break;
    case 9:
        mSortQuery = newSortQuery.arg("mantis.severity");
        break;
    case 10:
        mSortQuery = newSortQuery.arg("mantis.priority");
        break;
    case 11:
        mSortQuery = newSortQuery.arg("mantis.reproducibility");
        break;
    case 12:
        mSortQuery = newSortQuery.arg("mantis.assigned_to");
        break;
    case 13:
        mSortQuery = newSortQuery.arg("mantis.status");
        break;
    case 14:
        mSortQuery = newSortQuery.arg("mantis.summary");
        break;
    default:
        mSortQuery = newSortQuery.arg("mantis.bug_id");
        break;
    }
    QSettings settings("Entomologist");
    settings.setValue(QString("%1-sort-column").arg(pBackend->name()), logicalIndex);
    settings.setValue(QString("%1-sort-order").arg(pBackend->name()), order);
    reloadFromDatabase();
}
void
MantisUI::reloadFromDatabase()
{
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

    QString tempQuery = QString("WHERE (mantis.bug_type=\'Searched\' OR mantis.bug_type=\'%1\' OR mantis.bug_type=\'%2\' OR mantis.bug_type=\'%3\' OR mantis.bug_type=\'%4')")
                          .arg(showMy)
                          .arg(showRep)
                          .arg(showCC)
                          .arg(showMonitored);

    mActiveQuery = mBaseQuery + " " + tempQuery +" AND" + mWhereQuery + mSortQuery;

    pBugModel->setQuery(mActiveQuery);
}
