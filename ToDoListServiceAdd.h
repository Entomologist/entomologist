#ifndef TODOLISTSERVICEADD_H
#define TODOLISTSERVICEADD_H

#include <QDialog>

namespace Ui {
    class ToDoListServiceAdd;
}

class ToDoListServiceAdd : public QDialog
{
    Q_OBJECT

public:
    explicit ToDoListServiceAdd(QWidget *parent = 0);
    ~ToDoListServiceAdd();
    void setServiceCombo(QStringList services);
    QStringList getData();
    void setData(QString service,QString name,QString username, QString password, QString url);
    void disableUser();

public slots:
    void indexChanged(const QString &text);

private:
    void needsURL(const QString &text);
    void needsPassword(const QString &password);

    Ui::ToDoListServiceAdd *ui;
};

#endif // TODOLISTSERVICEADD_H
