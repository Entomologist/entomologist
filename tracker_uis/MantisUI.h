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

#ifndef MANTISUI_H
#define MANTISUI_H

#include <QWidget>
#include "BackendUI.h"

namespace Ui {
    class MantisUI;
}

class QModelIndex;
class QStringList;
class QHeaderView;

class MantisUI : public BackendUI
{
    Q_OBJECT

public:
    explicit MantisUI(const QString &id,
                      const QString &trackerName,
                      Backend *backend,
                      QWidget *parent = 0);
    ~MantisUI();
    void loadFields();

public slots:
    void reloadFromDatabase();
    void itemDoubleClicked(const QModelIndex &index);
    void headerContextMenu(const QPoint &pos);
    void sortIndicatorChanged(int logicalIndex, Qt::SortOrder order);
    void loadSearchResult(const QString &id);
    void searchResultFinished(QMap<QString, QString> resultMap);
private:
    void setupTable();
    Ui::MantisUI *ui;
    QStringList mPriorities, mSeverities, mStatuses;
    QStringList mVersions, mResolutions, mRepro;
};

#endif // MANTISUI_H
