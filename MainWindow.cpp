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
 *  Author: David Williams <redache@googlemail.com>
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
#include <QClipboard>
#include <QDesktopServices>
#include <QSqlTableModel>
#include <QMovie>
#include <QProgressDialog>
#include <QSpacerItem>
#include <QImageWriter>
#include <QSystemTrayIcon>
#include <QShortcut>
#include <QKeySequence>

#include "About.h"
#include "ChangelogWindow.h"
#include "Preferences.h"
#include "UploadDialog.h"
#include "CommentFrame.h"
#include "SqlBugDelegate.h"
#include "SqlBugModel.h"
#include "trackers/Bugzilla.h"
#include "trackers/NovellBugzilla.h"
#include "trackers/Launchpad.h"
#include "trackers/Trac.h"
//#include "trackers/Google.h"
#include "trackers/Mantis.h"
#include "NewTracker.h"
#include "MainWindow.h"
#include "Autodetector.h"
#include "ErrorHandler.h"
#include "Utilities.hpp"
#include "MonitorDialog.h"
#include "ui_MainWindow.h"

#define DB_VERSION 5

// TODOs:
// - URL handing needs to be improved
// - Autoscrolling in the details frame when a comment is added doesn't work right
// - Pressing cancel during tracker detection should actually cancel
// - Bug resolution - hardcode for bugzilla 3.4. 3.6 should be listable with the Bug.fields call
// - Move the SQL out of MainWindow and into the SqlBugModel
// - Bookmarking of bugs
// - 'Bug TODO list' maybe
// - Highlight new bugs
// - QtDBUS on linux for network insertion integration
// - Consider orphaned bug changes - when a bug is closed, but there is
//   something in the shadow tables.
// - Right clicking on the tracker in the list failed on Windows.
//   Check other platforms.
// - Bugzilla 3.2/3.4 component fetching could really be refactored

// Bugzilla backends cache all comments on sync
// When a backend does not provide enough functionality to
// load multiple bug details in a single call, we
// give the user the ability to cache specific comments, either by
// right clicking in the bug list, or when they load the details view.

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    pDetectorProgress = NULL;
    mDbUpdated = false;
    // mSyncRequests tracks how many sync requests have been made
    // in order to know when to re-enable the widgets
    mSyncRequests = 0;
    // mSyncPosition is used to iterate through the backend list,
    // so we only sync one repository at a time, rather than flinging
    // requests at all of them at once.
    mSyncPosition = 0;
    mUploading = false;
    QSettings settings("Entomologist");

    // Resize the splitters, otherwise they'll be 50/50
    QList<int> splitterSizes;
    splitterSizes << 100;
    splitterSizes << 400;
    pManager = new QNetworkAccessManager();
    connect(pManager, SIGNAL(sslErrors(QNetworkReply *, const QList<QSslError> &)),
            this, SLOT(handleSslErrors(QNetworkReply *, const QList<QSslError> &)));

    ui->setupUi(this);
    setupTrayIcon();
    ui->searchEdit->setPlaceholderText(tr("Search summaries and comments"));
    // Setup the "Show" menu and "Work Offline"
    ui->actionMy_Bugs->setChecked(settings.value("show-my-bugs", true).toBool());
    ui->actionMy_Reports->setChecked(settings.value("show-my-reports", true).toBool());
    ui->actionMy_CCs->setChecked(settings.value("show-my-ccs", true).toBool());
    ui->actionMonitored_Components->setChecked(settings.value("show-my-monitored", true).toBool());
    ui->action_Work_Offline->setChecked(settings.value("work-offline", false).toBool());

    // We want context menus on right clicks in the tracker list,
    // and we want to ignore the scroll wheel in certain combo boxes
    ui->trackerList->setContextMenuPolicy(Qt::CustomContextMenu);
    ui->priorityCombo->installEventFilter(this);
    ui->severityCombo->installEventFilter(this);
    ui->statusCombo->installEventFilter(this);

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

    // Keyboard shortcuts for search bar focus / upload changes.
    QShortcut* searchFocus;
    QShortcut* uploadChange;

    searchFocus = new QShortcut(QKeySequence(Qt::META + Qt::Key_Space),this);
    searchFocus->setContext(Qt::ApplicationShortcut);
    connect(searchFocus,SIGNAL(activated()),this,SLOT(searchFocusTriggered()));

    uploadChange = new QShortcut(QKeySequence(Qt::CTRL + Qt::Key_S),this);
    uploadChange->setContext(Qt::ApplicationShortcut);
    connect(uploadChange,SIGNAL(activated()),this,SLOT(upload()));

    // Menu actions
    connect(ui->action_Add_Tracker, SIGNAL(triggered()),
            this, SLOT(addTrackerTriggered()));
    connect(ui->action_Refresh_Tracker,SIGNAL(triggered()),this,SLOT(resync()));

    connect(ui->action_About, SIGNAL(triggered()),
            this, SLOT(aboutTriggered()));
    connect(ui->action_Web_Site, SIGNAL(triggered()),
            this, SLOT(websiteTriggered()));
    connect(ui->action_Preferences, SIGNAL(triggered()),
            this, SLOT(prefsTriggered()));
    connect(ui->action_Quit, SIGNAL(triggered()),
            this, SLOT(quitEvent()));
    connect(ui->actionMy_Bugs, SIGNAL(triggered()),
            this, SLOT(showActionTriggered()));
    connect(ui->actionMy_CCs, SIGNAL(triggered()),
            this, SLOT(showActionTriggered()));
    connect(ui->actionMy_Reports, SIGNAL(triggered()),
            this, SLOT(showActionTriggered()));
    connect(ui->actionMonitored_Components, SIGNAL(triggered()),
            this, SLOT(showActionTriggered()));
    connect(ui->actionEdit_Monitored_Components, SIGNAL(triggered()),
            this, SLOT(showEditMonitoredComponents()));
    connect(ui->action_Work_Offline, SIGNAL(triggered()),
            this, SLOT(workOfflineTriggered()));

    // Set up the search button
    connect(ui->searchButton, SIGNAL(clicked()),
            this, SLOT(searchTriggered()));
    connect(ui->searchEdit, SIGNAL(returnPressed()),
            this, SLOT(searchTriggered()));
    connect(ui->changelogButton, SIGNAL(clicked()),
            this, SLOT(changelogTriggered()));


    // Handle certain events in the tracker list
    connect(ui->trackerList, SIGNAL(itemChanged(QListWidgetItem*)),
            this, SLOT(trackerListItemChanged(QListWidgetItem*)));
    connect(ui->trackerList, SIGNAL(customContextMenuRequested(QPoint)),
            this, SLOT(customContextMenuRequested(QPoint)));

    // Signal for a Tab Change so switch the bugTable contents to that tab
    ui->trackerTab->removeTab(0);
    ui->trackerTab->removeTab(0);
    connect(ui->trackerTab,SIGNAL(currentChanged(int)),this,SLOT(populateCurrentTab(int)));
    // Append initial "All" table to the table list.
    QTableView* table = new QTableView;
    trackerTabsList.append(table);
    ui->trackerTab->addTab(table,"All");

    mStartup = 0;
    // Seems like this doesn't work on Windows?  Or is it due to
    // Qt 4.7?  It results in the right click failing to bring up
    // the context menu.
#ifndef Q_WS_WIN
    connect(ui->trackerList, SIGNAL(itemClicked(QListWidgetItem*)),
            this, SLOT(trackerItemClicked(QListWidgetItem*)));
#endif d

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
    if ((settings.value("startup-sync", false).toBool() == true)
       || (mDbUpdated))
        syncNextTracker();
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

// sqlite starts autoincrementing primary keys at 1,
// so it's safe to use this function as both a boolean check and an
// ID lookup
int
MainWindow::trackerNameExists(const QString &name)
{
    int ret = 0;
    QSqlQuery q;
    q.prepare("SELECT id FROM trackers WHERE name = :name");
    q.bindValue(":name", name);
    if (q.exec())
    {
        q.next();
        ret = q.value(0).toInt();
    }
    return ret;
}

// We have three main tables: trackers, bugs, and comments.
// Bugs and comments have shadow tables that store modified values,
// and then we merge the data using an SQL query.

void
MainWindow::setupDB()
{

    mBaseQuery = "SELECT bugs.id, bugs.tracker_id, trackers.name, bugs.bug_id, bugs.last_modified,"
                   "coalesce(shadow_bugs.severity, bugs.severity),"
                   "coalesce(shadow_bugs.priority, bugs.priority),"
                   "coalesce(shadow_bugs.assigned_to, bugs.assigned_to),"
                   "coalesce(shadow_bugs.status, bugs.status),"
                   "coalesce(shadow_bugs.summary, bugs.summary) "
                   "from bugs left outer join shadow_bugs ON bugs.bug_id = shadow_bugs.bug_id AND bugs.tracker_id = shadow_bugs.tracker_id "
                   "join trackers ON bugs.tracker_id = trackers.id";

    mActiveQuery = mBaseQuery;

    mDbPath = QString("%1%2%3%4%5")
              .arg(QDesktopServices::storageLocation(QDesktopServices::DataLocation))
              .arg(QDir::separator())
              .arg("entomologist")
              .arg(QDir::separator())
              .arg("entomologist.bugs.db");

    if (!QFile::exists(mDbPath))
    {
        openDB();
        QSqlDatabase db = QSqlDatabase::database();
        qDebug() << "Will create " << mDbPath;
        if (!db.open())
        {
            qDebug() << "Error " << db.lastError().text();
            ErrorHandler::handleError("Error creating database.", db.lastError().text());
            exit(1);
        }
        createTables();
    }
    else
    {
        openDB();
        QSqlDatabase db = QSqlDatabase::database();

        db.open();
        checkDatabaseVersion();
    }

}

void
MainWindow::openDB()
{
    QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE");
    if (!db.isValid())
    {
        qDebug() << "Couldn't create the database connection";
        ErrorHandler::handleError("Couldn't create the database.", "");
        exit(1);
    }

    db.setDatabaseName(mDbPath);
}

void
MainWindow::createTables()
{
    QString createMetaSql = "CREATE TABLE entomologist(db_version INT)";
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
                                                      "valid_statuses TEXT,"
                                                      "monitored_components TEXT,"
                                                      "auto_cache_comments INTEGER)";

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
                                              "bug_type TEXT,"
                                              "last_modified TEXT)";

    QString createCommentsSql = "CREATE TABLE %1 (id INTEGER PRIMARY KEY,"
                               "tracker_id INTEGER,"
                               "bug_id INTEGER,"
                               "comment_id INTEGER,"
                               "author TEXT,"
                               "comment TEXT,"
                               "timestamp TEXT,"
                               "private INTEGER)";

    qDebug() << "Creating tables";
    // Create database
    QSqlDatabase db = QSqlDatabase::database();
    QSqlQuery query(db);
    if (!query.exec(createTrackerSql))
    {
        qDebug() << "Database error: " << db.lastError().text();
        ErrorHandler::handleError("Couldn't create the tracker table.", db.lastError().text());
        exit(1);
    }

    if (!query.exec(QString(createBugSql).arg("bugs")))
    {
        qDebug() << "Database error: " << db.lastError().text();
        ErrorHandler::handleError("Couldn't create the bugs table.", db.lastError().text());
        exit(1);
    }

    if (!query.exec(QString(createBugSql).arg("shadow_bugs")))
    {
        qDebug() << "Database error: " << db.lastError().text();
        ErrorHandler::handleError("Couldn't create the shadow bugs table.", db.lastError().text());
        exit(1);
    }

    if (!query.exec(QString(createCommentsSql).arg("comments")))
    {
        qDebug() << "Database error: " << db.lastError().text();
        ErrorHandler::handleError("Couldn't create the comment table.", db.lastError().text());
        exit(1);
    }

    if (!query.exec(QString(createCommentsSql).arg("shadow_comments")))
    {
        qDebug() << "Database error: " << db.lastError().text();
        ErrorHandler::handleError("Couldn't create the shadow comment table.", db.lastError().text());
        exit(1);
    }

    if (!query.exec(createMetaSql))
    {
        qDebug() << "Database error: " << db.lastError().text();
        ErrorHandler::handleError("Couldn't create the entomologist table.", db.lastError().text());
        exit(1);
    }

    if (!query.exec(QString("INSERT INTO entomologist VALUES (\'%1\')").arg(DB_VERSION)))
    {
        qDebug() << "Database error: " << db.lastError().text();
        ErrorHandler::handleError("Couldn't set the database version.", db.lastError().text());
        exit(1);
    }
}

void
MainWindow::checkDatabaseVersion()
{
    QSqlQuery query;
    query.exec("SELECT db_version FROM entomologist");
    query.next();
    if (query.value(0).toInt() == DB_VERSION)
        return;

    QList< QMap<QString, QString> > trackerList;
    query.exec("SELECT type, name, url, username, password,"
               "version, valid_priorities, valid_severities,"
               "valid_statuses, auto_cache_comments, monitored_components FROM trackers");
    while (query.next())
    {
        QMap<QString, QString> tracker;
        tracker["type"] = query.value(0).toString();
        tracker["name"] = query.value(1).toString();
        tracker["url"] = query.value(2).toString();
        tracker["username"] = query.value(3).toString();
        tracker["password"] = query.value(4).toString();
        tracker["version"] = query.value(5).toString();
        tracker["valid_priorities"] = query.value(6).toString();
        tracker["valid_severities"] = query.value(7).toString();
        tracker["valid_statuses"] = query.value(8).toString();
        tracker["auto_cache_comments"] = query.value(9).toString();
        tracker["monitored_components"] = query.value(10).toString();
        tracker["last_sync"] = "1970-01-01T12:13:14";
        trackerList << tracker;
    }

    qDebug() << "Version mismatch";
    QSqlDatabase db = QSqlDatabase::database();
    db.close();
    QFile::remove(mDbPath);
    openDB();
    createTables();
    for (int i = 0; i < trackerList.size(); ++i)
        insertTracker(trackerList.at(i));
    mDbUpdated = true;
}

int
MainWindow::insertTracker(QMap<QString, QString> tracker)
{
    QSqlQuery q;
    q.prepare("INSERT INTO trackers (type, name, url, username, password, last_sync, version, valid_priorities, valid_severities, valid_statuses, monitored_components, auto_cache_comments) "
              "VALUES (:type, :name, :url, :username, :password, :last_sync, :version, :valid_priorities, :valid_severities, :valid_statuses, :monitored_components, :auto_cache_comments)");

    q.bindValue(":type", tracker["type"]);
    q.bindValue(":name", tracker["name"]);
    q.bindValue(":url", tracker["url"]);
    q.bindValue(":username", tracker["username"]);
    q.bindValue(":password", tracker["password"]);
    q.bindValue(":last_sync", tracker["last_sync"]);
    q.bindValue(":version", tracker["version"]);
    q.bindValue(":valid_priorities", tracker["valid_priorities"]);
    q.bindValue(":valid_severities", tracker["valid_severities"]);
    q.bindValue(":valid_statuses", tracker["valid_statuses"]);
    q.bindValue(":monitored_components", tracker["monitored_components"]);
    q.bindValue(":auto_cache_comments", tracker["auto_cache_comments"]);
    if (!q.exec())
    {
        qDebug() << "insertTracker failed: " << q.lastError().text();
        return (-1);
    }
    qDebug() << "Tracker: " << q.lastInsertId().toInt();
    return (q.lastInsertId().toInt());
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
    mActivePriority = r.value(6).toString();
    mActivePriority.remove(QRegExp("<[^>]*>"));
    ui->priorityCombo->setCurrentIndex(ui->priorityCombo->findText(mActivePriority));
    ui->severityCombo->clear();
    ui->severityCombo->insertItems(0, pActiveBackend->validSeverities());
    mActiveSeverity = r.value(5).toString();
    mActiveSeverity.remove(QRegExp("<[^>]*>"));
    ui->severityCombo->setCurrentIndex(ui->severityCombo->findText(mActiveSeverity));
    ui->statusCombo->clear();
    ui->statusCombo->insertItems(0, pActiveBackend->validStatuses());
    mActiveStatus = r.value(8).toString();
    mActiveStatus.remove(QRegExp("<[^>]*>"));
    ui->statusCombo->setCurrentIndex(ui->statusCombo->findText(mActiveStatus));

    QString bugDetailsLabel = QString("%1 #%2 - %3")
            .arg(r.value(2).toString())
            .arg(r.value(3).toString())
            .arg(r.value(9).toString());
    ui->bugDetailsLabel->setText(bugDetailsLabel);
    if ((pActiveBackend->autoCacheComments() == "0") && (isOnline()))
    {
        mSyncRequests++;
        startAnimation();
        pActiveBackend->getComments(mActiveBugId);
    }
    else
    {
        loadComments();
    }
}

void
MainWindow::loadComments()
{
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
    model.setFilter(QString("bug_id=%1 AND tracker_id=%2").arg(mActiveBugId).arg(pActiveBackend->id()));
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
    model.setFilter(QString("bug_id=%1 AND tracker_id=%2").arg(mActiveBugId).arg(pActiveBackend->id()));
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
    QString sql = "INSERT INTO shadow_comments (tracker_id, bug_id, author, comment, timestamp) "
                          "VALUES(:tracker_id, :bug_id, :author, :comment, :timestamp)";
    qDebug() << "Saving " << sql;
    QSqlQuery q;
    q.prepare(sql);
    q.bindValue("tracker_id", pActiveBackend->id());
    q.bindValue(":bug_id", frame->bugId());
    q.bindValue(":author", pActiveBackend->username());
    q.bindValue(":comment", frame->commentText());
    q.bindValue(":timestamp", frame->timestamp());
    if (!q.exec())
        ErrorHandler::handleError("Couldn't save the comment!", q.lastError().text());

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
        info["monitored_components"] = record.value(11).toString();
        info["auto_cache_comments"] = record.value(12).toString();
        addTracker(info);
    }
    // Populate the "All" Tab after loading the trackers.
    mStartup = 1;
    populateCurrentTab(0);

}

// This adds trackers to the database (when called from the New Tracker window)
// and to the list of trackers.
void
MainWindow::addTracker(QMap<QString,QString> info)
{
    // Create the proper Backend object
    if (QUrl(info["url"]).host().toLower() == "bugzilla.novell.com")
    {
        NovellBugzilla *newBug = new NovellBugzilla(info["url"]);
        setupTracker(newBug, info);
    }
    else if (QUrl(info["url"]).host().toLower().endsWith("launchpad.net"))
    {
        Launchpad *newBug = new Launchpad(info["url"]);
        setupTracker(newBug, info);
    }
//    else if (info["type"] == "Google")
//    {
//        Google *newBug = new Google(info["url"]);
//        setupTracker(newBug, info);
//    }
    else if (info["type"] == "Bugzilla")
    {
        Bugzilla *newBug = new Bugzilla(info["url"]);
        setupTracker(newBug, info);
    }
    else if (info["type"] == "Mantis")
    {
        Mantis *newBug = new Mantis(info["url"]);
        setupTracker(newBug, info);
    }
    else if (info["type"] == "Trac")
    {
        Trac *newBug = new Trac(info["url"], info["username"], info["password"]);
        setupTracker(newBug, info);
    }
}

// Fills in the various bits of information the tracker
// needs to keep around
void
MainWindow::setupTracker(Backend *newBug, QMap<QString, QString> info)
{
    qDebug() << "Setup tracker";
    bool autosync = false;
    newBug->setId(info["id"]);
    newBug->setName(info["name"]);
    newBug->setUsername(info["username"]);
    newBug->setPassword(info["password"]);
    newBug->setLastSync(info["last_sync"]);
    newBug->setVersion(info["version"]);
    newBug->setValidPriorities(info["valid_priorities"].split(","));
    newBug->setValidSeverities(info["valid_severities"].split(","));
    newBug->setValidStatuses(info["valid_statuses"].split(","));
    newBug->setMonitorComponents(info["monitored_components"].split(","));
    connect(newBug, SIGNAL(bugsUpdated()),
            this, SLOT(bugsUpdated()));
    connect(newBug, SIGNAL(backendError(QString)),
            this, SLOT(backendError(QString)));
    connect(newBug, SIGNAL(commentsCached()),
            this, SLOT(commentsCached()));
    // If this was called after the user used the Add Tracker window,
    // insert the data into the DB
    if (info["id"] == "-1")
    {
        info["auto_cache_comments"] = newBug->autoCacheComments();
        int tracker = insertTracker(info);
        newBug->setId(QString("%1").arg(tracker));
        mSyncPosition = mBackendList.size() + 1;
        autosync = true;
    }

    QTableView* tabBugTable = new QTableView;
    trackerTabsList.append(tabBugTable);
    ui->trackerTab->addTab(tabBugTable,newBug->name());
    addTrackerToList(newBug,true);
    mStartup = 1;
    populateCurrentTab(trackerTabsList.indexOf(trackerTabsList.last()));

}

// Adds a tracker to the tracker list on the left
// side of the screen
void
MainWindow::addTrackerToList(Backend *newTracker, bool sync)
{
    qDebug() << "Adding tracker to list";
    QListWidgetItem *newItem = new QListWidgetItem(newTracker->name());
    newItem->setFlags(Qt::ItemIsUserCheckable|Qt::ItemIsEnabled);
    newItem->setCheckState(Qt::Checked);
    newItem->setData(Qt::UserRole, newTracker->id());
    ui->trackerList->addItem(newItem);

    QString iconPath = QDesktopServices::storageLocation(QDesktopServices::DataLocation);
    iconPath.append(QDir::separator()).append("entomologist");
    iconPath.append(QDir::separator()).append(QString("%1.png").arg(newTracker->name()));
    mBackendMap[newTracker->id()] = newTracker;
    mBackendList.append(newTracker);
    if(!QFile::exists(iconPath))
        fetchIcon(newTracker->url(), iconPath);
    if (sync)
        syncTracker(newTracker);
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
        mSortQuery = newSortQuery.arg("bugs.last_modified");
        break;
    case 5:
        mSortQuery = newSortQuery.arg("bugs.severity");
        break;
    case 6:
        mSortQuery = newSortQuery.arg("bugs.priority");
        break;
    case 7:
        mSortQuery = newSortQuery.arg("bugs.assigned_to");
        break;
    case 8:
        mSortQuery = newSortQuery.arg("bugs.status");
        break;
    case 9:
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
    qDebug() << "mSyncRequests is: " << mSyncRequests;
    mSyncRequests--;
    if (mSyncRequests == 0)
    {
        if (mSyncPosition == mBackendList.size())
        {
            filterTable();
            mUploading = false;
            stopAnimation();
            notifyUser();
        }
        else
        {
            syncNextTracker();
        }
    }
}

void
MainWindow::commentsCached()
{
    mSyncRequests--;
    if (mSyncRequests == 0)
    {
        stopAnimation();
        loadComments();
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
    pSpinnerMovie->start();
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
    ui->trackerList->setEnabled(false);
    ui->detailsScrollArea->setEnabled(false);
    ui->splitter_2->setEnabled(false);

    for(int i = 0; i < trackerTabsList.length(); i++)
    {
        trackerTabsList.at(i)->setEnabled(false);

    }
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
    ui->trackerList->setEnabled(true);
    ui->detailsScrollArea->setEnabled(true);
    ui->splitter_2->setEnabled(true);

    for(int i = 0; i < trackerTabsList.length(); i++)
    {
        trackerTabsList.at(i)->setEnabled(true);

    }

    toggleButtons();
}

// Grab the favicon.ico for each tracker to make the bug list prettier
void
MainWindow::fetchIcon(const QString &url,
                      const QString &savePath)
{
    QUrl u(url);
    QString fetch = "http://" + u.host() + "/favicon.ico";
    qDebug() << "fetchIcon: " << fetch;

    QNetworkRequest req = QNetworkRequest(QUrl(fetch));
    req.setAttribute(QNetworkRequest::User, QVariant(savePath));
    // We set this to 0 for a first request, and then to 1 if the url is redirected.
    req.setAttribute((QNetworkRequest::Attribute)(QNetworkRequest::User+1), QVariant(0));
    QNetworkReply *rep = pManager->get(req);
    connect(rep, SIGNAL(finished()),
            this, SLOT(iconDownloaded()));
}

// Callback from fetchIcon
void
MainWindow::iconDownloaded()
{
    QNetworkReply *reply = qobject_cast<QNetworkReply*>(sender());
    QString savePath = reply->request().attribute(QNetworkRequest::User).toString();
    int wasRedirected = reply->request().attribute((QNetworkRequest::Attribute)(QNetworkRequest::User+1)).toInt();
    QVariant redirect = reply->attribute(QNetworkRequest::RedirectionTargetAttribute);

    if (!redirect.toUrl().isEmpty() && !wasRedirected)
    {
        qDebug() << "Was redirected to " << redirect.toUrl();
        reply->deleteLater();
        QNetworkRequest req = QNetworkRequest(redirect.toUrl());
        req.setAttribute(QNetworkRequest::User, QVariant(savePath));
        req.setAttribute((QNetworkRequest::Attribute)(QNetworkRequest::User+1), QVariant(1));
        QNetworkReply *rep = pManager->get(req);
        connect(rep, SIGNAL(finished()),
                this, SLOT(iconDownloaded()));
        return;
    }

    qDebug() << "Icon downloaded";
    if (reply->error())
    {
        reply->close();
        qDebug() << "Couldn't get icon";
        fetchHTMLIcon(reply->url().toString(), savePath);
        return;
    }

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
        if (icon.format() == QImage::Format_Invalid)
            fetchHTMLIcon(reply->url().toString(), savePath);
        else
            icon.save(savePath, "PNG");
    }
    else
    {
        qDebug() << "Invalid image";
        fetchHTMLIcon(reply->url().toString(), savePath);
    }

    logoBuffer.close();
    reply->close();
}

void
MainWindow::fetchHTMLIcon(const QString &url,
                          const QString &savePath)
{
    QUrl u(url);
    QString fetch = "https://" + u.host() + "/";
    qDebug() << "Fetching " << fetch;
    QNetworkRequest req = QNetworkRequest(QUrl(fetch));
    req.setAttribute(QNetworkRequest::User, QVariant(savePath));
    QNetworkReply *rep = pManager->get(req);
    connect(rep, SIGNAL(finished()),
            this, SLOT(htmlIconDownloaded()));
}

void
MainWindow::htmlIconDownloaded()
{
    QNetworkReply *reply = qobject_cast<QNetworkReply*>(sender());
    QString url = reply->url().toString();
    QString savePath = reply->request().attribute(QNetworkRequest::User).toString();
    if (reply->error())
    {
        reply->close();
        qDebug() << "Couldn't get an icon or HTML for " << url << " so I'm giving up: " << reply->errorString();
        qDebug() << reply->readAll();
        return;
    }

    QString html(reply->readAll());
    QRegExp reg("<link (rel=\"([^\"]+)\")?\\s*(type=\"([^\"]+)\")?\\s*(href=\"([^\"]+)\")?\\s*/?>");
    QString iconPath = "";
    // Look for the first match
    int pos = 0;
    while ((pos = reg.indexIn(html, pos)) != -1)
    {
        if (reg.cap(2).endsWith("icon"))
        {
            iconPath = reg.cap(6);
            break;
        }

        pos += reg.matchedLength();
    }

    if (iconPath.isEmpty())
    {
        qDebug() << "Couldn't find an icon in " << url;
        return;
    }

    if (!iconPath.startsWith("http"))
    {
        qDebug() << "Path was wrong, fixing";
        iconPath= "https://" + QUrl(url).host() + "/" + iconPath;
    }
    qDebug() << "Going to fetch " << iconPath;

    QNetworkRequest req = QNetworkRequest(QUrl(iconPath));
    req.setAttribute(QNetworkRequest::User, QVariant(savePath));
    QNetworkReply *rep = pManager->get(req);
    connect(rep, SIGNAL(finished()),
            this, SLOT(iconDownloaded()));
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

        QString cleanUrl = cleanupUrl(info["url"]);
        info["url"] = cleanUrl;
        if (pDetectorProgress == NULL)
            pDetectorProgress = new QProgressDialog(tr("Detecting tracker type..."), tr("Cancel"), 0, 0);
        pDetectorProgress->setWindowModality(Qt::ApplicationModal);
        pDetectorProgress->setMinimumDuration(0);
        pDetectorProgress->setValue(1);

        Autodetector *detector = new Autodetector();
        connect(detector, SIGNAL(finishedDetecting(QMap<QString,QString>)),
                this, SLOT(finishedDetecting(QMap<QString,QString>)));
        connect(detector, SIGNAL(backendError(QString)),
                this, SLOT(backendError(QString)));
        detector->detect(info);
    }
}

QString
MainWindow::cleanupUrl(QString &url)
{
    QString cleanUrl = url;
    if (!cleanUrl.startsWith("http://") &&
        !cleanUrl.startsWith("https://"))
    {
        cleanUrl = url.prepend("https://");
    }

    QUrl tmp(cleanUrl);
    // First remove any extra slashes
    QString path = tmp.path();
    path = QDir::cleanPath(path);

    // Remove a trailing slash
    if (path.right(1) == "/")
        path.remove(path.size() - 1, 1);
    path.remove(QRegExp("/login/xmlrpc$"));
    path.remove(QRegExp("/xmlrpc.cgi$"));

    tmp.setPath(path);
    cleanUrl = tmp.toString();
    return(cleanUrl);
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
    QString type = data["type"];
    if (type == "Unknown")
    {
        ErrorHandler::handleError("Couldn't detect tracker type.", "");
        delete detector;
        return;
    }
    else if (type == "Google")
    {
        // Google Code Hosting support will have to wait until we sort out how to handle their tag
        // system in the UI
        QMessageBox box;
        box.setText(tr("Sorry, Google Project Hosting is not yet supported."));
        box.exec();
        delete detector;
        return;
    }
    else if (type == "Launchpad")
    {

// Launchpad uses a non-standard HTTP method ("PATCH") to update bugs.  Support for custom HTTP commands is
// only available in Qt 4.7+, so we must disable that functionality in older versions.
 #if QT_VERSION < 0x040700
        QMessageBox box;
        box.setText(tr("Launchpad support will be read-only."));
        box.setDetailedText(tr("Entomologist was compiled against an older version of Qt, probably because you're running an older distribution. "
                               "In order to have write-access to Launchpad, you'll need a binary compiled to use Qt 4.7 or higher"));
        box.exec();
#endif

        // If the user managed to enter a password into the Add Tracker dialog, we want to discard
        // it, as Launchpad uses OAuth, and we store the OAuth token and secret in the
        // password field.  It's hacky, I know.
        data["password"] =  "";
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

    mSyncPosition = 0;
    syncNextTracker();
}

void
MainWindow::syncNextTracker()
{
    if (mBackendList.empty()) return;
    Backend *b = mBackendList.at(mSyncPosition);
    ui->syncingLabel->setText(QString("Syncing %1...").arg(b->name()));
    mSyncPosition++;
    if (mUploading)
    {
        mSyncRequests++;
        b->uploadAll();
    }
    else
    {
        syncTracker(b);
    }
}

// This is called when the user presses the upload button
void
MainWindow::upload()
{
    QSettings settings("Entomologist");
    bool noUploadDialog = settings.value("no-upload-confirmation", false).toBool();
    bool reallyUpload = true;

    if (!isOnline())
    {
        QMessageBox box;
        box.setText(tr("Sorry, you can only upload changes when you are connected to the Internet."));
        box.exec();
        return;
    }

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
        mUploading = true;
        mSyncPosition = 0;
        syncNextTracker();
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
    qDebug() << "quitEvent";
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
    qDebug() << "Close event";
    if (isVisible())
    {
        // Save the window geometry and position
        QSettings settings("Entomologist");
        settings.setValue("window-geometry", saveGeometry());
        if (!settings.value("minimize-warning", false).toBool())
        {
            QMessageBox box;
            box.setText("Entomologist will be minimized to your system tray.");
            box.exec();
            settings.setValue("minimize-warning", true);
        }
        qDebug() << "Hiding";
        hide();
        event->ignore();
    }
    else
    {
        qDebug() << "Accepting close event";
        event->accept();
    }
}

// Help -> Website
void
MainWindow::websiteTriggered()
{
#ifdef Q_OS_ANDROID
    Utilities::openAndroidUrl(QUrl("http://www.entomologist-project.org"));
#else
    QDesktopServices::openUrl(QUrl("http://www.entomologist-project.org"));
#endif
}

// Help -> About
void
MainWindow::aboutTriggered()
{
    About aboutBox;
    aboutBox.exec();
}

void
MainWindow::showEditMonitoredComponents()
{
    MonitorDialog d;
    // When the user modifies the monitored components,
    // we want to update the affected trackers, and then
    // we need to set the last sync time to 1970 in order
    // to fetch the appropriate components.
    if (d.exec() == QDialog::Accepted)
    {
        qDebug() << "Going to do a sync";
        QSqlQuery q("SELECT name, monitored_components FROM trackers WHERE monitored_components !=\"\"");
        while (q.next())
        {
            QString id = QString::number(trackerNameExists(q.value(0).toString()));
            qDebug() << "Looking up backend for ID " << id;

            Backend *b = mBackendMap.value(id, NULL);
            if (b != NULL)
            {
                qDebug() << "Setting monitor components and resyncing";
                b->setMonitorComponents(q.value(1).toString().split(","));
                b->setLastSync("1970-01-01T12:13:14");
                mSyncPosition = mBackendList.size();
                syncTracker(b);
            }
        }

    }
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
        case QEvent::LanguageChange:
        {
            ui->retranslateUi(this);
            break;
        }
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
    settings.setValue("show-my-monitored", ui->actionMonitored_Components->isChecked());
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
    QString showMy, showRep, showCC, showMonitored;
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

    if (ui->actionMonitored_Components->isChecked())
        showMonitored = "Monitored";
    else
        showMonitored = "XXXMonitored";

    QString tempQuery = QString("WHERE (bugs.bug_type=\'%1\' OR bugs.bug_type=\'%2\' OR bugs.bug_type=\'%3\' OR bugs.bug_type=\'%4')")
                          .arg(showMy)
                          .arg(showRep)
                          .arg(showCC)
                          .arg(showMonitored);

    if(ui->trackerTab->currentIndex() == 0)
        mActiveQuery = mBaseQuery + " " + mTrackerQuery + mSortQuery;
    else
        mActiveQuery = mBaseQuery + " " + tempQuery +" AND" + mWhereQuery + mTrackerQuery + mSortQuery;

        pBugModel->setQuery(mActiveQuery);
//    qDebug() << pBugModel->query().lastQuery();
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
        populateCurrentTab(0);
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
    if (pDetectorProgress != NULL)
        if (pDetectorProgress->isVisible())
            pDetectorProgress->reset();
    ErrorHandler::handleError("An error occurred.", message);
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
    QAction *resyncAction = contextMenu.addAction(tr("Resync"));
    QAction *a = contextMenu.exec(QCursor::pos());
    Backend *b = mBackendMap[id];
    QString trackerName = b->name();
    compareTabName(trackerName);
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
    else if (a == resyncAction)
    {
        mSyncPosition = mBackendList.size();
        syncTracker(b);
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
    QString iconDir = QDesktopServices::storageLocation(QDesktopServices::DataLocation);
    iconDir.append(QDir::separator()).append("entomologist");

    // Rename the icon, if necessary
    QString oldPath = QString ("%1%2%3.png")
                                .arg(iconDir)
                                .arg(QDir::separator())
                                .arg(b->name());
    QString newPath = QString ("%1%2%3.png")
                                .arg(iconDir)
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

    QString trackerName = b->name();
    ui->trackerTab->setTabText(workingTab,trackerName);

    filterTable();
}

// The user pressed the search button, or pressed enter
// in the search line edit
void
MainWindow::searchTriggered()
{
    QString searchText = ui->searchEdit->text();
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
    ui->trackerTab->removeTab(workingTab);
    trackerTabsList.removeAt(workingTab);
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
    qDebug() << "Tray activated";
    if (reason == QSystemTrayIcon::DoubleClick)
    {
        if (isVisible())
        {
            qDebug() << "Closing";
            close();
        }
        else
        {
            showNormal();
        }
    }
}

void
MainWindow::tableViewContextMenu(const QPoint &p)
{
    int currentTab = ui->trackerTab->currentIndex();
    QTableView *table = trackerTabsList.at(currentTab);
    QModelIndex i = table->indexAt(p);
    QString tracker_id = i.sibling(i.row(), 1).data().toString();
    QString bug = i.sibling(i.row(), 3).data().toString();
    QMenu contextMenu(tr("Bug Menu"), this);
    QAction *copyAction = contextMenu.addAction(tr("Copy URL"));
    contextMenu.addSeparator();
    QAction *openAction = contextMenu.addAction(tr("Open in a browser"));
    QAction *a = contextMenu.exec(QCursor::pos());
    Backend *b = mBackendMap[tracker_id];
    if (a == copyAction)
    {
        QString url = b->buildBugUrl(bug);
        QApplication::clipboard()->setText(url);
    }
    else if (a == openAction)
    {
        QString url = b->buildBugUrl(bug);
#ifdef Q_OS_ANDROID
        Utilities::openAndroidUrl(QUrl(url));
#else
        QDesktopServices::openUrl(QUrl(url));
#endif
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

void
MainWindow::changelogTriggered()
{
    ChangelogWindow *newWindow = new ChangelogWindow(this);
    newWindow->loadChangelog();
    newWindow->exec();
    delete newWindow;
    filterTable();
}


// Changes the application focus to the search bar.
void
MainWindow::searchFocusTriggered() {

    ui->searchEdit->setFocus();
}

// Populates the Currently Hilighted tab with the bug data for that tab.
void
MainWindow::populateCurrentTab(int currentTab) {

    QTableView *table = trackerTabsList.at(currentTab);

    table->setSortingEnabled(true);
    table->setGridStyle(Qt::PenStyle(Qt::DotLine));
    pBugModel = new SqlBugModel;
    int compare = QString::compare(ui->trackerTab->tabText(currentTab),"All");
    if(compare != 0)
    {

        mWhereQuery= QString(" bugs.tracker_id IN"
                       " (SELECT trackers.id FROM trackers WHERE trackers.name = \"%1\")"
                       ). arg(ui->trackerTab->tabText(currentTab));
                       mActiveQuery = mBaseQuery + " WHERE" + mWhereQuery+ mTrackerQuery + mSortQuery;
                       qDebug() << mActiveQuery;
                       pBugModel->setQuery(mActiveQuery);
                       ui->trackstatsFrame->hide(); // Hide the stats frame

    }
    else
    {
        ui->trackstatsFrame->show();
        populateStats();
    }
    filterTable();
    SqlBugDelegate *delegate = new SqlBugDelegate();
    table->setItemDelegate(delegate);
    table->setModel(pBugModel);

    pBugModel->setHeaderData(2, Qt::Horizontal, tr("Tracker"));
    pBugModel->setHeaderData(3, Qt::Horizontal, tr("Bug ID"));
    pBugModel->setHeaderData(4, Qt::Horizontal, tr("Last Modified"));
    pBugModel->setHeaderData(5, Qt::Horizontal, tr("Severity"));
    pBugModel->setHeaderData(6, Qt::Horizontal, tr("Priority"));
    pBugModel->setHeaderData(7, Qt::Horizontal, tr("Assignee"));
    pBugModel->setHeaderData(8, Qt::Horizontal, tr("Status"));
    pBugModel->setHeaderData(9, Qt::Horizontal, tr("Summary"));
    table->hideColumn(0); // Hide the internal row id
    table->hideColumn(1); // Hide the tracker id
    if(compare > 0) table->hideColumn(2); // Hide the tracker name for individual trackers.
    table->verticalHeader()->hide(); // Hide the Row numbers
    table->resizeColumnsToContents();
    table->resizeRowsToContents();
    table->setAlternatingRowColors(true);

    /* These have been added here to keep signals for the tables in one place
     * and because populateCurrentTab() will always be called at least once on a TableView
     */

    // Make sure we don't keep reconnecting signals/setting icons on already instanced tabs.
    if(mStartup == 1)
    {
        // Bug list sorting
        connect(table->horizontalHeader(), SIGNAL(sortIndicatorChanged(int,Qt::SortOrder)),
                this, SLOT(sortIndicatorChanged(int,Qt::SortOrder)));
        //Context Menu
        connect(table, SIGNAL(customContextMenuRequested(QPoint)),
                this, SLOT(tableViewContextMenu(QPoint)));
        // Double Click Comments Pane.
        connect(table, SIGNAL(doubleClicked(QModelIndex)),
                this, SLOT(bugClicked(QModelIndex)));

        // Set the FavIcon for the Tab
        QString iconpath = pBugModel->getIcon(ui->trackerTab->tabText(currentTab));
        ui->trackerTab->setTabIcon(currentTab,QIcon(iconpath));
        mStartup = 0;
    }

}
void
MainWindow::populateStats() {
    QString currentAssignedBugs;
    QSqlQuery bugs;
    ui->statsList->clear();
    bugs.exec("SELECT COUNT(id) FROM bugs");
    bugs.next();
    currentAssignedBugs = QString("Currently assigned bugs: %1")
            .arg(bugs.value(0).toString());

    QListWidgetItem *newStat = new QListWidgetItem("Bug Statistics");
    newStat->setTextAlignment(Qt::AlignHCenter);
    newStat->setFlags(Qt::ItemIsEnabled);
    ui->statsList->addItem(newStat);

    // Use a frame to draw the separator between "Bug Statistics" and the list
    QFrame *f = new QFrame( this );
    f->setFrameStyle( QFrame::HLine | QFrame::Sunken );

    QListWidgetItem *spacer = new QListWidgetItem("");
    spacer->setFlags(Qt::NoItemFlags);
    ui->statsList->addItem(spacer);
    ui->statsList->setItemWidget(spacer, f);

    newStat = new QListWidgetItem(currentAssignedBugs);
    newStat->setFlags(Qt::ItemIsEnabled);
    ui->statsList->addItem(newStat);

}

void
MainWindow::compareTabName(QString compareItem)
{
    for(int i = 0; i < trackerTabsList.length();i++)
    {
        if(QString::compare(compareItem,ui->trackerTab->tabText(i)) == 0)
            workingTab = i;
    }

}

// TODO implement this?
void
MainWindow::handleSslErrors(QNetworkReply *reply,
                         const QList<QSslError> &errors)
{
    reply->ignoreSslErrors();
}
