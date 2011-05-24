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

#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QMap>
#include <QThread>
#include <QSystemTrayIcon>
#include <QSslError>
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
class QSqlDatabase;
class Backend;
class Autodetector;
class SqlBugModel;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    MainWindow(QWidget *parent = 0);
    ~MainWindow();
   void closeEvent(QCloseEvent *event);

public slots:
    void quitEvent();
    void handleSslErrors(QNetworkReply *reply,
                         const QList<QSslError> &errors);
    void trayActivated(QSystemTrayIcon::ActivationReason reason);
    void addTrackerTriggered();
    void prefsTriggered();
    void websiteTriggered();
    void aboutTriggered();
    void showActionTriggered();
    void searchTriggered();
    void changelogTriggered();
    void workOfflineTriggered();
    void trackerListItemChanged(QListWidgetItem  *item);
    void sortIndicatorChanged(int logicalIndex, Qt::SortOrder order);
    void tableViewContextMenu(const QPoint &p);

    void iconDownloaded();
    void htmlIconDownloaded();
    void bugsUpdated();
    void commentsCached();
    void bugClicked(const QModelIndex &);
    void finishedDetecting(QMap<QString, QString> data);
    void newCommentClicked();
    void saveCommentClicked();
    void resync();
    void upload();
    void severityChanged(const QString &text);
    void priorityChanged(const QString &text);
    void statusChanged(const QString &text);
    void backendError(const QString &message);
    void customContextMenuRequested(const QPoint &pos);
    void trackerItemClicked(QListWidgetItem  *item);
    void commentBugTriggered();
    void searchFocusTriggered();
protected:
    void changeEvent(QEvent *e);
    bool eventFilter(QObject *obj, QEvent *event);

private:
    void setupDB();
    void openDB();
    void createTables();
    void checkDatabaseVersion();
    void deleteTracker(const QString &id);
    void updateTracker(const QString &id, QMap<QString, QString> data);
    void setupTracker(Backend *newBug, QMap<QString, QString> info);
    void syncTracker(Backend *tracker);
    void setupTrayIcon();
    void notifyUser();

    void startAnimation();
    void stopAnimation();
    void loadTrackers();
    void loadComments();
    void addTracker(QMap<QString, QString> info);
    void addTrackerToList(Backend *newTracker, bool sync = false);
    void fetchIcon(const QString &url, const QString &savePath);
    void fetchHTMLIcon(const QString &url, const QString &savePath);
    bool isOnline();
    void loadDetails(long long id);
    bool hasPendingChanges();
    bool hasShadowBug();
    bool trackerNameExists(const QString &name);
    void toggleButtons();
    void filterTable();
    void setTimer();
    int insertTracker(QMap<QString, QString> tracker);
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
    QString mActiveBugId, mActivePriority, mActiveStatus, mActiveSeverity;
    QString mBaseQuery, mWhereQuery, mTrackerQuery, mSortQuery, mActiveQuery;
    bool mLoadingDetails;
    SqlBugModel *pBugModel;
    QMovie *pSpinnerMovie;
    QLabel *pStatusIcon, *pStatusMessage;
    QNetworkAccessManager *pManager;
    QProgressDialog *pDetectorProgress;
    QSpacerItem *pCommentSpacer;
    QTimer *pUpdateTimer;
    QSystemTrayIcon *pTrayIcon;
    QMenu *pTrayIconMenu;
    Ui::MainWindow *ui;
};

#endif // MAINWINDOW_H
