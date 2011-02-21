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

#include <QtSql>
#include <QScrollBar>
#include <QCloseEvent>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QListWidget>
#include <QFile>
#include <QMessageBox>
#include <QDesktopServices>
#include <QSqlTableModel>
#include <QMovie>
#include <QProgressDialog>
#include <QSpacerItem>
#include <QImageWriter>
#include <QSystemTrayIcon>

#include "About.h"
#include "Preferences.h"
#include "UploadDialog.h"
#include "CommentFrame.h"
#include "SqlBugDelegate.h"
#include "SqlBugModel.h"
#include "Bugzilla.h"
#include "NovellBugzilla.h"
#include "NewTracker.h"
#include "MainWindow.h"
#include "Autodetector.h"
#include "ui_MainWindow.h"

// TODOs:
// - Use XMLRPC and configuration dialogs from Qxt?
// - URL handing needs to be improved
// - Autoscrolling in the details frame when a comment is added doesn't work right
// - sqlite database versioning + migration
// - Retrieve resolved bugs as well?
// - Pressing cancel during tracker detection should actually cancel
// - Bug resolution - hardcode for bugzilla 3.4. 3.6 should be listable with the Bug.fields call
// - Move the SQL out of MainWindow and into the SqlBugModel
// - Bookmarking of bugs
// - 'Bug TODO list' maybe
// - Highlight new bugs
// - QtDBUS on linux for network insertion integration
// - Search combo box is too small

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    // mSyncRequests tracks how many sync requests have been made
    // in order to know when to re-enable the widgets
    mSyncRequests = 0;
    QSettings settings("Entomologist");

    // Resize the splitters, otherwise they'll be 50/50
    QList<int> splitterSizes;
    splitterSizes << 100;
    splitterSizes << 400;
    pManager = new QNetworkAccessManager();

    ui->setupUi(this);
    setupTrayIcon();
    ui->searchEdit->setPlaceholderText(tr("Search summaries and comments"));
    // Setup the "Show" menu and "Work Offline"
    ui->actionMy_Bugs->setChecked(settings.value("show-my-bugs", true).toBool());
    ui->actionMy_Reports->setChecked(settings.value("show-my-reports", true).toBool());
    ui->actionMy_CCs->setChecked(settings.value("show-my-ccs", true).toBool());
    ui->action_Work_Offline->setChecked(settings.value("work-offline", false).toBool());

    // We want context menus on right clicks in the tracker list,
    // and we want to ignore the scroll wheel in certain combo boxes
    ui->trackerList->setContextMenuPolicy(Qt::CustomContextMenu);
    ui->bugTable->setContextMenuPolicy(Qt::CustomContextMenu);
    ui->priorityCombo->installEventFilter(this);
    ui->severityCombo->installEventFilter(this);
    ui->statusCombo->installEventFilter(this);
    ui->splitter->setSizes(splitterSizes);

    // Set the default network status
    pStatusIcon = new QLabel();
    pStatusIcon->setPixmap(QPixmap(":/online"));
    pStatusMessage = new QLabel("");
    ui->statusBar->addPermanentWidget(pStatusMessage);
    ui->statusBar->addPermanentWidget(pStatusIcon);

    // Hide the details area until a bug is double clicked
    ui->detailsScrollArea->hide();

    // We use a spinner animation to show that we're doing things
    pSpinnerMovie = new QMovie(this);
    pSpinnerMovie->setFileName(":/spinner");
    pSpinnerMovie->setScaledSize(QSize(48,48));
    ui->spinnerLabel->setMovie(pSpinnerMovie);
    ui->spinnerLabel->hide();
    ui->syncingLabel->hide();

    // Set up the resync timer
    pUpdateTimer = new QTimer(this);
    connect(pUpdateTimer, SIGNAL(timeout()),
            this, SLOT(resync()));
    setTimer();

    // Menu actions
    connect(ui->action_Add_Tracker, SIGNAL(triggered()),
            this, SLOT(addTrackerTriggered()));
    connect(ui->action_About, SIGNAL(triggered()),
            this, SLOT(aboutTriggered()));
    connect(ui->action_Web_Site, SIGNAL(triggered()),
            this, SLOT(websiteTriggered()));
    connect(ui->action_Preferences, SIGNAL(triggered()),
            this, SLOT(prefsTriggered()));
    connect(ui->action_Quit, SIGNAL(triggered()),
            this, SLOT(quitEvent()));
//            this, SLOT(close()));
    connect(ui->actionMy_Bugs, SIGNAL(triggered()),
            this, SLOT(showActionTriggered()));
    connect(ui->actionMy_CCs, SIGNAL(triggered()),
            this, SLOT(showActionTriggered()));
    connect(ui->actionMy_Reports, SIGNAL(triggered()),
            this, SLOT(showActionTriggered()));
    connect(ui->action_Work_Offline, SIGNAL(triggered()),
            this, SLOT(workOfflineTriggered()));
    connect(ui->bugTable, SIGNAL(customContextMenuRequested(QPoint)),
            this, SLOT(tableViewContextMenu(QPoint)));
    // Set up the search button
    connect(ui->searchButton, SIGNAL(clicked()),
            this, SLOT(searchTriggered()));
    connect(ui->searchEdit, SIGNAL(returnPressed()),
            this, SLOT(searchTriggered()));

    // Bug list sorting
    connect(ui->bugTable->horizontalHeader(), SIGNAL(sortIndicatorChanged(int,Qt::SortOrder)),
            this, SLOT(sortIndicatorChanged(int,Qt::SortOrder)));

    // Handle certain events in the tracker list
    connect(ui->trackerList, SIGNAL(itemChanged(QListWidgetItem*)),
            this, SLOT(trackerListItemChanged(QListWidgetItem*)));
    connect(ui->trackerList, SIGNAL(customContextMenuRequested(QPoint)),
            this, SLOT(customContextMenuRequested(QPoint)));
    connect(ui->trackerList, SIGNAL(itemClicked(QListWidgetItem*)),
            this, SLOT(trackerItemClicked(QListWidgetItem*)));

    // And finally set up the various other widgets
    connect(ui->newCommentButton, SIGNAL(clicked()),
            this, SLOT(newCommentClicked()));
    connect(ui->refreshButton, SIGNAL(clicked()),
            this, SLOT(resync()));
    connect(ui->uploadButton, SIGNAL(clicked()),
            this, SLOT(upload()));
    connect(ui->priorityCombo, SIGNAL(currentIndexChanged(QString)),
            this, SLOT(priorityChanged(QString)));
    connect(ui->severityCombo, SIGNAL(currentIndexChanged(QString)),
            this, SLOT(severityChanged(QString)));
    connect(ui->statusCombo, SIGNAL(currentIndexChanged(QString)),
            this, SLOT(statusChanged(QString)));

    restoreGeometry(settings.value("window-geometry").toByteArray());

    // Set the network status bar
    isOnline();

    setupDB();
    toggleButtons();
    loadTrackers();
}

MainWindow::~MainWindow()
{
    delete ui;
}

// Small SQL utility functions
bool
MainWindow::hasPendingChanges()
{
    QSqlQuery q;
    q.exec("SELECT COUNT(id) FROM shadow_bugs");
    q.next();
    if (q.value(0).toInt() > 0)
        return true;

    q.exec("SELECT COUNT(id) FROM shadow_comments");
    q.next();
    if (q.value(0).toInt() > 0)
        return true;

    return false;
}

bool
MainWindow::trackerNameExists(const QString &name)
{
    bool ret = false;
    QSqlQuery q;
    q.prepare("SELECT COUNT(id) FROM trackers WHERE name = :name");
    q.bindValue(":name", name);
    if (q.exec())
    {
        q.next();
        if (q.value(0).toInt() > 0)
            ret = true;
    }
    return ret;
}

// We have three main tables: trackers, bugs, and comments.
// Bugs and comments have shadow tables that store modified values,
// and then we merge the data using an SQL query.

void
MainWindow::setupDB()
{
    QString createTrackerSql = "CREATE TABLE trackers(id INTEGER PRIMARY KEY,"
                                                      "type TEXT,"
                                                      "name TEXT,"
                                                      "url TEXT,"
                                                      "username TEXT,"
                                                      "password TEXT,"
                                                      "last_sync TEXT,"
                                                      "version TEXT,"
                                                      "valid_priorities TEXT,"
                                                      "valid_severities TEXT,"
                                                      "valid_statuses TEXT)";

    QString createBugSql = "CREATE TABLE %1 (id INTEGER PRIMARY KEY,"
                                              "tracker_id INTEGER,"
                                              "bug_id TEXT,"
                                              "severity TEXT,"
                                              "priority TEXT,"
                                              "assigned_to TEXT,"
                                              "status TEXT,"
                                              "summary TEXT,"
                                              "component TEXT,"
                                              "product TEXT,"
                                              "bug_type TEXT)";

    mBaseQuery = "SELECT bugs.id, bugs.tracker_id, trackers.name, bugs.bug_id,"
                   "coalesce(shadow_bugs.severity, bugs.severity),"
                   "coalesce(shadow_bugs.priority, bugs.priority),"
                   "coalesce(shadow_bugs.assigned_to, bugs.assigned_to),"
                   "coalesce(shadow_bugs.status, bugs.status),"
                   "coalesce(shadow_bugs.summary, bugs.summary)"
                   "from bugs left outer join shadow_bugs ON bugs.bug_id = shadow_bugs.bug_id AND bugs.tracker_id = shadow_bugs.tracker_id "
                   "join trackers ON bugs.tracker_id = trackers.id";

    mActiveQuery = mBaseQuery;

    QString createCommentsSql = "CREATE TABLE %1 (id INTEGER PRIMARY KEY,"
                               "tracker_id INTEGER,"
                               "bug_id INTEGER,"
                               "comment_id INTEGER,"
                               "author TEXT,"
                               "comment TEXT,"
                               "timestamp TEXT,"
                               "private INTEGER)";

    QString dbPath = QDesktopServices::storageLocation(QDesktopServices::DataLocation);
    dbPath.append(QDir::separator()).append("entomologist.bugs.db");
    QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE");
    db.setDatabaseName(dbPath);

    if (!QFile::exists(dbPath))
    {
        qDebug() << "Creating database";
        db.open();

        // Create database
        QSqlQuery query(db);
        if (!query.exec(createTrackerSql))
        {
            QMessageBox box;
            box.setText("Could not create tracker table");
            box.exec();
        }

        if (!query.exec(QString(createBugSql).arg("bugs")))
        {
            QMessageBox box;
            box.setText("Could not create bug table");
            box.exec();
        }

        if (!query.exec(QString(createBugSql).arg("shadow_bugs")))
        {
            QMessageBox box;
            box.setText("Could not create bug table");
            box.exec();
        }

        if (!query.exec(QString(createCommentsSql).arg("comments")))
        {
            QMessageBox box;
            box.setText("Could not create comment table");
            box.exec();
        }

        if (!query.exec(QString(createCommentsSql).arg("shadow_comments")))
        {
            QMessageBox box;
            box.setText("Could not create shadow comments table");
            box.exec();
        }
    }
    else
    {
        qDebug() << "DB is open";
        db.open();
    }

    pBugModel = new SqlBugModel;
    filterTable();
    SqlBugDelegate *delegate = new SqlBugDelegate();

    ui->bugTable->setItemDelegate(delegate);
    ui->bugTable->setModel(pBugModel);
    pBugModel->setHeaderData(2, Qt::Horizontal, tr("Tracker"));
    pBugModel->setHeaderData(3, Qt::Horizontal, tr("Bug ID"));
    pBugModel->setHeaderData(4, Qt::Horizontal, tr("Severity"));
    pBugModel->setHeaderData(5, Qt::Horizontal, tr("Priority"));
    pBugModel->setHeaderData(6, Qt::Horizontal, tr("Assignee"));
    pBugModel->setHeaderData(7, Qt::Horizontal, tr("Status"));
    pBugModel->setHeaderData(8, Qt::Horizontal, tr("Summary"));
    ui->bugTable->hideColumn(0); // Hide the internal row id
    ui->bugTable->hideColumn(1); // Hide the tracker id
    connect(ui->bugTable, SIGNAL(doubleClicked(QModelIndex)),
            this, SLOT(bugClicked(QModelIndex)));
}

// Triggered when a user double clicks a bug in the list
void
MainWindow::bugClicked(const QModelIndex &index)
{
    QModelIndex idIndex = pBugModel->index(index.row(), 0);
    loadDetails(idIndex.data().toLongLong());
    ui->detailsScrollArea->show();
}

// When a user double clicks a bug, this shows the details
// at the bottom of the screen
void
MainWindow::loadDetails(long long id)
{
    QSqlQueryModel mergedModel;
    QString sql = mBaseQuery + QString(" WHERE bugs.id=%1").arg(id);
    mergedModel.setQuery(sql);

    QSqlRecord r = mergedModel.record(0);
    QString trackerId = r.value(1).toString();
    mActiveBugId = r.value(3).toString();
    pActiveBackend = mBackendMap[trackerId];
    if (pActiveBackend == NULL)
    {
        qDebug() << "backend is NULL!";
        return;
    }

    mLoadingDetails = true;
    ui->priorityCombo->clear();
    ui->priorityCombo->insertItems(0, pActiveBackend->validPriorities());
    mActivePriority = r.value(5).toString();
    mActivePriority.remove(QRegExp("<[^>]*>"));
    ui->priorityCombo->setCurrentIndex(ui->priorityCombo->findText(mActivePriority));
    ui->severityCombo->clear();
    ui->severityCombo->insertItems(0, pActiveBackend->validSeverities());
    mActiveSeverity = r.value(4).toString();
    mActiveSeverity.remove(QRegExp("<[^>]*>"));
    ui->severityCombo->setCurrentIndex(ui->severityCombo->findText(mActiveSeverity));
    ui->statusCombo->clear();
    ui->statusCombo->insertItems(0, pActiveBackend->validStatuses());
    mActiveStatus = r.value(7).toString();
    mActiveStatus.remove(QRegExp("<[^>]*>"));
    ui->statusCombo->setCurrentIndex(ui->statusCombo->findText(mActiveStatus));

    QLayoutItem *child = NULL;
    while ((child = ui->commentLayout->takeAt(0)) != NULL)
    {
        delete child->widget();
        delete child;
    }

    // This spacer is used in the details area to keep the comments in order.
    // We keep track of it so it can be removed from the layout when a new comment is added,
    // and then reinserted to force the layout again.
    pCommentSpacer = new QSpacerItem(20, 40, QSizePolicy::Minimum, QSizePolicy::Expanding);
    QSqlTableModel model;
    model.setTable("comments");
    model.setFilter(QString("bug_id=%1 AND tracker_id=%2").arg(mActiveBugId).arg(trackerId));
    model.select();
    for(int i = 0; i < model.rowCount(); ++i)
    {
        QSqlRecord record = model.record(i);
        CommentFrame *newFrame = new CommentFrame(ui->detailsScrollArea);
        ui->commentLayout->addWidget(newFrame);
        newFrame->setName(record.value(4).toString());
        newFrame->setComment(record.value(5).toString());
        newFrame->setDate(record.value(6).toString());
        newFrame->setPrivate(record.value(7).toBool());
        newFrame->setBugId(mActiveBugId);
    }

    model.setTable("shadow_comments");
    model.setFilter(QString("bug_id=%1 AND tracker_id=%2").arg(mActiveBugId).arg(trackerId));
    model.select();
    for(int i = 0; i < model.rowCount(); ++i)
    {
        QSqlRecord record = model.record(i);
        CommentFrame *newFrame = new CommentFrame(ui->detailsScrollArea);
        newFrame->setRedHeader();
        ui->commentLayout->addWidget(newFrame);
        newFrame->setName(record.value(4).toString());
        newFrame->setComment(record.value(5).toString());
        newFrame->setDate(record.value(6).toString());
        newFrame->setPrivate(record.value(7).toBool());
        newFrame->setBugId(mActiveBugId);
    }

    ui->commentLayout->addSpacerItem(pCommentSpacer);
    mLoadingDetails = false;
    ui->detailsScrollArea->show();
}

// Triggered when a user clicks the "Reply" button in the
// bug details
void
MainWindow::newCommentClicked()
{
    ui->commentLayout->removeItem(pCommentSpacer);
    CommentFrame *newFrame = new CommentFrame(ui->detailsScrollArea, true);
    connect(newFrame, SIGNAL(saveCommentClicked()),
            this, SLOT(saveCommentClicked()));
    ui->commentLayout->addWidget(newFrame);
    newFrame->setComment("");
    newFrame->setName(pActiveBackend->email());
    newFrame->setBugId(mActiveBugId);
    newFrame->setDate(QDateTime::currentDateTime().toString("yyyy-MM-ddThh:mm:ss"));
    ui->commentLayout->addSpacerItem(pCommentSpacer);
    ui->detailsScrollArea->verticalScrollBar()->setSliderPosition(
            ui->detailsScrollArea->verticalScrollBar()->maximum() + newFrame->height());
}

// The user would like to save their comment
void
MainWindow::saveCommentClicked()
{
    CommentFrame *frame = qobject_cast<CommentFrame*>(sender());
    QString sql = QString("INSERT INTO shadow_comments (tracker_id, bug_id, author, comment, timestamp)"
                          "VALUES(\'%1\', \'%2\', \'%3\', \'%4\', \'%5\')")
                          .arg(pActiveBackend->id())
                          .arg(frame->bugId())
                          .arg(pActiveBackend->username())
                          .arg(frame->commentText())
                          .arg(frame->timestamp());
    qDebug() << "Saving " << sql;
    QSqlQuery q;
    q.exec(sql);
    frame->toggleEdit();
    toggleButtons();
}

// Loads cached trackers from the database
void
MainWindow::loadTrackers()
{
    QListWidgetItem *trackerLabel = new QListWidgetItem("Trackers");
    trackerLabel->setTextAlignment(Qt::AlignHCenter);
    trackerLabel->setFlags(Qt::ItemIsEnabled);
    ui->trackerList->addItem(trackerLabel);

    // Use a frame to draw the separator between "all trackers" and the list
    QFrame *f = new QFrame( this );
    f->setFrameStyle( QFrame::HLine | QFrame::Sunken );

    QListWidgetItem *spacer = new QListWidgetItem("");
    spacer->setFlags(Qt::NoItemFlags);
    ui->trackerList->addItem(spacer);
    ui->trackerList->setItemWidget(spacer, f);

    QSqlTableModel model;
    model.setTable("trackers");
    model.select();
    for(int i = 0; i < model.rowCount(); ++i)
    {
        QSqlRecord record = model.record(i);
        QMap<QString, QString> info;
        info["id"] = record.value(0).toString();
        info["type"] = record.value(1).toString();
        info["name"] = record.value(2).toString();
        info["url"] = record.value(3).toString();
        info["username"] = record.value(4).toString();
        info["password"] = record.value(5).toString();
        info["last_sync"] = record.value(6).toString();
        info["version"] = record.value(7).toString();
        info["valid_priorities"] = record.value(8).toString();
        info["valid_severities"] = record.value(9).toString();
        info["valid_statuses"] = record.value(10).toString();
        addTracker(info);
    }
}

// This adds trackers to the database (when called from the New Tracker window)
// and to the list of trackers.
void
MainWindow::addTracker(QMap<QString,QString> info)
{
    // If this was called after the user used the Add Tracker window,
    // insert the data into the DB
    if (info["id"] == "-1")
    {
        QString query = QString("INSERT INTO trackers (type, name, url, username, password, last_sync, version, valid_priorities, valid_severities, valid_statuses) "
                                "VALUES (\'%1\', \'%2\', \'%3\', \'%4\', \'%5\', \'%6\', \'%7\', \'%8\', \'%9\', \'%10\')")
                        .arg(info["type"])
                        .arg(info["name"])
                        .arg(info["url"])
                        .arg(info["username"])
                        .arg(info["password"])
                        .arg(info["last_sync"])
                        .arg(info["version"])
                        .arg(info["valid_priorities"])
                        .arg(info["valid_severities"])
                        .arg(info["valid_statuses"]);
        QSqlQuery q;
        if (!q.exec(query))
            qDebug() << "Insert into trackers failed. Query : " << query;
        info["id"] = q.lastInsertId().toString();
    }


    // Create the proper Backend object
    if (QUrl(info["url"]).host() == "bugzilla.novell.com")
    {
        NovellBugzilla *newBug = new NovellBugzilla(info["url"]);
        setupTracker(newBug, info);
    }
    else if (info["type"] == "Bugzilla")
    {
        Bugzilla *newBug = new Bugzilla(info["url"]);
        setupTracker(newBug, info);
    }
}

// Fills in the various bits of information the tracker
// needs to keep around
void
MainWindow::setupTracker(Backend *newBug, QMap<QString, QString> info)
{
    qDebug() << "Setup tracker";
    newBug->setId(info["id"]);
    newBug->setName(info["name"]);
    newBug->setUsername(info["username"]);
    newBug->setPassword(info["password"]);
    newBug->setLastSync(info["last_sync"]);
    newBug->setVersion(info["version"]);
    newBug->setValidPriorities(info["valid_priorities"].split(","));
    newBug->setValidSeverities(info["valid_severities"].split(","));
    newBug->setValidStatuses(info["valid_statuses"].split(","));
    connect(newBug, SIGNAL(bugsUpdated()),
            this, SLOT(bugsUpdated()));
    connect(newBug, SIGNAL(backendError(QString)),
            this, SLOT(backendError(QString)));
    addTrackerToList(newBug);
}

// Adds a tracker to the tracker list on the left
// side of the screen
void
MainWindow::addTrackerToList(Backend *newTracker)
{
    qDebug() << "Adding tracker to list";
    QListWidgetItem *newItem = new QListWidgetItem(newTracker->name());
    newItem->setFlags(Qt::ItemIsUserCheckable|Qt::ItemIsEnabled);
    newItem->setCheckState(Qt::Checked);
    newItem->setData(Qt::UserRole, newTracker->id());
    ui->trackerList->addItem(newItem);

    QString iconPath = QDesktopServices::storageLocation(QDesktopServices::DataLocation);
    iconPath.append(QDir::separator()).append(QString("%1.png").arg(newTracker->name()));
    mBackendMap[newTracker->id()] = newTracker;

    syncTracker(newTracker);
    // TODO this doesn't work for sites that link to the shortcut icon in the html <head>
    if(!QFile::exists(iconPath))
        fetchIcon(newTracker->url(), iconPath);
}

// When the user clicks the sort header in the bug view, this
// does the actual sorting
void
MainWindow::sortIndicatorChanged(int logicalIndex, Qt::SortOrder order)
{
    QString sortOrder= "ASC";
    if (order == Qt::DescendingOrder)
        sortOrder = "DESC";

    QString newSortQuery ="ORDER BY %1 " + sortOrder;
    switch(logicalIndex)
    {
    case 2:
        mSortQuery = newSortQuery.arg("trackers.name");
        break;
    case 3:
        mSortQuery = newSortQuery.arg("bugs.bug_id");
        break;
    case 4:
        mSortQuery = newSortQuery.arg("bugs.severity");
        break;
    case 5:
        mSortQuery = newSortQuery.arg("bugs.priority");
        break;
    case 6:
        mSortQuery = newSortQuery.arg("bugs.assigned_to");
        break;
    case 7:
        mSortQuery = newSortQuery.arg("bugs.status");
        break;
    case 8:
        mSortQuery = newSortQuery.arg("bugs.summary");
        break;
    default:
        mSortQuery = newSortQuery.arg("bugs.bug_id");
        break;
    }

    filterTable();
}

// This slot is connected to a signal in each Backend object,
// so we know when we're done syncing and can let the user
// interact with the application again
void
MainWindow::bugsUpdated()
{

    mSyncRequests--;
    if (mSyncRequests == 0)
    {
        filterTable();
        stopAnimation();
        notifyUser();
    }
}

// After all of the trackers have been synced,
// this loops through and builds up information
// that will then be shown in a task tray popup
void
MainWindow::notifyUser()
{
    if (isVisible())
        return; // We don't show the popup when the window is on-screen

    int total = 0;
    QMapIterator<QString, Backend *> i(mBackendMap);
    while (i.hasNext())
    {
        i.next();
        total += i.value()->latestUpdateCount();
    }

    if (total > 0)
    {
        pTrayIcon->showMessage("Bugs Updated",
                               tr("%n bug(s) updated", "", total),
                               QSystemTrayIcon::Information,
                               5000);
    }
}

void
MainWindow::startAnimation()
{
    ui->spinnerLabel->show();
    ui->syncingLabel->show();
    ui->menuShow->setEnabled(false);
    ui->action_Add_Tracker->setEnabled(false);
    ui->action_Preferences->setEnabled(false);
    ui->action_Work_Offline->setEnabled(false);
    ui->uploadButton->setEnabled(false);
    ui->changelogButton->setEnabled(false);
    ui->refreshButton->setEnabled(false);
    ui->searchEdit->setEnabled(false);
    ui->searchButton->setEnabled(false);
    ui->bugTable->setEnabled(false);
    ui->trackerList->setEnabled(false);
    ui->detailsScrollArea->setEnabled(false);
    ui->splitter->setEnabled(false);
    ui->splitter_2->setEnabled(false);
    pSpinnerMovie->start();
}

void
MainWindow::stopAnimation()
{
    pSpinnerMovie->stop();
    ui->refreshButton->setEnabled(true);
    ui->menuShow->setEnabled(true);
    ui->action_Add_Tracker->setEnabled(true);
    ui->action_Preferences->setEnabled(true);
    ui->action_Work_Offline->setEnabled(true);
    ui->spinnerLabel->hide();
    ui->syncingLabel->hide();
    ui->searchEdit->setEnabled(true);
    ui->searchButton->setEnabled(true);
    ui->bugTable->setEnabled(true);
    ui->trackerList->setEnabled(true);
    ui->detailsScrollArea->setEnabled(true);
    ui->splitter->setEnabled(true);
    ui->splitter_2->setEnabled(true);
    toggleButtons();
}

// Grab the favicon.ico for each tracker to make the bug list prettier
void
MainWindow::fetchIcon(const QString &url,
                      const QString &savePath)
{
    QUrl u(url);
    QString fetch = "https://" + u.host() + "/favicon.ico";

    // Some sites might not use the favicon.ico convention,
    // but instead link to it in the HTML, but I don't think
    // we'll bother with that.

    QNetworkRequest req = QNetworkRequest(QUrl(fetch));
    req.setAttribute(QNetworkRequest::User, QVariant(savePath));
    QNetworkReply *rep = pManager->get(req);
    connect(rep, SIGNAL(finished()),
            this, SLOT(iconDownloaded()));
}

// Callback from fetchIcon
void
MainWindow::iconDownloaded()
{
    QNetworkReply *reply = qobject_cast<QNetworkReply*>(sender());
    if (reply->error())
    {
        reply->close();
        return;
    }
    QString savePath = reply->request().attribute(QNetworkRequest::User).toString();
    QByteArray logoData = reply->readAll();
    // The favicon can be in various formats, so convert it to something
    // we know we can safely display
    QBuffer logoBuffer(&logoData);
    logoBuffer.open(QIODevice::ReadOnly);
    QImageReader reader(&logoBuffer);
    QSize iconSize(16, 16);
    if(reader.canRead())
    {
        while((reader.imageCount() > 1) && (reader.currentImageRect() != QRect(0, 0, 16, 16)))
        {
            if (!reader.jumpToNextImage())
                   break;
        }

        reader.setScaledSize(iconSize);
        const QImage icon = reader.read();
        icon.save(savePath, "PNG");
    }
    reply->close();
}

// Sync a specific tracker
void
MainWindow::syncTracker(Backend *tracker)
{
    if (!isOnline())
        return;

    if (mSyncRequests == 0)
        startAnimation();

    mSyncRequests++;
    tracker->sync();
}

// This is triggered when the user selects File -> Add Tracker
void
MainWindow::addTrackerTriggered()
{
    if (!isOnline())
    {
        QMessageBox box;
        box.setText(tr("Sorry, you can only add a new tracker when you're connected to the Internet."));
        box.exec();
        return;
    }

    NewTracker t(this);
    if (t.exec() == QDialog::Accepted)
    {
        QMap<QString, QString> info = t.data();
        if (trackerNameExists(info["name"]))
        {
            QMessageBox box;
            box.setText(QString("You already have a tracker named \"%1\"").arg(info["name"]));
            box.exec();
            return;
        }

        pDetectorProgress = new QProgressDialog(tr("Detecting tracker type..."), tr("Cancel"), 0, 0);
        pDetectorProgress->setWindowModality(Qt::ApplicationModal);
        pDetectorProgress->setMinimumDuration(0);
        pDetectorProgress->setValue(1);

        Autodetector *detector = new Autodetector();
        connect(detector, SIGNAL(finishedDetecting(QMap<QString,QString>)),
                this, SLOT(finishedDetecting(QMap<QString,QString>)));
        detector->detect(info);
    }
}

// Signal called after the autodetector is finished
void
MainWindow::finishedDetecting(QMap<QString, QString> data)
{
    qDebug() << "Finished detecting";
    Autodetector *detector = qobject_cast<Autodetector*>(sender());

    if (pDetectorProgress->wasCanceled())
        return;

    pDetectorProgress->reset();
    if (data["type"] == "Unknown")
    {
        QMessageBox box;
        box.setText("Couldn't detect tracker type");
        box.exec();
        delete detector;
        return;
    }

    delete detector;
    addTracker(data);
}

// This is called when the user presses the refresh button
void
MainWindow::resync()
{
    if (!isOnline())
    {
        QMessageBox box;
        box.setText(tr("You are not currently online!"));
        box.exec();
        return;
    }

    QMapIterator<QString, Backend *> i(mBackendMap);
    while (i.hasNext())
    {
        i.next();
        syncTracker(i.value());
    }
}

// This is called when the user presses the upload button
void
MainWindow::upload()
{
    QSettings settings("Entomologist");
    bool noUploadDialog = settings.value("no-upload-confirmation", false).toBool();
    bool reallyUpload = true;

    if (!noUploadDialog)
    {
        UploadDialog *newDialog = new UploadDialog(this);
        newDialog->setChangelog(getChangelog());
        ui->detailsScrollArea->hide();
        if (newDialog->exec() != QDialog::Accepted)
            reallyUpload = false;
        delete newDialog;
    }

    if (reallyUpload)
    {
        startAnimation();
        QMapIterator<QString, Backend *> i(mBackendMap);
        while (i.hasNext())
        {
            i.next();
            mSyncRequests++;
            i.value()->uploadAll();
        }
    }

}

// Build up a changelog for the changelog dialog box
QString
MainWindow::getChangelog()
{
    QString ret;
    QSqlQueryModel model;
    QString bugsQuery = "SELECT trackers.name, "
                   "shadow_bugs.bug_id, "
                   "bugs.severity, "
                   "shadow_bugs.severity, "
                   "bugs.priority, "
                   "shadow_bugs.priority, "
                   "bugs.assigned_to, "
                   "shadow_bugs.assigned_to, "
                   "bugs.status, "
                   "shadow_bugs.status, "
                   "bugs.summary, "
                   "shadow_bugs.summary "
                   "from shadow_bugs left outer join bugs on shadow_bugs.bug_id = bugs.bug_id AND shadow_bugs.tracker_id = bugs.tracker_id "
                   "join trackers ON shadow_bugs.tracker_id = trackers.id";
    QString commentsQuery = "SELECT trackers.name, "
                            "shadow_comments.bug_id, "
                            "shadow_comments.comment "
                            "from shadow_comments join trackers ON shadow_comments.tracker_id = trackers.id";
    QString entry = tr("&bull; <b>%1</b> bug <font color=\"blue\"><u>%2</u></font>: Change "
                       "<b>%3</b> from <b>%4</b> to <b>%5</b><br>");
    QString commentEntry = tr("&bull; Add a comment to <b>%1</b> bug <font color=\"blue\"><b>%2</u></font>:<br>"
                              "<i>%3</i><br>");
    model.setQuery(bugsQuery);
    for (int i = 0; i < model.rowCount(); ++i)
    {
        QSqlRecord record = model.record(i);
        if (!record.value(3).isNull())
        {
            ret += entry.arg(record.value(0).toString(),
                             record.value(1).toString(),
                             tr("Severity"),
                             record.value(2).toString(),
                             record.value(3).toString());
        }

        if (!record.value(5).isNull())
        {
            ret += entry.arg(record.value(0).toString(),
                             record.value(1).toString(),
                             tr("Priority"),
                             record.value(4).toString(),
                             record.value(5).toString());
        }

        if (!record.value(7).isNull())
        {
            ret += entry.arg(record.value(0).toString(),
                             record.value(1).toString(),
                             tr("Assigned To"),
                             record.value(6).toString(),
                             record.value(7).toString());
        }
        if (!record.value(9).isNull())
        {
            ret += entry.arg(record.value(0).toString(),
                             record.value(1).toString(),
                             tr("Status"),
                             record.value(8).toString(),
                             record.value(9).toString());
        }
        if (!record.value(11).isNull())
        {
            ret += entry.arg(record.value(0).toString(),
                             record.value(1).toString(),
                             tr("Summary"),
                             record.value(10).toString(),
                             record.value(11).toString());
        }
    }

    model.setQuery(commentsQuery);
    for (int i = 0; i < model.rowCount(); ++i)
    {
        QSqlRecord record = model.record(i);
        ret += commentEntry.arg(record.value(0).toString(),
                                record.value(1).toString(),
                                record.value(2).toString());
    }
    return ret;
}

void
MainWindow::quitEvent()
{
    if (isVisible())
    {
        QSettings settings("Entomologist");
        settings.setValue("window-geometry", saveGeometry());
    }
    qApp->quit();
}

void
MainWindow::closeEvent(QCloseEvent *event)
{
    if (isVisible())
    {
        // Save the window geometry and position
        QSettings settings("Entomologist");
        settings.setValue("window-geometry", saveGeometry());
        hide();
        event->ignore();
    }
    else
    {
        event->accept();
    }
}

// Help -> Website
void
MainWindow::websiteTriggered()
{
    QDesktopServices::openUrl(QUrl("http://entomologist.sourceforge.net"));
}

// Help -> About
void
MainWindow::aboutTriggered()
{
    About aboutBox;
    aboutBox.exec();
}

// File->Preferences
void
MainWindow::prefsTriggered()
{
    Preferences prefs(this);
    prefs.exec();
    setTimer();
}

void
MainWindow::changeEvent(QEvent *e)
{
    QMainWindow::changeEvent(e);
    switch (e->type())
    {
        case QEvent::WindowStateChange:
            if (isMinimized())
                hide();
            break;
        case QEvent::LanguageChange:
            ui->retranslateUi(this);
            break;
        default:
            break;
    }
}


void
MainWindow::toggleButtons()
{
    if (!hasPendingChanges())
    {
        ui->uploadButton->setEnabled(false);
        ui->changelogButton->setEnabled(false);
    }
    else
    {
        ui->uploadButton->setEnabled(true);
        ui->changelogButton->setEnabled(true);
    }

}

// The next three are all slots connected to the bug details dropdown menus
void
MainWindow::severityChanged(const QString &text)
{
    if (mLoadingDetails || text == mActiveSeverity) return;

    QSqlQuery q;
    if (!hasShadowBug())
    {
        q.exec(QString("INSERT INTO shadow_bugs (tracker_id, bug_id, severity) VALUES (\'%1\', \'%2\', \'<font color=red>%3</font>\')")
               .arg(pActiveBackend->id())
               .arg(mActiveBugId)
               .arg(text));
    }
    else
    {
        q.exec(QString("UPDATE shadow_bugs SET severity = \'<font color=red>%1</font>\' WHERE tracker_id = \'%2\' AND bug_id = \'%3\'")
                .arg(text)
               .arg(pActiveBackend->id())
               .arg(mActiveBugId));
    }
    mActiveSeverity = text;
    filterTable();
    toggleButtons();
}

void
MainWindow::priorityChanged(const QString &text)
{
    if (mLoadingDetails || text == mActivePriority) return;

    QSqlQuery q;
    if (!hasShadowBug())
    {
        q.exec(QString("INSERT INTO shadow_bugs (tracker_id, bug_id, priority) VALUES (\'%1\', \'%2\', \'<font color=red>%3</font>\')")
               .arg(pActiveBackend->id())
               .arg(mActiveBugId)
               .arg(text));
    }
    else
    {
        q.exec(QString("UPDATE shadow_bugs SET priority = \'<font color=red>%1</font>\' WHERE tracker_id = \'%2\' AND bug_id = \'%3\'")
                .arg(text)
               .arg(pActiveBackend->id())
               .arg(mActiveBugId));
    }
    mActivePriority = text;
    filterTable();
    toggleButtons();
}

void
MainWindow::statusChanged(const QString &text)
{
    if (mLoadingDetails || text == mActiveStatus) return;
    QSqlQuery q;
    if (!hasShadowBug())
    {
        q.exec(QString("INSERT INTO shadow_bugs (tracker_id, bug_id, status) VALUES (\'%1\', \'%2\', \'<font color=red>%3</font>\')")
               .arg(pActiveBackend->id())
               .arg(mActiveBugId)
               .arg(text));
    }
    else
    {
        q.exec(QString("UPDATE shadow_bugs SET status = \'<font color=red>%1</font>\' WHERE tracker_id = \'%2\' AND bug_id = \'%3\'")
                .arg(text)
               .arg(pActiveBackend->id())
               .arg(mActiveBugId));
    }
    mActiveStatus = text;
    filterTable();
    toggleButtons();
}

bool
MainWindow::hasShadowBug()
{
    QString sql = QString("SELECT COUNT (id) FROM shadow_bugs WHERE bug_id = \'%1\' AND tracker_id=\'%2\'")
                  .arg(mActiveBugId)
                  .arg(pActiveBackend->id());
    QSqlQuery q;
    q.exec(sql);
    q.next();
    if (q.value(0).toInt() > 0)
        return true;
    else
        return false;
}


// Whenever the user ticks boxes in the Show menu, this fires
void
MainWindow::showActionTriggered()
{
    // Save the new state
    QSettings settings("Entomologist");
    settings.setValue("show-my-bugs", ui->actionMy_Bugs->isChecked());
    settings.setValue("show-my-reports", ui->actionMy_Reports->isChecked());
    settings.setValue("show-my-ccs", ui->actionMy_CCs->isChecked());
    filterTable();
}

void
MainWindow::workOfflineTriggered()
{
    QSettings settings("Entomologist");
    settings.setValue("work-offline", ui->action_Work_Offline->isChecked());
    isOnline();
}

// Refresh the bug list based on the various checkboxes that have been selected
void
MainWindow::filterTable()
{
    QString showMy, showRep, showCC;
    if (ui->actionMy_Bugs->isChecked())
        showMy = "Assigned";
    else
        showMy = "XXXAssigned";
    if (ui->actionMy_CCs->isChecked())
        showCC = "CC";
    else
        showCC = "XXXCC";
    if (ui->actionMy_Reports->isChecked())
        showRep = "Reported";
    else
        showRep = "XXXReported";

    mWhereQuery = QString("WHERE (bugs.bug_type=\'%1\' OR bugs.bug_type=\'%2\' OR bugs.bug_type=\'%3\')")
                          .arg(showMy)
                          .arg(showRep)
                          .arg(showCC);
    mActiveQuery = mBaseQuery + " " +  mWhereQuery + mTrackerQuery + mSortQuery;
    pBugModel->setQuery(mActiveQuery);

}

// Triggered when a user ticks boxes in the tracker list
void
MainWindow::trackerListItemChanged(QListWidgetItem  *item)
{
    if (ui->trackerList->row(item) < 2) return;

    QStringList list;
    for(int i = 2; i < ui->trackerList->count(); i++)
    {
        if (ui->trackerList->item(i)->checkState() == Qt::Checked)
        {
            list << QString("bugs.tracker_id=%1").arg(ui->trackerList->item(i)->data(Qt::UserRole).toString());
        }
    }


    // Don't let the user deselect the only checked box.  That just
    // looks weird.
    if (list.size() > 0)
    {
        mTrackerQuery = " AND (" +  list.join(" OR ") + ")";
        filterTable();
    }
    else
    {
        item->setCheckState(Qt::Checked);
    }

}


// This is a great example of Qt making things easier and easier over time
bool
MainWindow::isOnline()
{
    bool ret = false;

#if QT_VERSION >= 0x040700
    if (pManager->networkAccessible() != QNetworkAccessManager::NotAccessible)
        ret = true;
    else
        ret = false;
#else
    QNetworkInterface iface;
    QList<QNetworkInterface> ifaces = QNetworkInterface::allInterfaces();

    for (int i = 0; i < ifaces.count(); ++i)
    {
        iface = ifaces.at(i);
        if (iface.flags().testFlag(QNetworkInterface::IsUp)
            && !iface.flags().testFlag(QNetworkInterface::IsLoopBack) )
        {
            if (iface.addressEntries().count() >= 1)
            {
                ret = true;
                break;
            }
        }
    }
#endif

    if (ui->action_Work_Offline->isChecked())
        ret = false;

    if (ret)
    {
       pStatusMessage->setText(tr("Online"));
       pStatusIcon->setPixmap(QPixmap(":/online"));
    }
    else
    {
        pStatusMessage->setText(tr("Offline - operating in cached mode"));
        pStatusIcon->setPixmap(QPixmap(":/offline"));

    }

    return ret;

}

// Slot for any error at all from other objects
void
MainWindow::backendError(const QString &message)
{
    QMessageBox box;
    box.setText(tr("An error occurred."));
    box.setDetailedText(message);
    box.exec();
    bugsUpdated();
}

// Called when the user right clicks in the tracker list
void
MainWindow::customContextMenuRequested(const QPoint &pos)
{
    QListWidgetItem *currentItem = ui->trackerList->currentItem();
    if (currentItem == NULL) return;

    QString id = currentItem->data(Qt::UserRole).toString();

    if (id.isEmpty())
    {
        ui->trackerList->setCurrentItem(NULL);
        return;
    }

    QMenu contextMenu(tr("Context menu"), this);
    QAction *editAction = contextMenu.addAction(tr("Edit"));
    QAction *deleteAction = contextMenu.addAction(tr("Delete"));
    QAction *a = contextMenu.exec(QCursor::pos());
    Backend *b = mBackendMap[id];

    if (a == editAction)
    {
        NewTracker t(this, true);
        t.setName(b->name());
        t.setHost(b->url());
        t.setUsername(b->username());
        t.setPassword(b->password());
        if (t.exec() == QDialog::Accepted)
            updateTracker(id, t.data());
    }
    else if (a == deleteAction)
    {
        QMessageBox box;
        box.setText(QString("Are you sure you want to delete %1?").arg(b->name()));
        box.setStandardButtons(QMessageBox::Yes|QMessageBox::No);
        if (box.exec() == QMessageBox::Yes)
        {
            ui->trackerList->takeItem(ui->trackerList->row(currentItem));
            deleteTracker(id);
        }
    }
    // Reset any list selection
    ui->trackerList->setCurrentItem(NULL);
}

// When the user edits tracker information, this is called to save their new
// data
void
MainWindow::updateTracker(const QString &id, QMap<QString, QString> data)
{
    Backend *b = mBackendMap[id];

    QString query = "UPDATE trackers SET name=:name,username=:username,password=:password WHERE id=:id";
    QSqlQuery q;
    q.prepare(query);
    q.bindValue(":name", data["name"]);
    q.bindValue(":username", data["username"]);
    q.bindValue(":password", data["password"]);
    q.bindValue(":id", id);
    if (!q.exec())
    {
        backendError(q.lastError().text());
        return;
    }

    // Rename the icon, if necessary
    QString oldPath = QString ("%1%2%3.png")
                                .arg(QDesktopServices::storageLocation(QDesktopServices::DataLocation))
                                .arg(QDir::separator())
                                .arg(b->name());
    QString newPath = QString ("%1%2%3.png")
                                .arg(QDesktopServices::storageLocation(QDesktopServices::DataLocation))
                                .arg(QDir::separator())
                                .arg(data["name"]);

    if (QFile::exists(oldPath) && !QFile::exists(newPath))
        QFile::rename(oldPath, newPath);

    // Rename the tracker in the list
    QList<QListWidgetItem  *> itemList = ui->trackerList->findItems(b->name(), Qt::MatchExactly);
    itemList.at(0)->setText(data["name"]);
    b->setName(data["name"]);
    b->setUsername(data["username"]);
    b->setPassword(data["password"]);
    filterTable();
}

// The user pressed the search button, or pressed enter
// in the search line edit
void
MainWindow::searchTriggered()
{
    QString searchText = ui->searchEdit->text();
    if (searchText.isEmpty()) return;

    QStringList query;
    // Summary
    query << QString("(bugs.summary LIKE \'\%%1\%\')").arg(searchText);
    // Comment
    QSqlQuery q;
    QString sql = QString("SELECT bug_id FROM comments WHERE comment LIKE \'\%%1\%\'").arg(searchText);
    if (q.exec(sql))
    {
        QStringList idList;
        while (q.next())
        {
            idList << q.value(0).toString();
        }

        query << QString("(bugs.bug_id IN (%1))").arg(idList.join(","));
    }
    q.finish();

    if (query.size() > 0)
    {
        mWhereQuery = "WHERE " + query.join(" OR ");
        mActiveQuery = mBaseQuery + " " +  mWhereQuery + mTrackerQuery + mSortQuery;
        pBugModel->setQuery(mActiveQuery);
    }

}

void
MainWindow::deleteTracker(const QString &id)
{
    Backend *b = mBackendMap[id];
    mBackendMap.remove(id);
    delete b;

    QString query = "DELETE FROM trackers WHERE id=:tracker_id";
    QSqlQuery sql;
    sql.prepare(query);
    sql.bindValue(":tracker_id", id);
    sql.exec();

    query = "DELETE FROM bugs WHERE tracker_id=:tracker_id";
    sql.prepare(query);
    sql.bindValue(":tracker_id", id);
    sql.exec();

    query = "DELETE FROM comments WHERE tracker_id=:tracker_id";
    sql.prepare(query);
    sql.bindValue(":tracker_id", id);
    sql.exec();

    query = "DELETE FROM shadow_bugs WHERE tracker_id = :tracker_id";
    sql.prepare(query);
    sql.bindValue(":tracker_id", id);
    sql.exec();

    query = "DELETE FROM shadow_comments WHERE tracker_id = :tracker_id";
    sql.prepare(query);
    sql.bindValue(":tracker_id", id);
    sql.exec();

    pBugModel->setQuery(mActiveQuery);
}

void
MainWindow::trackerItemClicked(QListWidgetItem  *item)
{
    // Ignore any attempts at selecting an item
    ui->trackerList->setCurrentItem(NULL);
}

void
MainWindow::setTimer()
{
    QSettings settings("Entomologist");
    bool update = settings.value("update-automatically", false).toBool();
    int interval = settings.value("update-interval", 2).toInt();
    if (interval < 2)
        interval = 2;

    // interval is in hours, so convert it to msec
    int timeout = interval * 60 * 60 * 1000;
    pUpdateTimer->setInterval(timeout);
    if (update)
        pUpdateTimer->start();
    else
        pUpdateTimer->stop();
}

void
MainWindow::setupTrayIcon()
{
    pTrayIcon = new QSystemTrayIcon(QIcon(":/logo"), this);
    pTrayIcon->setToolTip("Entomologist");
    connect(pTrayIcon, SIGNAL(activated(QSystemTrayIcon::ActivationReason)),
            this, SLOT(trayActivated(QSystemTrayIcon::ActivationReason)));
    pTrayIconMenu = new QMenu(this);
    pTrayIconMenu->addAction(ui->action_Quit);
    pTrayIcon->setContextMenu(pTrayIconMenu);
    pTrayIcon->show();
}

void
MainWindow::trayActivated(QSystemTrayIcon::ActivationReason reason)
{
    if (reason == QSystemTrayIcon::DoubleClick)
    {
        if (isVisible())
            close();
        else
            showNormal();
    }
}

void
MainWindow::tableViewContextMenu(const QPoint &p)
{
    QModelIndex i = ui->bugTable->indexAt(p);
    QString tracker_id = i.sibling(i.row(), 1).data().toString();
    QString bug = i.sibling(i.row(), 3).data().toString();
    QMenu contextMenu(tr("Bug Menu"), this);
    QAction *copyAction = contextMenu.addAction(tr("Copy URL"));
    QAction *a = contextMenu.exec(QCursor::pos());
    Backend *b = mBackendMap[tracker_id];
    if (a == copyAction)
    {
        b->buildBugUrl(bug);
    }
}

// We don't want users to accidentally change the combobox values if
// they use the scrollwheel to scroll up and down the details view
bool
MainWindow::eventFilter(QObject *obj, QEvent *event)
{
    if (event->type() == QEvent::Wheel)
    {
        return true;
    }
    return false;
}
