#ifndef FILESENDERDIALOG_H
#define FILESENDERDIALOG_H

#include <QDialog>

namespace Ui {
class FileSenderDialog;
}

class FileSenderDialog : public QDialog
{
    Q_OBJECT

public:
    explicit FileSenderDialog(const QString &fileName, QWidget *parent = nullptr);
    virtual ~FileSenderDialog();

private slots:
    void on_btnOK_clicked();
    void on_btnCancel_clicked();

private:
    std::unique_ptr<Ui::FileSenderDialog> m_ui;
};

#endif // FILESENDERDIALOG_H
