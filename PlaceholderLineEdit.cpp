/*
 *  Copyright (c) 2012 Novell, Inc.
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


#include "PlaceholderLineEdit.h"

#include <QDebug>
#include <QFocusEvent>

// Qt 4.7 has placeholder text,
// but we need to handle it for lower versions ourselves
PlaceholderLineEdit::PlaceholderLineEdit(QWidget *parent) :
        QLineEdit(parent)

{
    setPlaceholder();
}

bool
PlaceholderLineEdit::isEmpty()
{
    bool ret = false;
    if (text() == mPlaceholderText)
        ret = true;
    return ret;
}

void
PlaceholderLineEdit::focusInEvent(QFocusEvent *e)
{
    if (text() == mPlaceholderText)
        unsetPlaceholder();
    QLineEdit::focusInEvent(e);
}

void
PlaceholderLineEdit::focusOutEvent(QFocusEvent *e)
{
    if (text() == "")
        setPlaceholder();
    QLineEdit::focusOutEvent(e);
    emit lostFocus();
}

void
PlaceholderLineEdit::setPlaceholder()
{
    setReadOnly(true);
    setStyleSheet("color: grey");
    setText(mPlaceholderText);
}

void
PlaceholderLineEdit::unsetPlaceholder()
{
    setReadOnly(false);
    clear();
    setStyleSheet("color: black");
}

void PlaceholderLineEdit::setPlaceholderText(const QString &placeholder)
{
    mPlaceholderText = placeholder;
    setText(mPlaceholderText);
}
