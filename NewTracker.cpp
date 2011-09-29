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

#include "NewTracker.h"
#include "ui_NewTracker.h"

NewTracker::NewTracker(QWidget *parent, bool edit) :
    QDialog(parent),
    ui(new Ui::NewTracker)
{
    ui->setupUi(this);
    ui->hintLabel->setText("");
    ui->saveButton->setIcon(style()->standardIcon(QStyle::SP_DialogSaveButton));
    ui->cancelButton->setIcon(style()->standardIcon(QStyle::SP_DialogCancelButton));

    connect(ui->urlEdit, SIGNAL(lostFocus()),
            this, SLOT(serverFocusOut()));
    connect(ui->urlEdit, SIGNAL(textChanged(QString)),
            this, SLOT(hostTextChanged(QString)));
    connect(ui->cancelButton, SIGNAL(clicked()),
            this, SLOT(close()));
    connect(ui->saveButton, SIGNAL(clicked()),
            this, SLOT(okClicked()));
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

void
NewTracker::okClicked()
{
    QString text = "";
    if (ui->nameEdit->text().isEmpty())
        text = "You must assign a name for your tracker.";
    if (ui->urlEdit->isEmpty())
        text = "I need a URL in order to proceed.";
    if (ui->userEdit->text().isEmpty())
        text = "I need a username in order to proceed.";

    if (!text.isEmpty())
    {
        QMessageBox msgBox;
        msgBox.setIcon(QMessageBox::Critical);
        msgBox.setText(text);
        msgBox.setStandardButtons(QMessageBox::Ok);
        msgBox.exec();
    }
    else
    {
        accept();
    }
}

void
NewTracker::hostTextChanged(const QString &text)
{
    if (text.startsWith("https://code.google.com") || text.startsWith("code.google.com"))
        ui->hintLabel->setText(tr("Your username should be your Gmail address, and the format of the URL should be <tt>code.google.com/p/<b>PROJECT_NAME</b></tt>"));
    else
        ui->hintLabel->setText("");
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
NewTracker::serverFocusOut()
{
    QString server = ui->urlEdit->text();
    if (server.toLower().endsWith("launchpad.net"))
        ui->passEdit->setEnabled(false);
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
