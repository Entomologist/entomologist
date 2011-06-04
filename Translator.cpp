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

#include "Translator.h"

#include <QDebug>
#include <QSqlDatabase>
#include <QVariant>
#include <QSqlQuery>
#include <QSqlError>
#include <QDir>
#include <QLibraryInfo>

Translator::Translator(QObject *parent) :
    QObject(parent)
{
}

Translator::~Translator()
{
    if (mDatabase.isOpen())
        mDatabase.close();
}

void
Translator::openDatabase()
{
    QString dbName = "entomologist.translations.db";
    QString path = "";
    QString separator = QDir::separator();
    mDatabase = QSqlDatabase::addDatabase("QSQLITE", "translations");
    if (!mDatabase.isValid())
    {
        qDebug() << "Couldn't create the database connection";
        exit(1);
    }

    // First see if the file is in the same directory
    // as the application.
    path = QString("%1%2%3")
           .arg(QDir::currentPath())
           .arg(separator)
           .arg(dbName);

    qDebug() << "Looking for column translations in " << path;
    if (QFile::exists(path))
    {
        mDatabase.setDatabaseName(path);
        if (mDatabase.open())
            return;
    }

    // No?  Let's check $PREFIX/entomologist
    path = QString ("%1%2entomologist%3%4")
           .arg(LOCALE_PREFIX)
           .arg(separator)
           .arg(separator)
           .arg(dbName);

    qDebug() << "Looking for column translations in " << path;
    if (QFile::exists(path))
    {
        mDatabase.setDatabaseName(path);
        if (mDatabase.open())
            return;
    }

    // Still not there?  Maybe in the translations directory
    path = QString("%1%2%3")
           .arg(QLibraryInfo::location(QLibraryInfo::TranslationsPath))
           .arg(separator)
           .arg(dbName);
    qDebug() << "Looking for column translations in " << path;
    if (QFile::exists(path))
    {
        mDatabase.setDatabaseName(path);
        if (mDatabase.open())
            return;
    }

    qDebug() << "Could not open column translation database";
}

void
Translator::closeDatabase()
{
    if (mDatabase.isOpen())
        mDatabase.close();
}

QString
Translator::translate(const QString &str)
{
    qDebug() << "Translate: " << str;
    if (!mDatabase.isOpen())
    {
        qDebug() << "Translations DB is not open";
        return "unknown";
    }

    QSqlQuery q(mDatabase);
    QString query = QString("SELECT entomologist FROM translations WHERE local=\'%1' LIMIT 1;")
                    .arg(str);
/*
    qDebug() << "Prepare";
    if (!q.prepare("SELECT entomologist FROM translations WHERE local=:local LIMIT 1"))
    {
        qDebug() << "Translator::translate: Could not prepare.";
        return "unknown";
    }

    qDebug() << "Bind";
    q.bindValue(":local", str);
    */
    qDebug() << query;
    if (!q.exec(query))
    {
        qDebug() << "Translator::translate: Could not exec query: " << q.lastQuery() << ":" << q.lastError().text();
        return "unknown";
    }

    qDebug() << "Next...";
    if (!q.next())
    {
        qDebug() << "Translator::translate: q.next failed for " << q.lastQuery() << ":" << q.lastError().text();
        return "unknown";
    }

    QString ret = "unknown";
    qDebug() << "SQL result: " << q.value(0).toString();
    ret = q.value(0).toString();
    q.finish();
    return (ret);
}
