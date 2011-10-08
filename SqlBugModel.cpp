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

#include <QColor>
#include <QDesktopServices>
#include <QDir>
#include "SqlUtilities.h"
#include <QPixmap>
#include <QDebug>

#include "SqlBugModel.h"

// We just subclass here so we can set custom queries

SqlBugModel::SqlBugModel(QObject *parent)
    : QSqlQueryModel(parent)
{
}

QVariant
SqlBugModel::data(const QModelIndex &item,
                  int role) const
{
     QVariant value = QSqlQueryModel::data(item, role);
     if (role == Qt::DecorationRole && item.column() == 1)
     {
        int type = item.sibling(item.row(), 1).data().toInt();
        if (type == SqlUtilities::HIGHLIGHT_RECENT)
            return qVariantFromValue(QPixmap(":/new_bug"));
        else if (type == SqlUtilities::HIGHLIGHT_SEARCH)
            return qVariantFromValue(QPixmap(":/search_small"));
        else if (type == SqlUtilities::HIGHLIGHT_TODO)
            return qVariantFromValue(QPixmap(":/plus_circle"));
     }
     else if (role == Qt::ToolTipRole && item.column() == 1)
     {
        int type = item.sibling(item.row(), 1).data().toInt();
        if (type == SqlUtilities::HIGHLIGHT_RECENT)
            return "This bug is recent";
        else if (type == SqlUtilities::HIGHLIGHT_SEARCH)
            return "This is a modified bug you found in a search";
        else if (type == SqlUtilities::HIGHLIGHT_TODO)
            return "This bug is in a ToDo list";
     }

     return value;
}
