#include <QMouseEvent>

#include "ClickableText.h"

ClickableText::ClickableText(QWidget *parent) :
    QLabel(parent)
{
    mOnClickText = "";
}

void
ClickableText::enterEvent(QEvent *event)
{
    Q_UNUSED(event);
    setCursor(Qt::PointingHandCursor);
}

void
ClickableText::leaveEvent(QEvent *event)
{
    Q_UNUSED(event);
    setCursor(Qt::ArrowCursor);
}

void
ClickableText::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton)
        emit clicked(mOnClickText);
}
void
ClickableText::setText(const QString &text)
{
    if (mOnClickText == "")
        mOnClickText = text;
    QLabel::setText(text);
}

void
ClickableText::setClickText(const QString &onClickText)
{
    mOnClickText = onClickText;
}
