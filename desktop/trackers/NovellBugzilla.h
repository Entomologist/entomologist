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

#ifndef NOVELLBUGZILLA_H
#define NOVELLBUGZILLA_H

#include "Bugzilla.h"

class NovellBugzilla : public Bugzilla
{
    Q_OBJECT
public:
    enum {
        NOVELL_CHECK_VERSION = 1,
        NOVELL_GET_COMMENTS
    };

    NovellBugzilla(const QString &url);
    ~NovellBugzilla();
    void sync();
    void login();
    void checkVersion();
    void getComments(const QString &bugId);
public slots:
    void finished();
    void syncFinished();

private:
    int state;
    QString mCommentBugId;
};

#endif // NOVELLBUGZILLA_H
