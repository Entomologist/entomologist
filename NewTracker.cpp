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

#include "NewTracker.h"
#include "ui_NewTracker.h"

NewTracker::NewTracker(QWidget *parent, bool edit) :
    QDialog(parent),
    ui(new Ui::NewTracker)
{
    ui->setupUi(this);
    ui->urlEdit->setPlaceholderText(tr("For example: bugzilla.novell.com or bugs.example.com/bugtracker/"));
    if (edit)
    {
        ui->urlEdit->setEnabled(false);
        setWindowTitle("Edit Tracker");
    }
}

NewTracker::~NewTracker()
{
    delete ui;
}

QMap<QString, QString>
NewTracker::data()
{
    QMap<QString, QString> info;
    info["id"] = "-1";
    info["name"] = ui->nameEdit->text();
    info["url"] = ui->urlEdit->text();
    info["username"] = ui->userEdit->text();
    info["password"] = ui->passEdit->text();
    info["last_sync"] = "1970-01-01T00:00:00";
    return(info);
}

void
NewTracker::setName(const QString &name)
{
    ui->nameEdit->setText(name);
}

void
NewTracker::setHost(const QString &host)
{
    ui->urlEdit->setText(host);
}

void
NewTracker::setUsername(const QString &username)
{
    ui->userEdit->setText(username);
}

void
NewTracker::setPassword(const QString &password)
{
    ui->passEdit->setText(password);
}

void NewTracker::changeEvent(QEvent *e)
{
    QDialog::changeEvent(e);
    switch (e->type()) {
    case QEvent::LanguageChange:
        ui->retranslateUi(this);
        break;
    default:
        break;
    }
}
