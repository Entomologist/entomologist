#include <QNetworkAccessManager>
#include <QDesktopServices>
#include <QFileDialog>
#include <QMovie>
#include <QSettings>
#include <QMessageBox>

#include "trackers/Backend.h"
#include "tracker_uis/BackendDetails.h"
#include "Utilities.hpp"
#include "SqlUtilities.h"
#include "AttachmentWidget.h"
#include "BugDetailsDialog.h"
#include "ui_BugDetailsDialog.h"

BugDetailsDialog::BugDetailsDialog(Backend *backend, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::BugDetailsDialog)
{
    setAttribute(Qt::WA_DeleteOnClose);
    pBackend = backend;
    connect(pBackend, SIGNAL(commentsCached()),
            this, SLOT(commentsCached()));

    ui->setupUi(this);
    ui->cancelButton->setIcon(style()->standardIcon(QStyle::SP_DialogCancelButton));
    ui->saveButton->setIcon(style()->standardIcon(QStyle::SP_DialogSaveButton));

    QSettings settings("Entomologist");
    restoreGeometry(settings.value("comment-window-geometry").toByteArray());

    pSpinnerMovie = new QMovie(this);
    pSpinnerMovie->setFileName(":/spinner");
    pSpinnerMovie->setScaledSize(QSize(48,48));

    ui->spinnerLabel->setMovie(pSpinnerMovie);
    ui->loadingFrame->hide();
    ui->summaryFrame->setEnabled(false);
    ui->descriptionFrame->setEnabled(false);
    ui->commentsFrame->setEnabled(false);
    ui->newCommentFrame->setEnabled(false);
    ui->attachmentsFrame->setEnabled(false);
    ui->detailsFrame->setEnabled(false);
    ui->detailsFrame->setStyleSheet("QFrame#detailsFrame {border: 1px solid black; border-radius: 4px; }");
    ui->descriptionFrame->setStyleSheet("QFrame#descriptionFrame {border: 1px solid black; background: #ffffff; border-radius: 4px; }");

    connect(pBackend, SIGNAL(attachmentDownloaded(QString)),
            this, SLOT(attachmentDownloaded(QString)));
    connect(ui->cancelButton,SIGNAL(clicked()),SLOT(cancel()));
    connect(ui->saveButton,SIGNAL(clicked()),SLOT(save()));
    connect(ui->detailsLabel, SIGNAL(clicked(QString)),
            this, SLOT(textClicked(QString)));
    connect(ui->summaryLabel, SIGNAL(clicked(QString)),
            this, SLOT(textClicked(QString)));
    connect(ui->descriptionLabel, SIGNAL(clicked(QString)),
            this, SLOT(textClicked(QString)));
    connect(ui->commentsLabel, SIGNAL(clicked(QString)),
            this, SLOT(textClicked(QString)));
    connect(ui->attachmentsLabel, SIGNAL(clicked(QString)),
            this, SLOT(textClicked(QString)));
}

BugDetailsDialog::~BugDetailsDialog()
{
    delete ui;
}

void
BugDetailsDialog::styleSplitter(QSplitterHandle *handle)
{
    //QVBoxLayout *layout = new QVBoxLayout(handle);
    //layout->setSpacing(0);
    //layout->setMargin(0);
    //QFrame *line = new QFrame(handle);
    //line->setFrameShape(QFrame::HLine);
    //line->setFrameShadow(QFrame::Sunken);
    //layout->addWidget(line);
}

void
BugDetailsDialog::frameToggle(QWidget *frame, QLabel *arrow)
{
    if (frame->isVisible())
    {
        frame->hide();
        arrow->setPixmap(QPixmap(":/small_arrow_right"));
    }
    else
    {
        frame->show();
        arrow->setPixmap(QPixmap(":/small_arrow_down"));
    }
}

void
BugDetailsDialog::textClicked(const QString &text)
{
    if (text == tr("Details:"))
    {
        frameToggle(ui->detailsFrame, ui->detailsGraphic);
    }
    else if (text == tr("Summary:"))
    {
        frameToggle(ui->summaryFrame, ui->summaryGraphic);
    }
    else if (text == tr("Description:"))
    {
        frameToggle(ui->descriptionFrame, ui->descriptionGraphic);
    }
    else if (text == tr("Comments:"))
    {
        frameToggle(ui->commentsFrame, ui->commentsGraphic);
    }
    else if (text == tr("Attachments:"))
    {
        frameToggle(ui->attachmentsFrame, ui->attachmentsGraphic);
    }
}

void
BugDetailsDialog::commentsCached()
{
    stopSpinner();
    setComments();
}

void
BugDetailsDialog::startSpinner()
{
    pSpinnerMovie->start();
    ui->loadingFrame->show();
    ui->summaryFrame->setEnabled(false);
    ui->descriptionFrame->setEnabled(false);
    ui->commentsFrame->setEnabled(false);
    ui->newCommentFrame->setEnabled(false);
    ui->detailsFrame->setEnabled(false);
}

void
BugDetailsDialog::stopSpinner()
{
    pSpinnerMovie->stop();
    ui->loadingFrame->hide();
    ui->detailsFrame->setEnabled(true);
    ui->summaryFrame->setEnabled(true);
    ui->descriptionFrame->setEnabled(true);
    ui->commentsFrame->setEnabled(true);
    ui->newCommentFrame->setEnabled(true);
    ui->attachmentsFrame->setEnabled(true);
}

void
BugDetailsDialog::setDetailsWidget(BackendDetails *widget)
{
    ui->detailsLayout->addWidget(widget);
    widget->setParent(ui->detailsFrame);
    pDetails = widget;
    ui->verticalLayout->activate();
}

// We don't want users to accidentally change the combobox values if
// they use the scrollwheel to scroll up and down the details view
bool
BugDetailsDialog::eventFilter(QObject *obj, QEvent *event)
{
    Q_UNUSED(obj);
    if (event->type() == QEvent::Wheel)
    {
        return true;
    }

    return false;
}

void
BugDetailsDialog::setBugInfo(QMap<QString, QString> details)
{
    mCurrentBugId = details["bug_id"];
    mTrackerId = details["tracker_id"];
    QString windowTitle = QString("Bug #%1 - %2")
                          .arg(mCurrentBugId)
                          .arg(details["tracker_name"]);
    setWindowTitle(windowTitle);
    ui->summaryTextBox->setText(details["summary"]);
    ui->descriptionText->setText(details["description"]);
}

void
BugDetailsDialog::loadComments()
{
    QNetworkAccessManager m(this);
    if (Utilities::isOnline(&m))
    {
        // Fetch comments
        startSpinner();
        pBackend->getComments(mCurrentBugId);
    }
    else
    {
        setComments();
    }
}

void
BugDetailsDialog::setComments()
{
    int i = 0;
    int total = 0;
    if (ui->descriptionText->text().isEmpty())
        ui->descriptionText->setText(SqlUtilities::getBugDescription(pBackend->type(), mCurrentBugId));
    QList < QMap<QString, QString> > mainComments = SqlUtilities::loadComments(mTrackerId, mCurrentBugId, false);
    QList < QMap<QString, QString> > shadowComments = SqlUtilities::loadComments(mTrackerId, mCurrentBugId, true);
    QList < QMap<QString, QString> > attachmentList = SqlUtilities::loadAttachments(mTrackerId, mCurrentBugId);
    if (attachmentList.size() == 0)
    {
        ui->topAttachmentsFrame->hide();
    }
    else
    {
        for (i = 0; i < attachmentList.size(); ++i)
        {
            QMap<QString, QString> attachment = attachmentList.at(i);
            AttachmentWidget *widget = new AttachmentWidget(ui->attachmentsFrame);
            connect(widget, SIGNAL(clicked(int)),
                    this, SLOT(attachmentClicked(int)));
            connect(widget, SIGNAL(saveAsClicked(int)),
                    this, SLOT(attachmentSaveAsClicked(int)));
            widget->setRowId(attachment["id"].toInt());
            widget->setFilename(attachment["filename"]);
            widget->setSummary(attachment["summary"]);
            widget->setDetails(attachment["creator"], attachment["last_modified"]);
            ui->attachmentsLayout->addWidget(widget);
        }

        ui->attachmentsFrame->layout();
    }

    total = mainComments.size() + shadowComments.size();
    if (total > 0)
        ui->noCommentsLabel->hide();

    QSpacerItem* commentSpacer = new QSpacerItem(20, 40, QSizePolicy::Minimum, QSizePolicy::Expanding);
    for(i = 0; i < mainComments.size(); ++i)
    {
        QMap<QString, QString> comment = mainComments.at(i);
        CommentFrame *frame = new CommentFrame(ui->commentsFrame);
         ui->commentsLayout->addWidget(frame);
         frame->setName(comment["author"]);
         frame->setComment(comment["comment"]);
         frame->setDate(comment["timestamp"]);
         if (comment["private"] == "1")
            frame->setPrivate();
         frame->setBugId(mCurrentBugId);
    }

    for(i = 0; i < shadowComments.size(); ++i)
    {
        QMap<QString, QString> comment = shadowComments.at(i);
        CommentFrame *frame = new CommentFrame(ui->commentsFrame);
        ui->commentsLayout->addWidget(frame);
        frame->setName(comment["author"]);
        frame->setComment(comment["comment"]);
        frame->setDate(comment["timestamp"]);
        if (comment["private"] == "1")
           frame->setPrivate();

        frame->setBugId(mCurrentBugId);
        frame->setRedHeader();
    }

//    ui->commentsLayout->addSpacerItem(commentSpacer);
    ui->commentsFrame->layout();
}

QStringList
BugDetailsDialog::getData()
{
    QStringList list;
    list.append(mCurrentBugId);
    list.append(ui->commentEdit->document()->toPlainText());

    return list;
}

void
BugDetailsDialog::save()
{
    QMap<QString, QString> details = pDetails->fieldDetails();
    QMapIterator<QString, QString> i(details);
    while (i.hasNext())
    {
        i.next();
        details[i.key()] = QString("<font color=\"red\">%1</font>").arg(i.value());
    }

    details["tracker_id"] = mTrackerId;
    details["bug_id"] = mCurrentBugId;
    details["private_comment"] = (ui->newCommentPrivateCheckbox->isChecked()) ? "1" : "0";
    QString newComment = ui->commentEdit->document()->toPlainText();
    emit commentsDialogClosing(details, newComment);
    close();
}

void
BugDetailsDialog::cancel()
{
    emit commentsDialogCanceled(mTrackerId, mCurrentBugId);
    close();
}

void
BugDetailsDialog::keyPressEvent(QKeyEvent *event)
{
    if(event->key() == Qt::Key_Escape)
        this->close();
}

void
BugDetailsDialog::attachmentClicked(int rowId)
{
    QMap<QString, QString> attachment = SqlUtilities::attachmentDetails(rowId);
    openAttachment = true;
    QString path = QString("%1%2entomologist-attachment-%3")
            .arg(QDir::tempPath())
            .arg(QDir::separator())
            .arg(attachment["filename"]);

    ui->loadingCommentsLabel->setText("Downloading attachment...");
    startSpinner();
    pBackend->downloadAttachment(rowId, path);
}

void
BugDetailsDialog::attachmentSaveAsClicked(int rowId)
{
    QMap<QString, QString> attachment = SqlUtilities::attachmentDetails(rowId);
    openAttachment = false;
    QString path = QString("%1%2%3")
            .arg(QDesktopServices::storageLocation(QDesktopServices::DocumentsLocation))
            .arg(QDir::separator())
            .arg(attachment["filename"]);

    QString fileName = QFileDialog::getSaveFileName(0, tr("Save File As"),
                               path);
    if (!fileName.isEmpty())
    {
        ui->loadingCommentsLabel->setText("Downloading attachment...");
        startSpinner();
        pBackend->downloadAttachment(rowId, fileName);
    }
}

void
BugDetailsDialog::attachmentDownloaded(const QString &filePath)
{
    stopSpinner();
    if (filePath.isEmpty())
    {
        QMessageBox box(QMessageBox::Critical, "Error", "Could not download attachment", QMessageBox::Ok);
        box.exec();
        return;
    }

    if (openAttachment)
    {
        QString file = QString("file://%1").arg(filePath);
        QDesktopServices::openUrl(file);
    }
}

void
BugDetailsDialog::closeEvent(QCloseEvent *event)
{
    QSettings settings("Entomologist");
//    settings.setValue("comment-window-splitter", ui->splitter->saveState());
    settings.setValue("comment-window-geometry", saveGeometry());
    QDialog::closeEvent(event);
}
