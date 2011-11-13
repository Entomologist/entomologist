#include <QMouseEvent>
#include <QMenu>

#include "AttachmentLink.h"

AttachmentLink::AttachmentLink(QWidget *parent) :
    QLabel(parent)
{
    mOnClickText = "";
    setStyleSheet("QLabel { color: blue; text-decoration: underline;}");
}

void
AttachmentLink::enterEvent(QEvent *event)
{
    Q_UNUSED(event);
    setCursor(Qt::PointingHandCursor);
}

void
AttachmentLink::leaveEvent(QEvent *event)
{
    Q_UNUSED(event);
    setCursor(Qt::ArrowCursor);
}

void
AttachmentLink::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton)
        emit clicked(mRowId);
    else if (event->button() == Qt::RightButton)
    {
        QMenu contextMenu(tr("Download Link"), this);
        QAction *saveAs = contextMenu.addAction(tr("Save Attachment As"));
        QAction *a = contextMenu.exec(QCursor::pos());
        if (a == saveAs)
            emit saveAsClicked(mRowId);
    }
}
void
AttachmentLink::setText(const QString &text)
{
    if (mOnClickText == "")
        mOnClickText = text;
    QLabel::setText(text);
}

void
AttachmentLink::setClickText(const QString &onClickText)
{
    mOnClickText = onClickText;
}
