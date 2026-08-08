#include "keymap.h"

#include <QKeyEvent>
#include <QSettings>

#include "commandregistry.h"

Keymap::Keymap(QObject *parent) : QObject(parent) {
}

void Keymap::load() {
	m_overrides.clear();
	QSettings settings;
	settings.beginGroup(QStringLiteral("keymap"));
	for (const auto &cmd : commandregistry::allCommands()) {
		if (!settings.contains(cmd.id)) {
			continue;
		}
		const QStringList stored = settings.value(cmd.id).toStringList();
		QVector<InputGesture> gestures;
		for (const QString &s : stored) {
			if (const auto g = InputGesture::fromStorageString(s); g.has_value()) {
				gestures.append(*g);
			}
			// 個別に壊れた値は黙って捨てる (Phase 17 の設定バリデーション方針)。
		}
		if (!gestures.isEmpty()) {
			m_overrides.insert(cmd.id, gestures);
		}
		// gestures が空 (全滅) の場合は上書きなし = 既定値にフォールバックする。
	}
	settings.endGroup();
}

void Keymap::save() const {
	QSettings settings;
	settings.beginGroup(QStringLiteral("keymap"));
	settings.remove(QString());  // 一旦クリアしてから書き直す (削除された上書きを残さないため)
	for (auto it = m_overrides.constBegin(); it != m_overrides.constEnd(); ++it) {
		QStringList stored;
		for (const auto &g : it.value()) {
			stored << g.toStorageString();
		}
		settings.setValue(it.key(), stored);
	}
	settings.endGroup();
}

void Keymap::resetAll() {
	m_overrides.clear();
	save();
	emit changed();
}

void Keymap::reset(const QString &commandId) {
	if (m_overrides.remove(commandId) > 0) {
		save();
		emit changed();
	}
}

QVector<InputGesture> Keymap::gesturesFor(const QString &commandId) const {
	if (const auto it = m_overrides.constFind(commandId); it != m_overrides.constEnd()) {
		return it.value();
	}
	if (const auto *def = commandregistry::find(commandId)) {
		return def->defaults;
	}
	return {};
}

void Keymap::setGestures(const QString &commandId, const QVector<InputGesture> &gestures) {
	m_overrides.insert(commandId, gestures);
	save();
	emit changed();
}

bool Keymap::isCustomized(const QString &commandId) const {
	return m_overrides.contains(commandId);
}

QVector<Keymap::Conflict> Keymap::conflicts() const {
	QVector<Conflict> out;
	const auto &commands = commandregistry::allCommands();
	for (int i = 0; i < commands.size(); ++i) {
		const auto gesturesA = gesturesFor(commands[i].id);
		for (int j = i + 1; j < commands.size(); ++j) {
			const bool sameScope = commands[i].category == commands[j].category ||
									commands[i].category == QStringLiteral("global") ||
									commands[j].category == QStringLiteral("global");
			if (!sameScope) {
				continue;
			}
			const auto gesturesB = gesturesFor(commands[j].id);
			for (const auto &ga : gesturesA) {
				for (const auto &gb : gesturesB) {
					if (ga == gb) {
						out.append({commands[i].id, commands[j].id, ga});
					}
				}
			}
		}
	}
	return out;
}

bool Keymap::matchesKey(const QString &commandId, const QKeyEvent *event) const {
	if (!event) {
		return false;
	}
	for (const auto &g : gesturesFor(commandId)) {
		if (g.matchesKey(event->key(), event->nativeScanCode(), event->modifiers())) {
			return true;
		}
	}
	return false;
}

bool Keymap::matchesMouseButton(const QString &commandId, Qt::MouseButton button, Qt::KeyboardModifiers mods,
								 InputKind asKind) const {
	for (const auto &g : gesturesFor(commandId)) {
		if (g.matchesMouseButton(button, mods, asKind)) {
			return true;
		}
	}
	return false;
}

QString Keymap::displayFor(const QString &commandId) const {
	QStringList parts;
	for (const auto &g : gesturesFor(commandId)) {
		parts << g.toDisplayString();
	}
	return parts.join(QObject::tr("または"));
}
