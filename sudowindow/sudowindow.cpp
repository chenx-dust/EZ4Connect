#include "sudowindow.h"

#include <QPushButton>
#include <QKeyEvent>

SudoWindow::SudoWindow(QWidget *parent)
	: QDialog(parent),
	ui(new Ui::SudoWindow)
{
	ui->setupUi(this);

	setWindowModality(Qt::WindowModal);
	setAttribute(Qt::WA_DeleteOnClose);

	connect(ui->buttonBox->button(QDialogButtonBox::Ok), &QPushButton::clicked,
		[&]()
		{
			emit sudo(ui->passwordLineEdit->text(), ui->savePasswordCheckBox->isChecked());
			emit accept();
		}
	);

	connect(ui->passwordVisibleCheckBox, &QCheckBox::checkStateChanged,
		[&](Qt::CheckState state)
		{
			ui->passwordLineEdit->setEchoMode(state == Qt::Checked ? QLineEdit::Normal : QLineEdit::Password);
		}
	);
}

SudoWindow::~SudoWindow()
{
	delete ui;
}

void SudoWindow::keyPressEvent(QKeyEvent *event)
{
	if (event->key() == Qt::Key_Enter || event->key() == Qt::Key_Return)
		ui->buttonBox->button(QDialogButtonBox::Ok)->click();
	else if (event->key() == Qt::Key_Escape)
		reject();
	else
		QDialog::keyPressEvent(event);
}
