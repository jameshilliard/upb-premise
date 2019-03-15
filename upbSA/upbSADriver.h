#ifndef __UPBSADRIVER_H_
#define __UPBSADRIVER_H_

#include "resource.h"
#include "..\upb\upbCommon.h"

#define NOBREAK
#define	SIZEOF_ARRAY(a) (sizeof(a)/sizeof(a[0]))

/*
	Byte align all structures as they are used to communicate with h/w.
*/
#pragma pack(push)
#pragma pack(1)

#define SA_REG_RECEIVECOMPONENTS 0x40
#define SA_REG_TRANSMITCOMPONENTS 0x70
#define SA_REG_CM_TRANSMITCOMPONENTS 0xA0
#define SA_REG_ROCKERACTIONS 0x7A
#define SA_REG_LEDOPTIONS 0x8B
#define SA_REG_CM_LEDOPTIONS 0xC1
#define SA_REG_ROCKERCONFIG 0x8C
#define SA_REG_DIMMEROPTIONS 0x8D
#define SA_REG_TXCONTROL 0x8E
#define SA_REG_CM_TXCONTROL 0xC0
#define SA_REG_CM_INITIALRELAYSTATE 0xC2
#define SA_REG_ROCKEROPTIONS 0x8F
#define SA_REG_TRANSMITCOMMANDS 0x90
#define SA_REG_USQ_TRANSMITCOMPONENTS 0xCA

#define SA_REG_TXCOMPONENTENTRY 5
#define SA_REG_CM_TXCOMPONENTENTRY 4
#define SA_REG_TXCOMMANDENTRY 3

#define SA_REG_RECEIVECOMPONENTS_BYTES 0x30
#define SA_REG_CM_RECEIVECOMPONENTS_BYTES 0x60
#define SA_REG_TRANSMITCOMPONENTS_BYTES 0x0A
#define SA_REG_CM_TRANSMITCOMPONENTS_BYTES 0x18
#define SA_REG_ROCKERACTIONS_BYTES 0x08
#define SA_REG_LEDOPTIONS_BYTES 0x01
#define SA_REG_ROCKERCONFIG_BYTES 0x01
#define SA_REG_DIMMEROPTIONS_BYTES 0x01
#define SA_REG_TXCONTROL_BYTES 0x01
#define SA_REG_ROCKEROPTIONS_BYTES 0x01
#define SA_REG_TRANSMITCOMMANDS_BYTES 0x2D
#define SA_REG_USQ_TRANSMITCOMPONENTS_BYTES 0x1E

#define SA_REG_RECEIVECOMPONENTENTRY 0x03
#define SA_REG_ROCKERACTIONENTRY 0x02
#define SA_REG_CM_RELAYCOMPONENTS (SA_REG_CM_RECEIVECOMPONENTS_BYTES / SA_REG_RECEIVECOMPONENTENTRY / SA_CM_RELAYS)

#define SA_MODULE_LEDMODE(l) (((l) >> 7) & 0x01)
#define SA_MODULE_LEDONCOLOR(l) (((l) >> 2) & 0x03)
#define SA_MODULE_LEDOFFCOLOR(l) ((l) & 0x03)
#define SA_MODULE_SETOPTION_LEDMODE(l, o) (l) |= (((o) & 0x01) << 7)
#define SA_MODULE_SETOPTION_LEDONCOLOR(l, o) (l) |= (((o) & 0x03) << 2)
#define SA_MODULE_SETOPTION_LEDOFFCOLOR(l, o) (l) |= ((o) & 0x03)

#define SA_MODULE_EVENTPACKETTYPE(l) ((l) & 0x80)
#define SA_MODULE_ACKMESSAGEREQUEST(l) ((l) & 0x40)
#define SA_MODULE_IDPULSEREQUEST(l) ((l) & 0x20)
#define SA_MODULE_ACKPULSEREQUEST(l) ((l) & 0x10)
#define SA_MODULE_TRANSMISSIONCOUNT(l) (((l) >> 2) & 0x03)
#define SA_MODULE_SETOPTION_EVENTPACKETTYPE(l, o) (l) |= ((o) ? 0x80 : 0)
#define SA_MODULE_SETOPTION_ACKMESSAGEREQUEST(l, o) (l) |= ((o) ? 0x40 : 0)
#define SA_MODULE_SETOPTION_IDPULSEREQUEST(l, o) (l) |= ((o) ? 0x20 : 0)
#define SA_MODULE_SETOPTION_ACKPULSEREQUEST(l, o) (l) |= ((o) ? 0x10 : 0)
#define SA_MODULE_SETOPTION_TRANSMISSIONCOUNT(l, o) (l) |= (((o) & 0x03) << 2)

#define SA_SWITCH_LOADTYPE(d) (((d) >> 7) & 0x01)
#define SA_SWITCH_REPORTSTATE(d) ((d) & 0x10)
#define SA_SWITCH_TRIGGERSWITCH(d) ((d) & 0x20)
#define SA_SWITCH_TRIGGERLASTLEVEL(d) ((d) & 0x40)
#define SA_SWITCH_DEFAULTFADERATE(d) ((d) & 0x0F)
#define SA_SWITCH_SETOPTION_LOADTYPE(d, o) (d) |= (((o) & 1) << 7)
#define SA_SWITCH_SETOPTION_REPORTSTATE(d, o) (d) |= ((o) ? 0x10 : 0)
#define SA_SWITCH_SETOPTION_TRIGGERSWITCH(d, o) (d) |= ((o) ? 0x20 : 0)
#define SA_SWITCH_SETOPTION_TRIGGERLASTLEVEL(d, o) (d) |= ((o) ? 0x40 : 0)
#define SA_SWITCH_SETOPTION_DEFAULTFADERATE(d, o) (d) |= ((o) & 0x0f)

#define SA_USQ_LOCALLOADCONNECT(d) ((d) & 0x80)
#define SA_USQ_LASTONLEVEL(d) ((d) & 0x40)
#define SA_USQ_TALLROCKER(d) ((d) & 0x20)
#define SA_USQ_LOCALLOADCONTROL(d) ((d) & 0x10)
#define SA_USQ_VARIANTSELECTION(d) ((d) & 0x0f)
#define SA_USQ_SETOPTION_LOCALLOADCONNECT(d, o) (d) |= ((o) ? 0x80 : 0)
#define SA_USQ_SETOPTION_LASTONLEVEL(d, o) (d) |= ((o) ? 0x40 : 0)
#define SA_USQ_SETOPTION_TALLROCKER(d, o) (d) |= ((o) ? 0x20 : 0)
#define SA_USQ_SETOPTION_LOCALLOADCONTROL(d, o) (d) |= ((o) ? 0x10 : 0)
#define SA_USQ_SETOPTION_VARIANTSELECTION(d, o) (d) |= ((o) & 0x0f)
#define SA_USQ_APPLY_VARIANTSELECTION(d, o) (d) = ((d) & 0xf0) | ((o) & 0x0f)

#define SA_ROCKEROPTION_US24 0 // Half Height Quad Rocker
#define SA_ROCKEROPTION_US22S 1 // Half Height Dual Rocker
#define SA_ROCKEROPTION_US23 2 // Half Height Triple Rocker
#define SA_ROCKEROPTION_US11 8 // Full Height Single Rocker
#define SA_ROCKEROPTION_US12 9 // Full Height Dual Rocker
#define SA_ROCKEROPTION_US21 10 // Full Height Single Rocker
#define SA_ROCKEROPTION_US22T 11 // Full Height Dual Rocker
#define SA_ROCKEROPTION_US25 12 // Single Rocker, Four Pushbuttons
#define SA_ROCKEROPTION_US26 13 // Dual Rocker, Four Pushbuttons
#define SA_ROCKEROPTION_US28 14 // Eight Pushbuttons
#define SA_ROCKEROPTION_US_UNDEFINED 15 // Default

#define SA_CM_CONTACTSTATE(d, c) (((d) >> (c - 1)) & 0x1)
#define SA_CM_SETOPTION_INITIALRELAYSTATE(d, r, o) (d) |= ((o) ? (1 << (r - 1)) : 0)

#define SA_USQ_BUTTONS 8

#define SA_CM_RELAYS 2
#define SA_CM_INPUTS 3

#define SA_CM_INPUT_MESSAGE_CLOSE 1
#define SA_CM_INPUT_MESSAGE_OPEN 2

#pragma pack(pop) /* pack(1) */

#define XML_UPBSA_SCHEMA_ROOT L"sys://Schema/upbSA"
#define XML_SA_UPBSABASE L"upbSABase"
#define XML_SA_LM1 L"LM1"
#define XML_SA_AM1 L"AM1"
#define XML_SA_USQ L"USQ"
#define XML_SA_CM01 L"CM01"
#define XML_SA_SWITCH L"Switch"
#define XML_SA_TRIGGERINGSWITCH L"TriggeringSwitch"
#define XML_SA_DIMMER L"Dimmer"

#define XML_SA_LOADTYPE L"LoadType"
#define XML_SA_REPORTSTATE L"ReportState"
#define XML_SA_TRIGGERSWITCH L"TriggerSwitch"
#define XML_SA_TRIGGERLASTLEVEL L"TriggerLastLevel"
#define XML_SA_ONTOGGLE L"OnToggle"
#define XML_SA_OFFTOGGLE L"OffToggle"
#define XML_SA_VARIANTSELECTION L"VariantSelection"
#define XML_SA_LOCALLOADCONNECT L"LocalLoadConnect"
#define XML_SA_LOCALLOADCONTROL L"LocalLoadControl"
#define XML_SA_KEYPADBUTTON L"KeypadButton"
#define XML_SA_BUTTONID L"ButtonID"
#define XML_SA_ROCKERACTION L"RockerAction"
#define XML_SA_RELAYCONTACT L"RelayContact"
#define XML_SA_RELAYID L"RelayID"
#define XML_SA_AUTORESETTIMEOUT L"AutoResetTimeout"
#define XML_SA_INPUTCONTACT L"InputContact"
#define XML_SA_INPUTID L"InputID"
#define XML_SA_INPUTTRANSMITCOMPONENT L"InputTransmitComponent"
#define XML_SA_RELAYRECEIVECOMPONENT L"RelayReceiveComponent"
#define XML_SA_INITIALSTATE L"InitialState"

#define XML_SYS_STATE L"State"
#define XML_SYS_INVERT L"Invert"
#define XML_SYS_AUTORESET L"AutoReset"

/*
	The class representing the driver as a whole.
*/
class ATL_NO_VTABLE CupbSA : 
	public CComObjectRootEx<CComMultiThreadModel>,
	public CComCoClass<CupbSA, &CLSID_upbSA>,
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

	BEGIN_COM_MAP(CupbSA)
		COM_INTERFACE_ENTRY(IPremiseNotify)
		COM_INTERFACE_ENTRY(IObjectWithSite)
	END_COM_MAP()

	HRESULT STDMETHODCALLTYPE OnUpbNetworkDataChanged(IN IPremiseObject* Object, IN VARIANT* newValue);
	HRESULT STDMETHODCALLTYPE OnDeviceInitializedChanged(IN IPremiseObject* Object, IN VARIANT* newValue);
	HRESULT STDMETHODCALLTYPE OnEventPacketTypeChanged(IN IPremiseObject* Object, IN VARIANT* newValue);
	HRESULT STDMETHODCALLTYPE OnAckMessageRequestChanged(IN IPremiseObject* Object, IN VARIANT* newValue);
	HRESULT STDMETHODCALLTYPE OnIdPulseRequestChanged(IN IPremiseObject* Object, IN VARIANT* newValue);
	HRESULT STDMETHODCALLTYPE OnAckPulseRequestChanged(IN IPremiseObject* Object, IN VARIANT* newValue);
	HRESULT STDMETHODCALLTYPE OnTransmissionCountChanged(IN IPremiseObject* Object, IN VARIANT* newValue);
	HRESULT STDMETHODCALLTYPE OnLoadTypeChanged(IN IPremiseObject* Object, IN VARIANT* newValue);
	HRESULT STDMETHODCALLTYPE OnDefaultFadeRateChanged(IN IPremiseObject* Object, IN VARIANT* newValue);
	HRESULT STDMETHODCALLTYPE OnFadeRateChanged(IN IPremiseObject* Object, IN VARIANT* newValue);
	HRESULT STDMETHODCALLTYPE OnReportStateChanged(IN IPremiseObject* Object, IN VARIANT* newValue);
	HRESULT STDMETHODCALLTYPE OnTriggerSwitchChanged(IN IPremiseObject* Object, IN VARIANT* newValue);
	HRESULT STDMETHODCALLTYPE OnTriggerLastLevelChanged(IN IPremiseObject* Object, IN VARIANT* newValue);
	HRESULT STDMETHODCALLTYPE OnLedModeChanged(IN IPremiseObject* Object, IN VARIANT* newValue);
	HRESULT STDMETHODCALLTYPE OnLedOnColorChanged(IN IPremiseObject* Object, IN VARIANT* newValue);
	HRESULT STDMETHODCALLTYPE OnLedOffColorChanged(IN IPremiseObject* Object, IN VARIANT* newValue);
	HRESULT STDMETHODCALLTYPE OnLightLevelChanged(IN IPremiseObject* Object, IN VARIANT* newValue);
	HRESULT STDMETHODCALLTYPE OnLastOnLevelChanged(IN IPremiseObject* Object, IN VARIANT* newValue);
	HRESULT STDMETHODCALLTYPE OnLinkIdChanged(IN IPremiseObject* Object, IN VARIANT* newValue);
	HRESULT STDMETHODCALLTYPE OnCommandChanged(IN IPremiseObject* Object, IN VARIANT* newValue);
	HRESULT STDMETHODCALLTYPE OnOnToggleChanged(IN IPremiseObject* Object, IN VARIANT* newValue);
	HRESULT STDMETHODCALLTYPE OnOffToggleChanged(IN IPremiseObject* Object, IN VARIANT* newValue);
	HRESULT STDMETHODCALLTYPE OnBrightnessChanged(IN IPremiseObject* Object, IN VARIANT* newValue);
	HRESULT STDMETHODCALLTYPE OnPowerStateChanged(IN IPremiseObject* Object, IN VARIANT* newValue);
	HRESULT STDMETHODCALLTYPE OnStateChanged(IN IPremiseObject* Object, IN VARIANT* newValue);
	HRESULT STDMETHODCALLTYPE OnInvertChanged(IN IPremiseObject* Object, IN VARIANT* newValue);
	HRESULT STDMETHODCALLTYPE OnAutoResetChanged(IN IPremiseObject* Object, IN VARIANT* newValue);
	HRESULT STDMETHODCALLTYPE OnInitialStateChanged(IN IPremiseObject* Object, IN VARIANT* newValue);
	HRESULT STDMETHODCALLTYPE OnVirtualizeChanged(IN IPremiseObject* Object, IN VARIANT* newValue);
	HRESULT STDMETHODCALLTYPE OnPresetDimChanged(IN IPremiseObject* Object, IN VARIANT* newValue);
	HRESULT STDMETHODCALLTYPE OnLocalLoadConnectChanged(IN IPremiseObject* Object, IN VARIANT* newValue);
	HRESULT STDMETHODCALLTYPE OnLocalLoadControlChanged(IN IPremiseObject* Object, IN VARIANT* newValue);
	HRESULT STDMETHODCALLTYPE OnVariantSelectionChanged(IN IPremiseObject* Object, IN VARIANT* newValue);
	HRESULT STDMETHODCALLTYPE OnUpbNetworkIdleChanged(IN IPremiseObject* Object, IN VARIANT* newValue);
};

#endif /* __UPBSADRIVER_H_ */
