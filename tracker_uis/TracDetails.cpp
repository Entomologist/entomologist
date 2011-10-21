#include <QComboBox>
#include "TracDetails.h"
#include "ui_TracDetails.h"

TracDetails::TracDetails(const QString &bugId, QWidget *parent) :
    BackendDetails(parent),
    ui(new Ui::TracDetails),
    mBugId(bugId)
{
    ui->setupUi(this);
    ui->componentCombo->installEventFilter(parent);
    ui->priorityCombo->installEventFilter(parent);
    ui->resolutionCombo->installEventFilter(parent);
    ui->severityCombo->installEventFilter(parent);
    ui->statusCombo->installEventFilter(parent);
    ui->versionCombo->installEventFilter(parent);

    connect(ui->statusCombo, SIGNAL(currentIndexChanged(QString)),
            this, SLOT(statusIndexChanged(QString)));
    ui->resolutionCombo->setEnabled(false);
}

TracDetails::~TracDetails()
{
    delete ui;
}

void
TracDetails::statusIndexChanged(const QString &value)
{
    if (value.toLower() == "closed")
    {
        ui->resolutionCombo->setEnabled(true);
    }
    else
    {
        ui->resolutionCombo->setEnabled(false);
        ui->resolutionCombo->setCurrentIndex(ui->resolutionCombo->findText(mResolution));
    }
}

QMap<QString, QString>
TracDetails::fieldDetails()
{
    QMap<QString, QString> newMap;
    if (mSeverity != ui->severityCombo->currentText())
        newMap["severity"] = ui->severityCombo->currentText();

    if (mPriority != ui->priorityCombo->currentText())
        newMap["priority"] = ui->priorityCombo->currentText();

    if (mComponent != ui->componentCombo->currentText())
        newMap["component"] = ui->componentCombo->currentText();

    if (mVersion != ui->versionCombo->currentText())
        newMap["version"] = ui->versionCombo->currentText();

    if (mStatus != ui->statusCombo->currentText())
        newMap["status"] = ui->statusCombo->currentText();

    if (mResolution != ui->resolutionCombo->currentText())
        newMap["resolution"] = ui->resolutionCombo->currentText();
    return newMap;
}


void
TracDetails::setSeverities(const QString &selected, QStringList severities)
{
    mSeverity = selected;
    ui->severityCombo->addItems(severities);
    ui->severityCombo->setCurrentIndex(ui->severityCombo->findText(selected));
}

void
TracDetails::setPriorities(const QString &selected, QStringList priorities)
{
    mPriority = selected;
    ui->priorityCombo->addItems(priorities);
    ui->priorityCombo->setCurrentIndex(ui->priorityCombo->findText(selected));
}

void
TracDetails::setComponents(const QString &selected, QStringList components)
{
    mComponent = selected;
    ui->componentCombo->addItems(components);
    ui->componentCombo->setCurrentIndex(ui->componentCombo->findText(selected));
}

void
TracDetails::setVersions(const QString &selected, QStringList versions)
{
    mVersion = selected;
    ui->versionCombo->addItems(versions);
    ui->versionCombo->setCurrentIndex(ui->versionCombo->findText(selected));
}

void
TracDetails::setStatuses(const QString &selected, QStringList statuses)
{
    mStatus = selected;
    ui->statusCombo->addItems(statuses);
    ui->statusCombo->setCurrentIndex(ui->statusCombo->findText(selected));
}

void
TracDetails::setResolutions(const QString &selected, QStringList resolutions)
{
    mResolution = selected;
    ui->resolutionCombo->addItems(resolutions);
    ui->resolutionCombo->setCurrentIndex(ui->resolutionCombo->findText(selected));
}
