#ifndef BUGDETAILSDIALOG_H
#define BUGDETAILSDIALOG_H

#include <QDialog>
#include <QtSql>
#include "CommentFrame.h"

namespace Ui {
    class BugDetailsDialog;
}

class Backend;
class BackendDetails;
class QMovie;
class QLabel;
class QSplitterHandle;

class BugDetailsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit BugDetailsDialog(Backend *backend, QWidget *parent = 0);
    ~BugDetailsDialog();
    void setDetailsWidget(BackendDetails *widget);
    bool eventFilter(QObject *obj, QEvent *event);
    void setBugInfo(QMap<QString, QString> details);
    void setComments();
    QStringList getData();
    void loadComments();

public slots:
    void attachmentClicked(int rowId);
    void attachmentSaveAsClicked(int rowId);
    void attachmentDownloaded(const QString &filePath);
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
    void startSpinner();
    void stopSpinner();
    Ui::BugDetailsDialog *ui;
    QString mCurrentBugId, mTrackerId;
    Backend *pBackend;
    BackendDetails *pDetails;
    QMovie *pSpinnerMovie;
    bool openAttachment;
};




#endif // NEWCOMMENTSDIALOG_H
