#include "toolcontext.h"

#include "../../render/boardscene.h"

void ToolContext::syncBothScenesPlacements() const {
	if (frontScene) frontScene->syncPlacements();
	if (backScene) backScene->syncPlacements();
}

void ToolContext::syncBothScenesWires() const {
	if (frontScene) frontScene->syncWires();
	if (backScene) backScene->syncWires();
}
