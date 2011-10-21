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
#include <QCloseEvent>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
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
#include <QPainter>

#include "About.h"
#include "ChangelogWindow.h"
#include "Preferences.h"
#include "UploadDialog.h"
#include "CommentFrame.h"
#include "SqlBugDelegate.h"
#include "SqlBugModel.h"
#include "SearchTab.h"
#include "trackers/Bugzilla.h"
#include "trackers/NovellBugzilla.h"
#include "trackers/Trac.h"
#include "trackers/Mantis.h"
#include "NewTracker.h"
#include "MainWindow.h"
#include "Autodetector.h"
#include "ErrorHandler.h"
#include "Utilities.hpp"
#include "MonitorDialog.h"
#include "SqlUtilities.h"
#include "ui_MainWindow.h"
#include "ToDoListView.h"
#include "UpdatesAvailableDialog.h"

#define DB_VERSION 5

bool mLogAllXmlRpcOutput;

// TODOs:
// - URL handing needs to be improved
// - Pressing cancel during tracker detection should actually cancel
// - Bug resolution - hardcode for bugzilla 3.4. 3.6 should be listable with the Bug.fields call
// - Highlight new bugs
// - QtDBUS on linux for network insertion integration
// - Consider orphaned bug changes - when a bug is closed, but there is
//   something in the shadow tables.
//   Check other platforms.
// - Bugzilla 3.2/3.4 component fetching could really be refactored

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    pDetectorProgress = NULL;
    mLogAllXmlRpcOutput = false;
    mDbUpdated = false;
    pTodoListView = NULL;
    // mSyncRequests tracks how many sync requests have been made
    // in order to know when to re-enable the widgets
    mSyncRequests = 0;
    // mSyncPosition is used to iterate through the backend list,
    // so we only sync one repository at a time, rather than flinging
    // requests at all of them at once.
    mSyncPosition = 0;
    mUploading = false;
    QSettings settings("Entomologist");

    pManager = new QNetworkAccessManager();
    connect(pManager, SIGNAL(sslErrors(QNetworkReply *, const QList<QSslError> &)),
            this, SLOT(handleSslErrors(QNetworkReply *, const QList<QSslError> &)));

    ui->setupUi(this);
    ui->refreshButton->setIcon(style()->standardIcon(QStyle::SP_BrowserReload));
    ui->uploadButton->setIcon(style()->standardIcon(QStyle::SP_ArrowUp));
    ui->changelogButton->setIcon(style()->standardIcon(QStyle::SP_FileDialogInfoView));

    setupTrayIcon();
    // Setup the "Show" menu and "Work Offline"
    ui->actionMy_Bugs->setChecked(settings.value("show-my-bugs", true).toBool());
    ui->actionMy_Reports->setChecked(settings.value("show-my-reports", true).toBool());
    ui->actionMy_CCs->setChecked(settings.value("show-my-ccs", true).toBool());
    ui->actionMonitored_Components->setChecked(settings.value("show-my-monitored", true).toBool());
    ui->action_Work_Offline->setChecked(settings.value("work-offline", false).toBool());

    // Set the default network status
    pStatusIcon = new QLabel();
    pStatusIcon->setPixmap(QPixmap(":/online"));
    pStatusMessage = new QLabel("");
    ui->statusBar->addPermanentWidget(pStatusMessage);
    ui->statusBar->addPermanentWidget(pStatusIcon);

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
    QShortcut *logXmlRpc;

    searchFocus = new QShortcut(QKeySequence(Qt::META + Qt::Key_Space),this);
    searchFocus->setContext(Qt::ApplicationShortcut);
    connect(searchFocus,SIGNAL(activated()),this,SLOT(searchFocusTriggered()));

    uploadChange = new QShortcut(QKeySequence(Qt::CTRL + Qt::Key_S),this);
    uploadChange->setContext(Qt::ApplicationShortcut);
    connect(uploadChange,SIGNAL(activated()),this,SLOT(upload()));

    logXmlRpc = new QShortcut(QKeySequence(Qt::CTRL + Qt::Key_0), this);
    logXmlRpc->setContext(Qt::ApplicationShortcut);
    connect(logXmlRpc, SIGNAL(activated()), this, SLOT(toggleXmlRpcLogging()));

    // Menu actions
    connect(ui->action_Add_Tracker, SIGNAL(triggered()),
            this, SLOT(addTrackerTriggered()));
    connect(ui->action_Refresh_Tracker,SIGNAL(triggered()),
            this, SLOT(resync()));
    connect(ui->action_Todo_Lists, SIGNAL(triggered()),
            this, SLOT(showTodoList()));
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
    connect(ui->changelogButton, SIGNAL(clicked()),
            this, SLOT(changelogTriggered()));

    connect(ui->trackerTab, SIGNAL(showMenu(int)),
            this, SLOT(showMenu(int)));
    ui->trackerTab->removeTab(0);
    ui->trackerTab->removeTab(0);

    // And finally set up the various other widgets
    connect(ui->refreshButton, SIGNAL(clicked()),
            this, SLOT(resync()));
    connect(ui->uploadButton, SIGNAL(clicked()),
            this, SLOT(upload()));
    restoreGeometry(settings.value("window-geometry").toByteArray());

    // Set the network status bar and check for updates if possible
    if (isOnline())
        if (settings.value("update-check", true).toBool() == true)
            checkForUpdates();

    setupDB();
    toggleButtons();
    pSearchTab = new SearchTab(this);
    connect(pSearchTab, SIGNAL(openSearchedBug(QString,QString)),
            this, SLOT(openSearchedBug(QString,QString)));
    loadTrackers();
    ui->trackerTab->addTab(pSearchTab, QIcon(":/search"), "Search");

    if ((settings.value("startup-sync", false).toBool() == true)
       || (mDbUpdated))
        syncNextTracker();
}

MainWindow::~MainWindow()
{
    delete ui;
}

void
MainWindow::setupDB()
{

    mDbPath = QString("%1%2%3%4%5")
              .arg(QDesktopServices::storageLocation(QDesktopServices::DataLocation))
              .arg(QDir::separator())
              .arg("entomologist")
              .arg(QDir::separator())
              .arg("entomologist.bugs.db");

    if (!QFile::exists(mDbPath))
    {
        qDebug() << "Creating " << mDbPath;
        SqlUtilities::openDb(mDbPath);
        SqlUtilities::createTables(DB_VERSION);
    }
    else
    {
        SqlUtilities::openDb(mDbPath);
        checkDatabaseVersion();
    }
}

void
MainWindow::checkDatabaseVersion()
{
    if (SqlUtilities::dbVersion() == DB_VERSION)
        return;

    qDebug() << "Version mismatch";
    QList< QMap<QString, QString> > trackerList;
    trackerList = SqlUtilities::loadTrackers();

    SqlUtilities::closeDb();
    QFile::remove(mDbPath);

    SqlUtilities::openDb(mDbPath);
    SqlUtilities::createTables(DB_VERSION);

    for (int i = 0; i < trackerList.size(); ++i)
    {
        QMap<QString, QString> t = trackerList.at(i);
        t["last_sync"] = "1970-01-01T12:13:14";
        SqlUtilities::simpleInsert("trackers", t);
    }
    mDbUpdated = true;
}
// Loads cached trackers from the database
void
MainWindow::loadTrackers()
{
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
        info["monitored_components"] = record.value(8).toString();
        info["auto_cache_comments"] = record.value(9).toString();
        addTracker(info);
    }
}

// This adds trackers to the database (when called from the New Tracker window)
// and to the list of trackers.
void
MainWindow::addTracker(QMap<QString,QString> info)
{
    qDebug() << "Add tracker";
    // Create the proper Backend object
    if (QUrl(info["url"]).host().toLower() == "bugzilla.novell.com")
    {
        NovellBugzilla *newBug = new NovellBugzilla(info["url"]);
        setupTracker(newBug, info);
    }
    else if (QUrl(info["url"]).host().toLower().endsWith("launchpad.net"))
    {
        QMessageBox box;
        box.setText(tr("Entomologist no longer supports Launchpad."));
        box.exec();
    }
//    else if (info["type"] == "Google")
//    {
//        Google *newBug = new Google(info["url"]);
//        setupTracker(newBug, info);
//    }
    else if (info["type"] == "bugzilla")
    {
        Bugzilla *newBug = new Bugzilla(info["url"]);
        setupTracker(newBug, info);
    }
    else if (info["type"] == "mantis")
    {
        Mantis *newBug = new Mantis(info["url"]);
        setupTracker(newBug, info);
    }
    else if (info["type"] == "trac")
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
    newBug->setId(info["id"]);
    newBug->setName(info["name"]);
    newBug->setUsername(info["username"]);
    newBug->setPassword(info["password"]);
    newBug->setLastSync(info["last_sync"]);
    newBug->setVersion(info["version"]);
    if (!info["monitored_components"].isEmpty())
        newBug->setMonitorComponents(info["monitored_components"].split(","));
    connect(newBug, SIGNAL(bugsUpdated()),
            this, SLOT(bugsUpdated()));
    connect(newBug, SIGNAL(backendError(QString)),
            this, SLOT(backendError(QString)));

    // If this was called after the user used the Add Tracker window,
    // insert the data into the DB
    if (info["id"] == "-1")
    {
        info.take("id");
        info["auto_cache_comments"] = newBug->autoCacheComments();
        int tracker = SqlUtilities::simpleInsert("trackers", info);
        newBug->setId(QString("%1").arg(tracker));
        mSyncPosition = mBackendList.size() + 1;
        connect(newBug, SIGNAL(fieldsFound()),
                this, SLOT(fieldsChecked()));
        newBug->checkFields();
    }
    else
    {
        addTrackerToList(newBug, false);
    }
}

void
MainWindow::addTrackerToList(Backend *newTracker, bool sync)
{
    qDebug() << "Adding tracker to list";
    if (pDetectorProgress != NULL)
    {
        pDetectorProgress->reset();
        pDetectorProgress->hide();
    }

    connect(this, SIGNAL(reloadFromDatabase()),
            newTracker->displayWidget(), SLOT(reloadFromDatabase()));
    connect(this, SIGNAL(setShowOptions(bool,bool,bool,bool)),
            newTracker->displayWidget(), SLOT(setShowOptions(bool,bool,bool,bool)));
    connect(newTracker->displayWidget(), SIGNAL(bugChanged()),
            this, SLOT(toggleButtons()));
    int newIndex = ui->trackerTab->insertTab(0, newTracker->displayWidget(), newTracker->name());
    pSearchTab->addTracker(newTracker);
    filterTable();

    QString iconPath = QDesktopServices::storageLocation(QDesktopServices::DataLocation);
    iconPath.append(QDir::separator()).append("entomologist");
    iconPath.append(QDir::separator()).append(QString("%1.png").arg(newTracker->name()));
    ui->trackerTab->setTabIcon(newIndex, QIcon(iconPath));
    ui->trackerTab->setCurrentIndex(newIndex);
    mBackendMap[newTracker->id()] = newTracker;
    mBackendList.append(newTracker);
    if(!QFile::exists(iconPath))
        fetchIcon(newTracker->url(), iconPath);
    if (sync)
        syncTracker(newTracker);
}

// This slot is connected to a signal in each Backend object,
// so we know when we're done syncing and can let the user
// interact with the application again
void
MainWindow::bugsUpdated()
{
    mSyncRequests--;
    qDebug() << "bugsUpdated: mSyncRequests is now: " << mSyncRequests;

    if (mSyncRequests == 0)
    {
        if (mSyncPosition == mBackendList.size())
        {
            filterTable();
            mUploading = false;
            stopAnimation();
            notifyUser();
            emit reloadFromDatabase();
        }
        else
        {
            syncNextTracker();
        }
    }
}

// After all of the trackers have been synced,
// this loops through and builds up information
// that will then be shown in a task tray popup
void
MainWindow::notifyUser()
{

    // On non-mac platforms, we can assume that if the window is
    // not visible, then it's in the system tray.  That doesn't
    // necessarily work out on OSX when we're minimized to the dock.
#ifndef Q_OS_MAC
    if (isVisible())
        return;
#endif

    int total = 0;
    QMapIterator<QString, Backend *> i(mBackendMap);
    while (i.hasNext())
    {
        i.next();
        total += i.value()->latestUpdateCount();
    }

    if (total > 0)
    {
#ifdef Q_OS_MAC
        QApplication::alert(this);
        if (isMinimized())
        {
            QPixmap newIcon(":/logo_128");
            QPainter painter(&newIcon);
            painter.setPen(QColor(Qt::darkRed));
            painter.setBrush(Qt::red);
            QRectF circle(70, 60, 50, 50);
            painter.drawEllipse(circle);
            painter.setPen(Qt::white);
            if (total < 100)
                painter.setFont(QFont("Helvetica", 30));
            else
                painter.setFont(QFont("Helvetica", 24));

            if (total < 1000)
                painter.drawText(circle, Qt::AlignCenter, QString::number(total));
            else
                painter.drawText(circle, Qt::AlignCenter, ">999");

            painter.end();
            QApplication::setWindowIcon(QIcon(newIcon));
        }
#else
        pTrayIcon->showMessage("Bugs Updated",
                               tr("%n bug(s) updated", "", total),
                               QSystemTrayIcon::Information,
                               5000);
#endif
    }
}

void
MainWindow::showEvent(QShowEvent *e)
{
#ifdef Q_OS_MAC
    qDebug() << "showEvent";
    if (e->spontaneous())
    {
        qDebug() << "spontaneous";
        QApplication::setWindowIcon(QIcon(":/logo_128"));
    }
#endif
    QWidget::showEvent(e);
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
    ui->splitter_2->setEnabled(false);

    for(int i = 0; i < ui->trackerTab->count(); i++)
    {
        ui->trackerTab->setTabEnabled(i, false);
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
    ui->splitter_2->setEnabled(true);

    for(int i = 0; i < ui->trackerTab->count(); i++)
    {
        ui->trackerTab->setTabEnabled(i, true);
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
        {
            fetchHTMLIcon(reply->url().toString(), savePath);
        }
        else
        {
            icon.save(savePath, "PNG");
            QFileInfo info(savePath);
            int tabIndex = compareTabName(info.baseName());
            if (tabIndex != -1)
                ui->trackerTab->setTabIcon(tabIndex, QIcon(QPixmap::fromImage(icon)));
        }
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

    qDebug() << "Syncing " << tracker->name();
    ui->syncingLabel->setText(QString("Syncing %1...").arg(tracker->name()));
    if (mSyncRequests == 0)
        startAnimation();
    else
        qDebug() << "Outstanding sync requests: " << mSyncRequests;
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
        QString cleanUrl = cleanupUrl(info["url"]);
        info["url"] = cleanUrl;
#if 0
        if (pDetectorProgress == NULL)
            pDetectorProgress = new QProgressDialog(tr("Detecting tracker version..."), tr("Cancel"), 0, 0);
        pDetectorProgress->setWindowModality(Qt::ApplicationModal);
        pDetectorProgress->setMinimumDuration(0);
        pDetectorProgress->setValue(2);
#endif
        ui->syncingLabel->setText("Detecting tracker version...");
        startAnimation();
        if (info["tracker_type"] == "Bugzilla")
        {
            if (QUrl(cleanUrl).host().toLower() == "bugzilla.novell.com")
                pNewTracker = new NovellBugzilla(cleanUrl);
            else
                pNewTracker = new Bugzilla(cleanUrl);
        }
        else if (info["tracker_type"] == "Mantis")
        {
            pNewTracker = new Mantis(cleanUrl);
        }
        else if (info["tracker_type"] == "Trac")
        {
            pNewTracker = new Trac(cleanUrl, info["username"], info["password"]);
        }
        else
        {
            QMessageBox box;
            box.setText(tr("Something has gone horribly wrong.  I have no idea what tracker you want to add!"));
            box.exec();
            return;
        }

        pNewTracker->setName(info["name"]);
        pNewTracker->setUsername(info["username"]);
        pNewTracker->setPassword(info["password"]);
        checkVersion(pNewTracker);
    }
}

void
MainWindow::checkVersion(Backend *b)
{
    connect(b, SIGNAL(versionChecked(QString, QString)),
            this, SLOT(versionChecked(QString, QString)));
    connect(b, SIGNAL(backendError(QString)),
            this, SLOT(backendError(QString)));
    b->checkVersion();
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

void
MainWindow::versionChecked(const QString &version,
                           const QString &message)
{
    pNewTracker->deleteLater();
    if (version == "-1")
    {
        ErrorHandler::handleError("This tracker is not valid.", message);
        stopAnimation();
        return;
    }

    // TODO: Refactor the addTracker(...) call to take a Backend object
    // directly
    QMap<QString, QString> info;
    info["id"] = "-1";
    info["url"] = pNewTracker->url();
    info["name"] = pNewTracker->name();
    info["type"] = pNewTracker->type();
    info["username"] = pNewTracker->username();
    info["password"] = pNewTracker->password();
    info["last_sync"] = "1970-01-01T12:13:14";
    info["version"] = version;
    addTracker(info);
}

// Signal called after the autodetector is finished
void
MainWindow::finishedDetecting(QMap<QString, QString> data)
{
    qDebug() << "Finished detecting";
    Autodetector *detector = qobject_cast<Autodetector*>(sender());
    stopAnimation();

    if (pDetectorProgress->wasCanceled())
        return;

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
        QMessageBox box;
        box.setText(tr("Entomologist no longer support Launchpad."));
        box.exec();
        delete detector;
        return;

// Launchpad uses a non-standard HTTP method ("PATCH") to update bugs.  Support for custom HTTP commands is
// only available in Qt 4.7+, so we must disable that functionality in older versions.
 #if QT_VERSION < 0x040700
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

void
MainWindow::showTodoList()
{
    if (pTodoListView == NULL)
        pTodoListView = new ToDoListView();

    pTodoListView->show();
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
    if (mBackendList.empty())
        return;

    Backend *b = mBackendList.at(mSyncPosition);
    mSyncPosition++;
    if (mUploading)
    {
        mSyncRequests++;
        ui->syncingLabel->setText(QString("Uploading changes to %1").arg(b->name()));
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
        if (newDialog->exec() != QDialog::Accepted)
            reallyUpload = false;
        delete newDialog;
    }

    if (reallyUpload)
    {
        qDebug() << "Uploading...";
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
    QList< QMap<QString, QString> > commentsChangelist;
    QVariantList bugChangelist;
    QString entry = tr("<b>%1</b> bug <font color=\"blue\"><u>%2</u></font>: Change "
                       "<b>%3</b> from <b>%4</b> to <b>%5</b><br>");
    QString commentEntry = tr("Add a comment to <b>%1</b> bug <font color=\"blue\"><b>%2</u></font>:<br>"
                              "<i>%3</i><br>");

    bugChangelist = SqlUtilities::getTracChangelog();
    bugChangelist << SqlUtilities::getBugzillaChangelog();
    bugChangelist << SqlUtilities::getMantisChangelog();

    for (int i = 0; i < bugChangelist.size(); ++i)
    {
        QVariantList changes = bugChangelist.at(i).toList();
        for (int j = 0; j < changes.size(); ++j)
        {
            QVariantMap newChange = changes.at(j).toMap();
            ret += QString(entry).arg(newChange.value("tracker_name").toString(),
                                      newChange.value("bug_id").toString(),
                                      newChange.value("column_name").toString(),
                                      newChange.value("from").toString(),
                                      newChange.value("to").toString());
        }
    }

    commentsChangelist = SqlUtilities::getCommentsChangelog();
    for (int i = 0; i < commentsChangelist.size(); ++i)
    {
        QMap<QString, QString> comment = commentsChangelist.at(i);
        ret += commentEntry.arg(comment["tracker_name"],
                                comment["bug_id"],
                                comment["comment"]);
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
#ifdef Q_OS_MAC
        showMinimized();
#else
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

        hide();
#endif
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
    QMap<QString, QString> componentMap;
    // First get the list of monitored components for each tracker, so
    // we can compare later.
    QMapIterator<QString, Backend *> i(mBackendMap);
    while (i.hasNext())
    {
        i.next();
        componentMap[i.key()] = SqlUtilities::getMonitoredComponents(i.key());
    }

    // When the user modifies the monitored components,
    // we want to update the affected trackers, and then
    // we need to set the last sync time to 1970 in order
    // to fetch the appropriate components.
    if (d.exec() == QDialog::Accepted)
    {
        i.toFront();
        while (i.hasNext())
        {
            i.next();
            QString components =  SqlUtilities::getMonitoredComponents(i.key());
            qDebug() << "Components for " << i.key() << " are now " << components;

            if (componentMap[i.key()] != components)
            {
                Backend *b = i.value();
                // Remove all of the bugs for this tracker, and resync
                SqlUtilities::clearBugs(b->type(), i.key());
                if (components.isEmpty())
                    b->setMonitorComponents(QStringList());
                else
                    b->setMonitorComponents(components.split(","));
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
    if (!SqlUtilities::hasPendingChanges())
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

void
MainWindow::filterTable()
{
    emit setShowOptions(ui->actionMy_Bugs->isChecked(),
                        ui->actionMy_Reports->isChecked(),
                        ui->actionMy_CCs->isChecked(),
                        ui->actionMonitored_Components->isChecked());
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

// This is a great example of Qt making things easier and easier over time
bool
MainWindow::isOnline()
{

    bool ret = Utilities::isOnline(pManager);
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

// When the user edits tracker information, this is called to save their new
// data
void
MainWindow::updateTracker(const QString &id, QMap<QString, QString> data)
{
    Backend *b = mBackendMap[id];
    for (int i = 0; i < mBackendList.size(); ++i)
    {
        if (data["name"] == mBackendList.at(i)->name())
        {
            QMessageBox box;
            box.setText(QString("A tracker named <b>%1</b> already exists.").arg(data["name"]));
            box.exec();
            return;
        }
    }

    if (!SqlUtilities::renameTracker(id, data["name"], data["username"], data["password"]))
    {
        backendError("An error occurred renaming the tracker");
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

    QString oldName, newName;
    oldName = b->name();
    newName = data["name"];
    ui->trackerTab->setTabText(ui->trackerTab->currentIndex(), newName);
    pSearchTab->renameTracker(oldName, newName);
    b->setName(newName);
    b->setUsername(data["username"]);
    b->setPassword(data["password"]);
    QSettings settings("Entomologist");

    QVariant oldSortCol = settings.value(QString("%1-sort-column").arg(oldName));
    settings.remove(QString("%1-sort-column").arg(oldName));
    settings.setValue(QString("%1-sort-column").arg(newName), oldSortCol);

    QVariant oldSortOrd = settings.value(QString("%1-sort-order").arg(oldName));
    settings.remove(QString("%1-sort-order").arg(oldName));
    settings.setValue(QString("%1-sort-order").arg(newName), oldSortOrd);

    QVariant oldGeom = settings.value(QString("%1-header-geometry").arg(oldName));
    settings.remove(QString("%1-header-geometry").arg(oldName));
    settings.setValue(QString("%1-header-geometry").arg(newName), oldGeom);

}

void
MainWindow::deleteTracker(const QString &id)
{
    Backend *b = mBackendMap[id];
    mBackendMap.remove(id);
    for (int i = 0; i < mBackendList.size(); ++i)
    {
        if (mBackendList.at(i) == b)
        {
            mBackendList.removeAt(i);
            break;
        }
    }

    QString name = b->name();
    SqlUtilities::removeTracker(b->id(), name);
    pSearchTab->removeTracker(b);
    delete b; // This removes the tab as well, as the widget is destroyed

    QSettings settings("Entomologist");
    settings.remove(QString("%1-sort-column").arg(name));
    settings.remove(QString("%1-sort-order").arg(name));
    settings.remove(QString("%1-header-geometry").arg(name));
}

void
MainWindow::fieldsChecked()
{
    Backend *tracker= qobject_cast<Backend*>(sender());
    addTrackerToList(tracker, true);
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

#ifndef Q_OS_MAC
    pTrayIcon = new QSystemTrayIcon(QIcon(":/logo"), this);
    pTrayIcon->setToolTip("Entomologist");
    connect(pTrayIcon, SIGNAL(activated(QSystemTrayIcon::ActivationReason)),
            this, SLOT(trayActivated(QSystemTrayIcon::ActivationReason)));
    pTrayIconMenu = new QMenu(this);
    pTrayIconMenu->addAction(ui->action_Quit);
    pTrayIcon->setContextMenu(pTrayIconMenu);
    pTrayIcon->show();
#endif
}

// Called from the tracker tab to display the context menu
void
MainWindow::showMenu(int tabIndex)
{
    QMenu contextMenu(tr("Context menu"), this);
    QAction *a;
    // Show a different context menu for the search tab
    int searchIndex = ui->trackerTab->indexOf(pSearchTab);
    if (tabIndex == searchIndex)
    {
        QAction *searchAction = contextMenu.addAction(tr("Clear Results"));
        a = contextMenu.exec(QCursor::pos());
        if (a == searchAction)
        {
            SqlUtilities::clearSearch();
            QSettings settings("Entomologist");
            settings.setValue("last-search-query", "");
            pSearchTab->refreshResults();
        }
        return;
    }

    QString trackerName = ui->trackerTab->tabText(tabIndex);
    Backend *b = NULL;
    for (int i = 0; i < mBackendList.size(); ++i)
    {
        b = mBackendList.at(i);
        if (trackerName == b->name())
            break;
    }

    if (b == NULL)
        return;

    QString id = b->id();
    QAction *editAction = contextMenu.addAction(tr("Edit"));
    QAction *deleteAction = contextMenu.addAction(tr("Delete"));
    QAction *resyncAction = contextMenu.addAction(tr("Resync"));
    a = contextMenu.exec(QCursor::pos());
    if (a == editAction)
    {
        NewTracker t(this, true);
        t.setName(b->name());
        t.setHost(b->url());
        t.setUsername(b->username());
        t.setPassword(b->password());
        t.setTrackerType(b->type());
        if (t.exec() == QDialog::Accepted)
        {

            updateTracker(id, t.data());
        }
    }
    else if (a == deleteAction)
    {
        QMessageBox box;
        box.setText(QString("Are you sure you want to delete %1?").arg(b->name()));
        box.setStandardButtons(QMessageBox::Yes|QMessageBox::No);
        if (box.exec() == QMessageBox::Yes)
        {
            deleteTracker(id);
        }
    }
    else if (a == resyncAction)
    {
        mSyncPosition = mBackendList.size();
        syncTracker(b);
    }
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
MainWindow::searchFocusTriggered()
{
    int index = ui->trackerTab->indexOf(pSearchTab);
    if (index != -1)
        ui->trackerTab->setCurrentIndex(index);
}

int
MainWindow::compareTabName(QString compareItem)
{
    int ret = -1;
    for(int i = 0; i < ui->trackerTab->count(); i++)
    {
        if(QString::compare(compareItem,ui->trackerTab->tabText(i)) == 0)
        {
            ret = i;
            break;
        }
    }
    return(ret);
}

void
MainWindow::openSearchedBug(const QString &trackerName,
                            const QString &bugId)
{
    for (int i = 0; i < mBackendList.size(); ++i)
    {
        Backend *b = mBackendList.at(i);
        if (b->name() == trackerName)
        {
            qDebug() << "openSearchedBug: mSyncRequests is now " << mSyncRequests;
            b->displayWidget()->loadSearchResult(bugId);
        }
    }
}

void
MainWindow::checkForUpdates()
{
    QNetworkRequest req = QNetworkRequest(QUrl("http://www.entomologist-project.org/latest_version"));
    QString userAgent = QString("Entomologist/%1").arg(APP_VERSION);
    req.setRawHeader("User-Agent", userAgent.toLocal8Bit());
    QNetworkReply *rep = pManager->get(req);
    connect(rep, SIGNAL(finished()),
            this, SLOT(updateCheckResponse()));
}

void
MainWindow::updateCheckResponse()
{
    QNetworkReply *reply = qobject_cast<QNetworkReply*>(sender());
    if (reply->error())
        return;

    // The format of the "you can upgrade" file is simple:
    // MAJOR_VERSION (e.g, 1.0) \n
    // MINOR_VERSION (e.g, 2) \n
    // CHANGELOG
    QString versionInfo = QString::fromUtf8(reply->readAll());
    QString majorVersion = versionInfo.section("\n", 0,0);
    QString minorVersion = versionInfo.section("\n", 1, 1);
    QString notes = versionInfo.section("\n", 2);
    if (majorVersion.toDouble() >= QString(APP_MAJOR_VERSION).toDouble())
    {
        if (minorVersion.toInt() > QString(APP_MINOR_VERSION).toInt())
        {
            UpdatesAvailableDialog dlg(QString("%1.%2").arg(majorVersion, minorVersion), notes, this);
            dlg.exec();
        }
    }
}

void
MainWindow::toggleXmlRpcLogging()
{
    QMessageBox box;
    if (!mLogAllXmlRpcOutput)
    {
        mLogAllXmlRpcOutput = true;
        box.setText("Enabling XML-RPC logging");
    }
    else
    {
        mLogAllXmlRpcOutput  = false;
        box.setText("Disabling XML-RPC logging");
    }
    box.setIcon(QMessageBox::Information);
    box.setStandardButtons(QMessageBox::Ok);
    box.exec();
}

// TODO implement this?
void
MainWindow::handleSslErrors(QNetworkReply *reply,
                         const QList<QSslError> &errors)
{
    Q_UNUSED(errors);
    reply->ignoreSslErrors();
}
