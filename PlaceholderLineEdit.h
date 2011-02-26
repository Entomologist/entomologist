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

#ifndef PLACEHOLDERLINEEDIT_H
#define PLACEHOLDERLINEEDIT_H

#include <QLineEdit>

class PlaceholderLineEdit : public QLineEdit
{
    Q_OBJECT
public:
    PlaceholderLineEdit(QWidget *parent = 0);
    void setPlaceholderText(const QString &placeholder);

signals:
    void lostFocus();

protected:
    void focusInEvent(QFocusEvent *e);
    void focusOutEvent(QFocusEvent *e);

private:
    void setPlaceholder();
    void unsetPlaceholder();
    QString mPlaceholderText;
};

#endif // PLACEHOLDERLINEEDIT_H
