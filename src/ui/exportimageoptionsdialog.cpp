#include "exportimageoptionsdialog.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QRadioButton>
#include <QVBoxLayout>

#include "helphint.h"

ExportImageOptionsDialog::ExportImageOptionsDialog(QWidget *parent) : QDialog(parent) {
	setWindowTitle(tr("画像エクスポート設定"));

	auto *targetGroup = new QGroupBox(tr("対象"), this);
	m_frontRadio = new QRadioButton(tr("表面"), targetGroup);
	m_backRadio = new QRadioButton(tr("裏面"), targetGroup);
	m_bothRadio = new QRadioButton(tr("表裏を並べる"), targetGroup);
	m_frontRadio->setChecked(true);
	helphint::attach(m_bothRadio, tr("表面と裏面を横に並べた1枚の画像として書き出します。"));
	auto *targetLayout = new QVBoxLayout(targetGroup);
	targetLayout->addWidget(m_frontRadio);
	targetLayout->addWidget(m_backRadio);
	targetLayout->addWidget(m_bothRadio);

	m_scaleCombo = new QComboBox(this);
	for (int s : {1, 2, 3, 4, 6, 8}) {
		m_scaleCombo->addItem(tr("%1倍").arg(s), s);
	}
	helphint::attach(m_scaleCombo,
					 tr("書き出す画像の拡大率です。1単位=1画素が基準の等倍に対する倍率です。"));

	m_transparentCheck = new QCheckBox(tr("背景を透過する"), this);
	helphint::attach(m_transparentCheck,
					 tr("ON にすると基板の外側 (背景) を透過にします。OFF なら白で塗りつぶします。"));

	auto *form = new QFormLayout();
	form->addRow(tr("拡大率:"), m_scaleCombo);
	form->addRow(QString(), m_transparentCheck);

	auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
	connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
	connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

	auto *layout = new QVBoxLayout(this);
	layout->addWidget(targetGroup);
	layout->addLayout(form);
	layout->addWidget(buttons);
}

imageexport::Options ExportImageOptionsDialog::options() const {
	imageexport::Options opt;
	if (m_backRadio->isChecked()) {
		opt.target = imageexport::Target::Back;
	} else if (m_bothRadio->isChecked()) {
		opt.target = imageexport::Target::Both;
	} else {
		opt.target = imageexport::Target::Front;
	}
	opt.scale = m_scaleCombo->currentData().toInt();
	opt.transparentBackground = m_transparentCheck->isChecked();
	return opt;
}
