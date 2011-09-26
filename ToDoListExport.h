#ifndef TODOLISTEXPORT_H
#define TODOLISTEXPORT_H

#include <QDialog>
#include <QMap>

namespace Ui {
    class ToDoListExport;
}

class ToDoListExport : public QDialog
{
    Q_OBJECT

public:
    explicit ToDoListExport(QWidget *parent = 0);
    ~ToDoListExport();
    QMap<QString,QString> getData();

private:
    Ui::ToDoListExport *ui;
    void populateServices();
    void populateRecipients();
};

#endif // TODOLISTEXPORT_H
