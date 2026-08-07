#include "parteditordialog.h"

#include <QAbstractItemView>
#include <QCheckBox>
#include <QColorDialog>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPainter>
#include <QPushButton>
#include <QScrollArea>
#include <QSpinBox>
#include <QTableWidget>
#include <QVBoxLayout>
#include <algorithm>
#include <limits>

// ---------------------------------------------------------------- PartArtworkCanvas

PartArtworkCanvas::PartArtworkCanvas(QWidget *parent) : QWidget(parent) {
	setMinimumSize(120, 120);
}

void PartArtworkCanvas::recomputeScale() {
	if (m_image.isNull()) {
		m_scale = 1;
		return;
	}
	const int longest = std::max(m_image.width(), m_image.height());
	int scale = 1;
	while (longest * scale < 240 && scale < 16) {
		++scale;
	}
	m_scale = scale;
}

void PartArtworkCanvas::setImage(const QImage &image) {
	m_image = image;
	recomputeScale();
	resize(sizeHint());
	update();
}

void PartArtworkCanvas::setPins(const QVector<Pin> &pins) {
	m_pins = pins;
	update();
}

QSize PartArtworkCanvas::sizeHint() const {
	if (m_image.isNull()) {
		return QSize(240, 240);
	}
	return m_image.size() * m_scale;
}

QPoint PartArtworkCanvas::widgetToImage(const QPoint &widgetPos) const {
	return QPoint(widgetPos.x() / m_scale, widgetPos.y() / m_scale);
}

void PartArtworkCanvas::paintEvent(QPaintEvent *) {
	QPainter painter(this);
	painter.fillRect(rect(), palette().color(QPalette::Dark));
	if (m_image.isNull()) {
		return;
	}
	const QRect target(0, 0, m_image.width() * m_scale, m_image.height() * m_scale);
	painter.drawImage(target, m_image);

	for (const Pin &pin : m_pins) {
		const QPoint center(pin.pos.x() * m_scale + m_scale / 2, pin.pos.y() * m_scale + m_scale / 2);
		const int r = std::max(3, m_scale / 2);
		painter.setPen(QPen(Qt::black, 1));
		painter.setBrush(pin.hasNumber() ? QColor(255, 60, 60) : QColor(60, 140, 255));
		painter.drawEllipse(center, r, r);
		if (pin.hasNumber()) {
			painter.setPen(Qt::white);
			QFont f = painter.font();
			f.setPointSizeF(std::max(6.0, r * 1.1));
			painter.setFont(f);
			painter.drawText(QRect(center.x() - 20, center.y() - 20, 40, 40), Qt::AlignCenter,
							  QString::number(pin.number));
		}
	}
}

void PartArtworkCanvas::mousePressEvent(QMouseEvent *event) {
	if (m_image.isNull()) {
		return;
	}
	const QPoint imgPos = widgetToImage(event->pos());
	if (imgPos.x() < 0 || imgPos.y() < 0 || imgPos.x() >= m_image.width() || imgPos.y() >= m_image.height()) {
		return;
	}
	emit clicked(imgPos, event->button());
}

// ---------------------------------------------------------------- PartEditorDialog

PartEditorDialog::PartEditorDialog(QWidget *parent) : QDialog(parent) {
	setWindowTitle(tr("部品を編集"));

	m_canvas = new PartArtworkCanvas(this);
	m_hintLabel = new QLabel(tr("「画像を開く」から部品のラスタ画像を読み込んでください。"), this);
	m_hintLabel->setWordWrap(true);

	auto *openImageButton = new QPushButton(tr("画像を開く..."), this);
	connect(openImageButton, &QPushButton::clicked, this, &PartEditorDialog::onOpenImage);

	m_idEdit = new QLineEdit(this);
	m_idEdit->setPlaceholderText(tr("例: my-part-1"));
	m_nameEdit = new QLineEdit(this);
	m_keywordsEdit = new QLineEdit(this);
	m_keywordsEdit->setPlaceholderText(tr("カンマ区切り (例: resistor, 抵抗)"));

	m_kindCombo = new QComboBox(this);
	m_kindCombo->addItem(tr("通常部品"), static_cast<int>(PartKind::Normal));
	m_kindCombo->addItem(tr("テキスト"), static_cast<int>(PartKind::Text));
	m_kindCombo->addItem(tr("表面塗りつぶし"), static_cast<int>(PartKind::ToolFillTop));
	m_kindCombo->addItem(tr("裏面塗りつぶし"), static_cast<int>(PartKind::ToolFillBottom));
	m_kindCombo->addItem(tr("スルーホール"), static_cast<int>(PartKind::ToolThruHole));
	m_kindCombo->addItem(tr("ドリル穴"), static_cast<int>(PartKind::DrillHole));

	m_refPrefixEdit = new QLineEdit(this);
	m_refPrefixEdit->setPlaceholderText(tr("例: R, C, IC"));
	m_refPrefixEdit->setMaximumWidth(80);

	m_chromaKeyCheck = new QCheckBox(tr("背景色を透過する"), this);
	m_chromaColorButton = new QPushButton(this);
	m_chromaColorButton->setFixedWidth(80);
	updateColorButton(m_chromaColorButton, m_chromaKey);
	m_eyedropperButton = new QPushButton(tr("スポイトで拾う"), this);
	m_eyedropperButton->setCheckable(true);

	m_drillOnlyModeCheck = new QCheckBox(tr("番号なし (ドリル穴) として配置"), this);
	m_newPinDrillSpin = new QSpinBox(this);
	m_newPinDrillSpin->setRange(0, 99);
	m_newPinDrillSpin->setSuffix(tr(" (0=既定0.9mm)"));

	m_pinTable = new QTableWidget(this);
	m_pinTable->setColumnCount(4);
	m_pinTable->setHorizontalHeaderLabels({tr("番号"), tr("X"), tr("Y"), tr("ドリル")});
	m_pinTable->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
	m_pinTable->verticalHeader()->setVisible(false);
	m_pinTable->setSelectionBehavior(QAbstractItemView::SelectRows);
	m_pinTable->setSelectionMode(QAbstractItemView::SingleSelection);

	m_deletePinButton = new QPushButton(tr("選択したピンを削除"), this);
	connect(m_deletePinButton, &QPushButton::clicked, this, &PartEditorDialog::onDeleteSelectedPin);

	auto *canvasColumn = new QVBoxLayout();
	canvasColumn->addWidget(openImageButton);
	canvasColumn->addWidget(m_hintLabel);
	auto *canvasScroll = new QScrollArea(this);
	canvasScroll->setWidget(m_canvas);
	canvasScroll->setMinimumSize(300, 300);
	// キャンバスより余ったビューポート領域も同じ暗色で塗り、小さい画像でも
	// 浮いた明るい余白ができないようにする。
	canvasScroll->setBackgroundRole(QPalette::Dark);
	canvasColumn->addWidget(canvasScroll, /*stretch=*/1);
	auto *chromaRow = new QHBoxLayout();
	chromaRow->addWidget(m_chromaKeyCheck);
	chromaRow->addWidget(m_chromaColorButton);
	chromaRow->addWidget(m_eyedropperButton);
	chromaRow->addStretch(1);
	canvasColumn->addLayout(chromaRow);
	auto *pinModeRow = new QHBoxLayout();
	pinModeRow->addWidget(m_drillOnlyModeCheck);
	pinModeRow->addWidget(new QLabel(tr("新規ピンのドリル径:"), this));
	pinModeRow->addWidget(m_newPinDrillSpin);
	pinModeRow->addStretch(1);
	canvasColumn->addLayout(pinModeRow);

	auto *sideColumn = new QVBoxLayout();
	auto *form = new QFormLayout();
	form->addRow(tr("ID:"), m_idEdit);
	form->addRow(tr("名前:"), m_nameEdit);
	form->addRow(tr("キーワード:"), m_keywordsEdit);
	form->addRow(tr("種類:"), m_kindCombo);
	form->addRow(tr("参照子接頭辞:"), m_refPrefixEdit);
	sideColumn->addLayout(form);
	sideColumn->addWidget(new QLabel(tr("ピン一覧 (キャンバス左クリックで追加/右クリックで削除):"), this));
	sideColumn->addWidget(m_pinTable, /*stretch=*/1);
	sideColumn->addWidget(m_deletePinButton);

	auto *columns = new QHBoxLayout();
	columns->addLayout(canvasColumn, 1);
	columns->addLayout(sideColumn, 1);

	auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
	connect(buttons, &QDialogButtonBox::accepted, this, &PartEditorDialog::onAccept);
	connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

	auto *mainLayout = new QVBoxLayout(this);
	mainLayout->addLayout(columns, /*stretch=*/1);
	mainLayout->addWidget(buttons);
	resize(760, 560);

	connect(m_canvas, &PartArtworkCanvas::clicked, this, &PartEditorDialog::onCanvasClicked);
	connect(m_chromaColorButton, &QPushButton::clicked, this, &PartEditorDialog::onChromaColorButtonClicked);
	connect(m_eyedropperButton, &QPushButton::toggled, this, &PartEditorDialog::onToggleEyedropper);
	connect(m_chromaKeyCheck, &QCheckBox::toggled, this, &PartEditorDialog::onChromaKeyToggled);
	connect(m_pinTable, &QTableWidget::itemChanged, this, &PartEditorDialog::onPinTableItemChanged);
}

void PartEditorDialog::updateColorButton(QPushButton *button, const QColor &color) {
	button->setText(color.name());
	button->setStyleSheet(
		QStringLiteral("background-color: %1; color: %2;")
			.arg(color.name(), color.lightness() > 128 ? QStringLiteral("black") : QStringLiteral("white")));
}

void PartEditorDialog::onOpenImage() {
	const QString path = QFileDialog::getOpenFileName(this, tr("部品画像を開く"), QString(),
														tr("画像ファイル (*.png *.bmp *.jpg *.jpeg)"));
	if (path.isEmpty()) {
		return;
	}
	QImage img(path);
	if (img.isNull()) {
		QMessageBox::warning(this, tr("読み込み失敗"), tr("画像を読み込めませんでした: %1").arg(path));
		return;
	}
	if (!m_pins.isEmpty()) {
		const auto btn = QMessageBox::question(
			this, tr("確認"),
			tr("画像を差し替えます。ピンの座標はそのまま保持されるので、サイズが変わると\n"
			   "ピンが画像からはみ出す可能性があります。続けますか？"));
		if (btn != QMessageBox::Yes) {
			return;
		}
	}
	m_sourceImage = img.convertToFormat(QImage::Format_ARGB32);
	m_hintLabel->setText(tr("%1 x %2 px").arg(m_sourceImage.width()).arg(m_sourceImage.height()));
	// クロマキー色の初期値を、読み込んだ画像自身の左上画素から推定する。この種の部品画像は
	// 背景色を隅に置く慣習が強く、決め打ちの定数より実用的。ユーザーはこの後スポイトや
	// 色選択ダイアログで自由に変更できる。
	m_chromaKey = m_sourceImage.pixelColor(0, 0);
	updateColorButton(m_chromaColorButton, m_chromaKey);
	refreshCanvasArtwork();
}

void PartEditorDialog::onToggleEyedropper(bool on) {
	m_eyedropperActive = on;
	m_canvas->setCursor(on ? Qt::CrossCursor : Qt::ArrowCursor);
}

void PartEditorDialog::onChromaKeyToggled(bool) {
	refreshCanvasArtwork();
}

void PartEditorDialog::onChromaColorButtonClicked() {
	const QColor c = QColorDialog::getColor(m_chromaKey, this, tr("透過する色を選択"));
	if (c.isValid()) {
		m_chromaKey = c;
		updateColorButton(m_chromaColorButton, c);
		refreshCanvasArtwork();
	}
}

void PartEditorDialog::onCanvasClicked(QPoint imagePos, Qt::MouseButton button) {
	if (m_sourceImage.isNull()) {
		return;
	}
	if (m_eyedropperActive) {
		m_chromaKey = m_sourceImage.pixelColor(imagePos);
		updateColorButton(m_chromaColorButton, m_chromaKey);
		m_eyedropperButton->setChecked(false);  // onToggleEyedropper(false) 経由で m_eyedropperActive も戻る
		refreshCanvasArtwork();
		return;
	}

	if (button == Qt::RightButton) {
		// クリック位置に最も近いピンを、許容範囲内であれば削除する。
		int bestIdx = -1;
		int bestDist2 = std::numeric_limits<int>::max();
		for (int i = 0; i < m_pins.size(); ++i) {
			const QPoint d = m_pins[i].pos - imagePos;
			const int dist2 = d.x() * d.x() + d.y() * d.y();
			if (dist2 < bestDist2) {
				bestDist2 = dist2;
				bestIdx = i;
			}
		}
		constexpr int kPickRadius = 4;  // 画像ピクセル単位
		if (bestIdx >= 0 && bestDist2 <= kPickRadius * kPickRadius) {
			m_pins.removeAt(bestIdx);
			renumberSequentially();
			rebuildPinTable();
			m_canvas->setPins(m_pins);
		}
		return;
	}

	if (button == Qt::LeftButton) {
		Pin pin;
		pin.pos = imagePos;
		pin.number = m_drillOnlyModeCheck->isChecked() ? 0 : nextPinNumber();
		pin.drill = m_newPinDrillSpin->value();
		m_pins.append(pin);
		rebuildPinTable();
		m_canvas->setPins(m_pins);
	}
}

int PartEditorDialog::nextPinNumber() const {
	int maxN = 0;
	for (const Pin &p : m_pins) {
		maxN = std::max(maxN, p.number);
	}
	return maxN + 1;
}

void PartEditorDialog::renumberSequentially() {
	// PasS 実データのピン番号は常にギャップの無い連番なので、削除のたびに
	// 詰め直して同じ規約を保つ (番号なしピン=ドリル穴はそのまま0を維持する)。
	int next = 1;
	for (Pin &p : m_pins) {
		if (p.hasNumber()) {
			p.number = next++;
		}
	}
}

void PartEditorDialog::rebuildPinTable() {
	m_pinTable->blockSignals(true);
	m_pinTable->setRowCount(m_pins.size());
	for (int row = 0; row < m_pins.size(); ++row) {
		const Pin &p = m_pins[row];
		auto *numberItem = new QTableWidgetItem(p.hasNumber() ? QString::number(p.number) : tr("(なし)"));
		numberItem->setFlags(numberItem->flags() & ~Qt::ItemIsEditable);
		auto *xItem = new QTableWidgetItem(QString::number(p.pos.x()));
		xItem->setFlags(xItem->flags() & ~Qt::ItemIsEditable);
		auto *yItem = new QTableWidgetItem(QString::number(p.pos.y()));
		yItem->setFlags(yItem->flags() & ~Qt::ItemIsEditable);
		auto *drillItem = new QTableWidgetItem(QString::number(p.drill));  // これだけ編集可 (既定フラグのまま)

		m_pinTable->setItem(row, 0, numberItem);
		m_pinTable->setItem(row, 1, xItem);
		m_pinTable->setItem(row, 2, yItem);
		m_pinTable->setItem(row, 3, drillItem);
	}
	m_pinTable->blockSignals(false);
}

void PartEditorDialog::onPinTableItemChanged(QTableWidgetItem *item) {
	if (!item || item->column() != 3) {
		return;
	}
	const int row = item->row();
	if (row < 0 || row >= m_pins.size()) {
		return;
	}
	bool ok = false;
	const int value = item->text().toInt(&ok);
	if (!ok || value < 0) {
		m_pinTable->blockSignals(true);
		item->setText(QString::number(m_pins[row].drill));
		m_pinTable->blockSignals(false);
		return;
	}
	m_pins[row].drill = value;
}

void PartEditorDialog::onDeleteSelectedPin() {
	const QModelIndexList selected =
		m_pinTable->selectionModel() ? m_pinTable->selectionModel()->selectedRows() : QModelIndexList();
	if (selected.isEmpty()) {
		return;
	}
	const int row = selected.first().row();
	if (row < 0 || row >= m_pins.size()) {
		return;
	}
	m_pins.removeAt(row);
	renumberSequentially();
	rebuildPinTable();
	m_canvas->setPins(m_pins);
}

void PartEditorDialog::refreshCanvasArtwork() {
	if (m_sourceImage.isNull()) {
		m_canvas->setImage(QImage());
		m_canvas->setPins(m_pins);
		return;
	}
	if (m_chromaKeyCheck->isChecked()) {
		const Artwork preview = Artwork::fromChromaKeyed(m_sourceImage, m_chromaKey);
		m_canvas->setImage(preview.image);
	} else {
		m_canvas->setImage(m_sourceImage);
	}
	m_canvas->setPins(m_pins);
}

void PartEditorDialog::setPart(const Part &part) {
	m_idEdit->setText(part.id);
	m_nameEdit->setText(part.name);
	m_keywordsEdit->setText(part.keywords.join(QStringLiteral(", ")));
	const int kindIdx = m_kindCombo->findData(static_cast<int>(part.kind));
	m_kindCombo->setCurrentIndex(kindIdx >= 0 ? kindIdx : 0);
	m_refPrefixEdit->setText(part.refPrefix);

	// 既存部品のアートワークは (クロマキー処理済みなら) その透過済み画像をそのまま
	// 「元画像」として扱う (元の不透明画像はモデル上保持しておらず復元できないため)。
	m_sourceImage =
		part.artwork.image.isNull() ? QImage() : part.artwork.image.convertToFormat(QImage::Format_ARGB32);
	m_chromaKeyCheck->setChecked(false);
	if (part.artwork.chromaKey.has_value()) {
		m_chromaKey = *part.artwork.chromaKey;
		updateColorButton(m_chromaColorButton, m_chromaKey);
	}
	m_pins = part.pins;
	m_hintLabel->setText(m_sourceImage.isNull()
							  ? tr("「画像を開く」から部品のラスタ画像を読み込んでください。")
							  : tr("%1 x %2 px").arg(m_sourceImage.width()).arg(m_sourceImage.height()));
	refreshCanvasArtwork();
	rebuildPinTable();
}

Part PartEditorDialog::part() const {
	Part p;
	p.id = m_idEdit->text().trimmed();
	p.name = m_nameEdit->text().trimmed();
	p.kind = static_cast<PartKind>(m_kindCombo->currentData().toInt());
	p.refPrefix = m_refPrefixEdit->text().trimmed();
	const auto keywordParts = m_keywordsEdit->text().split(QLatin1Char(','), Qt::SkipEmptyParts);
	for (const QString &kw : keywordParts) {
		p.keywords.append(kw.trimmed());
	}
	p.pins = m_pins;
	p.outline = QRect(QPoint(0, 0), m_sourceImage.size());
	if (m_chromaKeyCheck->isChecked()) {
		p.artwork = Artwork::fromChromaKeyed(m_sourceImage, m_chromaKey);
	} else {
		p.artwork = Artwork::fromImageAsIs(m_sourceImage);
	}
	return p;
}

void PartEditorDialog::onAccept() {
	if (m_idEdit->text().trimmed().isEmpty() || m_nameEdit->text().trimmed().isEmpty()) {
		QMessageBox::warning(this, tr("入力エラー"), tr("ID と名前は必須です。"));
		return;
	}
	if (m_sourceImage.isNull()) {
		QMessageBox::warning(this, tr("入力エラー"), tr("部品画像を読み込んでください。"));
		return;
	}
	accept();
}
