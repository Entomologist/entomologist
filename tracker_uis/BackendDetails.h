#ifndef BAKENDUI_H
#define BAKENDUI_H

#include <QWidget>
#include <QMap>

class BackendDetails : public QWidget
{
    Q_OBJECT
public:
    explicit BackendDetails(QWidget *parent = 0);
    ~BackendDetails();
   virtual QMap<QString, QString> fieldDetails() { QMap<QString, QString> empty; return(empty); }
};

#endif // BAKENDUI_H
