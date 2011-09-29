#ifndef CLICKABLETEXT_H
#define CLICKABLETEXT_H

#include <QLabel>

class ClickableText : public QLabel
{
Q_OBJECT
public:
    explicit ClickableText(QWidget *parent = 0);
    void setClickText(const QString &onClickText);

public slots:
    void setText(const QString &text);

protected:
    void enterEvent(QEvent *event);
    void leaveEvent(QEvent *event);
    void mousePressEvent(QMouseEvent *event);

signals:
    void clicked(QString myText);

private:
    QString mOnClickText;

};

#endif // CLICKABLETEXT_H
