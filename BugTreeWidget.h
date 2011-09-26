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
 *  Author: David Williams <redache@googlemail.com>
 *
 */


#ifndef BUGTREEWIDGET_H
#define BUGTREEWIDGET_H

#include <QTreeWidget>

class BugTreeWidget : public QTreeWidget
{
    Q_OBJECT

public:
    BugTreeWidget(QWidget *parent = 0);

    void dragEnterEvent(QDragEnterEvent *event);

protected:
    bool dropMimeData(QTreeWidgetItem *parent,
                      int index,
                      const QMimeData *data,
                      Qt::DropAction action);
    void mouseMoveEvent(QMouseEvent * event);
    void dragMoveEvent(QDragMoveEvent *e);
    void dropEvent(QDropEvent *event);

signals:
    void bugDropped(const QString &data,
                    int parent);
    void bugMoved(int newParentId, int bugId);
    void internalTodoMove(QMap<int,  QVariant> data,
                          int parent);
};

#endif // BUGTREEWIDGET_H
