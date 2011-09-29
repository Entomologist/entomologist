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

#ifndef COMMENTFRAME_H
#define COMMENTFRAME_H

#include <QFrame>

namespace Ui {
    class CommentFrame;
}

class CommentFrame : public QFrame {
    Q_OBJECT
public:
    CommentFrame(QWidget *parent = 0, bool isNewComment = false);
    ~CommentFrame();

    void setRedHeader();
    void toggleEdit();
    void setName(const QString &name);
    void setDate(const QString &date);
    void setPrivate(bool checked);
    void setComment(const QString &comment);
    void setCommentNumber(const QString &number) { mCommentNumber = number; }

    void setBugId(const QString &id) { mBugId = id; }
    QString bugId() { return mBugId; }

    QString commentText();
    QString timestamp();

public slots:
    void saveButtonClicked();

signals:
    void saveCommentClicked();

protected:
    void changeEvent(QEvent *e);

private:
    Ui::CommentFrame *ui;
    QString mCommentNumber, mBugId;
};

#endif // COMMENTFRAME_H
