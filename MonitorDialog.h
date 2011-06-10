#ifndef MONITORDIALOG_H
#define MONITORDIALOG_H

#include <QDialog>
#include <QMap>
class QMovie;
class QListWidgetItem;

namespace Ui {
    class MonitorDialog;
}

class MonitorDialog : public QDialog {
    Q_OBJECT
public:
    MonitorDialog(QWidget *parent = 0);
    ~MonitorDialog();

public slots:
    void trackerActivated(QListWidgetItem  *item);

protected:
    void changeEvent(QEvent *e);

private:
    Ui::MonitorDialog *ui;
    QMovie *pSpinnerMovie;
    QMap<QString, QMap<QString, QString> > mTrackerMap;
};

#endif // MONITORDIALOG_H
