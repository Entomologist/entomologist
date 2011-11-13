#ifndef ATTACHMENTLINK_H
#define ATTACHMENTLINK_H

#include <QLabel>

class AttachmentLink : public QLabel
{
Q_OBJECT
public:
    explicit AttachmentLink(QWidget *parent = 0);
    void setClickText(const QString &onClickText);
    void setId(int id) { mRowId = id; }

public slots:
    void setText(const QString &text);

protected:
    void enterEvent(QEvent *event);
    void leaveEvent(QEvent *event);
    void mousePressEvent(QMouseEvent *event);

signals:
    void clicked(int rowId);
    void saveAsClicked(int rowId);

private:
    QString mOnClickText;
    int mRowId;

};

#endif // ATTACHMENTLINK_H
