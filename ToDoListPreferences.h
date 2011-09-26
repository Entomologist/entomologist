#ifndef TODOLISTPREFERENCES_H
#define TODOLISTPREFERENCES_H

#include <QDialog>
#include "ToDoListServiceAdd.h"
namespace Ui {
    class ToDoListPreferences;
}

class ToDoListPreferences : public QDialog
{
    Q_OBJECT

public:
    explicit ToDoListPreferences(QWidget *parent = 0);
    ~ToDoListPreferences();

public slots:
    void addService();
    void customContextMenuRequested(const QPoint &pos);
    void editService();
    void deleteService();
signals:
    void registerUser(QString serviceType,QString serviceName);

private:
    void populateServices();
    void insertData(ToDoListServiceAdd* service);
    void updateData(ToDoListServiceAdd* service,int id);
    Ui::ToDoListPreferences *ui;
};

#endif // TODOLISTPREFERENCES_H
