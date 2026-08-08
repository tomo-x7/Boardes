#include "boardeditordialog.h"

#include <QButtonGroup>
#include <QCheckBox>
#include <QColorDialog>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QImage>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPixmap>
#include <QPushButton>
#include <QRadioButton>
#include <QSpinBox>
#include <QVBoxLayout>

#include "../core/units.h"
#include "helphint.h"

namespace {
constexpr int kThumbW = 96;
constexpr int kThumbH = 60;
}  // namespace

BoardEditorDialog::BoardEditorDialog(QWidget *parent) : QDialog(parent) {
	m_imageModeRadio = new QRadioButton(tr("画像から作る"), this);
	m_paramModeRadio = new QRadioButton(tr("パラメトリック (画像なし)"), this);
	m_paramModeRadio->setChecked(true);
	m_modeGroup = new QButtonGroup(this);
	m_modeGroup->addButton(m_imageModeRadio);
	m_modeGroup->addButton(m_paramModeRadio);
	connect(m_modeGroup, &QButtonGroup::buttonToggled, this, &BoardEditorDialog::onCreationModeToggled);

	m_idEdit = new QLineEdit(this);
	m_idEdit->setPlaceholderText(tr("例: my-board-1"));
	m_nameEdit = new QLineEdit(this);
	m_nameEdit->setPlaceholderText(tr("例: 自作基板 70x50"));

	m_colsSpin = new QSpinBox(this);
	m_colsSpin->setRange(1, 500);
	m_colsSpin->setValue(10);
	m_rowsSpin = new QSpinBox(this);
	m_rowsSpin->setRange(1, 500);
	m_rowsSpin->setValue(10);
	m_pitchSpin = new QSpinBox(this);
	m_pitchSpin->setRange(1, 100);
	m_pitchSpin->setValue(units::Pitch);
	m_pitchSpin->setSuffix(tr(" 単位 (0.254mm)"));

	m_originXSpin = new QSpinBox(this);
	m_originXSpin->setRange(0, 1000);
	m_originXSpin->setValue(units::Pitch / 2);
	m_originYSpin = new QSpinBox(this);
	m_originYSpin->setRange(0, 1000);
	m_originYSpin->setValue(units::Pitch / 2);

	m_padShapeCombo = new QComboBox(this);
	m_padShapeCombo->addItem(tr("なし (背景画像等で表現)"), static_cast<int>(PadShape::None));
	m_padShapeCombo->addItem(tr("丸"), static_cast<int>(PadShape::Round));
	m_padShapeCombo->addItem(tr("四角"), static_cast<int>(PadShape::Square));
	m_padShapeCombo->setCurrentIndex(1);  // Round

	m_padDiameterSpin = new QSpinBox(this);
	m_padDiameterSpin->setRange(0, 100);
	m_padDiameterSpin->setValue(6);
	m_holeDiameterSpin = new QSpinBox(this);
	m_holeDiameterSpin->setRange(0, 100);
	m_holeDiameterSpin->setValue(3);

	m_copperCombo = new QComboBox(this);
	m_copperCombo->addItem(tr("なし"), static_cast<int>(CopperPattern::None));
	m_copperCombo->addItem(tr("穴ごとのランド (蛇の目)"), static_cast<int>(CopperPattern::PadPerHole));
	m_copperCombo->addItem(tr("横ストライプ (ベロボード)"), static_cast<int>(CopperPattern::StripHorizontal));
	m_copperCombo->addItem(tr("縦ストライプ"), static_cast<int>(CopperPattern::StripVertical));
	m_copperCombo->setCurrentIndex(1);  // PadPerHole

	m_doubleSidedCheck = new QCheckBox(tr("両面基板"), this);

	m_substrateColorButton = new QPushButton(this);
	m_padColorButton = new QPushButton(this);
	m_copperColorButton = new QPushButton(this);
	for (QPushButton *b : {m_substrateColorButton, m_padColorButton, m_copperColorButton}) {
		b->setFixedWidth(80);
	}
	updateColorButton(m_substrateColorButton, m_substrateColor);
	updateColorButton(m_padColorButton, m_padColor);
	updateColorButton(m_copperColorButton, m_copperColor);

	m_derivedLabel = new QLabel(this);
	m_derivedLabel->setStyleSheet(QStringLiteral("color: palette(mid);"));

	m_frontBgThumb = new QLabel(this);
	m_frontBgThumb->setFixedSize(kThumbW, kThumbH);
	m_frontBgThumb->setAlignment(Qt::AlignCenter);
	m_frontBgThumb->setStyleSheet(QStringLiteral("background: palette(base); border: 1px solid palette(mid);"));
	auto *frontBgOpenButton = new QPushButton(tr("開く..."), this);
	m_frontBgClearButton = new QPushButton(tr("クリア"), this);
	connect(frontBgOpenButton, &QPushButton::clicked, this, &BoardEditorDialog::onOpenFrontBackground);
	connect(m_frontBgClearButton, &QPushButton::clicked, this, &BoardEditorDialog::onClearFrontBackground);

	m_backBgThumb = new QLabel(this);
	m_backBgThumb->setFixedSize(kThumbW, kThumbH);
	m_backBgThumb->setAlignment(Qt::AlignCenter);
	m_backBgThumb->setStyleSheet(QStringLiteral("background: palette(base); border: 1px solid palette(mid);"));
	auto *backBgOpenButton = new QPushButton(tr("開く..."), this);
	m_backBgClearButton = new QPushButton(tr("クリア"), this);
	connect(backBgOpenButton, &QPushButton::clicked, this, &BoardEditorDialog::onOpenBackBackground);
	connect(m_backBgClearButton, &QPushButton::clicked, this, &BoardEditorDialog::onClearBackBackground);

	updateThumbnail(m_frontBgThumb, m_backgroundFront);
	updateThumbnail(m_backBgThumb, m_backgroundBack);

	auto *modeRow = new QHBoxLayout();
	modeRow->addWidget(m_imageModeRadio);
	modeRow->addWidget(m_paramModeRadio);
	modeRow->addStretch(1);

	m_form = new QFormLayout();
	m_form->addRow(tr("作成方法:"), modeRow);
	m_form->addRow(tr("ID:"), m_idEdit);
	m_form->addRow(tr("名前:"), m_nameEdit);
	m_form->addRow(tr("列数 (cols):"), m_colsSpin);
	m_form->addRow(tr("行数 (rows):"), m_rowsSpin);
	m_form->addRow(tr("ピッチ:"), m_pitchSpin);
	auto *originRow = new QHBoxLayout();
	originRow->addWidget(new QLabel(tr("X:"), this));
	originRow->addWidget(m_originXSpin);
	originRow->addWidget(new QLabel(tr("Y:"), this));
	originRow->addWidget(m_originYSpin);
	originRow->addStretch(1);
	m_form->addRow(tr("原点 (外形左上からの余白):"), originRow);
	m_form->addRow(tr("パッド形状:"), m_padShapeCombo);
	m_form->addRow(tr("パッド直径:"), m_padDiameterSpin);
	m_form->addRow(tr("穴直径:"), m_holeDiameterSpin);
	m_form->addRow(tr("銅箔パターン:"), m_copperCombo);
	m_form->addRow(QString(), m_doubleSidedCheck);
	m_colorRow = new QHBoxLayout();
	m_colorRow->addWidget(m_substrateColorButton);
	m_colorRow->addWidget(new QLabel(tr("パッド:"), this));
	m_colorRow->addWidget(m_padColorButton);
	m_colorRow->addWidget(new QLabel(tr("銅箔:"), this));
	m_colorRow->addWidget(m_copperColorButton);
	m_colorRow->addStretch(1);
	m_form->addRow(tr("基板色:"), m_colorRow);

	m_frontBgRow = new QHBoxLayout();
	m_frontBgRow->addWidget(m_frontBgThumb);
	m_frontBgRow->addWidget(frontBgOpenButton);
	m_frontBgRow->addWidget(m_frontBgClearButton);
	m_frontBgRow->addStretch(1);
	auto *frontBgLabelWidget = new QWidget(this);
	auto *frontBgLabelLayout = new QHBoxLayout(frontBgLabelWidget);
	frontBgLabelLayout->setContentsMargins(0, 0, 0, 0);
	frontBgLabelLayout->addWidget(new QLabel(tr("表面画像:"), this));
	frontBgLabelLayout->addWidget(
		helphint::button(tr("画像モードでは必須です。この画像の画素サイズがそのまま基板サイズになります。"), this));
	m_form->addRow(frontBgLabelWidget, m_frontBgRow);

	m_backBgRow = new QHBoxLayout();
	m_backBgRow->addWidget(m_backBgThumb);
	m_backBgRow->addWidget(backBgOpenButton);
	m_backBgRow->addWidget(m_backBgClearButton);
	m_backBgRow->addStretch(1);
	auto *backBgLabelWidget = new QWidget(this);
	auto *backBgLabelLayout = new QHBoxLayout(backBgLabelWidget);
	backBgLabelLayout->setContentsMargins(0, 0, 0, 0);
	backBgLabelLayout->addWidget(new QLabel(tr("裏面画像 (任意):"), this));
	backBgLabelLayout->addWidget(helphint::button(
		tr("「基板を裏から見た向き」で用意してください (PasS の *_.bmp と同じ規約)。"), this));
	m_form->addRow(backBgLabelWidget, m_backBgRow);

	m_buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
	connect(m_buttons, &QDialogButtonBox::accepted, this, &BoardEditorDialog::onAccept);
	connect(m_buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

	auto *layout = new QVBoxLayout(this);
	layout->addLayout(m_form);
	layout->addWidget(m_derivedLabel);
	layout->addWidget(m_buttons);

	connect(m_colsSpin, &QSpinBox::valueChanged, this, &BoardEditorDialog::updateDerivedLabel);
	connect(m_rowsSpin, &QSpinBox::valueChanged, this, &BoardEditorDialog::updateDerivedLabel);
	connect(m_pitchSpin, &QSpinBox::valueChanged, this, &BoardEditorDialog::updateDerivedLabel);
	connect(m_originXSpin, &QSpinBox::valueChanged, this, &BoardEditorDialog::updateDerivedLabel);
	connect(m_originYSpin, &QSpinBox::valueChanged, this, &BoardEditorDialog::updateDerivedLabel);
	connect(m_substrateColorButton, &QPushButton::clicked, this, &BoardEditorDialog::pickSubstrateColor);
	connect(m_padColorButton, &QPushButton::clicked, this, &BoardEditorDialog::pickPadColor);
	connect(m_copperColorButton, &QPushButton::clicked, this, &BoardEditorDialog::pickCopperColor);

	setMode(Mode::Create);
	onCreationModeToggled();
	updateDerivedLabel();
}

void BoardEditorDialog::onCreationModeToggled() {
	const bool imageMode = m_imageModeRadio->isChecked();
	// 画像モード: パッド形状・パッド直径・銅箔パターン・両面・色は背景画像がすべての
	// 見た目を担うため無意味 (かつ紛らわしい) なので隠す。
	m_form->setRowVisible(m_padShapeCombo, !imageMode);
	m_form->setRowVisible(m_padDiameterSpin, !imageMode);
	m_form->setRowVisible(m_copperCombo, !imageMode);
	m_form->setRowVisible(m_doubleSidedCheck, !imageMode);
	m_form->setRowVisible(m_colorRow, !imageMode);
	// パラメトリックモード: 画像欄は無意味なので隠す。
	m_form->setRowVisible(m_frontBgRow, imageMode);
	m_form->setRowVisible(m_backBgRow, imageMode);
	updateDerivedLabel();
}

void BoardEditorDialog::setMode(Mode mode) {
	if (mode == Mode::Create) {
		setWindowTitle(tr("基板を作成"));
		m_buttons->button(QDialogButtonBox::Ok)->setText(tr("作成"));
	} else {
		setWindowTitle(tr("基板を編集"));
		m_buttons->button(QDialogButtonBox::Ok)->setText(tr("保存"));
	}
}

void BoardEditorDialog::updateColorButton(QPushButton *button, const QColor &color) {
	button->setText(color.name());
	button->setStyleSheet(
		QStringLiteral("background-color: %1; color: %2;")
			.arg(color.name(), color.lightness() > 128 ? QStringLiteral("black") : QStringLiteral("white")));
}

void BoardEditorDialog::pickSubstrateColor() {
	const QColor c = QColorDialog::getColor(m_substrateColor, this, tr("基板色を選択"));
	if (c.isValid()) {
		m_substrateColor = c;
		updateColorButton(m_substrateColorButton, c);
	}
}

void BoardEditorDialog::pickPadColor() {
	const QColor c = QColorDialog::getColor(m_padColor, this, tr("パッド色を選択"));
	if (c.isValid()) {
		m_padColor = c;
		updateColorButton(m_padColorButton, c);
	}
}

void BoardEditorDialog::pickCopperColor() {
	const QColor c = QColorDialog::getColor(m_copperColor, this, tr("銅箔色を選択"));
	if (c.isValid()) {
		m_copperColor = c;
		updateColorButton(m_copperColorButton, c);
	}
}

void BoardEditorDialog::updateDerivedLabel() {
	const BoardSpec b = boardFromFields();
	const double widthMm = b.size.width() * units::MmPerUnit;
	const double heightMm = b.size.height() * units::MmPerUnit;
	const bool sizedByImage = b.backgroundFront.has_value() && !b.backgroundFront->image.isNull();
	m_derivedLabel->setText(tr("基板サイズ: %1 x %2 単位 (%3mm x %4mm)%5 / 穴数: %6")
								 .arg(b.size.width())
								 .arg(b.size.height())
								 .arg(widthMm, 0, 'f', 1)
								 .arg(heightMm, 0, 'f', 1)
								 .arg(sizedByImage ? tr(" (表面画像に合わせています)") : QString())
								 .arg(b.cols * b.rows));
}

void BoardEditorDialog::updateThumbnail(QLabel *thumb, const std::optional<Artwork> &art) {
	if (!art.has_value() || art->image.isNull()) {
		thumb->setPixmap(QPixmap());
		thumb->setText(QObject::tr("(未設定)"));
		return;
	}
	thumb->setText(QString());
	thumb->setPixmap(
		QPixmap::fromImage(art->image).scaled(kThumbW, kThumbH, Qt::KeepAspectRatio, Qt::SmoothTransformation));
}

void BoardEditorDialog::loadBackgroundInto(std::optional<Artwork> &target, QLabel *thumb,
										   const QString &dialogTitle) {
	const QString path =
		QFileDialog::getOpenFileName(this, dialogTitle, QString(), tr("画像ファイル (*.png *.bmp *.jpg *.jpeg)"));
	if (path.isEmpty()) {
		return;
	}
	QImage img(path);
	if (img.isNull()) {
		QMessageBox::warning(this, tr("読み込み失敗"), tr("画像を読み込めませんでした: %1").arg(path));
		return;
	}
	target = Artwork::fromImageAsIs(img);
	updateThumbnail(thumb, target);
	updateDerivedLabel();
}

void BoardEditorDialog::onOpenFrontBackground() {
	loadBackgroundInto(m_backgroundFront, m_frontBgThumb, tr("表面の背景画像を開く"));
}

void BoardEditorDialog::onClearFrontBackground() {
	m_backgroundFront.reset();
	updateThumbnail(m_frontBgThumb, m_backgroundFront);
	updateDerivedLabel();
}

void BoardEditorDialog::onOpenBackBackground() {
	loadBackgroundInto(m_backgroundBack, m_backBgThumb, tr("裏面の背景画像を開く"));
}

void BoardEditorDialog::onClearBackBackground() {
	m_backgroundBack.reset();
	updateThumbnail(m_backBgThumb, m_backgroundBack);
	updateDerivedLabel();
}

BoardSpec BoardEditorDialog::boardFromFields() const {
	BoardSpec b;
	b.id = m_idEdit->text().trimmed();
	b.name = m_nameEdit->text().trimmed();
	b.cols = m_colsSpin->value();
	b.rows = m_rowsSpin->value();
	b.pitch = m_pitchSpin->value();
	b.origin = QPoint(m_originXSpin->value(), m_originYSpin->value());
	b.holeDiameter = m_holeDiameterSpin->value();
	if (m_imageModeRadio->isChecked()) {
		// 画像モード: 見た目はすべて背景画像が担う。パッド・銅箔・色は既定値のまま
		// (UI 上も非表示にしてあるので、ここで強制しておかないと以前入力した値が
		// こっそり残ってしまう)。
		b.padShape = PadShape::None;
		b.copper = CopperPattern::None;
		b.doubleSided = false;
		b.substrateColor = boarddefaults::Substrate;
		b.padColor = boarddefaults::Pad;
		b.copperColor = boarddefaults::Copper;
		b.backgroundFront = m_backgroundFront;
		b.backgroundBack = m_backgroundBack;
	} else {
		b.padShape = static_cast<PadShape>(m_padShapeCombo->currentData().toInt());
		b.padDiameter = m_padDiameterSpin->value();
		b.copper = static_cast<CopperPattern>(m_copperCombo->currentData().toInt());
		b.doubleSided = m_doubleSidedCheck->isChecked();
		b.substrateColor = m_substrateColor;
		b.padColor = m_padColor;
		b.copperColor = m_copperColor;
		b.backgroundFront.reset();
		b.backgroundBack.reset();
	}
	// サイズ: 表面画像があればその画素サイズを基板サイズとする (1単位=1画素なので自然であり、
	// PasS 取込基板 (size == BMP のサイズ) と同じ規則になる)。無ければ従来どおり
	// cols/rows/pitch/origin から対称マージンで自動導出する。
	if (b.backgroundFront.has_value() && !b.backgroundFront->image.isNull()) {
		b.size = b.backgroundFront->image.size();
	} else {
		b.size = QSize(2 * b.origin.x() + (b.cols - 1) * b.pitch, 2 * b.origin.y() + (b.rows - 1) * b.pitch);
	}
	return b;
}

void BoardEditorDialog::setBoard(const BoardSpec &board) {
	// 背景画像を持つ基板は画像モード、持たない基板はパラメトリックモードとして編集する。
	const bool hasImage = board.backgroundFront.has_value() && !board.backgroundFront->image.isNull();
	m_imageModeRadio->setChecked(hasImage);
	m_paramModeRadio->setChecked(!hasImage);

	m_idEdit->setText(board.id);
	m_nameEdit->setText(board.name);
	m_colsSpin->setValue(board.cols);
	m_rowsSpin->setValue(board.rows);
	m_pitchSpin->setValue(board.pitch);
	m_originXSpin->setValue(board.origin.x());
	m_originYSpin->setValue(board.origin.y());
	const int padIdx = m_padShapeCombo->findData(static_cast<int>(board.padShape));
	m_padShapeCombo->setCurrentIndex(padIdx >= 0 ? padIdx : 0);
	m_padDiameterSpin->setValue(board.padDiameter);
	m_holeDiameterSpin->setValue(board.holeDiameter);
	const int copperIdx = m_copperCombo->findData(static_cast<int>(board.copper));
	m_copperCombo->setCurrentIndex(copperIdx >= 0 ? copperIdx : 0);
	m_doubleSidedCheck->setChecked(board.doubleSided);
	m_substrateColor = board.substrateColor;
	m_padColor = board.padColor;
	m_copperColor = board.copperColor;
	updateColorButton(m_substrateColorButton, m_substrateColor);
	updateColorButton(m_padColorButton, m_padColor);
	updateColorButton(m_copperColorButton, m_copperColor);
	m_backgroundFront = board.backgroundFront;
	m_backgroundBack = board.backgroundBack;
	updateThumbnail(m_frontBgThumb, m_backgroundFront);
	updateThumbnail(m_backBgThumb, m_backgroundBack);
	onCreationModeToggled();  // 行の表示/非表示を反映 (内部で updateDerivedLabel() も呼ぶ)
	setMode(Mode::Edit);
}

BoardSpec BoardEditorDialog::board() const {
	return boardFromFields();
}

void BoardEditorDialog::onAccept() {
	if (m_idEdit->text().trimmed().isEmpty() || m_nameEdit->text().trimmed().isEmpty()) {
		QMessageBox::warning(this, tr("入力エラー"), tr("ID と名前は必須です。"));
		return;
	}
	if (m_imageModeRadio->isChecked() && (!m_backgroundFront.has_value() || m_backgroundFront->image.isNull())) {
		QMessageBox::warning(this, tr("入力エラー"), tr("画像モードでは表面画像が必須です。"));
		return;
	}

	const BoardSpec b = boardFromFields();

	if (m_backgroundFront.has_value() && m_backgroundBack.has_value() &&
		m_backgroundFront->image.size() != m_backgroundBack->image.size()) {
		const auto btn = QMessageBox::question(
			this, tr("確認"),
			tr("表面と裏面の背景画像のサイズが一致していません (表: %1x%2 / 裏: %3x%4)。\n続けますか？")
				.arg(m_backgroundFront->image.width())
				.arg(m_backgroundFront->image.height())
				.arg(m_backgroundBack->image.width())
				.arg(m_backgroundBack->image.height()));
		if (btn != QMessageBox::Yes) {
			return;
		}
	}

	// 格子 (列数・行数・ピッチ・原点) が基板の外形からはみ出していないか確認する
	// (背景画像がある場合、基板外形はその画素サイズ)。ブロックはせず確認のみ。
	if (b.cols > 0 && b.rows > 0) {
		const QPoint lastHole = b.holeCenter(b.cols - 1, b.rows - 1);
		if (lastHole.x() > b.size.width() || lastHole.y() > b.size.height()) {
			const auto btn = QMessageBox::question(
				this, tr("確認"), tr("格子が基板の外形からはみ出しています。続けますか？"));
			if (btn != QMessageBox::Yes) {
				return;
			}
		}
	}

	accept();
}
