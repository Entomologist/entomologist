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

#ifndef ERRORREPORT_H
#define ERRORREPORT_H

#include <QDialog>
class MaiaXmlRpcClient;
namespace Ui {
    class ErrorReport;
}

class ErrorReport : public QDialog {
    Q_OBJECT
public:
    ErrorReport(QWidget *parent = 0);
    ~ErrorReport();

public slots:
    void submitReport();
    void submitResponse(QVariant &arg);
    void rpcError(int error, const QString &message);

protected:
    void changeEvent(QEvent *e);

private:
    void loadLog();
    Ui::ErrorReport *ui;
};

#endif // ERRORREPORT_H
