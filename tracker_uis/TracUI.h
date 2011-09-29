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

#ifndef TRACUI_H
#define TRACUI_H

#include <QWidget>
#include "BackendUI.h"

namespace Ui {
    class TracUI;
}

class QModelIndex;
class QStringList;

class TracUI : public BackendUI
{
    Q_OBJECT

public:
    explicit TracUI(const QString &id, Backend *backend, QWidget *parent = 0);
    ~TracUI();
    void loadFields();

public slots:
    void reloadFromDatabase();
    void itemDoubleClicked(const QModelIndex &index);
    void headerContextMenu(const QPoint &pos);
    void commentsDialogClosing(QMap<QString, QString> details, QString newComment);
    void sortIndicatorChanged(int logicalIndex, Qt::SortOrder order);
    void addBugToToDoList(const QString &bugId);
    void loadSearchResult(const QString &id);
    void searchResultFinished(QMap<QString, QString> resultMap);

private:
    void setupTable();
    Ui::TracUI *ui;
    QStringList mPriorities, mSeverities, mStatuses;
    QStringList mVersions, mComponents, mResolutions;
};

#endif // TRACUI_H
