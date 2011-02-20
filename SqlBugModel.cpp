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
#include <QPixmap>
#include <QDebug>

#include "SqlBugModel.h"

SqlBugModel::SqlBugModel(QObject *parent)
    : QSqlQueryModel(parent)
{
}

// This is necessary to display the icon next to the tracker name in the bug list
QVariant
SqlBugModel::data(const QModelIndex &item,
                  int role) const
{
     QVariant value = QSqlQueryModel::data(item, role);
     if (role == Qt::DecorationRole && item.column() == 2)
     {
        QString iconPath = QString ("%1%2%3.png")
                                    .arg(QDesktopServices::storageLocation(QDesktopServices::DataLocation))
                                    .arg(QDir::separator())
                                    .arg(QSqlQueryModel::data(item, Qt::DisplayRole).toString());
        if (!QFile::exists(iconPath))
            iconPath = ":/bug";
        return qVariantFromValue(QPixmap(iconPath));
     }
     return value;
}
