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

#include "UploadDialog.h"
#include "ui_UploadDialog.h"

UploadDialog::UploadDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::UploadDialog)
{
    ui->setupUi(this);
    connect(ui->buttonBox, SIGNAL(rejected()),
            this, SLOT(reject()));
    connect(ui->buttonBox, SIGNAL(accepted()),
            this, SLOT(okClicked()));
}

UploadDialog::~UploadDialog()
{
    delete ui;
}

void
UploadDialog::okClicked()
{
    if (ui->doNotAskAgainCheckBox->isChecked())
    {
        QSettings settings("Entomologist");
        settings.setValue("no-upload-confirmation", true);
    }

    done(QDialog::Accepted);
}

void
UploadDialog::setChangelog(const QString &changelog)
{
    ui->changelogEdit->setHtml(changelog);
}

void UploadDialog::changeEvent(QEvent *e)
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
