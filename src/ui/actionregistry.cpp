#include "actionregistry.h"

#include <QAction>

void ActionRegistry::add(const QString &commandId, QAction *action) {
	if (!m_actions.contains(commandId)) {
		m_order.append(commandId);
	}
	m_actions.insert(commandId, action);
}

QAction *ActionRegistry::action(const QString &commandId) const {
	return m_actions.value(commandId, nullptr);
}
