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

#include <QFile>
#include <QDesktopServices>
#include <QDir>
#include <QTextStream>
#include <QMessageBox>

#include "ErrorReport.h"
#include "ui_ErrorReport.h"
#include "libmaia/maiaXmlRpcClient.h"

ErrorReport::ErrorReport(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::ErrorReport)
{
    ui->setupUi(this);
    loadLog();
    connect(ui->cancelButton, SIGNAL(clicked()),
            this, SLOT(close()));
    connect(ui->submitButton, SIGNAL(clicked()),
            this, SLOT(submitReport()));
}

ErrorReport::~ErrorReport()
{
    delete ui;
}

void
ErrorReport::submitReport()
{
    MaiaXmlRpcClient *client = new MaiaXmlRpcClient(QUrl("https://trac.entomologist-project.org/rpc"), "Entomologist Bug Reporter", this);
    QSslConfiguration config = client->sslConfiguration();
    config.setProtocol(QSsl::AnyProtocol);
    config.setPeerVerifyMode(QSslSocket::VerifyNone);
    client->setSslConfiguration(config);
    QString email = ui->email->text();
    QRegExp atRx("@");
    QRegExp dotRx("\\.");
    email.replace(atRx, " at the domain ");
    email.replace(dotRx, " point ");
    QString summary = QString("Anonymous report from %1").arg(email);
    QVariantList args;
    args << summary;
    args << ui->debugEdit->toPlainText();
    client->call("ticket.create", args, this, SLOT(submitResponse(QVariant&)), this, SLOT(rpcError(int, const QString &)));
}

void
ErrorReport::submitResponse(QVariant &arg)
{
    int id = arg.toInt();
    QString url = QString("https://trac.entomologist-project.org/ticket/%1").arg(id);
    QString message = QString("Thank you!  The report is now available at <a href=%1>%1</a>.").arg(url);
    QMessageBox box;
    box.setText(message);
    box.exec();
    close();
}

void
ErrorReport::rpcError(int error,
                      const QString &message)
{
    QMessageBox box;
    box.setText("Submission failed!  Please submit manually at <a href=https://trac.entomologist-project.org>trac.entomologist-project.org</a>.");
    box.setIcon(QMessageBox::Critical);
    box.setDetailedText(message);
    box.exec();
    close();
}

void
ErrorReport::loadLog()
{
    QString fileName = QString("%1%2%3%4entomologist.log")
                       .arg(QDesktopServices::storageLocation(QDesktopServices::DataLocation))
                       .arg(QDir::separator())
                       .arg("entomologist")
                       .arg(QDir::separator());
    QFile file(fileName);
    if(file.open(QFile::ReadOnly))
    {
        QTextStream in(&file);
        ui->debugEdit->setPlainText(in.readAll());
        file.close();
    }
    else
    {
        ui->debugEdit->setPlainText("Could not open the debug log file.");
    }
}

void ErrorReport::changeEvent(QEvent *e)
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
