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
#include <QTextStream>

#include "MainWindow.h"

int singleInstance(void);
void logHandler(QtMsgType type,
                const char *msg);
void openLog(void);
QTextStream *outStream;

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    openLog();
    qInstallMsgHandler(logHandler);

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

void openLog(void)
{
    QString fileName = QString("%1%2%3%4/entomologist.log")
                       .arg(QDesktopServices::storageLocation(QDesktopServices::DataLocation))
                       .arg(QDir::separator())
                       .arg("entomologist")
                       .arg(QDir::separator());
    qDebug() << "Logging to " << fileName;
    QFile *log = new QFile(fileName);
    if (log->open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text))
        outStream = new QTextStream(log);
    else
        qDebug() << "Could not open log file!";

}
 void logHandler(QtMsgType type,
                 const char *msg)
 {
     switch (type)
     {
        case QtDebugMsg:
            *outStream << QTime::currentTime().toString().toAscii().data()
                      << " DEBUG: "
                      << msg
                      << "\n";
            break;
        case QtCriticalMsg:
            *outStream << QTime::currentTime().toString().toAscii().data()
                      << " CRITICAL: "
                      << msg
                      << "\n";
            break;
        case QtWarningMsg:
            *outStream << QTime::currentTime().toString().toAscii().data()
                      << " WARNING: "
                      << msg
                      << "\n";
            break;
        case QtFatalMsg:
            *outStream << QTime::currentTime().toString().toAscii().data()
                      << " FATAL: "
                      << msg
                      << "\n";
            outStream->flush();
            abort();
     }
     outStream->flush();
 }

int
singleInstance(void)
{
    struct flock lock;
    int fd;

    QString path = QDesktopServices::storageLocation(QDesktopServices::DataLocation);
    path.append(QDir::separator()).append("entomologist");
    QDir dir;
    if (!dir.mkpath(path))
    {
        qDebug() << "Could not mkpath " << path;
        QMessageBox box;
        box.setText("Could not create data directory.  Exiting");
        box.exec();
        exit(1);
    }
    path.append(QDir::separator()).append("entomologist.lock");


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
