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

#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QMap>
#include <QThread>
#include <QDockWidget>
#include <QSystemTrayIcon>
#include <QSslError>
#include <QTableView>

namespace Ui {
    class MainWindow;
}

class QListWidgetItem;
class QModelIndex;
class QMovie;
class QNetworkReply;
class QLabel;
class QNetworkAccessManager;
class QSpacerItem;
class QProgressDialog;
class QTimer;
class SearchTab;
class QSqlDatabase;
class Backend;
class Autodetector;
class SqlBugModel;
class BackendUI;
class ToDoListWidget;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    MainWindow(QWidget *parent = 0);
    ~MainWindow();
   void closeEvent(QCloseEvent *event);

signals:
    void reloadFromDatabase();
    void setShowOptions(bool showMyBugs,
                        bool showMyReports,
                        bool showMyCCs,
                        bool showMonitored);
public slots:
    void quitEvent();
    void updateCheckResponse();
    void fieldsChecked();
    void versionChecked(const QString &version, const QString &message);
    void handleSslErrors(QNetworkReply *reply,
                         const QList<QSslError> &errors);
    void dockVisibilityChanged(bool visible);
    void trayActivated(QSystemTrayIcon::ActivationReason reason);
    void addTrackerTriggered();
    void toggleButtons();
    void showMenu(int tabIndex);
    void prefsTriggered();
    void websiteTriggered();
    void aboutTriggered();
    void showActionTriggered();
    void changelogTriggered();
    void workOfflineTriggered();
    void showEditMonitoredComponents();
    void iconDownloaded();
    void htmlIconDownloaded();
    void bugsUpdated();
    void filterTable();
    void finishedDetecting(QMap<QString, QString> data);
    void resync();
    void showTodoList();
    void upload();
    void backendError(const QString &message);
    void searchFocusTriggered();
    void toggleXmlRpcLogging();
    void openSearchedBug(const QString &trackerName,
                         const QString &bugId);
protected:
    void changeEvent(QEvent *e);
    void showEvent(QShowEvent *e);
private:
    void checkForUpdates();
    QString cleanupUrl(QString &url);
    void populateStats();
    int compareTabName(QString compareItem);
    void setupDB();
    void checkDatabaseVersion();
    void deleteTracker(const QString &id);
    void updateTracker(const QString &id, QMap<QString, QString> data, bool updateName);
    void setupTracker(Backend *newBug, QMap<QString, QString> info);
    void syncTracker(Backend *tracker);
    void setupTrayIcon();
    void notifyUser();
    void checkVersion(Backend *b);
    void startAnimation();
    void stopAnimation();
    void loadTrackers();
    void addTracker(QMap<QString, QString> info);
    void addTrackerToList(Backend *newTracker, bool sync = false);
    void fetchIcon(const QString &url, const QString &savePath, const QString &username, const QString &password);
    void fetchHTMLIcon(const QString &url, const QString &savePath);
    bool isOnline();
    int trackerNameExists(const QString &name);
    void setTimer();

    QAction *refreshButton, *uploadButton, *changelogButton;
    QString getChangelog();
    QString autodetectTracker(const QString &url);
    void syncNextTracker();
    int mSyncRequests;
    int mSyncPosition;
    bool mUploading;
    QMap<QString, Backend*> mBackendMap;
    QList<Backend *> mBackendList;
    QString mDbPath;
    bool mDbUpdated;
    Backend *pActiveBackend;
    Backend *pNewTracker;
    QString mActiveBugId, mActivePriority, mActiveStatus, mActiveSeverity;
    bool mLoadingDetails;
    QMovie *pSpinnerMovie;
    QLabel *pStatusIcon, *pStatusMessage;
    QNetworkAccessManager *pManager;
    QProgressDialog *pDetectorProgress;
    QSpacerItem *pCommentSpacer;
    QTimer *pUpdateTimer;
    QSystemTrayIcon *pTrayIcon;
    QMenu *pTrayIconMenu;
    QDockWidget *pToDoDock;
    SearchTab *pSearchTab;
    ToDoListWidget *pToDoListWidget;
    Ui::MainWindow *ui;
    QList<BackendUI*> trackerTabsList;
};

#endif // MAINWINDOW_H
