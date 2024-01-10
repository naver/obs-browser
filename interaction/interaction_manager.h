#pragma once
#include <QObject>
#include <mutex>
#include <map>
#include "interaction_delegate.h"
#include "interaction_def.h"

struct BrowserSource;
class InteractionDelegate;
class InteractionManager : public QObject {
	Q_OBJECT

protected:
	InteractionManager();

public:
	static InteractionManager *Instance();

	virtual ~InteractionManager();

	void OnSourceCreated(InteractionDelegate *src);
	void OnSourceDeleted(InteractionDelegate *src);
	void RequestHideInteraction(SOURCE_HANDLE hdl);

public slots:
	void OnHideInteractionSlot(SOURCE_HANDLE hdl);

private:
	// here we should not save shared-ptr
	std::map<SOURCE_HANDLE, InteractionDelegate *> source_list;
	std::mutex source_lock;
};
