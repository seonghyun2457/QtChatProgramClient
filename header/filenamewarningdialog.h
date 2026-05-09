#ifndef FILENAMEWARNINGDIALOG_H
#define FILENAMEWARNINGDIALOG_H

#include <QDialog>

namespace Ui {
class FilenameWarningDialog;
}

class FilenameWarningDialog : public QDialog
{
    Q_OBJECT

public:
    explicit FilenameWarningDialog(QWidget *parent = nullptr);
    virtual ~FilenameWarningDialog();

private slots:
    void on_btnOK_clicked();

private:
    std::unique_ptr<Ui::FilenameWarningDialog> m_ui;
};

#endif // FILENAMEWARNINGDIALOG_H
