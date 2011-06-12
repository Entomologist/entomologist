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
    enum MonitorNodeRoles
    {
        MONITOR_NODE_PARENT = Qt::UserRole,
        MONITOR_NODE_IS_CACHED = Qt::UserRole + 1,
        MONITOR_NODE_TRACKER = Qt::UserRole + 2,
        MONITOR_NODE_IS_PRODUCT = Qt::UserRole + 3
    };

    MonitorDialog(QWidget *parent = 0);
    ~MonitorDialog();

public slots:
    void itemExpanded(QTreeWidgetItem *item);
    void componentFound(QStringList components);
    void backendError(const QString &msg);

protected:
    void changeEvent(QEvent *e);

private:
    void startSync();
    void setupBackend(Backend *b, QMap<QString, QString> tracker);
    void checkRequests();
    QMap<QString, QTreeWidgetItem *> mProductMap;

    Ui::MonitorDialog *ui;
    QMovie *pSpinnerMovie;
    QMap<QString, QTreeWidgetItem *> mTreeMap;
    int mRequests;
};

#endif // MONITORDIALOG_H
