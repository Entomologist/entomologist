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
 *  Author: David Williams <redache@googlemail.com>
 *
 */

#include "ToDoItem.h"

/*
 * This is the data Object for a ToDo Item. The primary purpose of this is to store the
 * status of the item so we know what we have to do when it comes to a sync.
 *
 * TODO:
 *
 * - Potentially serialize these objects to preserve their state if the window closes.
 * - If the application closes then delete the file? or delete the file once a sync occurs.
 */

ToDoItem::ToDoItem(QString list,
                   QString internalID,
                   QString bugID,
                   QString summary,
                   QString date,
                   QString lastModified,
                   bool completed) :
    QObject(),
    mListName(list),
    mInternalID(internalID),
    mBugID(bugID),
    mSummary(summary),
    mDate(date),
    mLastModified(lastModified),
    mCompleted(completed)
{
    mStatus = UNCHANGED;
}

