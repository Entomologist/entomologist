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

#include <QtGui>
#include <unistd.h>
#include <fcntl.h>

#include "MainWindow.h"

int singleInstance(void);

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    QString locale = QLocale::system().name();
    QTranslator qtTranslator;
    if (!qtTranslator.load("qt_" + locale, QLibraryInfo::location(QLibraryInfo::TranslationsPath)))
        qDebug() << "Could not load system locale: " << "qt_" + locale;
    a.installTranslator(&qtTranslator);

    QTranslator translator;
    if (!translator.load(QString("entomologist_") + locale, "/usr/share/entomologist"))
        qDebug() << "Could not load locale file";
    a.installTranslator(&translator);
    a.setApplicationVersion(APP_VERSION);

    if (!singleInstance())
    {
        QMessageBox box;
        box.setText("Another instance of Entomologist is running.");
        box.exec();
        exit(1);
    }

    MainWindow w;
    w.show();
    return a.exec();
}

int singleInstance(void)
{
    struct flock lock;
    int fd;

    QString path = QDesktopServices::storageLocation(QDesktopServices::DataLocation);
    path.append(QDir::separator()).append("entomologist").append(QDir::separator()).append("entomologist.lock");

    lock.l_type = F_WRLCK;
    lock.l_whence = SEEK_SET;
    lock.l_start = 0;
    lock.l_len = 0;

    if ((fd = open(path.toLocal8Bit(), O_WRONLY|O_CREAT, 0666)) == -1)
        return 0;

    if (fcntl(fd, F_SETLK, &lock) == -1)
        return 0;

    return 1;
}
