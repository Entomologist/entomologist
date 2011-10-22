#include "MantisDetails.h"
#include "ui_MantisDetails.h"

#include <QCompleter>
MantisDetails::MantisDetails(const QString &bugId, QWidget *parent) :
    BackendDetails(parent),
    ui(new Ui::MantisDetails),
    mBugId(bugId)
{
    ui->setupUi(this);
    ui->reproCombo->installEventFilter(parent);
    ui->priorityCombo->installEventFilter(parent);
    ui->resolutionCombo->installEventFilter(parent);
    ui->severityCombo->installEventFilter(parent);
    ui->statusCombo->installEventFilter(parent);
}

MantisDetails::~MantisDetails()
{
    delete ui;
}

QMap<QString, QString>
MantisDetails::fieldDetails()
{
    QMap<QString, QString> newMap;
    if (mSeverity != ui->severityCombo->currentText())
        newMap["severity"] = ui->severityCombo->currentText();

    if (mPriority != ui->priorityCombo->currentText())
        newMap["priority"] = ui->priorityCombo->currentText();

    if (mReproducibility != ui->reproCombo->currentText())
        newMap["reproducibility"] = ui->reproCombo->currentText();

    if (mStatus != ui->statusCombo->currentText())
        newMap["status"] = ui->statusCombo->currentText();

    if (mResolution != ui->resolutionCombo->currentText())
        newMap["resolution"] = ui->resolutionCombo->currentText();

    if (mAssigned != ui->assignedToEdit->text())
        if (!ui->assignedToEdit->text().isEmpty())
            newMap["assigned_to"] = ui->assignedToEdit->text();

    return newMap;
}
void
MantisDetails::setProject(const QString &project)
{
    ui->projectText->setText(project);
}

void
MantisDetails::setVersion(const QString &version)
{
    ui->versionText->setText(version);
}

void
MantisDetails::setAssigneds(const QString &assigned, QStringList assignedVals)
{
    mAssigned = assigned;
    ui->assignedToEdit->setText(assigned);
    QCompleter *completer = new QCompleter(assignedVals, this);
    completer->setCaseSensitivity(Qt::CaseInsensitive);
    ui->assignedToEdit->setCompleter(completer);
}

void
MantisDetails::setCategory(const QString &category)
{
    ui->categoryText->setText(category);
}

void
MantisDetails::setSeverities(const QString &selected, QStringList severities)
{
    mSeverity = selected;
    ui->severityCombo->addItems(severities);
    ui->severityCombo->setCurrentIndex(ui->severityCombo->findText(selected));
}

void
MantisDetails::setPriorities(const QString &selected, QStringList priorities)
{
    mPriority = selected;
    ui->priorityCombo->addItems(priorities);
    ui->priorityCombo->setCurrentIndex(ui->priorityCombo->findText(selected));

}

void
MantisDetails::setStatuses(const QString &selected, QStringList statuses)
{
    mStatus = selected;
    ui->statusCombo->addItems(statuses);
    ui->statusCombo->setCurrentIndex(ui->statusCombo->findText(selected));

}

void
MantisDetails::setResolutions(const QString &selected, QStringList resolutions)
{
    mResolution = selected;
    ui->resolutionCombo->addItems(resolutions);
    ui->resolutionCombo->setCurrentIndex(ui->resolutionCombo->findText(selected));

}

void
MantisDetails::setReproducibility(const QString &selected, QStringList reproducibilities)
{
    mReproducibility = selected;
    ui->reproCombo->addItems(reproducibilities);
    ui->reproCombo->setCurrentIndex(ui->reproCombo->findText(selected));
}
