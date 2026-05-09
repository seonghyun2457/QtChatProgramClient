#include "filesenderdialog.h"
#include "ui_filesenderdialog.h"

FileSenderDialog::FileSenderDialog(const QString &fileName, QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::FileSenderDialog())
{
    ui->setupUi(this);

    QString aMessage = QString("Do you want to send '%1' ?").arg(fileName);
    ui->lbMessage->setText(aMessage);
}

FileSenderDialog::~FileSenderDialog()
{
    delete ui;
}

void FileSenderDialog::on_btnOK_clicked()
{
    accept();
}


void FileSenderDialog::on_btnCancel_clicked()
{
    reject();
}

