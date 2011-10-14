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
#include "ToDoListServiceAdd.h"
#include "ui_ToDoListServiceAdd.h"
#include "QDebug"

ToDoListServiceAdd::ToDoListServiceAdd(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::ToDoListServiceAdd)
{
    ui->setupUi(this);

    ui->cancelButton->setIcon(style()->standardIcon(QStyle::SP_DialogCancelButton));
    ui->saveButton->setIcon(style()->standardIcon(QStyle::SP_DialogSaveButton));

    // Temporarily hide the widgets that we only need for WebDAV
    ui->urlLabel->hide();
    ui->urlLine->hide();
    ui->passwordLabel->hide();
    ui->passwordLine->hide();
    connect(ui->cancelButton, SIGNAL(clicked()),
            this, SLOT(reject()));
    connect(ui->saveButton, SIGNAL(clicked()),
            this, SLOT(accept()));
    connect(ui->servicesCombo, SIGNAL(currentIndexChanged(QString)),
            this, SLOT(indexChanged(QString)));

    needsURL(ui->servicesCombo->currentText());
    needsPassword(ui->servicesCombo->currentText());
}

ToDoListServiceAdd::~ToDoListServiceAdd()
{
    delete ui;
}

void
ToDoListServiceAdd::indexChanged(const QString &text)
{
    ui->nameLine->setText(text);
    if (text == "Google Tasks")
        ui->uNameLine->setPlaceholderText("Example: johndoe@googlemail.com");
    else
        ui->uNameLine->setPlaceholderText("");
    needsURL(text);
    needsPassword(text);
}

QStringList
ToDoListServiceAdd::getData()
{
    QStringList data;
    data.append(ui->servicesCombo->currentText());
    data.append(ui->nameLine->text());
    data.append(ui->uNameLine->text());
    data.append(ui->passwordLine->text());

    if(data.at(0).compare("Generic Web Dav") == 0)
        data.append(ui->urlLine->text());

    return data;
}


void
ToDoListServiceAdd::setServiceCombo(QStringList services)
{
    foreach(QString service, services)
        ui->servicesCombo->addItem(service);
}

void
ToDoListServiceAdd::needsURL(const QString &text)
{
    if(text.compare("Generic Web Dav") == 0)
    {
        ui->urlLabel->setEnabled(true);
        ui->urlLine->setEnabled(true);
    }
    else
    {
        ui->urlLabel->setEnabled(false);
        ui->urlLine->setEnabled(false);
    }
}

// Hide the password line edit for anything but the generic web dav as we don't need to know the password for the other services.
// We don't need the username either but it's nice to give context to the user.
void
ToDoListServiceAdd::needsPassword(const QString &password)
{
    if(password.compare("Generic Web Dav") == 0)
    {
        ui->passwordLine->setEnabled(true);
        ui->passwordLabel->setEnabled(true);

    }
    else
    {
        ui->passwordLine->setEnabled(false);
        ui->passwordLabel->setEnabled(false);
    }
}

void
ToDoListServiceAdd::setData(QString service,QString name, QString username, QString password, QString url)
{
    ui->servicesCombo->setCurrentIndex(ui->servicesCombo->findText(service));
    ui->nameLine->setText(name);
    ui->uNameLine->setText(username);
    ui->passwordLine->setText(password);

    if(url.compare("NULL") != 0)
        ui->urlLine->setText(url);
}

// Disable the username edit on service edit so the user can't change the selected username.
void
ToDoListServiceAdd::disableUser()
{
    ui->uNameLine->setDisabled(true);
}
