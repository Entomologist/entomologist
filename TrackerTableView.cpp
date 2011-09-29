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
 *  Author: Matt Barringer <matt@entomologist-project.org>
 *
 */

#include <QMouseEvent>
#include <QMenu>
#include <QApplication>
#include <QDir>
#include <QSqlQuery>
#include <QSqlError>

#include "TrackerTableView.h"
#include "QDebug"
#include "SqlBugModel.h"

/*
 * This class is an override of QTableView as we need drag and drop support.
 * All of the mouse events are reimplemented for dragging bugs between the main
 * Application Window and the TODO list window.
 *
 */
TrackerTableView::TrackerTableView(QWidget *parent)
    : QTableView(parent)
{
    mBugTableName = "";
    setAcceptDrops(true);
    setSelectionBehavior(QAbstractItemView::SelectRows);
    setSelectionMode(QAbstractItemView::SingleSelection);
    setContextMenuPolicy(Qt::CustomContextMenu);

    connect(this, SIGNAL(customContextMenuRequested(QPoint)),
            this, SLOT(contextMenu(QPoint)));
}

void
TrackerTableView::contextMenu(const QPoint &point)
{
    QModelIndex i = indexAt(point);
    QString bugId = i.sibling(i.row(), 2).data().toString();

    QMenu contextMenu(tr("Bug Menu"), this);
    QAction *copyAction = contextMenu.addAction(tr("Copy URL"));
    QAction *openAction = contextMenu.addAction(tr("Open in a browser"));
    // Maybe later...
//    contextMenu.addSeparator();
//    QAction *todoAction = contextMenu.addAction(tr("Add to ToDo List"));
    QAction *a = contextMenu.exec(QCursor::pos());

    if (a == copyAction)
        emit copyBugURL(bugId);
    else if (a == openAction)
        emit openBugInBrowser(bugId);
//    else if (a == todoAction)
//        emit addBugToToDoList(bugId);
}

void
TrackerTableView::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton)
            mStartPos = event->pos();

    QTableView::mousePressEvent(event);
}

void
TrackerTableView::mouseMoveEvent(QMouseEvent *event)
{
    if (Qt::LeftButton)
    {
        int distance = (event->pos() - mStartPos).manhattanLength();
        if (distance >= QApplication::startDragDistance())
            performDrag();
     }

    QTableView::mouseMoveEvent(event);
}

void TrackerTableView::performDrag()
{
    QModelIndex index = this->currentIndex();
    const QAbstractItemModel * model = index.model();
    int bugID = model->data(model->index(index.row(), 2), Qt::DisplayRole).toInt();

    QMimeData* mimeData = new QMimeData;
    QString data = QString("%1:%2:%3")
            .arg(mBugTableName)
            .arg(mTrackerId)
            .arg(bugID);
    mimeData->setData("text/entomologist", data.toLocal8Bit());
    QDrag *drag = new QDrag(this);
    drag->setMimeData(mimeData);
    drag->setPixmap(QPixmap(":/bug")); // Set the drag icon to the default Bug Icon.
    drag->exec();
}


