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
 *  Author: Matt Barringer <matt@entomologist-project.org>
 *
 */

#include <QDrag>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QDebug>
#include <QStandardItemModel>
#include <QProxyStyle>
#include <QPainter>

#include "BugTreeWidget.h"

BugTreeWidget::BugTreeWidget(QWidget *parent)
    : QTreeWidget(parent)
{
    setAcceptDrops(true);
    setAlternatingRowColors(true);
    setDropIndicatorShown(true);
    setContextMenuPolicy(Qt::CustomContextMenu);
    setSelectionBehavior(QAbstractItemView::SelectRows);
    setSelectionMode(QAbstractItemView::SingleSelection);
}

void
BugTreeWidget::mouseMoveEvent(QMouseEvent *event)
{
    QTreeWidget::mouseMoveEvent(event);
}

void
BugTreeWidget::dragEnterEvent(QDragEnterEvent *event)
{
    if (event->source() != this && event->mimeData()->hasFormat("text/entomologist"))
        event->acceptProposedAction();
    else if (event->source() == this && event->mimeData()->hasFormat("application/x-qabstractitemmodeldatalist"))
        event->acceptProposedAction();
}

void
BugTreeWidget::dragMoveEvent(QDragMoveEvent *event)
{
    if (event->mimeData()->hasFormat("application/x-qabstractitemmodeldatalist"))
    {
        QTreeWidgetItem *item = itemAt(event->pos());
        if ((item == NULL) || (item->parent() != NULL))
            event->ignore();
        else
            event->acceptProposedAction();
    }
    else
    {
        event->acceptProposedAction();
    }
}

// There's a strange bug(?) in Qt where dragging an item over
// to the left of the last top level item causes the dragged item
// to be moved over as a new top level item, rather than as a child
// of the todo list like we want.  So we'll do this by hand.
void
BugTreeWidget::dropEvent(QDropEvent *event)
{
    if (event->mimeData()->hasFormat("text/entomologist"))
    {
        QTreeWidget::dropEvent(event);
    }
    else
    {
        QList<QTreeWidgetItem *> moveItems = selectedItems();
        if (moveItems.size() < 0)
            return;

        QTreeWidgetItem *source = moveItems.at(0);
        if (indexOfTopLevelItem(source) != -1)
            return;

        QTreeWidgetItem *destination = itemAt(event->pos());
        // At the moment, the only supported drop targets
        // are top level items
        if (indexOfTopLevelItem(destination) == -1)
            return;

        int index = source->parent()->indexOfChild(source);
        source->parent()->takeChild(index);
        destination->addChild(source);
        emit bugMoved(destination->data(0, Qt::UserRole).toInt(), source->data(0, Qt::UserRole).toInt());
    }
}

bool
BugTreeWidget::dropMimeData(QTreeWidgetItem *parent,
                            int index,
                            const QMimeData *data,
                            Qt::DropAction action)
{
    Q_UNUSED(action);
    Q_UNUSED(index);
    if (data->hasFormat("text/entomologist"))
    {
        this->expandAll();
        int topLevelParent;
        if (this->indexOfTopLevelItem(parent) != -1)
        {
            topLevelParent = this->indexOfTopLevelItem(parent);
        }
        else
        {
            QModelIndex row = this->indexFromItem(parent);

            if(row.row() != -1)
                topLevelParent = row.parent().row();
            else
                topLevelParent = this->topLevelItemCount() - 1;
        }

        emit bugDropped(data->data("text/entomologist"), topLevelParent);
    }
   return true;
}

