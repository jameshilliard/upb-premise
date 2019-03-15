#include "stdafx.h"
#include <stdio.h>
#include <stdlib.h>
#include <IPTypes.h>
#include <iphlpapi.h>
#define STRSAFE_LIB
#include <strsafe.h>
#include "upbPCS.h"
#include "upbPCSDriver.h"

/*
	Set the new percentile value on the named property only if the
	current value, when rounded to the nearest integer, actually
	differs from the new value.
		Module - The node that represents virtually the h/w.
		PropertyName - The percentile property to update.
		DeviceValue - The new percentile value to apply to the
			property.
*/
void SetRoundedPercentileValue(
	IN IPremiseObject* Module,
	IN BSTR PropertyName,
	IN BYTE DeviceValue
)
{
	CComVariant PropertyValue;
	//
	// Determine if the rounded virtual value is really different than the
	// device value before updating the virtual value.
	//
	if (SUCCEEDED(Module->GetValue(PropertyName, &PropertyValue)) && (static_cast<BYTE>(static_cast<long>(PropertyValue.dblVal * 100.0 + 0.5)) != DeviceValue)) {
		Module->SetValueEx(SVCC_DRIVER | SVCC_NOTIFY, PropertyName, &CComVariant(static_cast<double>(DeviceValue) / 100.0));
	}
}

template <class T> static int parsehex_(
	IN LPCWSTR lpsz,
	OUT T& t
)
{
	t = 0;
	int nChar = sizeof(T) * 2;
	for (int i = 0; i < nChar; i++) {
		t = (t << 4) + HexCharToByte(*lpsz++);
	}
	return nChar;
}

/*
	Overrides CPremiseSubscriber::OnBrokerDetach in order to clean up
	any existing data.
	Return - S_OK.
*/
HRESULT CupbPCS::OnBrokerDetach(
)
{
	if (m_upbRoot) {
		m_upbRoot->UnsubscribeFromProperty(m_upbSubscription);
		m_upbRoot.Release();
	}
	return S_OK;
}

/*
	Overrides CPremiseSubscriber::OnBrokerAttach in order to initialize
	data.
	Return - Any error obtained while subscribing to properties.
*/
HRESULT CupbPCS::OnBrokerAttach(
)
{
	m_upbSubscription = 0;
	HRESULT hr = m_spSite->GetObject(XML_UPB_ROOT, &m_upbRoot);
	if (SUCCEEDED(hr)) {
		hr = m_upbRoot->SubscribeToProperty(NULL, GetUnknown(), &m_upbSubscription);
	}
	return hr;
}

/*
	Used by various device types to create a list of child objects
	under the root of the device, each with its own unique device
	identifier property.
		Parent - The object under which to create the child
			objects.
		StartID - The starting point for the sequential numbering of
			child device identifiers assigned to each child created. This
			is typically 0 or 1.
		Children - The number of children to create.
		Class - The full path to the class of child object to create.
		IdProperty - The name of the property to which the unique child
			identifier is assigned.
*/
void CreateChildrenWithIds(
	IN IPremiseObject* Parent,
	IN int StartID,
	IN int Children,
	IN const char** ChildNames OPTIONAL,
	IN const WCHAR* Class,
	IN const WCHAR* IdProperty
)
{
	Children += StartID;
	for (int Child = StartID; Child < Children; Child++) {
		CComPtr<IPremiseObject> ChildNode;
		if (SUCCEEDED(Parent->CreateObject(const_cast<WCHAR*>(Class), NULL, &ChildNode))) {
			ChildNode->SetValueEx(SVCC_DRIVER | SVCC_NOTIFY, const_cast<WCHAR*>(IdProperty), &CComVariant(Child));
			if (ChildNames) {
				ChildNode->SetValueEx(SVCC_DRIVER | SVCC_NOTIFY, XML_SYS_NAME, &CComVariant(ChildNames[Child - StartID]));
			}
			ChildNode = NULL;
		}
	}
}

void TransmitNetworkPacket(
	IN IPremiseObject* Module,
	IN BYTE mdid,
	IN LPCSTR Data = NULL OPTIONAL
)
{
	int DataLen = Data ? strlen(Data) / 2 : 0;
	char TransmitMessage[64];
	StringCchPrintfA(TransmitMessage, sizeof(TransmitMessage), "%02X%s", mdid, Data ? Data : "");
	Module->SetValueEx(SVCC_NOTIFY | SVCC_FORCE, XML_UPB_UPBNETWORKMESSAGE, &CComVariant(TransmitMessage));
}

static const char* ButtonStates[] = {
	"Release", "Press", "Hold", "DoubleTap"
};
static const char* TransmitCommandNames[] = {
	"0", "1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11", "12", "13", "14"
};
typedef struct {
	const char* ActionName;
	int ButtonState;
} ROCKERACTIONSMAP;
static const ROCKERACTIONSMAP RockerActionsMap[] = {
	{"Press", SYS_BUTTON_STATE_PRESS},
	{"DoubleTap", SYS_BUTTON_STATE_DOUBLETAP}
};

void InitializeCommonChildObjects(
	IN IPremiseObject* Module,
	IN int Buttons,
	IN const char** ButtonNames,
	IN const BSTR ButtonClass,
	IN int TransmitCommands
)
{
	CreateChildrenWithIds(Module, 1, Buttons, ButtonNames, ButtonClass, XML_PCS_BUTTONID);
	CComPtr<IPremiseObjectCollection> ButtonCol;
	if (SUCCEEDED(Module->GetObjectsByType(CComVariant(ButtonClass), collectionTypeNoRecurse, &ButtonCol))) {
		long EnumButton = 0;
		CComPtr<IPremiseObject> Button;
		for (; SUCCEEDED(ButtonCol->GetItemEx(EnumButton, &Button)); EnumButton++, Button = NULL) {
			CComPtr<IPremiseObject> TransmitComponent;
			if (SUCCEEDED(Button->CreateObject(XML_UPB_SCHEMA_ROOT XML_SYS_PATH_SEPARATOR XML_UPB_TRANSMITCOMPONENT, NULL, &TransmitComponent))) {
				CreateChildrenWithIds(TransmitComponent, 0, SIZEOF_ARRAY(ButtonStates), ButtonStates, XML_UPBPCS_SCHEMA_ROOT XML_SYS_PATH_SEPARATOR XML_UPB_COMMANDID, XML_SYS_BUTTONSTATE);
			}			
		}
	}
	CComPtr<IPremiseObject> ChildNode;
	if (SUCCEEDED(Module->CreateObject(XML_UPB_SCHEMA_ROOT XML_SYS_PATH_SEPARATOR XML_UPB_TRANSMITCOMMANDS, NULL, &ChildNode))) {
		CreateChildrenWithIds(ChildNode, 0, TransmitCommands, TransmitCommandNames, XML_UPB_SCHEMA_ROOT XML_SYS_PATH_SEPARATOR XML_UPB_TRANSMITCOMMAND, XML_UPB_COMMANDID);
	}
}

void InitializeKeypadChildObjects(
	IN IPremiseObject* Module,
	IN int Buttons,
	IN int ReceiveComponents,
	IN const char** ButtonNames
)
{
	InitializeCommonChildObjects(Module, Buttons, ButtonNames, XML_UPBPCS_SCHEMA_ROOT XML_SYS_PATH_SEPARATOR XML_PCS_KEYPADBUTTON, PCS_KEYPAD_COMMANDIDS);
	CComPtr<IPremiseObjectCollection> ButtonCol;
	if (SUCCEEDED(Module->GetObjectsByType(CComVariant(XML_UPBPCS_SCHEMA_ROOT XML_SYS_PATH_SEPARATOR XML_PCS_KEYPADBUTTON), collectionTypeNoRecurse, &ButtonCol))) {
		long EnumButton = 0;
		CComPtr<IPremiseObject> Button;
		for (; SUCCEEDED(ButtonCol->GetItemEx(EnumButton, &Button)); EnumButton++, Button = NULL) {
			CComVariant ButtonID;
			if (SUCCEEDED(Button->GetValue(XML_PCS_BUTTONID, &ButtonID)) && (ButtonID.lVal > 0) && (ButtonID.lVal <= ReceiveComponents)) {
				CComPtr<IPremiseObject> Component;
				Button->CreateObject(XML_UPB_SCHEMA_ROOT XML_SYS_PATH_SEPARATOR XML_UPB_RECEIVECOMPONENT, NULL, &Component);
				CComPtr<IPremiseObject> LedOnAction;
				Button->CreateObject(XML_UPBPCS_SCHEMA_ROOT XML_SYS_PATH_SEPARATOR XML_PCS_LEDONACTION, NULL, &LedOnAction);
			}
		}
	}
}

void InitializeSwitchChildObjects(
	IN IPremiseObject* Module,
	IN int Buttons,
	IN int ReceiveComponents,
	IN const char** ButtonNames
)
{
	InitializeCommonChildObjects(Module, Buttons, ButtonNames, XML_UPBPCS_SCHEMA_ROOT XML_SYS_PATH_SEPARATOR XML_PCS_SWITCHBUTTON, PCS_SWITCH_COMMANDIDS);
	CComPtr<IPremiseObjectCollection> ButtonCol;
	if (SUCCEEDED(Module->GetObjectsByType(CComVariant(XML_UPBPCS_SCHEMA_ROOT XML_SYS_PATH_SEPARATOR XML_PCS_SWITCHBUTTON), collectionTypeNoRecurse, &ButtonCol))) {
		long EnumButton = 0;
		CComPtr<IPremiseObject> Button;
		for (; SUCCEEDED(ButtonCol->GetItemEx(EnumButton, &Button)); EnumButton++, Button = NULL) {
			for (int Child = 0; Child < SIZEOF_ARRAY(RockerActionsMap); Child++) {
				CComPtr<IPremiseObject> ChildNode;
				if (SUCCEEDED(Button->CreateObject(XML_UPBPCS_SCHEMA_ROOT XML_SYS_PATH_SEPARATOR XML_PCS_ROCKERACTION, NULL, &ChildNode))) {
					ChildNode->SetValueEx(SVCC_DRIVER | SVCC_NOTIFY, XML_SYS_BUTTONSTATE, &CComVariant(RockerActionsMap[Child].ButtonState));
					ChildNode->SetValueEx(SVCC_DRIVER | SVCC_NOTIFY, XML_SYS_NAME, &CComVariant(RockerActionsMap[Child].ActionName));
					ChildNode = NULL;
				}
			}
		}
	}
	CComPtr<IPremiseObject> ChildNode;
	if (SUCCEEDED(Module->CreateObject(XML_UPB_SCHEMA_ROOT XML_SYS_PATH_SEPARATOR XML_UPB_SWITCHRECEIVECOMPONENTS, NULL, &ChildNode))) {
		CreateChildrenWithIds(ChildNode, 1, ReceiveComponents, NULL, XML_UPB_SCHEMA_ROOT XML_SYS_PATH_SEPARATOR XML_UPB_SWITCHRECEIVECOMPONENT, XML_UPB_COMPONENTID);
		ChildNode = NULL;
	}
}

static const char* C6[] = {
	"On", "Off", "A", "B", "C", "D", "Up", "Down"
};

static const char* C8[] = {
	"E", "F", "G", "H", "I", "J", "K", "L"
};

static const char* Ws[] = {
	"Top", "Bottom"
};

HRESULT STDMETHODCALLTYPE CupbPCS::OnObjectCreated(
	IN long subscriptionID,
	IN long transactionID,
	IN long propagationID,
	IN long controlCode,
	IN IPremiseObject* ObjectCreated,
	IN IPremiseObject* Container,
	IN IPremiseObject* InsertBefore
)
{
	if (!(controlCode & SVCC_PASTE)) {
		if (IsObjectOfExplicitType(ObjectCreated, XML_UPBPCS_SCHEMA_ROOT XML_SYS_PATH_SEPARATOR XML_PCS_WS1D6)) {
			InitializeSwitchChildObjects(ObjectCreated, 2, PCS_WSD1_RECEIVECOMPONENTS, Ws);
		} else if (IsObjectOfExplicitType(ObjectCreated, XML_UPBPCS_SCHEMA_ROOT XML_SYS_PATH_SEPARATOR XML_PCS_WMC6)) {
			InitializeKeypadChildObjects(ObjectCreated, 8, 6, C6);
		} else if (IsObjectOfExplicitType(ObjectCreated, XML_UPBPCS_SCHEMA_ROOT XML_SYS_PATH_SEPARATOR XML_PCS_DTC6)) {
			InitializeKeypadChildObjects(ObjectCreated, 8, 6, C6);
		} else if (IsObjectOfExplicitType(ObjectCreated, XML_UPBPCS_SCHEMA_ROOT XML_SYS_PATH_SEPARATOR XML_PCS_WMC8)) {
			InitializeKeypadChildObjects(ObjectCreated, 8, 8, C8);
		} else if (IsObjectOfExplicitType(ObjectCreated, XML_UPBPCS_SCHEMA_ROOT XML_SYS_PATH_SEPARATOR XML_PCS_DTC8)) {
			InitializeKeypadChildObjects(ObjectCreated, 8, 8, C8);
		} else if (IsObjectOfExplicitType(ObjectCreated, XML_UPBPCS_SCHEMA_ROOT XML_SYS_PATH_SEPARATOR XML_PCS_SERIALPIM)) {
			;
		}
	}
	return S_OK;
}

static const BYTE PremiseButtonStates[] = {SYS_BUTTON_STATE_DOUBLETAP, SYS_BUTTON_STATE_RELEASE, SYS_BUTTON_STATE_HOLD, SYS_BUTTON_STATE_PRESS};

void PopulateRockerActions(
	IN IPremiseObject* Module,
	IN const WCHAR* RegisterData,
	IN int Actions
)
{
	CComPtr<IPremiseObjectCollection> Buttons;
	if (SUCCEEDED(Module->GetObjectsByType(CComVariant(XML_UPBPCS_SCHEMA_ROOT XML_SYS_PATH_SEPARATOR XML_PCS_SWITCHBUTTON), collectionTypeNoRecurse, &Buttons))) {
		long EnumButton = 0;
		CComPtr<IPremiseObject> Button;
		for (; SUCCEEDED(Buttons->GetItemEx(EnumButton, &Button)); EnumButton++, Button = NULL) {
			CComVariant ButtonID;
			if (SUCCEEDED(Button->GetValue(XML_PCS_BUTTONID, &ButtonID)) && (ButtonID.lVal > 0) && (ButtonID.lVal <= Actions / 2)) {
				CComPtr<IPremiseObjectCollection> RockerActions;
				if (SUCCEEDED(Button->GetObjectsByType(CComVariant(XML_UPBPCS_SCHEMA_ROOT XML_SYS_PATH_SEPARATOR XML_PCS_ROCKERACTION), collectionTypeNoRecurse, &RockerActions))) {
					long EnumRockerAction = 0;
					CComPtr<IPremiseObject> RockerAction;
					for (; SUCCEEDED(RockerActions->GetItemEx(EnumRockerAction, &RockerAction)); EnumRockerAction++, RockerAction = NULL) {
						CComVariant ButtonState;
						if (SUCCEEDED(RockerAction->GetValue(XML_SYS_BUTTONSTATE, &ButtonState))) {
							int Offset = 2 * 2 * PCS_SWITCH_REG_ROCKERACTIONENTRY * (ButtonID.lVal - 1);
							BYTE LightLevel;
							switch (ButtonState.lVal) {
							case SYS_BUTTON_STATE_PRESS:
								parsehex_(RegisterData + Offset, LightLevel);
								//
								// If this is a top side rocker action
								// then pass on the value to the
								// PresetDim property.
								//
								if (ButtonID.lVal & 1) {
									if (LightLevel <= 100) {
										Module->SetValueEx(SVCC_DRIVER | SVCC_NOTIFY, XML_SYS_PRESETDIM, &CComVariant(static_cast<double>(LightLevel) / 100.0));
									}
								}
								break;
							case SYS_BUTTON_STATE_DOUBLETAP:
								Offset += 2 * PCS_SWITCH_REG_ROCKERACTIONENTRY;
								parsehex_(RegisterData + Offset, LightLevel);
								break;
							default:
								continue;
							}
							RockerAction->SetValueEx(SVCC_DRIVER | SVCC_NOTIFY, XML_UPB_LASTONLEVEL, &CComVariant(LightLevel > 100));
							if (LightLevel <= 100) {
								RockerAction->SetValueEx(SVCC_DRIVER | SVCC_NOTIFY, XML_UPB_LIGHTLEVEL, &CComVariant(static_cast<double>(LightLevel) / 100.0));
							}
							BYTE FadeRate;
							parsehex_(RegisterData + Offset + 2, FadeRate);
							RockerAction->SetValueEx(SVCC_DRIVER | SVCC_NOTIFY, XML_UPB_FADERATE, &CComVariant(FadeRate));
						}
					}
				}
			}
		}
	}
}

void PopulateSwitchReceiveComponents(
	IN IPremiseObject* Module,
	IN const WCHAR* RegisterData
)
{
	CComPtr<IPremiseObjectCollection> ComponentsCol;
	CComPtr<IPremiseObject> Components;
	if (SUCCEEDED(Module->GetObjectsByType(CComVariant(XML_UPB_SCHEMA_ROOT XML_SYS_PATH_SEPARATOR XML_UPB_SWITCHRECEIVECOMPONENTS), collectionTypeNoRecurse, &ComponentsCol)) && SUCCEEDED(ComponentsCol->GetItemEx(0, &Components))) {
		for (int ComponentID = 1; ComponentID <= PCS_SWITCH_REG_RECEIVECOMPONENTS_BYTES / PCS_SWITCH_REG_RXCOMPONENTENTRY; ComponentID++) {
			BYTE LinkID;
			RegisterData += parsehex_(RegisterData, LinkID);
			BYTE LightLevel;
			RegisterData += parsehex_(RegisterData, LightLevel);
			BYTE FadeRate;
			RegisterData += parsehex_(RegisterData, FadeRate);
			CComPtr<IPremiseObjectCollection> ComponentCol;
			CComPtr<IPremiseObject> Component;
			if (SUCCEEDED(Components->GetObjectsByTypeAndPropertyValue(XML_UPB_SCHEMA_ROOT XML_SYS_PATH_SEPARATOR XML_UPB_SWITCHRECEIVECOMPONENT, XML_UPB_COMPONENTID, CComVariant(ComponentID), collectionTypeNoRecurse, &ComponentCol)) && SUCCEEDED(ComponentCol->GetItemEx(0, &Component))) {
				Component->SetValueEx(SVCC_DRIVER | SVCC_NOTIFY, XML_UPB_LINKID, &CComVariant(LinkID));
				Component->SetValueEx(SVCC_DRIVER | SVCC_NOTIFY, XML_UPB_LASTONLEVEL, &CComVariant(LightLevel > 100));
				if (LightLevel <= 100) {
					Component->SetValueEx(SVCC_DRIVER | SVCC_NOTIFY, XML_UPB_LIGHTLEVEL, &CComVariant(static_cast<double>(LightLevel) / 100.0));
				}
				Component->SetValueEx(SVCC_DRIVER | SVCC_NOTIFY, XML_UPB_FADERATE, &CComVariant(FadeRate));
			}
			ComponentCol = NULL;
			Component = NULL;
		}
	}
}

void PopulateKeypadReceiveComponents(
	IN IPremiseObject* Module,
	IN const WCHAR* RegisterData
)
{
	CComPtr<IPremiseObjectCollection> Components;
	if (SUCCEEDED(Module->GetObjectsByType(CComVariant(XML_UPB_SCHEMA_ROOT XML_SYS_PATH_SEPARATOR XML_UPB_RECEIVECOMPONENT), collectionTypeRecurse, &Components))) {
		long EnumComponent = 0;
		CComPtr<IPremiseObject> Component;
		for (; SUCCEEDED(Components->GetItemEx(EnumComponent, &Component)); EnumComponent++, Component = NULL) {
			CComPtr<IPremiseObject> Button;
			CComVariant ButtonID;
			if (SUCCEEDED(Component->get_Parent(&Button)) && SUCCEEDED(Button->GetValue(XML_PCS_BUTTONID, &ButtonID)) && (ButtonID.lVal > 0) && (ButtonID.lVal <= PCS_KEYPAD_REG_RECEIVECOMPONENTS_BYTES)) {
				BYTE LinkID;
				parsehex_(RegisterData + 2 * (ButtonID.lVal - 1), LinkID);
				Component->SetValueEx(SVCC_DRIVER | SVCC_NOTIFY, XML_UPB_LINKID, &CComVariant(LinkID));
			}
			Button = NULL;
		}
	}
}

void PopulateTransmitComponents(
	IN IPremiseObject* Module,
	IN const WCHAR* RegisterData,
	IN int Buttons,
	IN const BSTR ButtonClass
)
{
	CComPtr<IPremiseObjectCollection> ButtonCol;
	if (SUCCEEDED(Module->GetObjectsByType(CComVariant(ButtonClass), collectionTypeNoRecurse, &ButtonCol))) {
		long EnumButton = 0;
		CComPtr<IPremiseObject> Button;
		for (; SUCCEEDED(ButtonCol->GetItemEx(EnumButton, &Button)); EnumButton++, Button = NULL) {
			CComVariant ButtonID;
			CComPtr<IPremiseObjectCollection> ComponentCol;
			CComPtr<IPremiseObject> Component;
			if (SUCCEEDED(Button->GetValue(XML_PCS_BUTTONID, &ButtonID)) && (ButtonID.lVal > 0) && (ButtonID.lVal <= Buttons) && SUCCEEDED(Button->GetObjectsByType(CComVariant(XML_UPB_SCHEMA_ROOT XML_SYS_PATH_SEPARATOR XML_UPB_TRANSMITCOMPONENT), collectionTypeNoRecurse, &ComponentCol)) && SUCCEEDED(ComponentCol->GetItemEx(0, &Component))) {
				int Offset = 2 * PCS_REG_TXCOMPONENTENTRY * (ButtonID.lVal - 1);
				BYTE LinkID;
				parsehex_(RegisterData + Offset, LinkID);
				Offset += 2;
				Component->SetValueEx(SVCC_DRIVER | SVCC_NOTIFY, XML_UPB_LINKID, &CComVariant(LinkID));
				CComPtr<IPremiseObjectCollection> CommandIDs;
				if (SUCCEEDED(Component->GetObjectsByType(CComVariant(XML_UPBPCS_SCHEMA_ROOT XML_SYS_PATH_SEPARATOR XML_UPB_COMMANDID), collectionTypeNoRecurse, &CommandIDs))) {
					long EnumCommandID = 0;
					CComPtr<IPremiseObject> CommandID;
					for (; SUCCEEDED(CommandIDs->GetItemEx(EnumCommandID, &CommandID)); EnumCommandID++, CommandID = NULL) {
						CComVariant ButtonState;
						if (SUCCEEDED(CommandID->GetValue(XML_SYS_BUTTONSTATE, &ButtonState)) && (ButtonState.lVal >= SYS_BUTTON_STATE_RELEASE) && (ButtonState.lVal <= SYS_BUTTON_STATE_DOUBLETAP)) {
							BYTE Toggle;
							parsehex_(RegisterData + Offset + 2 * PremiseButtonStates[ButtonState.lVal], Toggle);
							CommandID->SetValueEx(SVCC_DRIVER | SVCC_NOTIFY, XML_PCS_ONTOGGLE, &CComVariant((Toggle & 0xf0) >> 4));
							CommandID->SetValueEx(SVCC_DRIVER | SVCC_NOTIFY, XML_PCS_OFFTOGGLE, &CComVariant(Toggle & 0x0f));
						}
					}
				}
			}
			ComponentCol = NULL;
			Component = NULL;
		}
	}
}

void PopulateTransmitCommands(
	IN IPremiseObject* Module,
	IN const WCHAR* RegisterData,
	IN int Commands
)
{
	CComPtr<IPremiseObjectCollection> ComponentsCol;
	CComPtr<IPremiseObject> Components;
	if (SUCCEEDED(Module->GetObjectsByType(CComVariant(XML_UPB_SCHEMA_ROOT XML_SYS_PATH_SEPARATOR XML_UPB_TRANSMITCOMMANDS), collectionTypeNoRecurse, &ComponentsCol)) && SUCCEEDED(ComponentsCol->GetItemEx(0, &Components))) {
		for (int CommandID = 0; CommandID < Commands; CommandID++) {
			CComPtr<IPremiseObjectCollection> ComponentCol;
			CComPtr<IPremiseObject> Component;
			if (SUCCEEDED(Components->GetObjectsByTypeAndPropertyValue(XML_UPB_SCHEMA_ROOT XML_SYS_PATH_SEPARATOR XML_UPB_TRANSMITCOMMAND, XML_UPB_COMMANDID, CComVariant(CommandID), collectionTypeNoRecurse, &ComponentCol)) && SUCCEEDED(ComponentCol->GetItemEx(0, &Component))) {
				WCHAR Command[2 * PCS_REG_TXCOMMANDENTRY + 1];
				StringCchCopyW(Command, SIZEOF_ARRAY(Command), RegisterData);
				Component->SetValueEx(SVCC_DRIVER | SVCC_NOTIFY, XML_UPB_COMMAND, &CComVariant(Command));
			}
			ComponentCol = NULL;
			Component = NULL;
			RegisterData += 2 * PCS_REG_TXCOMMANDENTRY;
		}
	}
}

void PopulateSwitchLedOptions(
	IN IPremiseObject* Module,
	IN const WCHAR* RegisterData
)
{
	BYTE LedOptions;
	parsehex_(RegisterData, LedOptions);
	Module->SetValueEx(SVCC_DRIVER | SVCC_NOTIFY, XML_UPB_LEDMODE, &CComVariant(PCS_SWITCH_LEDMODE(LedOptions)));
	Module->SetValueEx(SVCC_DRIVER | SVCC_NOTIFY, XML_UPB_LEDONCOLOR, &CComVariant(PCS_SWITCH_LEDONCOLOR(LedOptions)));
	Module->SetValueEx(SVCC_DRIVER | SVCC_NOTIFY, XML_UPB_LEDOFFCOLOR, &CComVariant(PCS_SWITCH_LEDOFFCOLOR(LedOptions)));
}

void SetSwitchLedOptions(
	IN IPremiseObject* Module,
	IN VARIANT* LedMode OPTIONAL,
	IN VARIANT* LedOnColor OPTIONAL,
	IN VARIANT* LedOffColor OPTIONAL
)
{
	BYTE LedOptions = 0;
	CComVariant Option;
	if (!LedMode && SUCCEEDED(Module->GetValue(XML_UPB_LEDMODE, &Option))) {
		LedMode = &Option;
	}
	if (LedMode) {
		PCS_SWITCH_SETOPTION_LEDMODE(LedOptions, LedMode->lVal);
	}
	if (!LedOnColor && SUCCEEDED(Module->GetValue(XML_UPB_LEDONCOLOR, &Option))) {
		LedOnColor = &Option;
	}
	if (LedOnColor) {
		PCS_SWITCH_SETOPTION_LEDONCOLOR(LedOptions, LedOnColor->lVal);
	}
	if (!LedOffColor && SUCCEEDED(Module->GetValue(XML_UPB_LEDOFFCOLOR, &Option))) {
		LedOffColor = &Option;
	}
	if (LedOffColor) {
		PCS_SWITCH_SETOPTION_LEDOFFCOLOR(LedOptions, LedOffColor->lVal);
	}
	char data[16];
	StringCchPrintfA(data, SIZEOF_ARRAY(data), "%02X%02X%02X", MAKE_MDID(MDID_CORE_COMMANDS, MDID_CORE_COMMAND_SETREGISTERVALUES), PCS_SWITCH_REG_LEDOPTIONS, LedOptions);
	Module->SetValueEx(SVCC_NOTIFY | SVCC_FORCE, XML_UPB_UPBNETWORKMESSAGE, &CComVariant(data));
}

void PopulateKeypadLedOptions(
	IN IPremiseObject* Module,
	IN const WCHAR* RegisterData
)
{
	BYTE LedOptions;
	parsehex_(RegisterData, LedOptions);
	Module->SetValueEx(SVCC_DRIVER | SVCC_NOTIFY, XML_PCS_LEDBACKLIGHTING, &CComVariant(PCS_KEYPAD_LEDBACKLIGHTING(LedOptions)));
	Module->SetValueEx(SVCC_DRIVER | SVCC_NOTIFY, XML_PCS_LEDTRACKING, &CComVariant(PCS_KEYPAD_LEDTRACKING(LedOptions)));
	Module->SetValueEx(SVCC_DRIVER | SVCC_NOTIFY, XML_PCS_LEDOUTPUT, &CComVariant(PCS_KEYPAD_LEDOUTPUT(LedOptions)));
	Module->SetValueEx(SVCC_DRIVER | SVCC_NOTIFY, XML_PCS_LEDBRIGHTNESS, &CComVariant(PCS_KEYPAD_LEDBRIGHTNESS(LedOptions)));
}

void SetKeypadLedOptions(
	IN IPremiseObject* Module,
	IN VARIANT* LedBacklighting OPTIONAL,
	IN VARIANT* LedTracking OPTIONAL,
	IN VARIANT* LedOutput OPTIONAL,
	IN VARIANT* LedBrightness OPTIONAL
)
{
	BYTE LedOptions = 0;
	CComVariant Option;
	if (!LedBacklighting && SUCCEEDED(Module->GetValue(XML_PCS_LEDBACKLIGHTING, &Option))) {
		LedBacklighting = &Option;
	}
	if (LedBacklighting) {
		PCS_KEYPAD_SETOPTION_LEDBACKLIGHTING(LedOptions, LedBacklighting->boolVal);
	}
	if (!LedTracking && SUCCEEDED(Module->GetValue(XML_PCS_LEDTRACKING, &Option))) {
		LedTracking = &Option;
	}
	if (LedTracking) {
		PCS_KEYPAD_SETOPTION_LEDTRACKING(LedOptions, LedTracking->boolVal);
	}
	if (!LedOutput && SUCCEEDED(Module->GetValue(XML_PCS_LEDOUTPUT, &Option))) {
		LedOutput = &Option;
	}
	if (LedOutput) {
		PCS_KEYPAD_SETOPTION_LEDOUTPUT(LedOptions, LedOutput->boolVal);
	}
	if (!LedBrightness && SUCCEEDED(Module->GetValue(XML_PCS_LEDBRIGHTNESS, &Option))) {
		LedBrightness = &Option;
	}
	if (LedBrightness) {
		PCS_KEYPAD_SETOPTION_LEDBRIGHTNESS(LedOptions, LedBrightness->lVal);
	}
	char data[16];
	StringCchPrintfA(data, SIZEOF_ARRAY(data), "%02X%02X%02X", MAKE_MDID(MDID_CORE_COMMANDS, MDID_CORE_COMMAND_SETREGISTERVALUES), PCS_KEYPAD_REG_LEDOPTIONS, LedOptions);
	Module->SetValueEx(SVCC_NOTIFY | SVCC_FORCE, XML_UPB_UPBNETWORKMESSAGE, &CComVariant(data));
}

void PopulateIrOptions(
	IN IPremiseObject* Module,
	IN const WCHAR* RegisterData
)
{
	BYTE IrOptions;
	parsehex_(RegisterData, IrOptions);
	Module->SetValueEx(SVCC_DRIVER | SVCC_NOTIFY, XML_PCS_IRENABLED, &CComVariant(PCS_KEYPAD_IRENABLED(IrOptions)));
	Module->SetValueEx(SVCC_DRIVER | SVCC_NOTIFY, XML_PCS_IRROOM, &CComVariant(PCS_KEYPAD_IRROOM(IrOptions)));
}

void SetIrOptions(
	IN IPremiseObject* Module,
	IN VARIANT* IrEnabled OPTIONAL,
	IN VARIANT* IrRoom OPTIONAL
)
{
	BYTE IrOptions = 0;
	CComVariant Option;
	if (!IrEnabled && SUCCEEDED(Module->GetValue(XML_PCS_IRENABLED, &Option))) {
		IrEnabled = &Option;
	}
	if (IrEnabled) {
		PCS_KEYPAD_SETOPTION_IRENABLED(IrOptions, IrEnabled->boolVal);
	}
	if (!IrRoom && SUCCEEDED(Module->GetValue(XML_PCS_IRROOM, &Option))) {
		IrRoom = &Option;
	}
	if (IrRoom) {
		PCS_KEYPAD_SETOPTION_IRROOM(IrOptions, IrRoom->ulVal);
	}
	char data[16];
	StringCchPrintfA(data, SIZEOF_ARRAY(data), "%02X%02X%02X", MAKE_MDID(MDID_CORE_COMMANDS, MDID_CORE_COMMAND_SETREGISTERVALUES), PCS_KEYPAD_REG_IROPTIONS, IrOptions);
	Module->SetValueEx(SVCC_NOTIFY | SVCC_FORCE, XML_UPB_UPBNETWORKMESSAGE, &CComVariant(data));
}

void PopulateDimmerOptions(
	IN IPremiseObject* Module,
	IN const WCHAR* RegisterData
)
{
	BYTE DimmerOptions;
	parsehex_(RegisterData, DimmerOptions);
	Module->SetValueEx(SVCC_DRIVER | SVCC_NOTIFY, XML_PCS_LOADTYPE, &CComVariant(PCS_SWITCH_LOADTYPE(DimmerOptions)));
	Module->SetValueEx(SVCC_DRIVER | SVCC_NOTIFY, XML_PCS_REPORTSTATE, &CComVariant(PCS_SWITCH_REPORTSTATE(DimmerOptions)));
	Module->SetValueEx(SVCC_DRIVER | SVCC_NOTIFY, XML_UPB_DEFAULTFADERATE, &CComVariant(PCS_SWITCH_DEFAULTFADERATE(DimmerOptions)));
}

void SetDimmerOptions(
	IN IPremiseObject* Module,
	IN VARIANT* LoadType OPTIONAL,
	IN VARIANT* ReportState OPTIONAL,
	IN VARIANT* DefaultFadeRate OPTIONAL
)
{
	BYTE DimmerOptions = 0;
	CComVariant Option;
	if (!LoadType && SUCCEEDED(Module->GetValue(XML_PCS_LOADTYPE, &Option))) {
		LoadType = &Option;
	}
	if (LoadType) {
		PCS_SWITCH_SETOPTION_LOADTYPE(DimmerOptions, LoadType->lVal);
	}
	if (!ReportState && SUCCEEDED(Module->GetValue(XML_PCS_REPORTSTATE, &Option))) {
		ReportState = &Option;
	}
	if (ReportState) {
		PCS_SWITCH_SETOPTION_REPORTSTATE(DimmerOptions, ReportState->boolVal);
	}
	if (!DefaultFadeRate && SUCCEEDED(Module->GetValue(XML_UPB_DEFAULTFADERATE, &Option))) {
		DefaultFadeRate = &Option;
	}
	if (DefaultFadeRate) {
		PCS_SWITCH_SETOPTION_DEFAULTFADERATE(DimmerOptions, DefaultFadeRate->lVal);
	}
	char data[16];
	StringCchPrintfA(data, SIZEOF_ARRAY(data), "%02X%02X%02X", MAKE_MDID(MDID_CORE_COMMANDS, MDID_CORE_COMMAND_SETREGISTERVALUES), PCS_SWITCH_REG_DIMMEROPTIONS, DimmerOptions);
	Module->SetValueEx(SVCC_NOTIFY | SVCC_FORCE, XML_UPB_UPBNETWORKMESSAGE, &CComVariant(data));
}

void PopulateTransmissionOptions(
	IN IPremiseObject* Module,
	IN const WCHAR* RegisterData
)
{
	BYTE TransmissionOptions;
	parsehex_(RegisterData, TransmissionOptions);
	Module->SetValueEx(SVCC_DRIVER | SVCC_NOTIFY, XML_UPB_EVENTPACKETTYPE, &CComVariant(PCS_MODULE_EVENTPACKETTYPE(TransmissionOptions)));
	Module->SetValueEx(SVCC_DRIVER | SVCC_NOTIFY, XML_UPB_ACKMESSAGEREQUEST, &CComVariant(PCS_MODULE_ACKMESSAGEREQUEST(TransmissionOptions)));
	Module->SetValueEx(SVCC_DRIVER | SVCC_NOTIFY, XML_UPB_IDPULSEREQUEST, &CComVariant(PCS_MODULE_IDPULSEREQUEST(TransmissionOptions)));
	Module->SetValueEx(SVCC_DRIVER | SVCC_NOTIFY, XML_UPB_ACKPULSEREQUEST, &CComVariant(PCS_MODULE_ACKPULSEREQUEST(TransmissionOptions)));
	Module->SetValueEx(SVCC_DRIVER | SVCC_NOTIFY, XML_UPB_TRANSMISSIONCOUNT, &CComVariant(PCS_MODULE_TRANSMISSIONCOUNT(TransmissionOptions)));
}

void SetTransmissionOptions(
	IN IPremiseObject* Module,
	IN VARIANT* EventPacketType OPTIONAL,
	IN VARIANT* AckMessageRequest OPTIONAL,
	IN VARIANT* IdPulseRequest OPTIONAL,
	IN VARIANT* AckPulseRequest OPTIONAL,
	IN VARIANT* TransmissionCount OPTIONAL
)
{
	BYTE TransmissionOptions = 0;
	CComVariant Option;
	if (!EventPacketType && SUCCEEDED(Module->GetValue(XML_UPB_EVENTPACKETTYPE, &Option))) {
		EventPacketType = &Option;
	}
	if (EventPacketType) {
		PCS_MODULE_SETOPTION_EVENTPACKETTYPE(TransmissionOptions, EventPacketType->boolVal);
	}
	if (!AckMessageRequest && SUCCEEDED(Module->GetValue(XML_UPB_ACKMESSAGEREQUEST, &Option))) {
		AckMessageRequest = &Option;
	}
	if (AckMessageRequest) {
		PCS_MODULE_SETOPTION_ACKMESSAGEREQUEST(TransmissionOptions, AckMessageRequest->boolVal);
	}
	if (!IdPulseRequest && SUCCEEDED(Module->GetValue(XML_UPB_IDPULSEREQUEST, &Option))) {
		IdPulseRequest = &Option;
	}
	if (IdPulseRequest) {
		PCS_MODULE_SETOPTION_IDPULSEREQUEST(TransmissionOptions, IdPulseRequest->boolVal);
	}
	if (!AckPulseRequest && SUCCEEDED(Module->GetValue(XML_UPB_ACKPULSEREQUEST, &Option))) {
		AckPulseRequest = &Option;
	}
	if (AckPulseRequest) {
		PCS_MODULE_SETOPTION_ACKPULSEREQUEST(TransmissionOptions, AckPulseRequest->boolVal);
	}
	if (!TransmissionCount && SUCCEEDED(Module->GetValue(XML_UPB_TRANSMISSIONCOUNT, &Option))) {
		TransmissionCount = &Option;
	}
	if (TransmissionCount) {
		PCS_MODULE_SETOPTION_TRANSMISSIONCOUNT(TransmissionOptions, TransmissionCount->lVal);
	}
	char data[16];
	StringCchPrintfA(data, SIZEOF_ARRAY(data), "%02X%02X%02X", MAKE_MDID(MDID_CORE_COMMANDS, MDID_CORE_COMMAND_SETREGISTERVALUES), PCS_REG_TXCONTROL, TransmissionOptions);
	Module->SetValueEx(SVCC_NOTIFY | SVCC_FORCE, XML_UPB_UPBNETWORKMESSAGE, &CComVariant(data));
}

void PopulateLedOnActions(
	IN IPremiseObject* Module,
	IN const WCHAR* RegisterData
)
{
	CComPtr<IPremiseObjectCollection> LedOnActions;
	if (SUCCEEDED(Module->GetObjectsByType(CComVariant(XML_UPBPCS_SCHEMA_ROOT XML_SYS_PATH_SEPARATOR XML_PCS_LEDONACTION), collectionTypeRecurse, &LedOnActions))) {
		long EnumLedOnAction = 0;
		CComPtr<IPremiseObject> LedOnAction;
		for (; SUCCEEDED(LedOnActions->GetItemEx(EnumLedOnAction, &LedOnAction)); EnumLedOnAction++, LedOnAction = NULL) {
			CComPtr<IPremiseObject> Button;
			CComVariant ButtonID;
			if (SUCCEEDED(LedOnAction->get_Parent(&Button)) && SUCCEEDED(Button->GetValue(XML_PCS_BUTTONID, &ButtonID)) && (ButtonID.lVal > 0) && (ButtonID.lVal <= PCS_KEYPAD_REG_LEDONACTIONS_BYTES)) {
				BYTE ActionData;
				parsehex_(RegisterData + 2 * (ButtonID.lVal - 1), ActionData);
				LedOnAction->SetValueEx(SVCC_DRIVER | SVCC_NOTIFY, XML_PCS_LEDACTION, &CComVariant(PCS_KEYPAD_LEDACTION(ActionData)));
				LedOnAction->SetValueEx(SVCC_DRIVER | SVCC_NOTIFY, XML_PCS_LEDACTIONGROUP, &CComVariant(PCS_KEYPAD_LEDACTIONGROUP(ActionData)));
				LedOnAction->SetValueEx(SVCC_DRIVER | SVCC_NOTIFY, XML_PCS_LEDGROUPMASK, &CComVariant(PCS_KEYPAD_LEDGROUPMASK(ActionData)));
			}
			Button = NULL;
		}
	}
}

void SetLedOnActions(
	IN IPremiseObject* LedOnAction,
	IN VARIANT* LedAction OPTIONAL,
	IN VARIANT* LedActionGroup OPTIONAL,
	IN VARIANT* LedGroupMask OPTIONAL
)
{
	BYTE ActionData = 0;
	CComVariant Option;
	if (!LedAction && SUCCEEDED(LedOnAction->GetValue(XML_PCS_LEDACTION, &Option))) {
		LedAction = &Option;
	}
	if (LedActionGroup) {
		PCS_KEYPAD_SETOPTION_LEDACTION(ActionData, LedActionGroup->lVal);
	}
	if (!LedActionGroup && SUCCEEDED(LedOnAction->GetValue(XML_PCS_LEDACTIONGROUP, &Option))) {
		LedActionGroup = &Option;
	}
	if (LedActionGroup) {
		PCS_KEYPAD_SETOPTION_LEDACTIONGROUP(ActionData, LedActionGroup->lVal);
	}
	if (!LedGroupMask && SUCCEEDED(LedOnAction->GetValue(XML_PCS_LEDGROUPMASK, &Option))) {
		LedGroupMask = &Option;
	}
	if (LedGroupMask) {
		PCS_KEYPAD_SETOPTION_LEDGROUPMASK(ActionData, LedGroupMask->lVal);
	}
	CComPtr<IPremiseObject> Button;
	CComVariant ButtonID;
	CComPtr<IPremiseObject> Module;
	if (SUCCEEDED(LedOnAction->get_Parent(&Button)) && SUCCEEDED(Button->GetValue(XML_PCS_BUTTONID, &ButtonID)) && (ButtonID.lVal > 0) && (ButtonID.lVal <= PCS_KEYPAD_REG_LEDONACTIONS_BYTES)) {
		char data[16];
		StringCchPrintfA(data, SIZEOF_ARRAY(data), "%02X%02X%02X", MAKE_MDID(MDID_CORE_COMMANDS, MDID_CORE_COMMAND_SETREGISTERVALUES), PCS_KEYPAD_REG_LEDONACTIONS + (ButtonID.lVal - 1), ActionData);
		Module->SetValueEx(SVCC_NOTIFY | SVCC_FORCE, XML_UPB_UPBNETWORKMESSAGE, &CComVariant(data));
	}
}

void PopulateVirtualizedButtonState(
	IN IPremiseObject* Module,
	IN const BSTR ButtonClass,
	IN const WCHAR* MessageData,
	IN BYTE DestinationID
)
{
	BYTE ButtonState;
	MessageData += parsehex_(MessageData, ButtonState);
//	BYTE LedState;
//	parsehex_(MessageData, LedState);
	CComPtr<IPremiseObjectCollection> Buttons;
	CComPtr<IPremiseObject> Button;
	if (SUCCEEDED(Module->GetObjectsByTypeAndPropertyValue(ButtonClass, XML_PCS_BUTTONID, CComVariant(DestinationID), collectionTypeNoRecurse, &Buttons)) && SUCCEEDED(Buttons->GetItemEx(0, &Button))) {
		Button->SetValueEx(SVCC_DRIVER | SVCC_NOTIFY, XML_SYS_BUTTONSTATE, &CComVariant(ButtonState));
		if ((ButtonState == SYS_BUTTON_STATE_PRESS) || (ButtonState == SYS_BUTTON_STATE_DOUBLETAP)) {
			Button->SetValueEx(SVCC_DRIVER | SVCC_NOTIFY, XML_SYS_BUTTONSTATE, &CComVariant(SYS_BUTTON_STATE_RELEASE));
		}
//		Button->SetValueEx(SVCC_DRIVER | SVCC_NOTIFY, XML_SYS_STATUS, &CComVariant(LedState));
	}
}

void PopulateBrightness(
	IN IPremiseObject* Module,
	IN const WCHAR* MessageData
)
{
	BYTE Brightness;
	parsehex_(MessageData, Brightness);
	SetRoundedPercentileValue(Module, XML_SYS_BRIGHTNESS, Brightness);
	Module->SetValueEx(SVCC_DRIVER | SVCC_NOTIFY, XML_SYS_POWERSTATE, &CComVariant(Brightness > 0));
}

void PopulateLedStates(
	IN IPremiseObject* Module,
	IN const WCHAR* MessageData
)
{
	BYTE LedStates;
	parsehex_(MessageData, LedStates);
	CComPtr<IPremiseObjectCollection> Buttons;
	if (SUCCEEDED(Module->GetObjectsByType(CComVariant(XML_UPBPCS_SCHEMA_ROOT XML_SYS_PATH_SEPARATOR XML_PCS_KEYPADBUTTON), collectionTypeNoRecurse, &Buttons))) {
		long EnumButton = 0;
		CComPtr<IPremiseObject> Button;
		for (; SUCCEEDED(Buttons->GetItemEx(EnumButton, &Button)); EnumButton++, Button = NULL) {
			CComVariant ButtonID;
			if (SUCCEEDED(Button->GetValue(XML_PCS_BUTTONID, &ButtonID)) && (ButtonID.lVal > 0) && (ButtonID.lVal <= PCS_KEYPAD_REG_TRANSMITCOMPONENTS_BYTES / PCS_REG_TXCOMPONENTENTRY)) {
				Button->SetValueEx(SVCC_DRIVER | SVCC_NOTIFY, XML_SYS_STATUS, &CComVariant((LedStates >> (8 - ButtonID.lVal)) & 1));
			}
		}
	}
}

void SetKeypadReceiveComponentLinkID(
	IN IPremiseObject* Component,
	IN BYTE LinkID
)
{
	CComPtr<IPremiseObject> Button;
	CComPtr<IPremiseObject> Module;
	CComVariant ButtonID;
	if (SUCCEEDED(Component->get_Parent(&Button)) && SUCCEEDED(Button->get_Parent(&Module)) && SUCCEEDED(Button->GetValue(XML_PCS_BUTTONID, &ButtonID)) && (ButtonID.lVal > 0) && (ButtonID.lVal <= PCS_KEYPAD_REG_RECEIVECOMPONENTS_BYTES)) {
		char data[16];
		StringCchPrintfA(data, SIZEOF_ARRAY(data), "%02X%02X%02X", MAKE_MDID(MDID_CORE_COMMANDS, MDID_CORE_COMMAND_SETREGISTERVALUES), PCS_REG_RECEIVECOMPONENTS + ButtonID.lVal - 1, LinkID);
		Module->SetValueEx(SVCC_NOTIFY | SVCC_FORCE, XML_UPB_UPBNETWORKMESSAGE, &CComVariant(data));
	}
}

void SetRockerActionItem(
	IN IPremiseObject* Action,
	IN BYTE Item,
	IN int Buttons,
	IN int Offset
)
{
	//
	// In order to know the offset, the ButtonID from the parent, and
	// ButtonState properties are needed.
	//
	CComVariant ButtonState;
	CComPtr<IPremiseObject> Button;
	CComVariant ButtonID;
	CComPtr<IPremiseObject> Module;
	if (SUCCEEDED(Action->GetValue(XML_SYS_BUTTONSTATE, &ButtonState)) && (ButtonState.lVal >= SYS_BUTTON_STATE_RELEASE) && (ButtonState.lVal <= SYS_BUTTON_STATE_DOUBLETAP) && SUCCEEDED(Action->get_Parent(&Button)) && SUCCEEDED(Button->GetValue(XML_PCS_BUTTONID, &ButtonID)) && (ButtonID.lVal > 0) && (ButtonID.lVal <= Buttons) && SUCCEEDED(Button->get_Parent(&Module))) {
		char data[16];
		StringCchPrintfA(data, SIZEOF_ARRAY(data), "%02X%02X%02X", MAKE_MDID(MDID_CORE_COMMANDS, MDID_CORE_COMMAND_SETREGISTERVALUES), PCS_SWITCH_REG_ROCKERACTIONS + Offset + PCS_SWITCH_REG_ROCKERACTIONENTRY * (ButtonID.lVal - 1), Item);
		Module->SetValueEx(SVCC_NOTIFY | SVCC_FORCE, XML_UPB_UPBNETWORKMESSAGE, &CComVariant(data));
	}
}

void SetSwitchReceiveComponentItem(
	IN IPremiseObject* Component,
	IN BYTE Item,
	IN int Offset
)
{
	CComVariant ComponentID;
	if (SUCCEEDED(Component->GetValue(XML_UPB_COMPONENTID, &ComponentID)) && (ComponentID.lVal > 0) && (ComponentID.lVal <= PCS_SWITCH_REG_RECEIVECOMPONENTS_BYTES / PCS_SWITCH_REG_RXCOMPONENTENTRY)) {
		CComPtr<IPremiseObject> Components;
		CComPtr<IPremiseObject> Module;
		if (SUCCEEDED(Component->get_Parent(&Components)) && SUCCEEDED(Components->get_Parent(&Module))) {
			char data[16];
			StringCchPrintfA(data, SIZEOF_ARRAY(data), "%02X%02X%02X", MAKE_MDID(MDID_CORE_COMMANDS, MDID_CORE_COMMAND_SETREGISTERVALUES), PCS_REG_RECEIVECOMPONENTS + Offset + PCS_SWITCH_REG_RXCOMPONENTENTRY * (ComponentID.lVal - 1), Item);
			Module->SetValueEx(SVCC_NOTIFY | SVCC_FORCE, XML_UPB_UPBNETWORKMESSAGE, &CComVariant(data));
		}
	}
}

void VirtualizeTransmitCommands(
	IN IPremiseObject* Module,
	IN BYTE RegisterStart
)
{
	//
	// Each TransmitCommand with a CommandID that matches a Premise
	// ButtonState must have the Command string set to the Extended
	// Command Set with the Virtualized command, followed by the
	// Premise ButtonState that matches the CommandID, followed by 00.
	//
	CComPtr<IPremiseObjectCollection> Commands;
	if (SUCCEEDED(Module->GetObjectsByType(CComVariant(XML_UPB_SCHEMA_ROOT XML_SYS_PATH_SEPARATOR XML_UPB_TRANSMITCOMMAND), collectionTypeRecurse, &Commands))) {
		long EnumCommand = 0;
		CComPtr<IPremiseObject> Command;
		for (; SUCCEEDED(Commands->GetItemEx(EnumCommand, &Command)); EnumCommand++, Command = NULL) {
			CComVariant CommandID;
			if (SUCCEEDED(Command->GetValue(XML_UPB_COMMANDID, &CommandID)) && (CommandID.lVal >= SYS_BUTTON_STATE_RELEASE) && (CommandID.lVal <= SYS_BUTTON_STATE_DOUBLETAP)) {
				char data[16];
				StringCchPrintfA(data, SIZEOF_ARRAY(data), "%02X%02X00", MAKE_MDID(MDID_EXTENDED_MESSAGE_SET, VIRTUALIZED_MESSAGE_SET), CommandID.lVal);
				Command->SetValueEx(SVCC_DRIVER | SVCC_NOTIFY, XML_UPB_COMMAND, &CComVariant(data));
			}
		}
	}
	//
	// Collect the register data into a single write so that I/O is
	// reduced. The UPB driver will split the write up into maximum
	// lengths.
	//
	char RegisterData[2 * (1 + 1 + (SYS_BUTTON_STATE_DOUBLETAP + 1) * PCS_REG_TXCOMMANDENTRY) + 1];
	char* DataPointer;
	size_t DataRemaining;
	StringCchPrintfExA(RegisterData, SIZEOF_ARRAY(RegisterData), &DataPointer, &DataRemaining, 0, "%02X%02X", MAKE_MDID(MDID_CORE_COMMANDS, MDID_CORE_COMMAND_SETREGISTERVALUES), RegisterStart);
	for (int i = SYS_BUTTON_STATE_RELEASE; i <= SYS_BUTTON_STATE_DOUBLETAP; i++ ) {
		StringCchPrintfExA(DataPointer, DataRemaining, &DataPointer, &DataRemaining, 0, "%02X%02X00", MAKE_MDID(MDID_EXTENDED_MESSAGE_SET, VIRTUALIZED_MESSAGE_SET), i);
	}
	Module->SetValueEx(SVCC_NOTIFY | SVCC_FORCE, XML_UPB_UPBNETWORKMESSAGE, &CComVariant(RegisterData));
}

void VirtualizeTransmitComponents(
	IN IPremiseObject* Module,
	IN const BSTR ButtonClass,
	IN BYTE RegisterStart,
	IN int ButtonCount
)
{
	//
	// Set the LinkID in each TransmitComponent to the ButtonID in
	// which it is contained. Also set the OnToggle and OffToggle
	// CommandID to match the ButtonState.
	//
	CComPtr<IPremiseObjectCollection> Buttons;
	if (SUCCEEDED(Module->GetObjectsByType(CComVariant(ButtonClass), collectionTypeNoRecurse, &Buttons))) {
		long EnumButton = 0;
		CComPtr<IPremiseObject> Button;
		for (; SUCCEEDED(Buttons->GetItemEx(EnumButton, &Button)); EnumButton++, Button = NULL) {
			CComVariant ButtonID;
			CComPtr<IPremiseObjectCollection> ComponentCol;
			CComPtr<IPremiseObject> Component;
			if (SUCCEEDED(Button->GetValue(XML_PCS_BUTTONID, &ButtonID)) && (ButtonID.lVal > 0) && (ButtonID.lVal <= ButtonCount) && SUCCEEDED(Button->GetObjectsByType(CComVariant(XML_UPB_SCHEMA_ROOT XML_SYS_PATH_SEPARATOR XML_UPB_TRANSMITCOMPONENT), collectionTypeNoRecurse, &ComponentCol)) && SUCCEEDED(ComponentCol->GetItemEx(0, &Component))) {
				Component->SetValueEx(SVCC_DRIVER | SVCC_NOTIFY, XML_UPB_LINKID, &ButtonID);
				CComPtr<IPremiseObjectCollection> CommandIDs;
				if (SUCCEEDED(Component->GetObjectsByType(CComVariant(XML_UPBPCS_SCHEMA_ROOT XML_SYS_PATH_SEPARATOR XML_UPB_COMMANDID), collectionTypeNoRecurse, &CommandIDs))) {
					long EnumCommandID = 0;
					CComPtr<IPremiseObject> CommandID;
					for (; SUCCEEDED(CommandIDs->GetItemEx(EnumCommandID, &CommandID)); EnumCommandID++, CommandID = NULL) {
						CComVariant ButtonState;
						if (SUCCEEDED(CommandID->GetValue(XML_SYS_BUTTONSTATE, &ButtonState)) && (ButtonState.lVal >= SYS_BUTTON_STATE_RELEASE) && (ButtonState.lVal <= SYS_BUTTON_STATE_DOUBLETAP)) {
							CommandID->SetValueEx(SVCC_DRIVER | SVCC_NOTIFY, XML_PCS_ONTOGGLE, &CComVariant(ButtonState.lVal));
							CommandID->SetValueEx(SVCC_DRIVER | SVCC_NOTIFY, XML_PCS_OFFTOGGLE, &CComVariant(ButtonState.lVal));
						}
					}
				}
			}
			ComponentCol = NULL;
			Component = NULL;
		}
	}
	//
	// Collect the register data into a single write so that I/O is
	// reduced. The UPB driver will split the write up into maximum
	// lengths.
	//
	size_t DataRemaining = 2 * (1 + 1 + ButtonCount * PCS_REG_TXCOMPONENTENTRY) + 1;
	char* RegisterData = new char[DataRemaining];
	if (RegisterData) {
		char* DataPointer;
		StringCchPrintfExA(RegisterData, DataRemaining, &DataPointer, &DataRemaining, 0, "%02X%02X", MAKE_MDID(MDID_CORE_COMMANDS, MDID_CORE_COMMAND_SETREGISTERVALUES), RegisterStart);
		for (int i = 1; i <= ButtonCount; i++ ) {
			StringCchPrintfExA(DataPointer, DataRemaining, &DataPointer, &DataRemaining, 0, "%02X%02X%02X%02X%02X", i, SYS_BUTTON_STATE_PRESS << 4 | SYS_BUTTON_STATE_PRESS, SYS_BUTTON_STATE_DOUBLETAP << 4 | SYS_BUTTON_STATE_DOUBLETAP, SYS_BUTTON_STATE_HOLD << 4 | SYS_BUTTON_STATE_HOLD, SYS_BUTTON_STATE_RELEASE << 4 | SYS_BUTTON_STATE_RELEASE);
		}
		Module->SetValueEx(SVCC_NOTIFY | SVCC_FORCE, XML_UPB_UPBNETWORKMESSAGE, &CComVariant(RegisterData));
		delete [] RegisterData;
	}
}

void VirtualizeSwitch(
	IN IPremiseObject* Module
)
{
	//
	// Enable ReportState.
	//
	SetDimmerOptions(Module, NULL, &CComVariant(TRUE), NULL);
	//
	// Enable sending of Link packets.
	//
	SetTransmissionOptions(Module, &CComVariant(TRUE), NULL, NULL, NULL, NULL);
	VirtualizeTransmitComponents(Module, XML_UPBPCS_SCHEMA_ROOT XML_SYS_PATH_SEPARATOR XML_PCS_SWITCHBUTTON, PCS_SWITCH_REG_TRANSMITCOMPONENTS, 2);
	VirtualizeTransmitCommands(Module, PCS_REG_TRANSMITCOMMANDS);
}

void VirtualizeKeypad(
	IN IPremiseObject* Module
)
{
	//
	// Enable sending of Link packets.
	//
	SetTransmissionOptions(Module, &CComVariant(TRUE), NULL, NULL, NULL, NULL);
	VirtualizeTransmitComponents(Module, XML_UPBPCS_SCHEMA_ROOT XML_SYS_PATH_SEPARATOR XML_PCS_KEYPADBUTTON, PCS_KEYPAD_REG_TRANSMITCOMPONENTS, 8);
	VirtualizeTransmitCommands(Module, PCS_REG_TRANSMITCOMMANDS);
}

void UpdateKeypadLedOnAction(
	IN IPremiseObject* Module,
	IN IPremiseObject* Button
)
{
	CComPtr<IPremiseObjectCollection> LedOnActions;
	CComPtr<IPremiseObject> LedOnAction;
	CComVariant LedAction;
	CComVariant LedActionGroup;
	CComVariant ButtonID;
	if (SUCCEEDED(Button->GetObjectsByType(CComVariant(XML_UPBPCS_SCHEMA_ROOT XML_SYS_PATH_SEPARATOR XML_PCS_LEDONACTION), collectionTypeNoRecurse, &LedOnActions)) && SUCCEEDED(LedOnActions->GetItemEx(0, &LedOnAction)) && SUCCEEDED(LedOnAction->GetValue(XML_PCS_LEDACTION, &LedAction)) && ((LedAction.lVal == 2) || (LedAction.lVal == 3)) && SUCCEEDED(LedOnAction->GetValue(XML_PCS_LEDACTIONGROUP, &LedActionGroup)) && SUCCEEDED(Button->GetValue(XML_PCS_BUTTONID, &ButtonID)) && (ButtonID.lVal > 0) && (ButtonID.lVal <= 8)) {
		LedOnActions = NULL;
		if (SUCCEEDED(Module->GetObjectsByType(CComVariant(XML_UPBPCS_SCHEMA_ROOT XML_SYS_PATH_SEPARATOR XML_PCS_LEDONACTION), collectionTypeRecurse, &LedOnActions))) {
			long EnumLedOnAction = 0;
			LedOnAction = NULL;
			for (; SUCCEEDED(LedOnActions->GetItemEx(EnumLedOnAction, &LedOnAction)); EnumLedOnAction++, LedOnAction = NULL) {
				CComVariant LedGroupMask;
				CComPtr<IPremiseObject> ThisButton;
				CComVariant ThisButtonID;
				if (SUCCEEDED(LedOnAction->GetValue(XML_PCS_LEDGROUPMASK, &LedGroupMask)) && (LedGroupMask.lVal & (1 << (LedActionGroup.lVal & 0x03))) && SUCCEEDED(LedOnAction->get_Parent(&ThisButton)) && SUCCEEDED(ThisButton->GetValue(XML_PCS_BUTTONID, &ThisButtonID)) && (ThisButtonID.lVal != ButtonID.lVal) && (ThisButtonID.lVal > 0) && (ThisButtonID.lVal <= 8)) {
					ThisButton->SetValueEx(SVCC_DRIVER | SVCC_NOTIFY, XML_SYS_STATUS, &CComVariant(LedAction.lVal == 3));
				}
			}
		}
	}
}

void UpdateKeypadReceiveComponentResults(
	IN IPremiseObject* Module,
	IN BYTE did,
	IN BOOL Status
)
{
	CComPtr<IPremiseObjectCollection> Components;
	CComPtr<IPremiseObject> Component;
	CComPtr<IPremiseObject> Button;
	if (SUCCEEDED(Module->GetObjectsByTypeAndPropertyValue(XML_UPB_SCHEMA_ROOT XML_SYS_PATH_SEPARATOR XML_UPB_RECEIVECOMPONENT, XML_UPB_LINKID, CComVariant(did), collectionTypeRecurse, &Components)) && SUCCEEDED(Components->GetItemEx(0, &Component)) && SUCCEEDED(Component->get_Parent(&Button))) {
		Button->SetValueEx(SVCC_DRIVER | SVCC_NOTIFY, XML_SYS_STATUS, &CComVariant(Status));
		//
		// If this LED is turned on, then others may need to be
		// updated.
		//
		if (Status) {
			UpdateKeypadLedOnAction(Module, Button);
		}
	}
}

void UpdateSwitchReceiveComponentResults(
	IN IPremiseObject* Module,
	IN BYTE did,
	IN BOOL Status
)
{
	if (Status) {
		CComPtr<IPremiseObjectCollection> Components;
		CComPtr<IPremiseObject> Component;
		if (SUCCEEDED(Module->GetObjectsByTypeAndPropertyValue(XML_UPB_SCHEMA_ROOT XML_SYS_PATH_SEPARATOR XML_UPB_RECEIVECOMPONENT, XML_UPB_LINKID, CComVariant(did), collectionTypeRecurse, &Components)) && SUCCEEDED(Components->GetItemEx(0, &Component))) {
			CComVariant LastOnLevel;
			CComVariant LightLevel;
			if (FAILED(Component->GetValue(XML_UPB_LASTONLEVEL, &LastOnLevel)) || LastOnLevel.boolVal || FAILED(Component->GetValue(XML_UPB_LIGHTLEVEL, &LightLevel))) {
				Module->SetValueEx(SVCC_DRIVER | SVCC_NOTIFY, XML_SYS_BRIGHTNESS, &LightLevel);
				Module->SetValueEx(SVCC_DRIVER | SVCC_NOTIFY, XML_SYS_POWERSTATE, &CComVariant(LightLevel.dblVal > 0));
			} else {
				TransmitNetworkPacket(Module, MAKE_MDID(MDID_DEVICE_CONTROL_COMMANDS, MDID_DEVICE_CONTROL_COMMAND_REPORTSTATE));
			}
		}
	} else {
		Module->SetValueEx(SVCC_NOTIFY, XML_UPB_LIGHTLEVEL, &CComVariant(0));
		Module->SetValueEx(SVCC_DRIVER | SVCC_NOTIFY, XML_SYS_POWERSTATE, &CComVariant(FALSE));
	}
}

HRESULT FindMatchingCommandID(
	IN IPremiseObject* Module,
	IN VARIANT CommandID,
	IN BYTE did,
	OUT IPremiseObject** Button,
	OUT long* State
)
{
	CComPtr<IPremiseObjectCollection> CommandIDs;
	CComPtr<IPremiseObject> ToggleCommand;
	CComPtr<IPremiseObject> Component;
	CComVariant LinkID;
	CComVariant ButtonState;
	if (SUCCEEDED(Module->GetObjectsByTypeAndPropertyValue(XML_UPBPCS_SCHEMA_ROOT XML_SYS_PATH_SEPARATOR XML_UPB_COMMANDID, XML_PCS_ONTOGGLE, CommandID, collectionTypeRecurse, &CommandIDs)) && SUCCEEDED(CommandIDs->GetItemEx(0, &ToggleCommand)) && SUCCEEDED(ToggleCommand->get_Parent(&Component)) && SUCCEEDED(Component->GetValue(XML_UPB_LINKID, &LinkID)) && (LinkID.bVal == did) && SUCCEEDED(ToggleCommand->GetValue(XML_SYS_BUTTONSTATE, &ButtonState))) {
		*State = ButtonState.lVal;
		return Component->get_Parent(Button);
	}
	CommandIDs = NULL;
	ToggleCommand = NULL;
	Component = NULL;
	ButtonState = NULL;
	if (SUCCEEDED(Module->GetObjectsByTypeAndPropertyValue(XML_UPBPCS_SCHEMA_ROOT XML_SYS_PATH_SEPARATOR XML_UPB_COMMANDID, XML_PCS_OFFTOGGLE, CommandID, collectionTypeRecurse, &CommandIDs)) && SUCCEEDED(CommandIDs->GetItemEx(0, &ToggleCommand)) && SUCCEEDED(ToggleCommand->get_Parent(&Component)) && SUCCEEDED(Component->GetValue(XML_UPB_LINKID, &LinkID)) && (LinkID.bVal == did) && SUCCEEDED(ToggleCommand->GetValue(XML_SYS_BUTTONSTATE, &ButtonState))) {
		*State = ButtonState.lVal;
		return Component->get_Parent(Button);
	}
	return E_FAIL;
}

HRESULT FindMatchingButton(
	IN IPremiseObject* Module,
	IN BYTE did,
	IN BYTE mdid,
	IN WCHAR* MessageData,
	IN IPremiseObjectCollection* Commands
)
{
	long EnumCommand = 0;
	CComPtr<IPremiseObject> Command;
	for (; SUCCEEDED(Commands->GetItemEx(EnumCommand, &Command)); EnumCommand++, Command = NULL) {
		CComVariant CommandID;
		if (SUCCEEDED(Command->GetValue(XML_UPB_COMMANDID, &CommandID))) {
			CComPtr<IPremiseObject> Button;
			long ButtonState;
			if (SUCCEEDED(FindMatchingCommandID(Module, CommandID, did, &Button, &ButtonState))) {
				//
				// If the button state is Hold, then there is no guarantee that the Release
				// will send a message also.
				//
				Button->SetValueEx(SVCC_DRIVER | SVCC_NOTIFY, XML_SYS_BUTTONSTATE, &CComVariant(ButtonState));
				//
				// No release is generated for these.
				//
				if ((ButtonState == SYS_BUTTON_STATE_PRESS) || (ButtonState == SYS_BUTTON_STATE_DOUBLETAP)) {
					Button->SetValueEx(SVCC_DRIVER | SVCC_NOTIFY, XML_SYS_BUTTONSTATE, &CComVariant(SYS_BUTTON_STATE_RELEASE));
				}
				//
				// The keypad may need to update the state of the
				// other LEDs if one has been turned on.
				//
				if (IsObjectOfExplicitType(Module, XML_UPBPCS_SCHEMA_ROOT XML_SYS_PATH_SEPARATOR XML_PCS_KEYPAD)) {
					MessageData += 2;
					BOOL Status;
					switch (mdid) {
						case MAKE_MDID(MDID_DEVICE_CONTROL_COMMANDS, MDID_DEVICE_CONTROL_COMMAND_ACTIVATELINK):
							Status = TRUE;
							break;
						case MAKE_MDID(MDID_DEVICE_CONTROL_COMMANDS, MDID_DEVICE_CONTROL_COMMAND_DEACTIVATELINK):
							Status = FALSE;
							break;
						case MAKE_MDID(MDID_DEVICE_CONTROL_COMMANDS, MDID_DEVICE_CONTROL_COMMAND_GOTO):
						case MAKE_MDID(MDID_DEVICE_CONTROL_COMMANDS, MDID_DEVICE_CONTROL_COMMAND_FADESTART):
						{
							BYTE LightLevel;
							MessageData += parsehex_(MessageData, LightLevel);
							Status = LightLevel > 0;
							break;
						}
					}
					Button->SetValueEx(SVCC_DRIVER | SVCC_NOTIFY, XML_SYS_STATUS, &CComVariant(Status));
					if (Status) {
						UpdateKeypadLedOnAction(Module, Button);
					}
				}
				return S_OK;
			}
		}
	}
	return E_FAIL;
}

void UpdateTransmitComponentResults(
	IN IPremiseObject* Module,
	IN BYTE did,
	IN BYTE mdid,
	IN WCHAR* MessageData
)
{
	CComPtr<IPremiseObjectCollection> Commands;
	if (SUCCEEDED(Module->GetObjectsByTypeAndPropertyValue(XML_UPB_SCHEMA_ROOT XML_SYS_PATH_SEPARATOR XML_UPB_TRANSMITCOMMAND, XML_UPB_COMMAND, CComVariant(MessageData), collectionTypeRecurse, &Commands))) {
		if (FAILED(FindMatchingButton(Module, did, mdid, MessageData, Commands))) {
			FindMatchingButton(Module, UPB_LASTLINKID, mdid, MessageData, Commands);
		}
	}
}

void StoreSwitchState(
	IN IPremiseObject* Module,
	IN BYTE LinkID
)
{
	CComPtr<IPremiseObjectCollection> Components;
	if (SUCCEEDED(Module->GetObjectsByTypeAndPropertyValue(XML_UPB_SCHEMA_ROOT XML_SYS_PATH_SEPARATOR XML_UPB_SWITCHRECEIVECOMPONENT, XML_UPB_LINKID, CComVariant(LinkID), collectionTypeRecurse, &Components))) {
		//
		// If there is no Brightness, then just try PowerState.
		//
		CComVariant Brightness;
		if (FAILED(Module->GetValue(XML_SYS_BRIGHTNESS, &Brightness))) {
			CComVariant PowerState;
			if (FAILED(Module->GetValue(XML_SYS_POWERSTATE, &PowerState))) {
				return;
			}
			Brightness = PowerState.boolVal ? 1.0 : 0.0;
		}
		long EnumComponent = 0;
		CComPtr<IPremiseObject> Component;
		for (; SUCCEEDED(Components->GetItemEx(EnumComponent, &Component)); EnumComponent++, Component = NULL) {
			//
			// Ensure the entry is within range.
			//
			CComVariant ComponentID;
			if (SUCCEEDED(Component->GetValue(XML_UPB_COMPONENTID, &ComponentID)) && (ComponentID.lVal > 0) && (ComponentID.lVal <= PCS_WSD1_RECEIVECOMPONENTS)) {
				Component->SetValueEx(SVCC_DRIVER | SVCC_NOTIFY, XML_UPB_LASTONLEVEL, &CComVariant(FALSE));
				Component->SetValueEx(SVCC_DRIVER | SVCC_NOTIFY, XML_UPB_LIGHTLEVEL, &Brightness);
				//
				// Only update the cache, without writing to the device
				// again.
				//
				WCHAR data[16];
				StringCchPrintfW(data, SIZEOF_ARRAY(data), L"%c%02X%02X%02X", UPB_MESSAGE_CACHE_ONLY, MAKE_MDID(MDID_CORE_COMMANDS, MDID_CORE_COMMAND_SETREGISTERVALUES), PCS_REG_RECEIVECOMPONENTS + 1 + PCS_SWITCH_REG_RXCOMPONENTENTRY * (ComponentID.lVal - 1), static_cast<BYTE>(static_cast<long>(Brightness.dblVal * 100 + 0.5)));
				Module->SetValueEx(SVCC_NOTIFY | SVCC_FORCE, XML_UPB_UPBNETWORKMESSAGE, &CComVariant(data));
			}
		}
	}
}

void OnWS1D6NetworkData(
	IN IPremiseObject* Module,
	IN WCHAR* MessageData
)
{
	WCHAR NetworkData = *(MessageData++);
	//
	// Skip message type, nid.
	//
	MessageData += 3;
	BYTE did;
	MessageData += parsehex_(MessageData, did);
	//
	// Skip message sid.
	//
	MessageData += 2;
	BYTE mdid;
	MessageData += parsehex_(MessageData, mdid);
	switch (NetworkData) {
	case UPB_NETWORKDATA_DEVICEMESSAGE:
	case UPB_NETWORKDATA_DEVICERESPONSE:
		//
		// The hardware this module represents sent this message.
		//
		// The Link and Direct messages are fairly mutually exclusive, so there should not be
		// any need to differentiate between the two.
		//
		switch (mdid) {
		case MAKE_MDID(MDID_EXTENDED_MESSAGE_SET, VIRTUALIZED_MESSAGE_SET):
			PopulateVirtualizedButtonState(Module, XML_UPBPCS_SCHEMA_ROOT XML_SYS_PATH_SEPARATOR XML_PCS_SWITCHBUTTON, MessageData, did);
			break;
		case MAKE_MDID(MDID_DEVICE_CONTROL_COMMANDS, MDID_DEVICE_CONTROL_COMMAND_ACTIVATELINK):
		case MAKE_MDID(MDID_DEVICE_CONTROL_COMMANDS, MDID_DEVICE_CONTROL_COMMAND_DEACTIVATELINK):
		case MAKE_MDID(MDID_DEVICE_CONTROL_COMMANDS, MDID_DEVICE_CONTROL_COMMAND_GOTO):
		case MAKE_MDID(MDID_DEVICE_CONTROL_COMMANDS, MDID_DEVICE_CONTROL_COMMAND_FADESTART):
		case MAKE_MDID(MDID_DEVICE_CONTROL_COMMANDS, MDID_DEVICE_CONTROL_COMMAND_FADESTOP):
			UpdateTransmitComponentResults(Module, did, mdid, MessageData - 2);
			break;
		case MAKE_MDID(MDID_CORE_REPORTS, MDID_DEVICE_CORE_REPORT_DEVICESTATE):
			PopulateBrightness(Module, MessageData);
			break;
		}
		break;
	case UPB_NETWORKDATA_BROADCAST:
		switch (mdid) {
		case MAKE_MDID(MDID_DEVICE_CONTROL_COMMANDS, MDID_DEVICE_CONTROL_COMMAND_ACTIVATELINK):
			UpdateSwitchReceiveComponentResults(Module, did, TRUE);
			break;
		case MAKE_MDID(MDID_DEVICE_CONTROL_COMMANDS, MDID_DEVICE_CONTROL_COMMAND_DEACTIVATELINK):
			UpdateSwitchReceiveComponentResults(Module, did, FALSE);
			break;
		case MAKE_MDID(MDID_DEVICE_CONTROL_COMMANDS, MDID_DEVICE_CONTROL_COMMAND_GOTO):
			PopulateBrightness(Module, MessageData);
			break;
		case MAKE_MDID(MDID_DEVICE_CONTROL_COMMANDS, MDID_DEVICE_CONTROL_COMMAND_FADESTART):
		case MAKE_MDID(MDID_DEVICE_CONTROL_COMMANDS, MDID_DEVICE_CONTROL_COMMAND_FADESTOP):
			{
				CComVariant LoadType;
				if (SUCCEEDED(Module->GetValue(XML_PCS_LOADTYPE, &LoadType))) {
					if (LoadType.boolVal) {
						if (mdid == MAKE_MDID(MDID_DEVICE_CONTROL_COMMANDS, MDID_DEVICE_CONTROL_COMMAND_FADESTART)) {
							PopulateBrightness(Module, MessageData);
						} else {
							TransmitNetworkPacket(Module, MAKE_MDID(MDID_DEVICE_CONTROL_COMMANDS, MDID_DEVICE_CONTROL_COMMAND_REPORTSTATE));
						}
					}
				} else {
					TransmitNetworkPacket(Module, MAKE_MDID(MDID_DEVICE_CONTROL_COMMANDS, MDID_DEVICE_CONTROL_COMMAND_REPORTSTATE));
				}
				break;
			}
		case MAKE_MDID(MDID_DEVICE_CONTROL_COMMANDS, MDID_DEVICE_CONTROL_COMMAND_STORESTATE):
			StoreSwitchState(Module, did);
			break;
		}
		break;
	}
}

void OnKeypadNetworkData(
	IN IPremiseObject* Module,
	IN WCHAR* MessageData
)
{
	WCHAR NetworkData = *(MessageData++);
	//
	// Skip message type, nid.
	//
	MessageData += 3;
	BYTE did;
	MessageData += parsehex_(MessageData, did);
	//
	// Skip message sid.
	//
	MessageData += 2;
	BYTE mdid;
	MessageData += parsehex_(MessageData, mdid);
	switch (NetworkData) {
	case UPB_NETWORKDATA_DEVICEMESSAGE:
	case UPB_NETWORKDATA_DEVICERESPONSE:
		//
		// The hardware this module represents sent this message.
		//
		// The Link and Direct messages are fairly mutually exclusive, so there should not be
		// any need to differentiate between the two.
		//
		switch (mdid) {
		case MAKE_MDID(MDID_EXTENDED_MESSAGE_SET, VIRTUALIZED_MESSAGE_SET):
			PopulateVirtualizedButtonState(Module, XML_UPBPCS_SCHEMA_ROOT XML_SYS_PATH_SEPARATOR XML_PCS_KEYPADBUTTON, MessageData, did);
			break;
		case MAKE_MDID(MDID_DEVICE_CONTROL_COMMANDS, MDID_DEVICE_CONTROL_COMMAND_ACTIVATELINK):
		case MAKE_MDID(MDID_DEVICE_CONTROL_COMMANDS, MDID_DEVICE_CONTROL_COMMAND_DEACTIVATELINK):
		case MAKE_MDID(MDID_DEVICE_CONTROL_COMMANDS, MDID_DEVICE_CONTROL_COMMAND_GOTO):
		case MAKE_MDID(MDID_DEVICE_CONTROL_COMMANDS, MDID_DEVICE_CONTROL_COMMAND_FADESTART):
		case MAKE_MDID(MDID_DEVICE_CONTROL_COMMANDS, MDID_DEVICE_CONTROL_COMMAND_FADESTOP):
			UpdateTransmitComponentResults(Module, did, mdid, MessageData - 2);
			break;
		case MAKE_MDID(MDID_CORE_REPORTS, MDID_DEVICE_CORE_REPORT_REGISTERVALUES):
			{
				BYTE RegisterStart;
				MessageData += parsehex_(MessageData, RegisterStart);
				switch (RegisterStart) {
				case PCS_KEYPAD_REG_LEDSTATES:
					PopulateLedStates(Module, MessageData);
					break;
				}
				break;
			}
		}
		break;
	case UPB_NETWORKDATA_BROADCAST:
		switch (mdid) {
		case MAKE_MDID(MDID_DEVICE_CONTROL_COMMANDS, MDID_DEVICE_CONTROL_COMMAND_ACTIVATELINK):
			UpdateKeypadReceiveComponentResults(Module, did, TRUE);
			break;
		case MAKE_MDID(MDID_DEVICE_CONTROL_COMMANDS, MDID_DEVICE_CONTROL_COMMAND_DEACTIVATELINK):
			UpdateKeypadReceiveComponentResults(Module, did, TRUE);
			break;
		case MAKE_MDID(MDID_DEVICE_CONTROL_COMMANDS, MDID_DEVICE_CONTROL_COMMAND_GOTO):
		case MAKE_MDID(MDID_DEVICE_CONTROL_COMMANDS, MDID_DEVICE_CONTROL_COMMAND_FADESTART):
			{
				BYTE LightLevel;
				MessageData += parsehex_(MessageData, LightLevel);
				if (*MessageData) {
					MessageData += 2;
					//
					// If it is a Link packet, then it will not have a ButtonID and should be
					// ignored.
					//
					if (*MessageData) {
						BYTE ButtonID;
						MessageData += parsehex_(MessageData, ButtonID);
						UpdateKeypadReceiveComponentResults(Module, ButtonID, LightLevel);
					}
				}
				break;
			}
		}
		break;
	}
}

HRESULT STDMETHODCALLTYPE CupbPCS::OnUpbNetworkDataChanged(
	IN IPremiseObject* Object,
	IN VARIANT* newValue
)
{
	//
	// Ensure that this is one of the root module objects, and not
	// something added underneath.
	//
	if (newValue->bstrVal[0] && IsObjectOfExplicitType(Object, XML_UPBPCS_SCHEMA_ROOT XML_SYS_PATH_SEPARATOR XML_PCS_UPBPCSBASE)) {
		if (IsObjectOfExplicitType(Object, XML_UPBPCS_SCHEMA_ROOT XML_SYS_PATH_SEPARATOR XML_PCS_WS1D6)) {
			OnWS1D6NetworkData(Object, newValue->bstrVal);
		} else if (IsObjectOfExplicitType(Object, XML_UPBPCS_SCHEMA_ROOT XML_SYS_PATH_SEPARATOR XML_PCS_KEYPAD)) {
			OnKeypadNetworkData(Object, newValue->bstrVal);
//		} else if (IsObjectOfExplicitType(Object, XML_UPBPCS_SCHEMA_ROOT XML_SYS_PATH_SEPARATOR XML_PCS_SERIALPIM)) {
		}
	}
	return S_OK;
}

HRESULT STDMETHODCALLTYPE CupbPCS::OnDeviceInitializedChanged(
	IN IPremiseObject* Object,
	IN VARIANT* newValue
)
{
	CComVariant SetupRegisterCache;
	if (newValue->boolVal && IsObjectOfExplicitType(Object, XML_UPBPCS_SCHEMA_ROOT XML_SYS_PATH_SEPARATOR XML_PCS_UPBPCSBASE) && SUCCEEDED(Object->GetValue(XML_UPB_SETUPREGISTERCACHE, &SetupRegisterCache)) && (lstrlenW(SetupRegisterCache.bstrVal) == 2 * UPB_MODULE_REGISTERS)) {
		if (IsObjectOfExplicitType(Object, XML_UPBPCS_SCHEMA_ROOT XML_SYS_PATH_SEPARATOR XML_PCS_WS1D6)) {
			PopulateSwitchReceiveComponents(Object, &SetupRegisterCache.bstrVal[2 * PCS_REG_RECEIVECOMPONENTS]);
			PopulateTransmitComponents(Object, &SetupRegisterCache.bstrVal[2 * PCS_SWITCH_REG_TRANSMITCOMPONENTS], PCS_SWITCH_REG_TRANSMITCOMPONENTS_BYTES / PCS_REG_TXCOMPONENTENTRY, XML_UPBPCS_SCHEMA_ROOT XML_SYS_PATH_SEPARATOR XML_PCS_SWITCHBUTTON);
			PopulateRockerActions(Object, &SetupRegisterCache.bstrVal[2 * PCS_SWITCH_REG_ROCKERACTIONS], PCS_SWITCH_REG_ROCKERACTIONS_BYTES / PCS_SWITCH_REG_ROCKERACTIONENTRY);
			PopulateSwitchLedOptions(Object, &SetupRegisterCache.bstrVal[2 * PCS_SWITCH_REG_LEDOPTIONS]);
			PopulateDimmerOptions(Object, &SetupRegisterCache.bstrVal[2 * PCS_SWITCH_REG_DIMMEROPTIONS]);
			PopulateTransmissionOptions(Object, &SetupRegisterCache.bstrVal[2 * PCS_REG_TXCONTROL]);
			PopulateTransmitCommands(Object, &SetupRegisterCache.bstrVal[2 * PCS_REG_TRANSMITCOMMANDS], PCS_SWITCH_REG_TRANSMITCOMMANDS_BYTES / PCS_REG_TXCOMMANDENTRY);
			TransmitNetworkPacket(Object, MAKE_MDID(MDID_DEVICE_CONTROL_COMMANDS, MDID_DEVICE_CONTROL_COMMAND_REPORTSTATE));
		} else if (IsObjectOfExplicitType(Object, XML_UPBPCS_SCHEMA_ROOT XML_SYS_PATH_SEPARATOR XML_PCS_KEYPAD)) {
			PopulateKeypadReceiveComponents(Object, &SetupRegisterCache.bstrVal[2 * PCS_REG_RECEIVECOMPONENTS]);
			PopulateTransmitComponents(Object, &SetupRegisterCache.bstrVal[2 * PCS_KEYPAD_REG_TRANSMITCOMPONENTS], PCS_KEYPAD_REG_TRANSMITCOMPONENTS_BYTES / PCS_REG_TXCOMPONENTENTRY, XML_UPBPCS_SCHEMA_ROOT XML_SYS_PATH_SEPARATOR XML_PCS_KEYPADBUTTON);
			PopulateIrOptions(Object, &SetupRegisterCache.bstrVal[2 * PCS_KEYPAD_REG_IROPTIONS]);
			PopulateKeypadLedOptions(Object, &SetupRegisterCache.bstrVal[2 * PCS_KEYPAD_REG_LEDOPTIONS]);
			PopulateTransmissionOptions(Object, &SetupRegisterCache.bstrVal[2 * PCS_REG_TXCONTROL]);
			PopulateLedOnActions(Object, &SetupRegisterCache.bstrVal[2 * PCS_KEYPAD_REG_LEDONACTIONS]);
			PopulateTransmitCommands(Object, &SetupRegisterCache.bstrVal[2 * PCS_REG_TRANSMITCOMMANDS], PCS_KEYPAD_REG_TRANSMITCOMMANDS_BYTES / PCS_REG_TXCOMMANDENTRY);
			char r[16];
			StringCchPrintfA(r, SIZEOF_ARRAY(r), "%02X%02X", PCS_KEYPAD_REG_LEDSTATES, PCS_KEYPAD_REG_LEDSTATES_BYTES);
			TransmitNetworkPacket(Object, MAKE_MDID(MDID_CORE_COMMANDS, MDID_CORE_COMMAND_GETREGISTERVALUES), r);
	//	} else if (IsObjectOfExplicitType(Object, XML_UPBPCS_SCHEMA_ROOT XML_SYS_PATH_SEPARATOR XML_PCS_SERIALPIM)) {
		}
	}
	return S_OK;
}

HRESULT STDMETHODCALLTYPE CupbPCS::OnLedModeChanged(
	IN IPremiseObject* Object,
	IN VARIANT* newValue
)
{
	if (IsObjectOfExplicitType(Object, XML_UPBPCS_SCHEMA_ROOT XML_SYS_PATH_SEPARATOR XML_PCS_UPBPCSBASE)) {
		SetSwitchLedOptions(Object, newValue, NULL, NULL);
	}
	return S_OK;
}

HRESULT STDMETHODCALLTYPE CupbPCS::OnLedOnColorChanged(
	IN IPremiseObject* Object,
	IN VARIANT* newValue
)
{
	if (IsObjectOfExplicitType(Object, XML_UPBPCS_SCHEMA_ROOT XML_SYS_PATH_SEPARATOR XML_PCS_UPBPCSBASE)) {
		SetSwitchLedOptions(Object, NULL, newValue, NULL);
	}
	return S_OK;
}

HRESULT STDMETHODCALLTYPE CupbPCS::OnLedOffColorChanged(
	IN IPremiseObject* Object,
	IN VARIANT* newValue
)
{
	if (IsObjectOfExplicitType(Object, XML_UPBPCS_SCHEMA_ROOT XML_SYS_PATH_SEPARATOR XML_PCS_UPBPCSBASE)) {
		SetSwitchLedOptions(Object, NULL, NULL, newValue);
	}
	return S_OK;
}

HRESULT STDMETHODCALLTYPE CupbPCS::OnEventPacketTypeChanged(
	IN IPremiseObject* Object,
	IN VARIANT* newValue
)
{
	if (IsObjectOfExplicitType(Object, XML_UPBPCS_SCHEMA_ROOT XML_SYS_PATH_SEPARATOR XML_PCS_UPBPCSBASE)) {
		SetTransmissionOptions(Object, newValue, NULL, NULL, NULL, NULL);
	}
	return S_OK;
}

HRESULT STDMETHODCALLTYPE CupbPCS::OnAckMessageRequestChanged(
	IN IPremiseObject* Object,
	IN VARIANT* newValue
)
{
	if (IsObjectOfExplicitType(Object, XML_UPBPCS_SCHEMA_ROOT XML_SYS_PATH_SEPARATOR XML_PCS_UPBPCSBASE)) {
		SetTransmissionOptions(Object, NULL, newValue, NULL, NULL, NULL);
	}
	return S_OK;
}

HRESULT STDMETHODCALLTYPE CupbPCS::OnIdPulseRequestChanged(
	IN IPremiseObject* Object,
	IN VARIANT* newValue
)
{
	if (IsObjectOfExplicitType(Object, XML_UPBPCS_SCHEMA_ROOT XML_SYS_PATH_SEPARATOR XML_PCS_UPBPCSBASE)) {
		SetTransmissionOptions(Object, NULL, NULL, newValue, NULL, NULL);
	}
	return S_OK;
}

HRESULT STDMETHODCALLTYPE CupbPCS::OnAckPulseRequestChanged(
	IN IPremiseObject* Object,
	IN VARIANT* newValue
)
{
	if (IsObjectOfExplicitType(Object, XML_UPBPCS_SCHEMA_ROOT XML_SYS_PATH_SEPARATOR XML_PCS_UPBPCSBASE)) {
		SetTransmissionOptions(Object, NULL, NULL, NULL, newValue, NULL);
	}
	return S_OK;
}

HRESULT STDMETHODCALLTYPE CupbPCS::OnTransmissionCountChanged(
	IN IPremiseObject* Object,
	IN VARIANT* newValue
)
{
	if (IsObjectOfExplicitType(Object, XML_UPBPCS_SCHEMA_ROOT XML_SYS_PATH_SEPARATOR XML_PCS_UPBPCSBASE)) {
		SetTransmissionOptions(Object, NULL, NULL, NULL, NULL, newValue);
	}
	return S_OK;
}

HRESULT STDMETHODCALLTYPE CupbPCS::OnLedBacklightingChanged(
	IN IPremiseObject* Object,
	IN VARIANT* newValue
)
{
	if (IsObjectOfExplicitType(Object, XML_UPBPCS_SCHEMA_ROOT XML_SYS_PATH_SEPARATOR XML_PCS_UPBPCSBASE)) {
		SetKeypadLedOptions(Object, newValue, NULL, NULL, NULL);
	}
	return S_OK;
}

HRESULT STDMETHODCALLTYPE CupbPCS::OnLedTrackingChanged(
	IN IPremiseObject* Object,
	IN VARIANT* newValue
)
{
	if (IsObjectOfExplicitType(Object, XML_UPBPCS_SCHEMA_ROOT XML_SYS_PATH_SEPARATOR XML_PCS_UPBPCSBASE)) {
		SetKeypadLedOptions(Object, NULL, newValue, NULL, NULL);
	}
	return S_OK;
}

HRESULT STDMETHODCALLTYPE CupbPCS::OnLedOutputChanged(
	IN IPremiseObject* Object,
	IN VARIANT* newValue
)
{
	if (IsObjectOfExplicitType(Object, XML_UPBPCS_SCHEMA_ROOT XML_SYS_PATH_SEPARATOR XML_PCS_UPBPCSBASE)) {
		SetKeypadLedOptions(Object, NULL, NULL, newValue, NULL);
	}
	return S_OK;
}

HRESULT STDMETHODCALLTYPE CupbPCS::OnLedBrightnessChanged(
	IN IPremiseObject* Object,
	IN VARIANT* newValue
)
{
	if (IsObjectOfExplicitType(Object, XML_UPBPCS_SCHEMA_ROOT XML_SYS_PATH_SEPARATOR XML_PCS_UPBPCSBASE)) {
		SetKeypadLedOptions(Object, NULL, NULL, NULL, newValue);
	}
	return S_OK;
}

HRESULT STDMETHODCALLTYPE CupbPCS::OnLoadTypeChanged(
	IN IPremiseObject* Object,
	IN VARIANT* newValue
)
{
	if (IsObjectOfExplicitType(Object, XML_UPBPCS_SCHEMA_ROOT XML_SYS_PATH_SEPARATOR XML_PCS_UPBPCSBASE)) {
		SetDimmerOptions(Object, newValue, NULL, NULL);
	}
	return S_OK;
}

HRESULT STDMETHODCALLTYPE CupbPCS::OnReportStateChanged(
	IN IPremiseObject* Object,
	IN VARIANT* newValue
)
{
	if (IsObjectOfExplicitType(Object, XML_UPBPCS_SCHEMA_ROOT XML_SYS_PATH_SEPARATOR XML_PCS_UPBPCSBASE)) {
		SetDimmerOptions(Object, NULL, newValue, NULL);
	}
	return S_OK;
}

HRESULT STDMETHODCALLTYPE CupbPCS::OnDefaultFadeRateChanged(
	IN IPremiseObject* Object,
	IN VARIANT* newValue
)
{
	if (IsObjectOfExplicitType(Object, XML_UPBPCS_SCHEMA_ROOT XML_SYS_PATH_SEPARATOR XML_PCS_SWITCH)) {
		SetDimmerOptions(Object, NULL, NULL, newValue);
	}
	return S_OK;
}

HRESULT STDMETHODCALLTYPE CupbPCS::OnFadeRateChanged(
	IN IPremiseObject* Object,
	IN VARIANT* newValue
)
{
	if (IsObjectOfExplicitType(Object, XML_UPB_SCHEMA_ROOT XML_SYS_PATH_SEPARATOR XML_UPB_SWITCHRECEIVECOMPONENT)) {
		BYTE FadeRate = static_cast<BYTE>(newValue->ulVal);
		if (FadeRate > UPB_SWITCH_FADERATE_1HOUR) {
			FadeRate = UPB_SWITCH_FADERATE_DEFAULT;
		}
		SetSwitchReceiveComponentItem(Object, FadeRate, 2);
	} else if (IsObjectOfExplicitType(Object, XML_UPBPCS_SCHEMA_ROOT XML_SYS_PATH_SEPARATOR XML_PCS_ROCKERACTION)) {
		BYTE FadeRate = static_cast<BYTE>(newValue->ulVal);
		if (FadeRate > UPB_SWITCH_FADERATE_1HOUR) {
			FadeRate = UPB_SWITCH_FADERATE_DEFAULT;
		}
		SetRockerActionItem(Object, FadeRate, 2, 1);
	}
	return S_OK;
}

HRESULT STDMETHODCALLTYPE CupbPCS::OnLightLevelChanged(
	IN IPremiseObject* Object,
	IN VARIANT* newValue
)
{
	if (IsObjectOfExplicitType(Object, XML_UPB_SCHEMA_ROOT XML_SYS_PATH_SEPARATOR XML_UPB_SWITCHRECEIVECOMPONENT)) {
		CComVariant LastOnLevel;
		if (FAILED(Object->GetValue(XML_UPB_LASTONLEVEL, &LastOnLevel))) {
			LastOnLevel = FALSE;
		}
		if (!LastOnLevel.boolVal) {
			SetSwitchReceiveComponentItem(Object, static_cast<BYTE>(static_cast<long>(newValue->dblVal * 100 + 0.5)), 1);
		}
	} else if (IsObjectOfExplicitType(Object, XML_UPBPCS_SCHEMA_ROOT XML_SYS_PATH_SEPARATOR XML_PCS_ROCKERACTION)) {
		CComVariant LastOnLevel;
		if (FAILED(Object->GetValue(XML_UPB_LASTONLEVEL, &LastOnLevel))) {
			LastOnLevel = FALSE;
		}
		if (!LastOnLevel.boolVal) {
			SetRockerActionItem(Object, static_cast<BYTE>(static_cast<long>(newValue->dblVal * 100 + 0.5)), 2, 0);
		}
		//
		// If this is a top side rocker action, and the ButtonState
		// handled is Press, then pass on the new value to the
		// PresetDim property.
		//
		CComVariant ButtonState;
		CComPtr<IPremiseObject> Button;
		CComVariant ButtonID;
		CComPtr<IPremiseObject> Module;
		if (SUCCEEDED(Object->GetValue(XML_SYS_BUTTONSTATE, &ButtonState)) && (ButtonState.lVal == SYS_BUTTON_STATE_PRESS) && SUCCEEDED(Object->get_Parent(&Button)) && SUCCEEDED(Button->GetValue(XML_PCS_BUTTONID, &ButtonID)) && (ButtonID.lVal & 1) && SUCCEEDED(Button->get_Parent(&Module))) {
			Module->SetValueEx(SVCC_DRIVER | SVCC_NOTIFY, XML_SYS_PRESETDIM, newValue);
		}
	}
	return S_OK;
}

HRESULT STDMETHODCALLTYPE CupbPCS::OnLastOnLevelChanged(
	IN IPremiseObject* Object,
	IN VARIANT* newValue
)
{
	if (IsObjectOfExplicitType(Object, XML_UPB_SCHEMA_ROOT XML_SYS_PATH_SEPARATOR XML_UPB_SWITCHRECEIVECOMPONENT)) {
		CComVariant LightLevel;
		BYTE Level;
		if (newValue->boolVal) {
			Level = UPB_SWITCH_LASTONLEVEL;
		} else if (SUCCEEDED(Object->GetValue(XML_UPB_LIGHTLEVEL, &LightLevel))) {
			Level = static_cast<BYTE>(static_cast<long>(newValue->dblVal * 100 + 0.5));
		} else {
			Level = 0;
		}
		SetSwitchReceiveComponentItem(Object, Level, 1);
	} else if (IsObjectOfExplicitType(Object, XML_UPBPCS_SCHEMA_ROOT XML_SYS_PATH_SEPARATOR XML_PCS_ROCKERACTION)) {
		CComVariant LightLevel;
		BYTE Level;
		if (newValue->boolVal) {
			Level = UPB_SWITCH_LASTONLEVEL;
		} else if (SUCCEEDED(Object->GetValue(XML_UPB_LIGHTLEVEL, &LightLevel))) {
			Level = static_cast<BYTE>(static_cast<long>(newValue->dblVal * 100 + 0.5));
		} else {
			Level = 0;
		}
		SetRockerActionItem(Object, Level, 2, 0);
	}
	return S_OK;
}

HRESULT STDMETHODCALLTYPE CupbPCS::OnPresetDimChanged(
	IN IPremiseObject* Object,
	IN VARIANT* newValue
)
{
	if (IsObjectOfExplicitType(Object, XML_UPBPCS_SCHEMA_ROOT XML_SYS_PATH_SEPARATOR XML_PCS_SWITCH)) {
		//
		// Just pass it on to the top Press Rocker Action.
		//
		CComPtr<IPremiseObjectCollection> Buttons;
		CComPtr<IPremiseObject> Button;
		CComPtr<IPremiseObjectCollection> Actions;
		CComPtr<IPremiseObject> Action;
		if (SUCCEEDED(Object->GetObjectsByTypeAndPropertyValue(XML_UPBPCS_SCHEMA_ROOT XML_SYS_PATH_SEPARATOR XML_PCS_SWITCHBUTTON, XML_PCS_BUTTONID, CComVariant(1), collectionTypeNoRecurse, &Buttons)) && SUCCEEDED(Buttons->GetItemEx(0, &Button)) && SUCCEEDED(Button->GetObjectsByTypeAndPropertyValue(XML_UPBPCS_SCHEMA_ROOT XML_SYS_PATH_SEPARATOR XML_PCS_ROCKERACTION, XML_SYS_BUTTONSTATE, CComVariant(SYS_BUTTON_STATE_PRESS), collectionTypeNoRecurse, &Actions)) && SUCCEEDED(Actions->GetItemEx(0, &Action))) {
			Action->SetValueEx(SVCC_NOTIFY, XML_UPB_LIGHTLEVEL, newValue);
		}
	}
	return S_OK;
}

HRESULT STDMETHODCALLTYPE CupbPCS::OnLinkIdChanged(
	IN IPremiseObject* Object,
	IN VARIANT* newValue
)
{
	if (IsObjectOfExplicitType(Object, XML_UPB_SCHEMA_ROOT XML_SYS_PATH_SEPARATOR XML_UPB_TRANSMITCOMPONENT)) {
		CComPtr<IPremiseObject> Button;
		CComPtr<IPremiseObject> Module;
		if (SUCCEEDED(Object->get_Parent(&Button)) && SUCCEEDED(Button->get_Parent(&Module))) {
			ULONG ButtonIDs;
			BYTE Register;
			if (IsObjectOfExplicitType(Module, XML_UPBPCS_SCHEMA_ROOT XML_SYS_PATH_SEPARATOR XML_PCS_SWITCH)) {
				ButtonIDs = 2;
				Register = PCS_SWITCH_REG_TRANSMITCOMPONENTS;
			} else if (IsObjectOfExplicitType(Module, XML_UPBPCS_SCHEMA_ROOT XML_SYS_PATH_SEPARATOR XML_PCS_KEYPAD)) {
				ButtonIDs = 8;
				Register = PCS_KEYPAD_REG_TRANSMITCOMPONENTS;
			} else {
				return S_OK;
			}
			CComVariant ButtonID;
			if (SUCCEEDED(Button->GetValue(XML_PCS_BUTTONID, &ButtonID)) && (ButtonID.ulVal <= ButtonIDs)) {
				char data[16];
				StringCchPrintfA(data, SIZEOF_ARRAY(data), "%02X%02X%02X", MAKE_MDID(MDID_CORE_COMMANDS, MDID_CORE_COMMAND_SETREGISTERVALUES), Register + (ButtonID.ulVal - 1) * PCS_REG_TXCOMPONENTENTRY, newValue->bVal);
				Module->SetValueEx(SVCC_NOTIFY | SVCC_FORCE, XML_UPB_UPBNETWORKMESSAGE, &CComVariant(data));
			}
		}
	} else if (IsObjectOfExplicitType(Object, XML_UPB_SCHEMA_ROOT XML_SYS_PATH_SEPARATOR XML_UPB_SWITCHRECEIVECOMPONENT)) {
		SetSwitchReceiveComponentItem(Object, static_cast<BYTE>(newValue->ulVal), 0);
	} else if (IsObjectOfExplicitType(Object, XML_UPB_SCHEMA_ROOT XML_SYS_PATH_SEPARATOR XML_UPB_RECEIVECOMPONENT)) {
		SetKeypadReceiveComponentLinkID(Object, static_cast<BYTE>(newValue->ulVal));
	}
	return S_OK;
}

HRESULT STDMETHODCALLTYPE CupbPCS::OnCommandChanged(
	IN IPremiseObject* Object,
	IN VARIANT* newValue
)
{
	if (IsObjectOfExplicitType(Object, XML_UPB_SCHEMA_ROOT XML_SYS_PATH_SEPARATOR XML_UPB_TRANSMITCOMMAND) && (lstrlenW(newValue->bstrVal) == 2 * PCS_REG_TXCOMMANDENTRY)) {
		CComPtr<IPremiseObject> TransmitCommands;
		CComPtr<IPremiseObject> Module;
		if (SUCCEEDED(Object->get_Parent(&TransmitCommands)) && SUCCEEDED(TransmitCommands->get_Parent(&Module))) {
			ULONG CommandIDs;
			if (IsObjectOfExplicitType(Module, XML_UPBPCS_SCHEMA_ROOT XML_SYS_PATH_SEPARATOR XML_PCS_SWITCH)) {
				CommandIDs = PCS_SWITCH_COMMANDIDS;
			} else if (IsObjectOfExplicitType(Module, XML_UPBPCS_SCHEMA_ROOT XML_SYS_PATH_SEPARATOR XML_PCS_KEYPAD)) {
				CommandIDs = PCS_KEYPAD_COMMANDIDS;
			} else {
				return S_OK;
			}
			CComVariant CommandID;
			if (SUCCEEDED(Object->GetValue(XML_UPB_COMMANDID, &CommandID)) && (CommandID.ulVal < CommandIDs)) {
				char data[16];
				StringCchPrintfA(data, SIZEOF_ARRAY(data), "%02X%02X%S", MAKE_MDID(MDID_CORE_COMMANDS, MDID_CORE_COMMAND_SETREGISTERVALUES), PCS_REG_TRANSMITCOMMANDS + CommandID.ulVal * PCS_REG_TXCOMMANDENTRY, newValue->bstrVal);
				Module->SetValueEx(SVCC_NOTIFY | SVCC_FORCE, XML_UPB_UPBNETWORKMESSAGE, &CComVariant(data));
			}
		}
	}
	return S_OK;
}

HRESULT OnToggleChanged(
	IN IPremiseObject* Object,
	IN BYTE ToggleType,
	IN VARIANT* newValue
)
{
	if (IsObjectOfExplicitType(Object, XML_UPBPCS_SCHEMA_ROOT XML_SYS_PATH_SEPARATOR XML_UPB_COMMANDID)) {
		CComPtr<IPremiseObject> TransmitComponent;
		CComPtr<IPremiseObject> Button;
		CComPtr<IPremiseObject> Module;
		if (SUCCEEDED(Object->get_Parent(&TransmitComponent)) && SUCCEEDED(TransmitComponent->get_Parent(&Button)) && SUCCEEDED(Button->get_Parent(&Module))) {
			ULONG ButtonIDs;
			BYTE Register;
			if (IsObjectOfExplicitType(Module, XML_UPBPCS_SCHEMA_ROOT XML_SYS_PATH_SEPARATOR XML_PCS_SWITCH)) {
				ButtonIDs = 2;
				Register = PCS_SWITCH_REG_TRANSMITCOMPONENTS;
			} else if (IsObjectOfExplicitType(Module, XML_UPBPCS_SCHEMA_ROOT XML_SYS_PATH_SEPARATOR XML_PCS_KEYPAD)) {
				ButtonIDs = 8;
				Register = PCS_KEYPAD_REG_TRANSMITCOMPONENTS;
			} else {
				return S_OK;
			}
			CComVariant ButtonState;
			CComVariant ButtonID;
			CComVariant OtherToggle;
			if (SUCCEEDED(Object->GetValue(XML_SYS_BUTTONSTATE, &ButtonState)) && (ButtonState.lVal >= SYS_BUTTON_STATE_RELEASE) && (ButtonState.lVal <= SYS_BUTTON_STATE_DOUBLETAP) && SUCCEEDED(Button->GetValue(XML_PCS_BUTTONID, &ButtonID)) && (ButtonID.ulVal <= ButtonIDs) && SUCCEEDED(Object->GetValue(ToggleType ? XML_PCS_ONTOGGLE : XML_PCS_OFFTOGGLE, &OtherToggle))) {
				BYTE Toggle;
				if (ToggleType) {
					Toggle = (newValue->bVal << 4) + (OtherToggle.bVal & 0x0f);
				} else {
					Toggle = (OtherToggle.bVal << 4) + (newValue->bVal & 0x0f);
				}
				char data[16];
				StringCchPrintfA(data, SIZEOF_ARRAY(data), "%02X%02X%02X", MAKE_MDID(MDID_CORE_COMMANDS, MDID_CORE_COMMAND_SETREGISTERVALUES), Register + (ButtonID.ulVal - 1) * PCS_REG_TXCOMPONENTENTRY + PremiseButtonStates[ButtonState.ulVal] + 1, Toggle);
				Module->SetValueEx(SVCC_NOTIFY | SVCC_FORCE, XML_UPB_UPBNETWORKMESSAGE, &CComVariant(data));
			}
		}
	}
	return S_OK;
}

HRESULT STDMETHODCALLTYPE CupbPCS::OnOnToggleChanged(
	IN IPremiseObject* Object,
	IN VARIANT* newValue
)
{
	return OnToggleChanged(Object, 0, newValue);
}

HRESULT STDMETHODCALLTYPE CupbPCS::OnOffToggleChanged(
	IN IPremiseObject* Object,
	IN VARIANT* newValue
)
{
	return OnToggleChanged(Object, 1, newValue);
}

HRESULT STDMETHODCALLTYPE CupbPCS::OnBrightnessChanged(
	IN IPremiseObject* Object,
	IN VARIANT* newValue
)
{
	if (IsObjectOfExplicitType(Object, XML_UPBPCS_SCHEMA_ROOT XML_SYS_PATH_SEPARATOR XML_PCS_SWITCH)) {
		BYTE Brightness = static_cast<BYTE>(static_cast<long>(newValue->dblVal * 100 + 0.5));
		char Data[16];
		StringCchPrintfA(Data, sizeof(Data), "%02X", Brightness);
		TransmitNetworkPacket(Object, MAKE_MDID(MDID_DEVICE_CONTROL_COMMANDS, MDID_DEVICE_CONTROL_COMMAND_GOTO), Data);
		Object->SetValueEx(SVCC_DRIVER | SVCC_NOTIFY, XML_SYS_POWERSTATE, &CComVariant(Brightness));
	}
	return S_OK;
}

HRESULT STDMETHODCALLTYPE CupbPCS::OnPowerStateChanged(
	IN IPremiseObject* Object,
	IN VARIANT* newValue
)
{
	if (IsObjectOfExplicitType(Object, XML_UPBPCS_SCHEMA_ROOT XML_SYS_PATH_SEPARATOR XML_PCS_SWITCH)) {
		char Data[16];
		StringCchPrintfA(Data, sizeof(Data), "%02X%02X", newValue->boolVal ? 100 : 0, UPB_SWITCH_FADERATE_SNAP);
		TransmitNetworkPacket(Object, MAKE_MDID(MDID_DEVICE_CONTROL_COMMANDS, MDID_DEVICE_CONTROL_COMMAND_GOTO), Data);
		Object->SetValueEx(SVCC_DRIVER | SVCC_NOTIFY, XML_SYS_BRIGHTNESS, &CComVariant(newValue->boolVal ? 1 : 0));
	}
	return S_OK;
}

HRESULT STDMETHODCALLTYPE CupbPCS::OnTriggerChanged(
	IN IPremiseObject* Object,
	IN VARIANT* newValue
)
{
	CComVariant ButtonID;
	if (IsObjectOfExplicitType(Object, XML_UPBPCS_SCHEMA_ROOT XML_SYS_PATH_SEPARATOR XML_PCS_SWITCHBUTTON) && SUCCEEDED(Object->GetValue(XML_PCS_BUTTONID, &ButtonID)) && (ButtonID.lVal > 0) && (ButtonID.lVal <= 2)) {
		CComPtr<IPremiseObject> Switch;
		CComVariant LoadType;
		BOOL DimmerLoadType = SUCCEEDED(Object->get_Parent(&Switch)) && SUCCEEDED(Switch->GetValue(XML_PCS_LOADTYPE, &LoadType)) && (LoadType.lVal == 1);
		if (DimmerLoadType || newValue->boolVal) {
			char Data[16];
			BYTE mdid;
			if (DimmerLoadType) {
				if (newValue->boolVal) {
					mdid = MAKE_MDID(MDID_DEVICE_CONTROL_COMMANDS, MDID_DEVICE_CONTROL_COMMAND_FADESTART);
					StringCchPrintfA(Data, sizeof(Data), "%02X%02X", ButtonID.lVal & 1 ? 100 : 0, UPB_SWITCH_FADERATE_6POINT6SECONDS);
				} else {
					mdid = MAKE_MDID(MDID_DEVICE_CONTROL_COMMANDS, MDID_DEVICE_CONTROL_COMMAND_FADESTOP);
					Data[0] = 0;
				}
			} else if (newValue->boolVal) {
				StringCchPrintfA(Data, sizeof(Data), "%02X%02X", ButtonID.lVal & 1 ? 100 : 0, UPB_SWITCH_FADERATE_SNAP);
				mdid = MAKE_MDID(MDID_DEVICE_CONTROL_COMMANDS, MDID_DEVICE_CONTROL_COMMAND_GOTO);
			} else {
				return S_OK;;
			}
			TransmitNetworkPacket(Switch, mdid, Data);
		}
	}
	return S_OK;
}

HRESULT STDMETHODCALLTYPE CupbPCS::OnVirtualizeChanged(
	IN IPremiseObject* Object,
	IN VARIANT* newValue
)
{
	if (newValue->boolVal) {
		if (IsObjectOfExplicitType(Object, XML_UPBPCS_SCHEMA_ROOT XML_SYS_PATH_SEPARATOR XML_PCS_WS1D6)) {
			VirtualizeSwitch(Object);
		} else if (IsObjectOfExplicitType(Object, XML_UPBPCS_SCHEMA_ROOT XML_SYS_PATH_SEPARATOR XML_PCS_KEYPAD)) {
			VirtualizeKeypad(Object);
		}
	}
	return S_OK;
}

HRESULT STDMETHODCALLTYPE CupbPCS::OnStatusChanged(
	IN IPremiseObject* Object,
	IN VARIANT* newValue
)
{
	if (IsObjectOfExplicitType(Object, XML_UPBPCS_SCHEMA_ROOT XML_SYS_PATH_SEPARATOR XML_PCS_KEYPADBUTTON)) {
		CComVariant ButtonID;
		//
		// Just test for 8 buttons, not 6 or 8, since the WMC6 can
		// ignore the invalid command.
		//
		CComPtr<IPremiseObject> Module;
		if (SUCCEEDED(Object->GetValue(XML_PCS_BUTTONID, &ButtonID)) && (ButtonID.lVal > 0) && (ButtonID.lVal <= 8) && SUCCEEDED(Object->get_Parent(&Module))) {
			char r[16];
			StringCchPrintfA(r, SIZEOF_ARRAY(r), "%02X%02X%02X", newValue->boolVal ? 100 : 0, UPB_SWITCH_FADERATE_DEFAULT, ButtonID.lVal);
			TransmitNetworkPacket(Module, MAKE_MDID(MDID_DEVICE_CONTROL_COMMANDS, MDID_DEVICE_CONTROL_COMMAND_GOTO), r);
		}
	}
	return S_OK;
}

HRESULT STDMETHODCALLTYPE CupbPCS::OnIrEnabledChanged(
	IN IPremiseObject* Object,
	IN VARIANT* newValue
)
{
	if (IsObjectOfExplicitType(Object, XML_UPBPCS_SCHEMA_ROOT XML_SYS_PATH_SEPARATOR XML_PCS_WMC6)) {
		SetIrOptions(Object, newValue, NULL);
	}
	return S_OK;
}

HRESULT STDMETHODCALLTYPE CupbPCS::OnIrRoomChanged(
	IN IPremiseObject* Object,
	IN VARIANT* newValue
)
{
	if (IsObjectOfExplicitType(Object, XML_UPBPCS_SCHEMA_ROOT XML_SYS_PATH_SEPARATOR XML_PCS_WMC6)) {
		SetIrOptions(Object, NULL, newValue);
	}
	return S_OK;
}

HRESULT STDMETHODCALLTYPE CupbPCS::OnUpbNetworkIdleChanged(
	IN IPremiseObject* Object,
	IN VARIANT* newValue
)
{
	if (newValue->boolVal) {
		if (IsObjectOfExplicitType(Object, XML_UPBPCS_SCHEMA_ROOT XML_SYS_PATH_SEPARATOR XML_PCS_WS1D6)) {
			TransmitNetworkPacket(Object, MAKE_MDID(MDID_DEVICE_CONTROL_COMMANDS, MDID_DEVICE_CONTROL_COMMAND_REPORTSTATE));
		} else if (IsObjectOfExplicitType(Object, XML_UPBPCS_SCHEMA_ROOT XML_SYS_PATH_SEPARATOR XML_PCS_KEYPAD)) {
			char r[16];
			StringCchPrintfA(r, SIZEOF_ARRAY(r), "%02X%02X", PCS_KEYPAD_REG_LEDSTATES, PCS_KEYPAD_REG_LEDSTATES_BYTES);
			TransmitNetworkPacket(Object, MAKE_MDID(MDID_CORE_COMMANDS, MDID_CORE_COMMAND_GETREGISTERVALUES), r);
		}
	}
	return S_OK;
}

HRESULT STDMETHODCALLTYPE CupbPCS::OnLedActionChanged(
	IN IPremiseObject* Object,
	IN VARIANT* newValue
)
{
	if (IsObjectOfExplicitType(Object, XML_UPBPCS_SCHEMA_ROOT XML_SYS_PATH_SEPARATOR XML_PCS_LEDONACTION)) {
		SetLedOnActions(Object, newValue, NULL, NULL);
	}
	return S_OK;
}

HRESULT STDMETHODCALLTYPE CupbPCS::OnLedActionGroupChanged(
	IN IPremiseObject* Object,
	IN VARIANT* newValue
)
{
	if (IsObjectOfExplicitType(Object, XML_UPBPCS_SCHEMA_ROOT XML_SYS_PATH_SEPARATOR XML_PCS_LEDONACTION)) {
		SetLedOnActions(Object, NULL, newValue, NULL);
	}
	return S_OK;
}

HRESULT STDMETHODCALLTYPE CupbPCS::OnLedGroupMaskChanged(
	IN IPremiseObject* Object,
	IN VARIANT* newValue
)
{
	if (IsObjectOfExplicitType(Object, XML_UPBPCS_SCHEMA_ROOT XML_SYS_PATH_SEPARATOR XML_PCS_LEDONACTION)) {
		SetLedOnActions(Object, NULL, NULL, newValue);
	}
	return S_OK;
}

/*
	Overrides IPremiseNotifyImpl::OnPropertyChanged in order to pass on
	notification of that change to the associated object if needed.
		subscriptionID - Unique identifier returned by notification
		subscription. Not used.
		transactionID - Unique transaction identifier. Not used.
		propagationID - Unique propagation identifier. Not used.
		controlCode - Not used.
		Object - The owner of the property that was changed.
		Property - The property that was changed.
		NewValue - The new value of the property.
		OldValue - The previous value of the property.
	Return - S_OK.
*/
HRESULT STDMETHODCALLTYPE CupbPCS::OnPropertyChanged(
	IN long subscriptionID,
	IN long transactionID,
	IN long propagationID,
	IN long controlCode,
	IN IPremiseObject* Object,
	IN IPremiseObject* Property,
	IN VARIANT* NewValue,
	IN VARIANT* OldValue
)
{
	//
	// Ignore changes that are made by this driver, and changes that
	// have not yet been committed or represent a reset on an
	// autoreset property.
	//
	if (!(controlCode & (SVCC_DRIVER | SVCC_VOLATILE))) {
		CComVariant PropertyName;
		CComPtr<IPremiseObject> Ancestor;
		//
		// Filter based on properties of objects created by this
		// driver.
		//
		if (SUCCEEDED(Object->GetAncestorByType(XML_UPBPCS_SCHEMA_ROOT XML_SYS_PATH_SEPARATOR XML_PCS_UPBPCSBASE, &Ancestor)) && SUCCEEDED(Property->GetValue(XML_SYS_NAME, &PropertyName))) {
			if (!lstrcmpW(PropertyName.bstrVal, XML_UPB_UPBNETWORKDATA)) {
				return OnUpbNetworkDataChanged(Object, NewValue);
			} else if (!lstrcmpW(PropertyName.bstrVal, XML_SYS_BRIGHTNESS)) {
				return OnBrightnessChanged(Object, NewValue);
			} else if (!lstrcmpW(PropertyName.bstrVal, XML_SYS_POWERSTATE)) {
				return OnPowerStateChanged(Object, NewValue);
			} else if (!lstrcmpW(PropertyName.bstrVal, XML_UPB_UPBDEVICEINITIALIZED)) {
				return OnDeviceInitializedChanged(Object, NewValue);
			} else if (!lstrcmpW(PropertyName.bstrVal, XML_UPB_LEDMODE)) {
				return OnLedModeChanged(Object, NewValue);
			} else if (!lstrcmpW(PropertyName.bstrVal, XML_UPB_LEDONCOLOR)) {
				return OnLedOnColorChanged(Object, NewValue);
			} else if (!lstrcmpW(PropertyName.bstrVal, XML_UPB_LEDOFFCOLOR)) {
				return OnLedOffColorChanged(Object, NewValue);
			} else if (!lstrcmpW(PropertyName.bstrVal, XML_UPB_EVENTPACKETTYPE)) {
				return OnEventPacketTypeChanged(Object, NewValue);
			} else if (!lstrcmpW(PropertyName.bstrVal, XML_UPB_ACKMESSAGEREQUEST)) {
				return OnAckMessageRequestChanged(Object, NewValue);
			} else if (!lstrcmpW(PropertyName.bstrVal, XML_UPB_IDPULSEREQUEST)) {
				return OnIdPulseRequestChanged(Object, NewValue);
			} else if (!lstrcmpW(PropertyName.bstrVal, XML_UPB_ACKPULSEREQUEST)) {
				return OnAckPulseRequestChanged(Object, NewValue);
			} else if (!lstrcmpW(PropertyName.bstrVal, XML_UPB_TRANSMISSIONCOUNT)) {
				return OnTransmissionCountChanged(Object, NewValue);
			} else if (!lstrcmpW(PropertyName.bstrVal, XML_PCS_LEDBACKLIGHTING)) {
				return OnLedBacklightingChanged(Object, NewValue);
			} else if (!lstrcmpW(PropertyName.bstrVal, XML_PCS_LEDTRACKING)) {
				return OnLedTrackingChanged(Object, NewValue);
			} else if (!lstrcmpW(PropertyName.bstrVal, XML_PCS_LEDOUTPUT)) {
				return OnLedOutputChanged(Object, NewValue);
			} else if (!lstrcmpW(PropertyName.bstrVal, XML_PCS_LEDBRIGHTNESS)) {
				return OnLedBrightnessChanged(Object, NewValue);
			} else if (!lstrcmpW(PropertyName.bstrVal, XML_PCS_LOADTYPE)) {
				return OnLoadTypeChanged(Object, NewValue);
			} else if (!lstrcmpW(PropertyName.bstrVal, XML_PCS_REPORTSTATE)) {
				return OnReportStateChanged(Object, NewValue);
			} else if (!lstrcmpW(PropertyName.bstrVal, XML_UPB_DEFAULTFADERATE)) {
				return OnDefaultFadeRateChanged(Object, NewValue);
			} else if (!lstrcmpW(PropertyName.bstrVal, XML_UPB_FADERATE)) {
				return OnFadeRateChanged(Object, NewValue);
			} else if (!lstrcmpW(PropertyName.bstrVal, XML_UPB_LIGHTLEVEL)) {
				return OnLightLevelChanged(Object, NewValue);
			} else if (!lstrcmpW(PropertyName.bstrVal, XML_UPB_LASTONLEVEL)) {
				return OnLastOnLevelChanged(Object, NewValue);
			} else if (!lstrcmpW(PropertyName.bstrVal, XML_SYS_STATUS)) {
				return OnStatusChanged(Object, NewValue);
			} else if (!lstrcmpW(PropertyName.bstrVal, XML_UPB_LINKID)) {
				return OnLinkIdChanged(Object, NewValue);
			} else if (!lstrcmpW(PropertyName.bstrVal, XML_UPB_COMMAND)) {
				return OnCommandChanged(Object, NewValue);
			} else if (!lstrcmpW(PropertyName.bstrVal, XML_PCS_ONTOGGLE)) {
				return OnOnToggleChanged(Object, NewValue);
			} else if (!lstrcmpW(PropertyName.bstrVal, XML_PCS_OFFTOGGLE)) {
				return OnOffToggleChanged(Object, NewValue);
			} else if (!lstrcmpW(PropertyName.bstrVal, XML_SYS_TRIGGER)) {
				return OnTriggerChanged(Object, NewValue);
			} else if (!lstrcmpW(PropertyName.bstrVal, XML_UPB_VIRTUALIZE)) {
				return OnVirtualizeChanged(Object, NewValue);
			} else if (!lstrcmpW(PropertyName.bstrVal, XML_PCS_IRENABLED)) {
				return OnIrEnabledChanged(Object, NewValue);
			} else if (!lstrcmpW(PropertyName.bstrVal, XML_PCS_IRROOM)) {
				return OnIrRoomChanged(Object, NewValue);
			} else if (!lstrcmpW(PropertyName.bstrVal, XML_SYS_PRESETDIM)) {
				return OnPresetDimChanged(Object, NewValue);
			} else if (!lstrcmpW(PropertyName.bstrVal, XML_PCS_LEDACTION)) {
				return OnLedActionChanged(Object, NewValue);
			} else if (!lstrcmpW(PropertyName.bstrVal, XML_PCS_LEDACTIONGROUP)) {
				return OnLedActionGroupChanged(Object, NewValue);
			} else if (!lstrcmpW(PropertyName.bstrVal, XML_PCS_LEDGROUPMASK)) {
				return OnLedGroupMaskChanged(Object, NewValue);
			} else if (!lstrcmpW(PropertyName.bstrVal, XML_UPB_UPBNETWORKIDLE)) {
				return OnUpbNetworkIdleChanged(Object, NewValue);
			}
		}
	}
	return E_NOTIMPL;
}
