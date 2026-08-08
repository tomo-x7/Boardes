#include "commandregistry.h"

#include <QObject>

namespace {

InputGesture key(int k, Qt::KeyboardModifiers m = Qt::NoModifier) {
	InputGesture g;
	g.kind = InputKind::Key;
	g.key = k;
	g.mods = m;
	return g;
}

InputGesture mousePress(Qt::MouseButton b, Qt::KeyboardModifiers m = Qt::NoModifier) {
	InputGesture g;
	g.kind = InputKind::MouseButton;
	g.button = b;
	g.mods = m;
	return g;
}

InputGesture mouseDrag(Qt::MouseButton b, Qt::KeyboardModifiers m = Qt::NoModifier) {
	InputGesture g;
	g.kind = InputKind::MouseDrag;
	g.button = b;
	g.mods = m;
	return g;
}

}  // namespace

namespace commandregistry {

const QVector<CommandDef> &allCommands() {
	static const QVector<CommandDef> commands = {
		// --- global: どのツールがアクティブでも効く ---
		{QStringLiteral("tool.cancel"), QStringLiteral("global"), QObject::tr("ツールを解除/選択に戻る"),
		 QObject::tr("作図中の途中状態を破棄し、無ければ選択ツールに戻ります。"), {key(Qt::Key_Escape)}},

		// --- select: 選択ツールがアクティブな間 ---
		{QStringLiteral("select.delete"), QStringLiteral("select"), QObject::tr("選択を削除"),
		 QObject::tr("選択中の部品・配線を削除します。"), {key(Qt::Key_Delete), key(Qt::Key_Backspace)}},
		{QStringLiteral("select.rotate"), QStringLiteral("select"), QObject::tr("選択部品を回転"),
		 QObject::tr("選択中の部品を90度回転します。"), {key(Qt::Key_R)}},
		{QStringLiteral("select.flip"), QStringLiteral("select"), QObject::tr("選択部品の表裏切替"),
		 QObject::tr("選択中の部品を表面/裏面で切り替えます。"), {key(Qt::Key_F)}},
		{QStringLiteral("select.copy"), QStringLiteral("select"), QObject::tr("選択をコピー"),
		 QObject::tr("選択中の部品・配線をクリップボードにコピーします。"), {key(Qt::Key_C, Qt::ControlModifier)}},
		{QStringLiteral("select.paste"), QStringLiteral("select"), QObject::tr("貼り付け"),
		 QObject::tr("クリップボードの内容を少しずらして貼り付けます。"), {key(Qt::Key_V, Qt::ControlModifier)}},
		{QStringLiteral("select.moveLeft"), QStringLiteral("select"), QObject::tr("選択部品を左へ移動"),
		 QObject::tr("選択中の部品を1グリッド左へ移動します。"), {key(Qt::Key_Left)}},
		{QStringLiteral("select.moveRight"), QStringLiteral("select"), QObject::tr("選択部品を右へ移動"),
		 QObject::tr("選択中の部品を1グリッド右へ移動します。"), {key(Qt::Key_Right)}},
		{QStringLiteral("select.moveUp"), QStringLiteral("select"), QObject::tr("選択部品を上へ移動"),
		 QObject::tr("選択中の部品を1グリッド上へ移動します。"), {key(Qt::Key_Up)}},
		{QStringLiteral("select.moveDown"), QStringLiteral("select"), QObject::tr("選択部品を下へ移動"),
		 QObject::tr("選択中の部品を1グリッド下へ移動します。"), {key(Qt::Key_Down)}},

		// --- place: 部品配置ツールがアクティブな間 ---
		{QStringLiteral("place.rotate"), QStringLiteral("place"), QObject::tr("配置する部品を回転"),
		 QObject::tr("これから配置する部品の向きを90度回転します。"), {key(Qt::Key_R)}},
		{QStringLiteral("place.cancel"), QStringLiteral("place"), QObject::tr("配置ツールを解除"),
		 QObject::tr("配置ツールを終了し、選択ツールに戻ります。"), {mousePress(Qt::RightButton)}},

		// --- wire: 配線ツールがアクティブな間 ---
		{QStringLiteral("wire.commit"), QStringLiteral("wire"), QObject::tr("配線を確定"),
		 QObject::tr("作図中の配線を確定して配置します。"),
		 {mousePress(Qt::RightButton), key(Qt::Key_Return), key(Qt::Key_Enter)}},
		{QStringLiteral("wire.discard"), QStringLiteral("wire"), QObject::tr("配線を破棄"),
		 QObject::tr("作図中の配線を破棄します。"), {key(Qt::Key_Escape)}},

		// --- draft: 下書きツールがアクティブな間 ---
		{QStringLiteral("draft.stroke"), QStringLiteral("draft"), QObject::tr("下書きストローク"),
		 QObject::tr("ドラッグでフリーハンドの下書きを描きます (保存されません)。"), {mouseDrag(Qt::LeftButton)}},

		// --- view: ビュー全般 (ツールに関係なく効く) ---
		{QStringLiteral("view.pan"), QStringLiteral("view"), QObject::tr("ドラッグでビューをパン"),
		 QObject::tr("ドラッグしている間、ビューをスクロールします。"), {mouseDrag(Qt::MiddleButton)}},
		{QStringLiteral("view.panHold"), QStringLiteral("view"), QObject::tr("押している間パンモードにする"),
		 QObject::tr("押している間、左ドラッグがパン操作になります。"), {key(Qt::Key_Space)}},
		{QStringLiteral("view.fit"), QStringLiteral("view"), QObject::tr("ビューを基板全体に合わせる"),
		 QObject::tr("基板全体が収まるようにズームします。"), {key(Qt::Key_0, Qt::ControlModifier)}},
	};
	return commands;
}

const CommandDef *find(const QString &commandId) {
	for (const auto &cmd : allCommands()) {
		if (cmd.id == commandId) {
			return &cmd;
		}
	}
	return nullptr;
}

}  // namespace commandregistry
