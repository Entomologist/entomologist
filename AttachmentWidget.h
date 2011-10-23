#ifndef ATTACHMENTWIDGET_H
#define ATTACHMENTWIDGET_H

#include <QWidget>

namespace Ui {
    class AttachmentWidget;
}

class AttachmentWidget : public QWidget
{
    Q_OBJECT

public:
    explicit AttachmentWidget(QWidget *parent = 0);
    void setRowId(int id);
    void setFilename(const QString &filename);
    void setSummary(const QString &summary);
    void setDetails(const QString &creator, const QString &date);
    ~AttachmentWidget();

signals:
    void clicked(int rowId);
    void saveAsClicked(int rowId);

private:
    Ui::AttachmentWidget *ui;
};

#endif // ATTACHMENTWIDGET_H
