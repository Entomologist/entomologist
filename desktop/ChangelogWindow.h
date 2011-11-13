#ifndef CHANGELOGWINDOW_H
#define CHANGELOGWINDOW_H

#include <QDialog>
#include <QStyledItemDelegate>
namespace Ui {
    class ChangelogWindow;
}

class ChangelogWindow : public QDialog {
    Q_OBJECT
public:
    ChangelogWindow(QWidget *parent = 0);
    ~ChangelogWindow();

    void loadChangelog();

public slots:
    void revertButtonClicked();
    void selectionChanged();

protected:
    void changeEvent(QEvent *e);

private:
    Ui::ChangelogWindow *ui;
};

#endif // CHANGELOGWINDOW_H
