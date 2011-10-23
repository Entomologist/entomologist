#include <QDateTime>
#include <QDebug>
#include "AttachmentWidget.h"
#include "ui_AttachmentWidget.h"

AttachmentWidget::AttachmentWidget(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::AttachmentWidget)
{
    ui->setupUi(this);
    connect(ui->attachmentLink, SIGNAL(clicked(int)),
            this, SIGNAL(clicked(int)));
    connect(ui->attachmentLink, SIGNAL(saveAsClicked(int)),
            this, SIGNAL(saveAsClicked(int)));
}

AttachmentWidget::~AttachmentWidget()
{
    delete ui;
}

void
AttachmentWidget::setRowId(int id)
{
    ui->attachmentLink->setId(id);
}

void
AttachmentWidget::setFilename(const QString &filename)
{
    ui->attachmentLink->setText(filename);
}


void
AttachmentWidget::setSummary(const QString &summary)
{
    QString newSummary;
    if (summary.isEmpty())
        newSummary = "- <i>No summary available</i>";
    else
        newSummary = QString("- <i>%1</i>").arg(summary);
    ui->summaryLabel->setText(newSummary);
}

void
AttachmentWidget::setDetails(const QString &creator, const QString &date)
{
    QDateTime uploadedOn = QDateTime::fromString(date, Qt::ISODate);
    qDebug() << uploadedOn;
    QString uploadedDate = uploadedOn.date().toString(Qt::DefaultLocaleLongDate);
    QString uploadedTime = uploadedOn.time().toString(Qt::ISODate);
    QString details = QString("Attached by <b>%1</b> on %2 at %3").arg(creator).arg(uploadedDate).arg(uploadedTime);
    ui->uploadedLabel->setText(details);
}
