#ifndef MONITORDIALOG_H
#define MONITORDIALOG_H

#include <QDialog>
#include <QMap>

class QMovie;
class QListWidgetItem;
class QTreeWidgetItem;
class Backend;

namespace Ui {
    class MonitorDialog;
}

class MonitorDialog : public QDialog {
    Q_OBJECT
public:
    MonitorDialog(QWidget *parent = 0);
    ~MonitorDialog();

public slots:
    void componentFound(QStringList components);
    void backendError(const QString &msg);

protected:
    void changeEvent(QEvent *e);

private:
    void setupBackend(Backend *b, QMap<QString, QString> tracker);
    void checkRequests();
    Ui::MonitorDialog *ui;
    QMovie *pSpinnerMovie;
    QMap<QString, QTreeWidgetItem *> mTreeMap;
    int mRequests;
};

#endif // MONITORDIALOG_H
