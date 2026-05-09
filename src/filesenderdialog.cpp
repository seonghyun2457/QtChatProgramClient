#include "filesenderdialog.h"
#include "ui_filesenderdialog.h"

FileSenderDialog::FileSenderDialog(const QString &fileName, QWidget *parent)
    : QDialog(parent)
    , m_ui(std::make_unique<Ui::FileSenderDialog>())
{
    m_ui->setupUi(this);

    QString aMessage = QString("Do you want to send '%1' ?").arg(fileName);
    m_ui->lbMessage->setText(aMessage);
}

FileSenderDialog::~FileSenderDialog()
{
    m_ui = nullptr;
}

void FileSenderDialog::on_btnOK_clicked()
{
    accept();
}


void FileSenderDialog::on_btnCancel_clicked()
{
    reject();
}

