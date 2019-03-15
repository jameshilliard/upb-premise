#ifndef __UPBDRIVER_H_
#define __UPBDRIVER_H_

#include "resource.h"
#include "upbController.h"

/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
class ATL_NO_VTABLE CupbDriver : 
	public CComObjectRootEx<CComMultiThreadModel>,
	public CComCoClass<CupbDriver, &CLSID_upbDriver>,
	public CPremiseDriverImpl
{
public:

DECLARE_REGISTRY_RESOURCEID(IDR_UPBDRIVER)
DECLARE_NOT_AGGREGATABLE(CupbDriver)

BEGIN_COM_MAP(CupbDriver)
	COM_INTERFACE_ENTRY(IObjectWithSite)
	COM_INTERFACE_ENTRY(IPremiseNotify)
END_COM_MAP()

BEGIN_NOTIFY_MAP(CupbDriver) 
END_NOTIFY_MAP()

	HRESULT CreateControllerForSite(IPremiseObject*, IObjectWithSite** ppSite, bool bFirstTime)
	{
		return CupbController::CreateInstance(ppSite);
	}
};

#endif
