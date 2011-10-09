#ifndef NEWCOMMENTSDIALOG_H
#define NEWCOMMENTSDIALOG_H

#include <QDialog>
#include <QtSql>
#include "CommentFrame.h"

namespace Ui {
    class NewCommentsDialog;
}

class Backend;
class BackendDetails;
class QMovie;
class QLabel;
class QSplitterHandle;

class NewCommentsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit NewCommentsDialog(Backend *backend, QWidget *parent = 0);
    ~NewCommentsDialog();
    void setDetailsWidget(BackendDetails *widget);
    bool eventFilter(QObject *obj, QEvent *event);
    void setBugInfo(QMap<QString, QString> details);
    void setComments();
    QStringList getData();
    void loadComments();

public slots:
    void commentsCached();
    void textClicked(const QString &text);
    void save();
    void cancel();

signals:
    void commentsDialogClosing(QMap<QString, QString> details, QString newComment);
    void commentsDialogCanceled(const QString &trackerId, const QString & bugId);
protected:
    void closeEvent(QCloseEvent *event);
    void keyPressEvent(QKeyEvent *event);
private:
    void styleSplitter(QSplitterHandle *handle);
    void frameToggle(QWidget *frame, QLabel *arrow);
    Ui::NewCommentsDialog *ui;
    QString mCurrentBugId, mTrackerId;
    Backend *pBackend;
    BackendDetails *pDetails;
    QMovie *pSpinnerMovie;

};




#endif // NEWCOMMENTSDIALOG_H
