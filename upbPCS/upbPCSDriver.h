#ifndef __UPBPCSDRIVER_H_
#define __UPBPCSDRIVER_H_

#include "resource.h"
#include "..\upb\upbCommon.h"

#define NOBREAK
#define	SIZEOF_ARRAY(a) (sizeof(a)/sizeof(a[0]))

/*
	Byte align all structures as they are used to communicate with h/w.
*/
#pragma pack(push)
#pragma pack(1)

#define PCS_REG_RECEIVECOMPONENTS 0x40
#define PCS_REG_TRANSMITCOMMANDS 0x90
#define PCS_REG_TXCONTROL 0x8E

#define PCS_REG_TXCOMPONENTENTRY 5
#define PCS_REG_TXCOMMANDENTRY 3

//#define PCS_KEYPAD_REG_RECEIVECOMPONENTS PCS_REG_RECEIVECOMPONENTS
#define PCS_KEYPAD_REG_TRANSMITCOMPONENTS 0x48
#define PCS_KEYPAD_REG_IROPTIONS 0x8C
#define PCS_KEYPAD_REG_LEDOPTIONS 0x8D
//#define PCS_KEYPAD_REG_TXCONTROL PCS_REG_TXCONTROL
#define PCS_KEYPAD_REG_LEDONACTIONS 0x70
//#define PCS_KEYPAD_REG_TRANSMITCOMMANDS PCS_REG_TRANSMITCOMMANDS
#define PCS_KEYPAD_REG_LEDSTATES 0xF9

#define PCS_KEYPAD_REG_RECEIVECOMPONENTS_BYTES 0x08
#define PCS_KEYPAD_REG_TRANSMITCOMPONENTS_BYTES 0x28
#define PCS_KEYPAD_REG_IROPTIONS_BYTES 0x01
#define PCS_KEYPAD_REG_LEDOPTIONS_BYTES 0x01
#define PCS_KEYPAD_REG_TXCONTROL_BYTES 0x01
#define PCS_KEYPAD_REG_LEDONACTIONS_BYTES 0x08
#define PCS_KEYPAD_REG_TRANSMITCOMMANDS_BYTES 0x2D
#define PCS_KEYPAD_REG_LEDSTATES_BYTES 0x01

//#define PCS_KEYPAD_REG_TXCOMPONENTENTRY PCS_REG_TXCOMPONENTENTRY
//#define PCS_KEYPAD_REG_TXCOMMANDENTRY PCS_REG_TXCOMMANDENTRY

//#define PCS_SWITCH_REG_RECEIVECOMPONENTS PCS_REG_RECEIVECOMPONENTS
#define PCS_SWITCH_REG_TRANSMITCOMPONENTS 0x70
#define PCS_SWITCH_REG_ROCKERACTIONS 0x7A
#define PCS_SWITCH_REG_LEDOPTIONS 0x8B
#define PCS_SWITCH_REG_DIMMEROPTIONS 0x8D
//#define PCS_SWITCH_REG_TXCONTROL PCS_REG_TXCONTROL
//#define PCS_SWITCH_REG_TRANSMITCOMMANDS PCS_REG_TRANSMITCOMMANDS

#define PCS_SWITCH_REG_RECEIVECOMPONENTS_BYTES 0x30
#define PCS_SWITCH_REG_TRANSMITCOMPONENTS_BYTES 0x10
#define PCS_SWITCH_REG_ROCKERACTIONS_BYTES 0x08
#define PCS_SWITCH_REG_LEDOPTIONS_BYTES PCS_KEYPAD_REG_LEDOPTIONS_BYTES
#define PCS_SWITCH_REG_DIMMEROPTIONS_BYTES 0x01
#define PCS_SWITCH_REG_TXCONTROL_BYTES PCS_KEYPAD_REG_TXCONTROL_BYTES
#define PCS_SWITCH_REG_TRANSMITCOMMANDS_BYTES 0x2A

#define PCS_SWITCH_REG_RXCOMPONENTENTRY 0x03
//#define PCS_SWITCH_REG_TXCOMPONENTENTRY PCS_REG_TXCOMPONENTENTRY
//#define PCS_SWITCH_REG_TXCOMMANDENTRY PCS_REG_TXCOMMANDENTRY
#define PCS_SWITCH_REG_ROCKERACTIONENTRY 0x02

#define PCS_WSD1_RECEIVECOMPONENTS (PCS_SWITCH_REG_RECEIVECOMPONENTS_BYTES/PCS_SWITCH_REG_RXCOMPONENTENTRY)

#define PCS_SWITCH_LOADTYPE(d) (((d) >> 7) & 0x01)
#define PCS_SWITCH_REPORTSTATE(d) ((d) & 0x10)
#define PCS_SWITCH_DEFAULTFADERATE(d) ((d) & 0x0F)
#define PCS_SWITCH_SETOPTION_LOADTYPE(d, o) (d) |= (((o) & 1) << 7)
#define PCS_SWITCH_SETOPTION_REPORTSTATE(d, o) (d) |= ((o) ? 0x10 : 0)
#define PCS_SWITCH_SETOPTION_DEFAULTFADERATE(d, o) (d) |= ((o) & 0x0f)

#define PCS_SWITCH_LEDMODE(l) (((l) >> 7) & 0x01)
#define PCS_SWITCH_LEDONCOLOR(l) (((l) >> 2) & 0x03)
#define PCS_SWITCH_LEDOFFCOLOR(l) ((l) & 0x03)
#define PCS_SWITCH_SETOPTION_LEDMODE(l, o) (l) |= (((o) & 0x01) << 7)
#define PCS_SWITCH_SETOPTION_LEDONCOLOR(l, o) (l) |= (((o) & 0x03) << 2)
#define PCS_SWITCH_SETOPTION_LEDOFFCOLOR(l, o) (l) |= ((o) & 0x03)

#define PCS_MODULE_EVENTPACKETTYPE(l) ((l) & 0x80)
#define PCS_MODULE_ACKMESSAGEREQUEST(l) ((l) & 0x40)
#define PCS_MODULE_IDPULSEREQUEST(l) ((l) & 0x20)
#define PCS_MODULE_ACKPULSEREQUEST(l) ((l) & 0x10)
#define PCS_MODULE_TRANSMISSIONCOUNT(l) (((l) >> 2) & 0x03)
#define PCS_MODULE_SETOPTION_EVENTPACKETTYPE(l, o) (l) |= ((o) ? 0x80 : 0)
#define PCS_MODULE_SETOPTION_ACKMESSAGEREQUEST(l, o) (l) |= ((o) ? 0x40 : 0)
#define PCS_MODULE_SETOPTION_IDPULSEREQUEST(l, o) (l) |= ((o) ? 0x20 : 0)
#define PCS_MODULE_SETOPTION_ACKPULSEREQUEST(l, o) (l) |= ((o) ? 0x10 : 0)
#define PCS_MODULE_SETOPTION_TRANSMISSIONCOUNT(l, o) (l) |= (((o) & 0x03) << 2)

#define PCS_KEYPAD_LEDBACKLIGHTING(l) ((l) & 0x20)
#define PCS_KEYPAD_LEDTRACKING(l) ((l) & 0x08)
#define PCS_KEYPAD_LEDOUTPUT(l) ((l) & 0x04)
#define PCS_KEYPAD_LEDBRIGHTNESS(l) ((l) & 0x03)
#define PCS_KEYPAD_SETOPTION_LEDBACKLIGHTING(l, o) (l) |= ((o) ? 0x20 : 0)
#define PCS_KEYPAD_SETOPTION_LEDTRACKING(l, o) (l) |= ((o) ? 0x08 : 0)
#define PCS_KEYPAD_SETOPTION_LEDOUTPUT(l, o) (l) |= ((o) ? 0x04 : 0)
#define PCS_KEYPAD_SETOPTION_LEDOUTPUT(l, o) (l) |= ((o) ? 0x04 : 0)
#define PCS_KEYPAD_SETOPTION_LEDBRIGHTNESS(l, o) (l) |= ((o) & 0x03)

#define PCS_SWITCH_COMMANDIDS 14
#define PCS_KEYPAD_COMMANDIDS 15

#define PCS_KEYPAD_LEDACTION(l) (((l) >> 6) & 0x03)
#define PCS_KEYPAD_LEDACTIONGROUP(l) (((l) >> 4) & 0x03)
#define PCS_KEYPAD_LEDGROUPMASK(l) ((l) & 0x0f)
#define PCS_KEYPAD_SETOPTION_LEDACTION(l, o) (l) |= (((o) & 0x03) << 6)
#define PCS_KEYPAD_SETOPTION_LEDACTIONGROUP(l, o) (l) |= (((o) & 0x03) << 4)
#define PCS_KEYPAD_SETOPTION_LEDGROUPMASK(l, o) (l) |= ((o) & 0x0f)

#define PCS_KEYPAD_IRENABLED(i) ((i) & 0x01)
#define PCS_KEYPAD_IRROOM(i) ((i) >> 6)
#define PCS_KEYPAD_SETOPTION_IRENABLED(i, o) (i) |= ((o) ? 1 : 0)
#define PCS_KEYPAD_SETOPTION_IRROOM(i, o) (i) |= (((o) & 3) << 6)
#define PCS_KEYPAD_ROOMID_MAX 3

#pragma pack(pop) /* pack(1) */

#define XML_UPBPCS_SCHEMA_ROOT L"sys://Schema/upbPCS"
#define XML_PCS_UPBPCSBASE L"upbPCSBase"
#define XML_PCS_WS1D6 L"WS1D6"
#define XML_PCS_WMC6 L"WMC6"
#define XML_PCS_DTC6 L"DTC6"
#define XML_PCS_WMC8 L"WMC8"
#define XML_PCS_DTC8 L"DTC8"
#define XML_PCS_SERIALPIM L"SerialPIM"
#define XML_PCS_KEYPAD L"Keypad"
#define XML_PCS_SWITCH L"Switch"
#define XML_PCS_KEYPADBUTTON L"KeypadButton"
#define XML_PCS_SWITCHBUTTON L"SwitchButton"
#define XML_PCS_ROCKERACTION L"RockerAction"
#define XML_PCS_LEDONACTION L"LEDOnAction"

#define XML_PCS_BUTTONID L"ButtonID"
#define XML_PCS_ONTOGGLE L"OnToggle"
#define XML_PCS_OFFTOGGLE L"OffToggle"
#define XML_PCS_LOADTYPE L"LoadType"
#define XML_PCS_REPORTSTATE L"ReportState"
#define XML_PCS_LEDACTION L"LEDAction"
#define XML_PCS_LEDACTIONGROUP L"LEDActionGroup"
#define XML_PCS_LEDGROUPMASK L"LEDGroupMask"
#define XML_PCS_ROCKERACTIONS L"RockerActions"
#define XML_PCS_ROCKERACTION L"RockerAction"
#define XML_PCS_LEDBACKLIGHTING L"LEDBacklighting"
#define XML_PCS_LEDTRACKING L"LEDTracking"
#define XML_PCS_LEDOUTPUT L"LEDOutput"
#define XML_PCS_LEDBRIGHTNESS L"LEDBrightness"
#define XML_PCS_IRENABLED L"IREnabled"
#define XML_PCS_IRROOM L"IRRoom"

/*
	The class representing the driver as a whole.
*/
class ATL_NO_VTABLE CupbPCS : 
	public CComObjectRootEx<CComMultiThreadModel>,
	public CComCoClass<CupbPCS, &CLSID_upbPCS>,
	public CPremiseSubscriber
{
public:
	CComPtr<IPremiseObject> m_upbRoot;
	long m_upbSubscription;

	// CPremiseSubscriber overrides
	HRESULT OnBrokerAttach();
	HRESULT OnBrokerDetach();

	// IPremiseNotifyImpl overrides
	HRESULT STDMETHODCALLTYPE OnObjectCreated(IN long subscriptionID, IN long transactionID, IN long propagationID, IN long controlCode, IN IPremiseObject* ObjectCreated, IN IPremiseObject* Container, IN IPremiseObject* InsertBefore);
	HRESULT STDMETHODCALLTYPE OnPropertyChanged(IN long subscriptionID, IN long transactionID, IN long propagationID, IN long controlCode, IN IPremiseObject* Object, IN IPremiseObject* Property, IN VARIANT* NewValue, IN VARIANT* OldValue);

	DECLARE_REGISTRY_RESOURCEID(IDR_DRIVER)
	DECLARE_PROTECT_FINAL_CONSTRUCT()

	BEGIN_COM_MAP(CupbPCS)
		COM_INTERFACE_ENTRY(IPremiseNotify)
		COM_INTERFACE_ENTRY(IObjectWithSite)
	END_COM_MAP()

	HRESULT STDMETHODCALLTYPE OnUpbNetworkDataChanged(IN IPremiseObject* Object, IN VARIANT* newValue);
	HRESULT STDMETHODCALLTYPE OnDeviceInitializedChanged(IN IPremiseObject* Object, IN VARIANT* newValue);
	HRESULT STDMETHODCALLTYPE OnLedModeChanged(IN IPremiseObject* Object, IN VARIANT* newValue);
	HRESULT STDMETHODCALLTYPE OnLedOnColorChanged(IN IPremiseObject* Object, IN VARIANT* newValue);
	HRESULT STDMETHODCALLTYPE OnLedOffColorChanged(IN IPremiseObject* Object, IN VARIANT* newValue);
	HRESULT STDMETHODCALLTYPE OnEventPacketTypeChanged(IN IPremiseObject* Object, IN VARIANT* newValue);
	HRESULT STDMETHODCALLTYPE OnAckMessageRequestChanged(IN IPremiseObject* Object, IN VARIANT* newValue);
	HRESULT STDMETHODCALLTYPE OnIdPulseRequestChanged(IN IPremiseObject* Object, IN VARIANT* newValue);
	HRESULT STDMETHODCALLTYPE OnAckPulseRequestChanged(IN IPremiseObject* Object, IN VARIANT* newValue);
	HRESULT STDMETHODCALLTYPE OnTransmissionCountChanged(IN IPremiseObject* Object, IN VARIANT* newValue);
	HRESULT STDMETHODCALLTYPE OnLedBacklightingChanged(IN IPremiseObject* Object, IN VARIANT* newValue);
	HRESULT STDMETHODCALLTYPE OnLedTrackingChanged(IN IPremiseObject* Object, IN VARIANT* newValue);
	HRESULT STDMETHODCALLTYPE OnLedOutputChanged(IN IPremiseObject* Object, IN VARIANT* newValue);
	HRESULT STDMETHODCALLTYPE OnLedBrightnessChanged(IN IPremiseObject* Object, IN VARIANT* newValue);
	HRESULT STDMETHODCALLTYPE OnLoadTypeChanged(IN IPremiseObject* Object, IN VARIANT* newValue);
	HRESULT STDMETHODCALLTYPE OnReportStateChanged(IN IPremiseObject* Object, IN VARIANT* newValue);
	HRESULT STDMETHODCALLTYPE OnDefaultFadeRateChanged(IN IPremiseObject* Object, IN VARIANT* newValue);
	HRESULT STDMETHODCALLTYPE OnFadeRateChanged(IN IPremiseObject* Object, IN VARIANT* newValue);
	HRESULT STDMETHODCALLTYPE OnLightLevelChanged(IN IPremiseObject* Object, IN VARIANT* newValue);
	HRESULT STDMETHODCALLTYPE OnLastOnLevelChanged(IN IPremiseObject* Object, IN VARIANT* newValue);
	HRESULT STDMETHODCALLTYPE OnStatusChanged(IN IPremiseObject* Object, IN VARIANT* newValue);
	HRESULT STDMETHODCALLTYPE OnLinkIdChanged(IN IPremiseObject* Object, IN VARIANT* newValue);
	HRESULT STDMETHODCALLTYPE OnCommandChanged(IN IPremiseObject* Object, IN VARIANT* newValue);
	HRESULT STDMETHODCALLTYPE OnOnToggleChanged(IN IPremiseObject* Object, IN VARIANT* newValue);
	HRESULT STDMETHODCALLTYPE OnOffToggleChanged(IN IPremiseObject* Object, IN VARIANT* newValue);
	HRESULT STDMETHODCALLTYPE OnBrightnessChanged(IN IPremiseObject* Object, IN VARIANT* newValue);
	HRESULT STDMETHODCALLTYPE OnPowerStateChanged(IN IPremiseObject* Object, IN VARIANT* newValue);
	HRESULT STDMETHODCALLTYPE OnTriggerChanged(IN IPremiseObject* Object, IN VARIANT* newValue);
	HRESULT STDMETHODCALLTYPE OnVirtualizeChanged(IN IPremiseObject* Object, IN VARIANT* newValue);
	HRESULT STDMETHODCALLTYPE OnIrEnabledChanged(IN IPremiseObject* Object, IN VARIANT* newValue);
	HRESULT STDMETHODCALLTYPE OnIrRoomChanged(IN IPremiseObject* Object, IN VARIANT* newValue);
	HRESULT STDMETHODCALLTYPE OnPresetDimChanged(IN IPremiseObject* Object, IN VARIANT* newValue);
	HRESULT STDMETHODCALLTYPE OnUpbNetworkIdleChanged(IN IPremiseObject* Object, IN VARIANT* newValue);
	HRESULT STDMETHODCALLTYPE OnLedActionChanged(IN IPremiseObject* Object, IN VARIANT* newValue);
	HRESULT STDMETHODCALLTYPE OnLedActionGroupChanged(IN IPremiseObject* Object, IN VARIANT* newValue);
	HRESULT STDMETHODCALLTYPE OnLedGroupMaskChanged(IN IPremiseObject* Object, IN VARIANT* newValue);
};

#endif /* __UPBPCSDRIVER_H_ */
