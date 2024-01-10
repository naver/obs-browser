#include "interaction_view_mac.h"
#include "interaction_util_mac.hpp"
//#include "interaction_hdc.hpp"
#include "interaction_main_mac.h"
#include <util/util.hpp>

using namespace interaction;

//------------------------------------------------------------------------
InteractionViewMac::InteractionViewMac(BrowserInteractionMain &interaction_main)
	: InteractionView(interaction_main)
{
}

InteractionViewMac::~InteractionViewMac()
{
	
}

void InteractionViewMac::Create(WindowHandle hp)
{
	
}

void InteractionViewMac::ResetInteraction()
{
}

void InteractionViewMac::OnImeCompositionRangeChanged(
	CefRefPtr<CefBrowser> browser, const CefRange &selection_range,
	const CefRenderHandler::RectList &character_bounds)
{
//	DCHECK(CefCurrentlyOn(TID_UI));
//
//	if (ime_handler_) {
//		RECT rc;
//		GetClientRect(hwnd_, &rc);
//
//		int left, top;
//		float scale;
//		GetScaleAndCenterPos(source_cx_, source_cy_, RectWidth(rc),
//				     RectHeight(rc), left, top, scale);
//
//		// Convert from view coordinates to device coordinates.
//		CefRenderHandler::RectList device_bounds;
//		CefRenderHandler::RectList::const_iterator it =
//			character_bounds.begin();
//		for (; it != character_bounds.end(); ++it) {
//			CefRect temp = LogicalToDevice(*it, scale);
//			temp.x += left;
//			temp.y += top;
//			device_bounds.push_back(temp);
//		}
//
//		ime_handler_->ChangeCompositionRange(selection_range,
//						     device_bounds);
//	}
}

