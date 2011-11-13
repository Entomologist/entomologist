#include "NewBugDialog.hpp"
#include "ui_NewBugDialog.h"

NewBugDialog::NewBugDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::NewBugDialog)
{
    ui->setupUi(this);
}

NewBugDialog::~NewBugDialog()
{
    delete ui;
}
