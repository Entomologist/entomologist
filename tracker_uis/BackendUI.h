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

#ifndef BACKENDUI_H
#define BACKENDUI_H

#include <QWidget>
#include <QAction>
#include <QHeaderView>

class SqlBugModel;
class Backend;
class QProgressDialog;

class BackendUI : public QWidget
{
    Q_OBJECT
public:
    explicit BackendUI(const QString &id,
                       const QString &trackerName,
                       Backend *backend,
                       QWidget *parent = 0);
    ~BackendUI();

    void setID(const QString &id) { mId = id; }
    void setTrackerName(const QString &name) { mTrackerName = name; }
    void setBackend(Backend *backend);
    virtual void loadFields() {}
    virtual void loadSearchResult(const QString &id) { Q_UNUSED(id); }

signals:
    // Emitted when a bug has changed, so MainWindow knows to enable
    // the Upload and Changelog buttons
    void bugChanged();

public slots:
    virtual void reloadFromDatabase() {}
    void setShowOptions(bool showMyBugs,
                        bool showMyReports,
                        bool showMyCCs,
                        bool showMonitored);
    void copyBugUrl(const QString &bugId);
    void openBugInBrowser(const QString &bugId);
    void saveHeaderSetting(int logicalIndex, int oldSize, int newSize);

    virtual void addBugToToDoList(const QString &bugId) { Q_UNUSED(bugId); }
    virtual void searchResultFinished(QMap<QString, QString> resultMap) { Q_UNUSED(resultMap); }

protected:
    void hideColumnsMenu(const QPoint &pos,
                         const QString &settingName,
                         const QString &columnName);
    void saveNewShadowItems(const QString &tableName,
                            QMap<QString, QString> shadowBug,
                            const QString &newComment);
    void startSearchProgress();
    void stopSearchProgress();
    void restoreHeaderSetting();

    Backend *pBackend;
    bool mShowMyBugs, mShowMyReports, mShowMyCCs, mShowMonitored;
    QString mActiveQuery, mBaseQuery, mWhereQuery, mSortQuery;
    QString mId, mTrackerName;
    QStringList mTableHeaders;
    QHeaderView *v;
    SqlBugModel *pBugModel;
    QVariantMap mHiddenColumns;

private:
    QProgressDialog *pSearchProgress;
};

#endif // BACKENDUI_H
