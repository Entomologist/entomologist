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

#include <QSettings>

#include "Preferences.h"
#include "ui_Preferences.h"

Preferences::Preferences(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::Preferences)
{
    QSettings settings("Entomologist");
    ui->setupUi(this);
    ui->autoUpdateSpinBox->setValue(settings.value("update-interval", "2").toInt());
    ui->autoUpdateCheckBox->setChecked(settings.value("update-automatically", false).toBool());
    ui->confirmationCheckBox->setChecked(settings.value("no-upload-confirmation", "false").toBool());
    ui->startupSyncCheckbox->setChecked(settings.value("startup-sync", false).toBool());
    if (!ui->autoUpdateCheckBox->isChecked())
        ui->autoUpdateSpinBox->setEnabled(false);

    connect(ui->autoUpdateCheckBox, SIGNAL(stateChanged(int)),
            this, SLOT(stateChanged(int)));
    connect(ui->buttonBox, SIGNAL(accepted()),
            this, SLOT(accepted()));
    connect(ui->buttonBox, SIGNAL(rejected()),
            this, SLOT(close()));
}

Preferences::~Preferences()
{
    delete ui;
}


void
Preferences::stateChanged(int state)
{
    if (state == Qt::Checked)
        ui->autoUpdateSpinBox->setEnabled(true);
    else
        ui->autoUpdateSpinBox->setEnabled(false);
}

void
Preferences::accepted()
{
    QSettings settings("Entomologist");
    settings.setValue("update-interval", ui->autoUpdateSpinBox->value());
    settings.setValue("update-automatically", ui->autoUpdateCheckBox->isChecked());
    settings.setValue("no-upload-confirmation", ui->confirmationCheckBox->isChecked());
    settings.setValue("startup-sync", ui->startupSyncCheckbox->isChecked());

    close();
}

void Preferences::changeEvent(QEvent *e)
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
