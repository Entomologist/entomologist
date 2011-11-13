#ifndef UPDATESAVAILABLEDIALOG_H
#define UPDATESAVAILABLEDIALOG_H

#include <QDialog>

namespace Ui {
    class UpdatesAvailableDialog;
}

class UpdatesAvailableDialog : public QDialog {
    Q_OBJECT
public:
    UpdatesAvailableDialog(const QString &version,
                           const QString &notes,
                           QWidget *parent = 0);
    ~UpdatesAvailableDialog();

public slots:
    void okClicked();

protected:
    void changeEvent(QEvent *e);

private:
    Ui::UpdatesAvailableDialog *ui;
};

#endif // UPDATESAVAILABLEDIALOG_H
