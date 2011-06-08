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

#include <QMessageBox>
#include <QDebug>
#include "ErrorHandler.h"
#include "ErrorReport.h"

void
ErrorHandler::handleError(const QString &message, const QString &details)
{
    qDebug() << "handleError: " << message;
    QMessageBox box;
    box.setText(message);
    if (details.size() > 0)
        box.setDetailedText(details);
    box.addButton("Report", QMessageBox::ActionRole);
    box.addButton(QMessageBox::Ok);
    box.setIcon(QMessageBox::Critical);
    box.exec();

    if (box.buttonRole(box.clickedButton()) == QMessageBox::ActionRole)
    {
        ErrorReport report;
        report.exec();
    }
}
