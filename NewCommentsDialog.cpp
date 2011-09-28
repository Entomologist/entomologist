#include <QNetworkAccessManager>
#include <QMovie>
#include <QSettings>
#include "trackers/Backend.h"
#include "tracker_uis/BackendDetails.h"
#include "Utilities.hpp"
#include "SqlUtilities.h"
#include "NewCommentsDialog.h"
#include "ui_NewCommentsDialog.h"

NewCommentsDialog::NewCommentsDialog(Backend *backend, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::NewCommentsDialog)
{
    pBackend = backend;
    connect(pBackend, SIGNAL(commentsCached()),
            this, SLOT(commentsCached()));
    ui->setupUi(this);
    ui->cancelButton->setIcon(style()->standardIcon(QStyle::SP_DialogCancelButton));
    ui->saveButton->setIcon(style()->standardIcon(QStyle::SP_DialogSaveButton));
    QSettings settings("Entomologist");
    QByteArray restoredSplitterState = settings.value("comment-window-splitter").toByteArray();

    pSpinnerMovie = new QMovie(this);
    pSpinnerMovie->setFileName(":/spinner");
    pSpinnerMovie->setScaledSize(QSize(48,48));
    ui->splitter->setChildrenCollapsible(false);

    if (!ui->splitter->restoreState(restoredSplitterState))
    {
        QList<int> splitterSizes;
        splitterSizes << 60 << 40 << 0;
        ui->splitter->setSizes(splitterSizes);
    }

    ui->spinnerLabel->setMovie(pSpinnerMovie);
    ui->loadingFrame->hide();
    ui->summaryFrame->setEnabled(false);
    ui->descriptionFrame->setEnabled(false);
    ui->commentsFrame->setEnabled(false);
    ui->newCommentFrame->setEnabled(false);
    ui->detailsFrame->setEnabled(false);

    ui->splitter->setCollapsible(0, false);

    // The splitter by default is difficult to see (on Linux, anyway),
    // so let's make it a little more obvious
    styleSplitter(ui->splitter->handle(1));
    styleSplitter(ui->splitter->handle(2));
//    setStyleSheet("QDialog { background-color: white; }");
    ui->detailsFrame->setStyleSheet("QFrame#detailsFrame {border: 1px solid black; border-radius: 4px; }");
    connect(ui->cancelButton,SIGNAL(clicked()),SLOT(close()));
    connect(ui->saveButton,SIGNAL(clicked()),SLOT(save()));
    connect(ui->detailsLabel, SIGNAL(clicked(QString)),
            this, SLOT(textClicked(QString)));
    connect(ui->summaryLabel, SIGNAL(clicked(QString)),
            this, SLOT(textClicked(QString)));
}

NewCommentsDialog::~NewCommentsDialog()
{
    delete ui;
}

void
NewCommentsDialog::styleSplitter(QSplitterHandle *handle)
{
    QVBoxLayout *layout = new QVBoxLayout(handle);
    layout->setSpacing(0);
    layout->setMargin(0);
    QFrame *line = new QFrame(handle);
    line->setFrameShape(QFrame::HLine);
    line->setFrameShadow(QFrame::Sunken);
    layout->addWidget(line);
}

void
NewCommentsDialog::frameToggle(QWidget *frame, QLabel *arrow)
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
NewCommentsDialog::textClicked(const QString &text)
{
    QList<int> sizes = ui->splitter->sizes();
    QList<int> newSizes;
    qDebug() << "Text clicked: " << text;
    if (text == tr("Details:"))
    {
        frameToggle(ui->detailsFrame, ui->detailsGraphic);
    }
    else if (text == tr("Summary:"))
    {
        frameToggle(ui->summaryFrame, ui->summaryGraphic);
    }
}

void
NewCommentsDialog::commentsCached()
{
    pSpinnerMovie->stop();
    ui->loadingFrame->hide();
    ui->detailsFrame->setEnabled(true);
    ui->summaryFrame->setEnabled(true);
    ui->descriptionFrame->setEnabled(true);
    ui->commentsFrame->setEnabled(true);
    ui->newCommentFrame->setEnabled(true);
    setComments();
}

void
NewCommentsDialog::setDetailsWidget(BackendDetails *widget)
{
    ui->detailsLayout->addWidget(widget);
    widget->setParent(ui->detailsFrame);
    pDetails = widget;
    ui->verticalLayout->activate();
}

// We don't want users to accidentally change the combobox values if
// they use the scrollwheel to scroll up and down the details view
bool
NewCommentsDialog::eventFilter(QObject *obj, QEvent *event)
{
    Q_UNUSED(obj);
    if (event->type() == QEvent::Wheel)
    {
        return true;
    }

    return false;
}

void
NewCommentsDialog::setBugInfo(QMap<QString, QString> details)
{
    mCurrentBugId = details["bug_id"];
    mTrackerId = details["tracker_id"];
    QString windowTitle = QString("Bug #%1 - %2")
                          .arg(mCurrentBugId)
                          .arg(details["tracker_name"]);
    setWindowTitle(windowTitle);
    ui->summaryTextBox->setText(details["summary"]);
    ui->descriptionTextBox->setText(details["description"]);
}

void
NewCommentsDialog::loadComments()
{
    QNetworkAccessManager m(this);
    if (Utilities::isOnline(&m))
    {
        // Fetch comments
        pSpinnerMovie->start();
        ui->loadingFrame->show();
        ui->summaryFrame->setEnabled(false);
        ui->descriptionFrame->setEnabled(false);
        ui->commentsFrame->setEnabled(false);
        ui->newCommentFrame->setEnabled(false);
        ui->detailsFrame->setEnabled(false);

        qDebug() << "Loading comments for " << mCurrentBugId;
        pBackend->getComments(mCurrentBugId);
    }
    else
    {
        setComments();
    }
}

void
NewCommentsDialog::setComments()
{
    int i = 0;
    int total = 0;
    if (ui->descriptionTextBox->toPlainText().isEmpty())
        ui->descriptionTextBox->setText(SqlUtilities::getBugDescription(pBackend->type(), mCurrentBugId));
    qDebug() << mTrackerId << mCurrentBugId;
    QList < QMap<QString, QString> > mainComments = SqlUtilities::loadComments(mTrackerId, mCurrentBugId, false);
    QList < QMap<QString, QString> > shadowComments = SqlUtilities::loadComments(mTrackerId, mCurrentBugId, true);
    total = mainComments.size() + shadowComments.size();
    if (total > 0)
        ui->noCommentsLabel->hide();

    QSpacerItem* commentSpacer = new QSpacerItem(20, 40, QSizePolicy::Minimum, QSizePolicy::Expanding);
    for(i = 0; i < mainComments.size(); ++i)
    {
        QMap<QString, QString> comment = mainComments.at(i);
        CommentFrame *frame = new CommentFrame(ui->scrollArea);
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
        CommentFrame *frame = new CommentFrame(ui->scrollArea);
        ui->commentsLayout->addWidget(frame);
        frame->setName(comment["author"]);
        frame->setComment(comment["comment"]);
        frame->setDate(comment["timestamp"]);
        if (comment["private"] == "1")
           frame->setPrivate();

        frame->setBugId(mCurrentBugId);
        frame->setRedHeader();
    }

    ui->commentsLayout->addSpacerItem(commentSpacer);
}

QStringList
NewCommentsDialog::getData()
{
    QStringList list;
    list.append(mCurrentBugId);
    list.append(ui->commentEdit->document()->toPlainText());

    return list;
}

void
NewCommentsDialog::save()
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
NewCommentsDialog::closeEvent(QCloseEvent *event)
{
    qDebug() << "Closing";
    QSettings settings("Entomologist");
    settings.setValue("comment-window-splitter", ui->splitter->saveState());
    QDialog::closeEvent(event);
}
