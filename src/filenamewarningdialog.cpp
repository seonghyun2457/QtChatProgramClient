#include "filenamewarningdialog.h"
#include "ui_filenamewarningdialog.h"

#include "packetHeader.h"

FilenameWarningDialog::FilenameWarningDialog(QWidget *parent)
    : QDialog(parent)
    , m_ui(std::make_unique<Ui::FilenameWarningDialog>())
{
    m_ui->setupUi(this);

    QString aMessage = QString("Filename length should be less than %1 characters.").arg(FILENAME_LENGTH);
    m_ui->lbMessage->setText(aMessage);
}

FilenameWarningDialog::~FilenameWarningDialog()
{
    m_ui = nullptr;
}

void FilenameWarningDialog::on_btnOK_clicked()
{
    accept();
}

