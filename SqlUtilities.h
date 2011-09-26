/*
 *  Copyright (c) 2011 SUSE Linux Products GmbH
 *  All Rights Reserved.
 *
 *  This file is part of Entomologist.
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
 *  You should have received a copy of the GNU General Public License version 2
 *  along with Foobar.  If not, see <http://www.gnu.org/licenses/>
 *
 *  Author: Matt Barringer <mbarringer@suse.de>
 *
 */

#ifndef SQLUTILITIES_H
#define SQLUTILITIES_H

#include <QObject>
#include <QList>
#include <QMap>
#include <QSqlDatabase>
#include <QStringList>

class QString;

class SqlUtilities : public QObject
{
Q_OBJECT
public:
    enum {
        HIGHLIGHT_RECENT = 1,
        HIGHLIGHT_SEARCH,
        HIGHLIGHT_TODO
    };

    SqlUtilities();
    ~SqlUtilities();

    // Returns the version of the schema
    static int dbVersion();
    static void openDb(const QString &dbPath);
    static void closeDb();

    // Checks the various shadow tables to see
    // if the user has modified anything
    static bool hasPendingChanges();

    // Returns the ID of the tracker if the name exists
    static int trackerNameExists(const QString &name);

    // Does this particular bug have a shadow bug?
    static int hasShadowBug(const QString &table_name,
                             const QString &bugId,
                             const QString &trackerId);

    // Create the tables
    static void createTables(int dbVersion);

    // Return a list of the tracker details
    static QList< QMap<QString, QString> > loadTrackers();

    // Get all comments for a particular bugs
    static QList< QMap<QString, QString> > loadComments(const QString &trackerId,
                                                        const QString &bugId,
                                                        bool shadow);
    // A generic insertion function that converts the QMap keys into the column names
    static int simpleInsert(const QString &tableName,
                            QMap<QString, QString> data);
    static bool simpleUpdate(const QString &tableName,
                             QMap<QString, QString> update,
                             QMap<QString, QString> where);
    static QString getBugDescription(const QString &table,
                                     const QString &bugId);
    static void directExec(QSqlDatabase db,
                           const QString &sql);
    static QStringList fieldValues(const QString &tracker_id,
                                   const QString &fieldName);
    // Deletes all entries in the search table
    static void clearSearch();

    // Clears the highlight_type when highlight_type is HIGHLIGHT_RECENT
    static void clearRecentBugs(const QString &tableName);

    // Tracker-specific SELECT calls
    static QMap<QString, QString> tracBugDetail(const QString &rowId);
    static QMap<QString, QString> mantisBugDetail(const QString &rowId);
    static QMap<QString, QString> bugzillaBugDetail(const QString &rowId);


signals:
    void success();
    void failure(QString message);
    void commentFinished();
    void bugsFinished(QStringList idList);

public slots:
    void deleteBugs(const QString &trackerId);
    void insertBugs(const QString &tableName, QList< QMap<QString, QString> > list);
    void multiInsert(const QString &tableName, QList< QMap<QString, QString> > list);

    // insertComments inserts comments for a number of different bugs.
    // insertBugComments inserts comments for just one bug
    void insertComments(QList<QMap<QString, QString> > commentList);
    void insertBugComments(QList<QMap<QString, QString> > commentList);

    void syncDB(int id, const QString &timestamp);
    void saveCredentials(int id, const QString &username, const QString &password);

private:
    QSqlDatabase mDatabase;
};

#endif // SQLUTILITIES_H
