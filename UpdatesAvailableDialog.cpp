#include <QSettings>

#include "UpdatesAvailableDialog.h"
#include "ui_UpdatesAvailableDialog.h"

UpdatesAvailableDialog::UpdatesAvailableDialog(const QString &version,
                                               const QString &notes,
                                               QWidget *parent) :
    QDialog(parent),
    ui(new Ui::UpdatesAvailableDialog)
{
    ui->setupUi(this);
    ui->versionLabel->setText(version);
    ui->changelogTextEdit->setHtml(notes);
    ui->okButton->setIcon(style()->standardIcon(QStyle::SP_DialogOkButton));
    connect(ui->okButton, SIGNAL(clicked()),
            this, SLOT(okClicked()));
}

UpdatesAvailableDialog::~UpdatesAvailableDialog()
{
    delete ui;
}

void
UpdatesAvailableDialog::okClicked()
{
    if (ui->updatesCheckBox->isChecked())
    {
        QSettings settings("Entomologist");
        settings.setValue("update-check", false);
    }

    close();
}

void
UpdatesAvailableDialog::changeEvent(QEvent *e)
{
    QDialog::changeEvent(e);
    switch (e->type()) {
    case QEvent::LanguageChange:
        ui->retranslateUi(this);
        break;
    default:
        break;
    }
}
