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

#ifndef TRANSLATOR_H
#define TRANSLATOR_H

#include <QObject>
#include <QSqlDatabase>

class QString;

// Mantis 1.2 (and possibly others) translate CSV column titles, which we use for retrieving
// the user's bugs.  This class uses an SQL database to translate non-English Mantis column
// values to ones that we understand.

class Translator : public QObject
{
Q_OBJECT
public:
    Translator(QObject *parent = 0);
    ~Translator();

    void openDatabase();
    bool loadFile(const QString &path);
    QString translate(const QString &str);
    void closeDatabase();

private:
    QSqlDatabase mDatabase;
};

#endif // TRANSLATOR_H
