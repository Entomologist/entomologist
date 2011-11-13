#ifndef NEWBUGDIALOG_HPP
#define NEWBUGDIALOG_HPP

#include <QDialog>

namespace Ui {
    class NewBugDialog;
}

class NewBugDialog : public QDialog
{
    Q_OBJECT

public:
    explicit NewBugDialog(QWidget *parent = 0);
    ~NewBugDialog();

private:
    Ui::NewBugDialog *ui;
};

#endif // NEWBUGDIALOG_HPP
