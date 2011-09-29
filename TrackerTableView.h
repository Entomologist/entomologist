/*
 *  Copyright (c) 2012 Novell, Inc.
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
 *  Author: David Williams <redache@googlemail.com>
 *
 */

#ifndef TRACKERTABLEVIEW_H
#define TRACKERTABLEVIEW_H

#include <QWidget>
#include <QTableView>

class TrackerTableView : public QTableView
{
    Q_OBJECT

public:
    TrackerTableView(QWidget *parent = 0);
    void setBugTableName(const QString &tableName)
        { mBugTableName = tableName; }
    void setTrackerId(const QString &trackerId)
        { mTrackerId = trackerId; }

public slots:
    void contextMenu(const QPoint &point);

signals:
    void copyBugURL(const QString &bugId);
    void openBugInBrowser(const QString &bugId);
    void addBugToToDoList(const QString &bugId);

protected:
    void mousePressEvent(QMouseEvent *event);
    void mouseMoveEvent(QMouseEvent *event);

private:
    void performDrag();
    QString mBugTableName;
    QString mTrackerId;
    QPoint mStartPos;
};



#endif // TRACKERTABLEVIEW_H
