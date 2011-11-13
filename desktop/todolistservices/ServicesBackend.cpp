#include <QObject>
#include <QNetworkAccessManager>
#include <QSqlQuery>
#include <QSqlError>
#include "ServicesBackend.h"

ServicesBackend::ServicesBackend()
{

    pManager = new QNetworkAccessManager();

}

ServicesBackend::~ServicesBackend()
{

    delete(pManager);

}

void
ServicesBackend::insertItem(const QString &item, const QString &serviceName)
{

    QSqlQuery query("INSERT INTO service_tasks (task_id,service_name) VALUES(:item,:service_name)");
    query.bindValue(":item",item);
    query.bindValue(":service_name",serviceName);
    if(!query.exec())
    {

        qDebug() << "Error adding task to list";
        qDebug() << query.lastQuery() <<  " Bound Values: " << query.boundValues();
        qDebug() << query.lastError();

    }
}



