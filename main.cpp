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
#include <QSysInfo>
#include <QSslSocket>
#include "MainWindow.h"
#include "ErrorHandler.h"
#include "qtsingleapplication/qtsingleapplication.h"

#ifdef Q_OS_UNIX
#include <sys/utsname.h>
#endif

void logHandler(QtMsgType type,
                const char *msg);
void makeDirs();
void openLog();
void digForSystemInfo();
QTextStream *outStream;

int main(int argc, char *argv[])
{
    QtSingleApplication a(argc, argv);
    makeDirs();
    if (a.isRunning())
    {
        QMessageBox box;
        box.setText("Another instance of Entomologist is running!");
        box.exec();
        exit(1);
    }

    openLog();
    qInstallMsgHandler(logHandler);

    QString locale = QLocale::system().name();
    QTranslator qtTranslator;
    QString qtLocaleString = "qt_" + locale;
    if (!qtTranslator.load(qtLocaleString, QLibraryInfo::location(QLibraryInfo::TranslationsPath)))
    {
        qDebug() << "Could not load system locale: "
                 << qtLocaleString
                 << " from "
                 << QLibraryInfo::location(QLibraryInfo::TranslationsPath);
    }
    a.installTranslator(&qtTranslator);

    QTranslator translator;
    QString localDir = QString("%1%2share%3entomologist")
                        .arg(LOCALE_PREFIX)
                        .arg(QDir::separator())
                        .arg(QDir::separator());
    if (!qtTranslator.load(qtLocaleString, QLibraryInfo::location(QLibraryInfo::TranslationsPath)))
    {
        qDebug() << "Could not load Entomologist locale file: "
                 << qtLocaleString
                 << " from "
                 << QLibraryInfo::location(QLibraryInfo::TranslationsPath);
        if (!translator.load(QString("entomologist_") + locale, localDir))
            qDebug() << "Could not load Entomologist locale file :-(";
    }
    a.installTranslator(&translator);
    a.setApplicationVersion(APP_VERSION);

    digForSystemInfo();
    MainWindow w;
    a.setActivationWindow(&w);
    w.show();
    return a.exec();
}

// Add some useful debug information in case of an error report
void
digForSystemInfo(void)
{
    qDebug() << "--------------------------------";
    qDebug() << "Entomologist version: " << APP_VERSION;
    qDebug() << "Compiled Qt version: " << QT_VERSION_STR;
    qDebug() << "Runtime Qt version: " << qVersion();
    if (QSslSocket::supportsSsl())
    {
        qDebug() << "SSL is supported";
    }
    else
    {
        qDebug() << "SSL is NOT supported";
    }

    #ifdef Q_OS_WIN32
        QSysInfo::WinVersion v = QSysInfo::WindowsVersion;
        if (v == QSysInfo::WV_95)
                qDebug() << "Windows 95";
        else if (v == QSysInfo::WV_98)
                qDebug() << "Windows 98";
        else if (v == QSysInfo::WV_Me)
                qDebug() << "Windows Me";
        else if (v == QSysInfo::WV_DOS_based)
                qDebug() << "Windows 9x/Me";
        else if (v == QSysInfo::WV_NT)
                qDebug() << "Windows NT 4.x";
        else if (v == QSysInfo::WV_2000)
                qDebug() << "Windows 2000";
        else if (v == QSysInfo::WV_XP)
                qDebug() << "Windows XP";
        else if (v == QSysInfo::WV_2003)
                qDebug() << "Windows Server 2003";
        else if (v == QSysInfo::WV_VISTA)
                qDebug() << "Windows Vista";
        else if (v == QSysInfo::WV_WINDOWS7)
                qDebug() << "Windows 7";
        else if (v == QSysInfo::WV_NT_based)
                qDebug() << "Windows NT based";
        else
            qDebug() << "Unknown Windows version";
    #elif defined Q_OS_MAC
        QSysInfo::MacVersion v = QSysInfo::MacintoshVersion;
        if (v == QSysInfo::MV_10_3)
            qDebug() << "OS X 10.3";
        else if (v == QSysInfo::MV_10_4)
            qDebug() << "OS X 10.4";
        else if (v == QSysInfo::MV_10_5)
            qDebug() << "OS X 10.5";
        else if(v == QSysInfo::MV_10_6)
            qDebug() << "OS X 10.6";
        else
            qDebug() << "Unknown OS X version";
    #elif defined Q_OS_UNIX
        struct utsname *buf = (struct utsname *) malloc(sizeof(struct utsname));
        if (uname(buf) == 0)
        {
            qDebug() << buf->sysname
                     << " "
                     << buf->release
                     << " "
                     << buf->version
                     << " "
                     << buf->machine;
            free(buf);
        }
        else
        {
            qDebug() << "Unknown UNIX platform: uname failed";
        }
    #elif defined Q_OS_ANDROID
        qDebug() << "Android";
    #else
        qDebug() << "Unknown platform";
    #endif
    qDebug() << "--------------------------------";
}

void openLog(void)
{
    QString fileName = QString("%1%2%3%4entomologist.log")
                       .arg(QDesktopServices::storageLocation(QDesktopServices::DataLocation))
                       .arg(QDir::separator())
                       .arg("entomologist")
                       .arg(QDir::separator());
    qDebug() << "Logging to " << fileName;
    QFile *log = new QFile(fileName);
    if (log->open(QIODevice::WriteOnly | QIODevice::Truncate))
    { 
        outStream = new QTextStream(log);
    }
    else
    {
        qDebug() << "Could not open log file";
        log->open(stderr, QIODevice::WriteOnly);
        outStream = new QTextStream(log);
    }


}

void logHandler(QtMsgType type,
                 const char *msg)
 {
     switch (type)
     {
        case QtDebugMsg:
            *outStream << QTime::currentTime().toString().toAscii().data()
                      << " DEBUG: "
                      << QString::fromUtf8(msg)
                      << "\n";
            break;
        case QtCriticalMsg:
            *outStream << QTime::currentTime().toString().toAscii().data()
                      << " CRITICAL: "
                      << QString::fromUtf8(msg)
                      << "\n";
            break;
        case QtWarningMsg:
            *outStream << QTime::currentTime().toString().toAscii().data()
                      << " WARNING: "
                      << QString::fromUtf8(msg)
                      << "\n";
            break;
        case QtFatalMsg:
            *outStream << QTime::currentTime().toString().toAscii().data()
                      << " FATAL: "
                      << QString::fromUtf8(msg)
                      << "\n";
            outStream->flush();
            ErrorHandler::handleError("A fatal application error occurred.  I must exit.", msg);
            abort(); // Abort to trigger a stack trace on OS X
     }
     outStream->flush();
 }

void
makeDirs(void)
{
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
}
