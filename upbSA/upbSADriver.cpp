#include "stdafx.h"
#include <stdio.h>
#include <stdlib.h>
#include <IPTypes.h>
#include <iphlpapi.h>
#define STRSAFE_LIB
#include <strsafe.h>
#include "upbSA.h"
#include "upbSADriver.h"

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
HRESULT CupbSA::OnBrokerDetach(
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
HRESULT CupbSA::OnBrokerAttach(
)
{
	m_upbSubscription = 0;
	HRESULT hr = m_spSite->GetObject(XML_UPB_ROOT, &m_upbRoot);
	if (SUCCEEDED(hr)) {
		//
		// Subscribe to the tree, but filter within OnPropertyChanged.
		//
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
		//
		// Allow skipping button identifiers when names are provided.
		//
		if (!ChildNames || *ChildNames[Child - StartID]) {
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

void InitializeSwitchChildObjects(
	IN IPremiseObject* Module,
	IN int ReceiveComponents
)
{
	CComPtr<IPremiseObject> ChildNode;
	if (SUCCEEDED(Module->CreateObject(XML_UPB_SCHEMA_ROOT XML_SYS_PATH_SEPARATOR XML_UPB_SWITCHRECEIVECOMPONENTS, NULL, &ChildNode))) {
		CreateChildrenWithIds(ChildNode, 1, ReceiveComponents, NULL, XML_UPB_SCHEMA_ROOT XML_SYS_PATH_SEPARATOR XML_UPB_SWITCHRECEIVECOMPONENT, XML_UPB_COMPONENTID);
	}
}

//
// The names correspond to the functionality of each button, and are
// ordered by button number, some of which are skipped on various
// configurations.
//
static const char* US24[] = {
	"LeftTopUpper", "LeftTopLower", "LeftBottomUpper", "LeftBottomLower", "RightTopUpper", "RightTopLower", "RightBottomUpper", "RightBottomLower"
};
static const char* US22S[] = {
	"TopUpper", "BottomUpper", "TopLower", "BottomLower"
};
static const char* US23[] = {
	"TopUpper", "BottomUpper", "LeftTopLower", "LeftBottomLower", "", "", "RightTopLower", "RightBottomLower"
};
static const char* US11[] = { // US21
	"Top", "Bottom"
};
static const char* US12[] = { // US22T
	"LeftTop", "LeftBottom", "RightTop", "RightBottom"
};
static const char* US22T[] = {
	"LeftTop", "LeftBottom", "", "", "RightTop", "RightBottom"
};
static const char* US25[] = {
	"TopUpper", "BottomUpper", "Button1", "Button2", "", "", "Button3", "Button4"
};
static const char* US26[] = {
	"LeftTopUpper", "LeftBottomUpper", "Button1", "Button2", "RightTopUpper", "RightBottomUpper", "Button3", "Button4"
};
static const char* US28[] = {
	"Button1", "Button2", "Button3", "Button4", "Button5", "Button6", "Button7", "Button8"
};

typedef struct {
	int RockerOption;
	int Buttons;
	const char** Names;
	BYTE RockerConfig;
} ROCKERCONFIGURATION;
static const ROCKERCONFIGURATION RockerConfigurations[] = {
	SA_ROCKEROPTION_US24, SIZEOF_ARRAY(US24), US24, 0xff, // Half Height Quad Rocker
	SA_ROCKEROPTION_US22S, SIZEOF_ARRAY(US22S), US22S, 0x0f, // Half Height Dual Rocker
	SA_ROCKEROPTION_US23, SIZEOF_ARRAY(US23), US23, 0xcf, // Half Height Triple Rocker
	SA_ROCKEROPTION_US11, SIZEOF_ARRAY(US11), US11, 0x03, // Full Height Single Rocker
	SA_ROCKEROPTION_US12, SIZEOF_ARRAY(US12), US12, 0x0f, // Full Height Dual Rocker
	SA_ROCKEROPTION_US21, SIZEOF_ARRAY(US11), US11, 0x03, // Full Height Single Rocker
	SA_ROCKEROPTION_US22T, SIZEOF_ARRAY(US22T), US22T, 0x33, // Full Height Dual Rocker
	SA_ROCKEROPTION_US25, SIZEOF_ARRAY(US25), US25, 0xcf, // Single Rocker, Four Pushbuttons
	SA_ROCKEROPTION_US26, SIZEOF_ARRAY(US26), US26, 0xff, // Dual Rocker, Four Pushbuttons
	SA_ROCKEROPTION_US28, SIZEOF_ARRAY(US28), US28, 0xff // Eight Pushbuttons
};

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
	IN const BSTR ButtonClass
)
{
	CreateChildrenWithIds(Module, 1, Buttons, ButtonNames, ButtonClass, XML_SA_BUTTONID);
	CComPtr<IPremiseObjectCollection> ButtonCol;
	if (SUCCEEDED(Module->GetObjectsByType(CComVariant(ButtonClass), collectionTypeNoRecurse, &ButtonCol))) {
		long EnumButton = 0;
		CComPtr<IPremiseObject> Button;
		for (; SUCCEEDED(ButtonCol->GetItemEx(EnumButton, &Button)); EnumButton++, Button = NULL) {
			CComPtr<IPremiseObject> TransmitComponent;
			if (SUCCEEDED(Button->CreateObject(XML_UPB_SCHEMA_ROOT XML_SYS_PATH_SEPARATOR XML_UPB_TRANSMITCOMPONENT, NULL, &TransmitComponent))) {
				CreateChildrenWithIds(TransmitComponent, 0, SIZEOF_ARRAY(ButtonStates), ButtonStates, XML_UPBSA_SCHEMA_ROOT XML_SYS_PATH_SEPARATOR XML_UPB_COMMANDID, XML_SYS_BUTTONSTATE);
			}			
		}
	}
}

void ReconfigUsqButtons(
	IN IPremiseObject* Module,
	IN BYTE CurrentVariantSelection,
	IN BYTE NewVariantSelection
)
{
	if (CurrentVariantSelection != NewVariantSelection) {
		for (int i = 0; i < SIZEOF_ARRAY(RockerConfigurations); i++) {
			if (NewVariantSelection == RockerConfigurations[i].RockerOption) {
				//
				// Delete the old button configuration.
				//
				CComPtr<IPremiseObjectCollection> Buttons;
				if (SUCCEEDED(Module->GetObjectsByType(CComVariant(XML_UPBSA_SCHEMA_ROOT XML_SYS_PATH_SEPARATOR XML_SA_KEYPADBUTTON), collectionTypeNoRecurse, &Buttons))) {
					long EnumButton = 0;
					CComPtr<IPremiseObject> Button;
					for (; SUCCEEDED(Buttons->GetItemEx(EnumButton, &Button)); EnumButton++, Button = NULL) {
						Button->DeleteObject(NULL);
					}
				}
				//
				// Remove all references before proceeding.
				//
				Buttons = NULL;
				//
				// Create the new button configuration.
				//
				InitializeCommonChildObjects(Module, RockerConfigurations[i].Buttons, RockerConfigurations[i].Names, XML_UPBSA_SCHEMA_ROOT XML_SYS_PATH_SEPARATOR XML_SA_KEYPADBUTTON);
				//
				// Create the rocker actions for the first 2 buttons.
				//!! except when it is a top rocker config, which would be buttons 1,5? or 8 buttons, and then only button 1?
				//
				CComPtr<IPremiseObjectCollection> ButtonCol;
				if (SUCCEEDED(Module->GetObjectsByType(CComVariant(XML_UPBSA_SCHEMA_ROOT XML_SYS_PATH_SEPARATOR XML_SA_KEYPADBUTTON), collectionTypeNoRecurse, &ButtonCol))) {
					long EnumButton = 0;
					CComPtr<IPremiseObject> Button;
					for (; SUCCEEDED(ButtonCol->GetItemEx(EnumButton, &Button)); EnumButton++, Button = NULL) {
						CComVariant ButtonID;
						if (SUCCEEDED(Button->GetValue(XML_SA_BUTTONID, &ButtonID)) && (ButtonID.lVal > 0) && (ButtonID.lVal <= 2)) {
							for (int Child = 0; Child < SIZEOF_ARRAY(RockerActionsMap); Child++) {
								CComPtr<IPremiseObject> ChildNode;
								if (SUCCEEDED(Button->CreateObject(XML_UPBSA_SCHEMA_ROOT XML_SYS_PATH_SEPARATOR XML_SA_ROCKERACTION, NULL, &ChildNode))) {
									ChildNode->SetValueEx(SVCC_DRIVER | SVCC_NOTIFY, XML_SYS_BUTTONSTATE, &CComVariant(RockerActionsMap[Child].ButtonState));
									ChildNode->SetValueEx(SVCC_DRIVER | SVCC_NOTIFY, XML_SYS_NAME, &CComVariant(RockerActionsMap[Child].ActionName));
									ChildNode = NULL;
								}
							}
						}
					}
				}
				//
				// Apply the new button mask and variant selection to the device.
				//
				CComVariant SetupRegisterCache;
				if (SUCCEEDED(Module->GetValue(XML_UPB_SETUPREGISTERCACHE, &SetupRegisterCache)) && (lstrlenW(SetupRegisterCache.bstrVal) == 2 * UPB_MODULE_REGISTERS)) {
					BYTE RockerOptions;
					parsehex_(&SetupRegisterCache.bstrVal[2 * SA_REG_ROCKEROPTIONS], RockerOptions);
					BYTE RockerConfig;
					parsehex_(&SetupRegisterCache.bstrVal[2 * SA_REG_ROCKERCONFIG], RockerConfig);
					//
					// Determine if the Tall Rocker bit should be set.
					//
					BOOL TallRocker = (SA_USQ_VARIANTSELECTION(RockerOptions) == SA_ROCKEROPTION_US21) || (SA_USQ_VARIANTSELECTION(RockerOptions) == SA_ROCKEROPTION_US22T);
					//
					// Program the device if there is actually a change
					// in the variant selection, or if associated
					// registers do not match the variant.
					//
					if ((SA_USQ_VARIANTSELECTION(RockerOptions) != NewVariantSelection) || (RockerConfig != RockerConfigurations[i].RockerConfig) || (SA_USQ_TALLROCKER(RockerOptions) && !TallRocker) || (!SA_USQ_TALLROCKER(RockerOptions) && TallRocker)) {
						SA_USQ_APPLY_VARIANTSELECTION(RockerOptions, NewVariantSelection);
						SA_USQ_SETOPTION_TALLROCKER(RockerOptions, TallRocker);
						//
						// Make the assumption that these two items are very close together, so that
						// a single write would be better than two separate writes.
						//
#if (SA_REG_ROCKEROPTIONS > SA_REG_ROCKERCONFIG)
						const BYTE StartRegister = SA_REG_ROCKERCONFIG;
						const BYTE EndRegister = SA_REG_ROCKEROPTIONS;
						BYTE FirstRegister = RockerConfigurations[i].RockerConfig;
						BYTE LastRegister = RockerOptions;
#else
						const BYTE StartRegister = SA_REG_ROCKEROPTIONS;
						const BYTE EndRegister = SA_REG_ROCKERCONFIG;
						BYTE FirstRegister = RockerOptions;
						BYTE LastRegister = RockerConfigurations[i].RockerConfig;
#endif
						//
						// Copy the data between the two registers, if any. Then insert the new
						// register data on the front and end of the copied register data.
						//
						WCHAR Registers[2 * (EndRegister - StartRegister - 1) + 1];
						StringCchCopyNW(Registers, SIZEOF_ARRAY(Registers), &SetupRegisterCache.bstrVal[2 * (StartRegister + 1)], 2 * (EndRegister - StartRegister - 1));
						Registers[SIZEOF_ARRAY(Registers) - 1] = 0;
						WCHAR data[16 + SIZEOF_ARRAY(Registers)];
						StringCchPrintfW(data, SIZEOF_ARRAY(data), L"%02X%02X%02X%s%02X", MAKE_MDID(MDID_CORE_COMMANDS, MDID_CORE_COMMAND_SETREGISTERVALUES), StartRegister, FirstRegister, Registers, LastRegister);
						Module->SetValueEx(SVCC_NOTIFY | SVCC_FORCE, XML_UPB_UPBNETWORKMESSAGE, &CComVariant(data));
					}
				}
				break;
			}
		}
	}
}

void InitializeUsqChildObjects(
	IN IPremiseObject* Module
)
{
	CComPtr<IPremiseObject> ChildNode;
	if (SUCCEEDED(Module->CreateObject(XML_UPB_SCHEMA_ROOT XML_SYS_PATH_SEPARATOR XML_UPB_TRANSMITCOMMANDS, NULL, &ChildNode))) {
		CreateChildrenWithIds(ChildNode, 0, SIZEOF_ARRAY(TransmitCommandNames), TransmitCommandNames, XML_UPB_SCHEMA_ROOT XML_SYS_PATH_SEPARATOR XML_UPB_TRANSMITCOMMAND, XML_UPB_COMMANDID);
	}
	InitializeSwitchChildObjects(Module, SA_REG_RECEIVECOMPONENTS_BYTES / SA_REG_RECEIVECOMPONENTENTRY);
}

static const char* RelayNames[] = {
	"Relay1", "Relay2"
};
static const char* InputNames[] = {
	"Input1", "Input2", "Input3"
};
static const char* TransmitNames[] = {
	"CloseEvent", "OpenEvent"
};

void InitializeCmChildObjects(
	IN IPremiseObject* Module
)
{
	//
	// Create the Relays and Inputs. Under each Relay create the
	// container and child Receive Components. Under each Input create the
	// container and child Transmit Components.
	//
	CreateChildrenWithIds(Module, 1, SIZEOF_ARRAY(RelayNames), RelayNames, XML_UPBSA_SCHEMA_ROOT XML_SYS_PATH_SEPARATOR XML_SA_RELAYCONTACT, XML_SA_RELAYID);
	CComPtr<IPremiseObjectCollection> Relays;
	if (SUCCEEDED(Module->GetObjectsByType(CComVariant(XML_UPBSA_SCHEMA_ROOT XML_SYS_PATH_SEPARATOR XML_SA_RELAYCONTACT), collectionTypeNoRecurse, &Relays))) {
		long EnumRelay = 0;
		CComPtr<IPremiseObject> Relay;
		for (; SUCCEEDED(Relays->GetItemEx(EnumRelay, &Relay)); EnumRelay++, Relay = NULL) {
			CreateChildrenWithIds(Relay, 0, SA_REG_CM_RELAYCOMPONENTS, NULL, XML_UPBSA_SCHEMA_ROOT XML_SYS_PATH_SEPARATOR XML_SA_RELAYRECEIVECOMPONENT, XML_UPB_COMPONENTID);
		}
	}
	CreateChildrenWithIds(Module, 1, SIZEOF_ARRAY(InputNames), InputNames, XML_UPBSA_SCHEMA_ROOT XML_SYS_PATH_SEPARATOR XML_SA_INPUTCONTACT, XML_SA_INPUTID);
	CComPtr<IPremiseObjectCollection> Inputs;
	if (SUCCEEDED(Module->GetObjectsByType(CComVariant(XML_UPBSA_SCHEMA_ROOT XML_SYS_PATH_SEPARATOR XML_SA_INPUTCONTACT), collectionTypeNoRecurse, &Inputs))) {
		long EnumInput = 0;
		CComPtr<IPremiseObject> Input;
		for (; SUCCEEDED(Inputs->GetItemEx(EnumInput, &Input)); EnumInput++, Input = NULL) {
			CreateChildrenWithIds(Input, 0, SIZEOF_ARRAY(TransmitNames), TransmitNames, XML_UPBSA_SCHEMA_ROOT XML_SYS_PATH_SEPARATOR XML_SA_INPUTTRANSMITCOMPONENT, XML_UPB_COMMANDID);
		}
	}
}

HRESULT STDMETHODCALLTYPE CupbSA::OnObjectCreated(
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
		if (IsObjectOfExplicitType(ObjectCreated, XML_UPBSA_SCHEMA_ROOT XML_SYS_PATH_SEPARATOR XML_SA_USQ)) {
			InitializeUsqChildObjects(ObjectCreated);
		} else if (IsObjectOfExplicitType(ObjectCreated, XML_UPBSA_SCHEMA_ROOT XML_SYS_PATH_SEPARATOR XML_SA_CM01)) {
			InitializeCmChildObjects(ObjectCreated);
		} else if (IsObjectOfExplicitType(ObjectCreated, XML_UPBSA_SCHEMA_ROOT XML_SYS_PATH_SEPARATOR XML_SA_SWITCH)) {
			InitializeSwitchChildObjects(ObjectCreated, SA_REG_RECEIVECOMPONENTS_BYTES / SA_REG_RECEIVECOMPONENTENTRY);
		}
	}
	return S_OK;
}

void PopulateUsqRockerOptions(
	IN IPremiseObject* Module,
	IN const WCHAR* RegisterData
)
{
	BYTE RockerOptions;
	parsehex_(RegisterData, RockerOptions);
	Module->SetValueEx(SVCC_DRIVER | SVCC_NOTIFY, XML_SA_LOCALLOADCONNECT, &CComVariant(SA_USQ_LOCALLOADCONNECT(RockerOptions)));
	Module->SetValueEx(SVCC_DRIVER | SVCC_NOTIFY, XML_SA_LOCALLOADCONTROL, &CComVariant(SA_USQ_LOCALLOADCONTROL(RockerOptions)));
	Module->SetValueEx(SVCC_DRIVER | SVCC_NOTIFY, XML_UPB_LASTONLEVEL, &CComVariant(SA_USQ_LASTONLEVEL(RockerOptions)));
	Module->SetValueEx(SVCC_DRIVER | SVCC_NOTIFY, XML_SA_VARIANTSELECTION, &CComVariant(SA_USQ_VARIANTSELECTION(RockerOptions)));
}

void SetUsqRockerOptions(
	IN IPremiseObject* Module,
	IN VARIANT* LocalLoadConnect OPTIONAL,
	IN VARIANT* LocalLoadControl OPTIONAL,
	IN VARIANT* LastOnLevel OPTIONAL
)
{
	BYTE RockerOptions = 0;
	CComVariant Option;
	if (!LocalLoadConnect && SUCCEEDED(Module->GetValue(XML_SA_LOCALLOADCONNECT, &Option))) {
		LocalLoadConnect = &Option;
	}
	if (LocalLoadConnect) {
		SA_USQ_SETOPTION_LOCALLOADCONNECT(RockerOptions, LocalLoadConnect->boolVal);
	}
	if (!LocalLoadControl && SUCCEEDED(Module->GetValue(XML_SA_LOCALLOADCONTROL, &Option))) {
		LocalLoadControl = &Option;
	}
	if (LocalLoadControl) {
		SA_USQ_SETOPTION_LOCALLOADCONTROL(RockerOptions, LocalLoadControl->boolVal);
	}
	if (!LastOnLevel && SUCCEEDED(Module->GetValue(XML_UPB_LASTONLEVEL, &Option))) {
		LastOnLevel = &Option;
	}
	if (LastOnLevel) {
		SA_USQ_SETOPTION_LASTONLEVEL(RockerOptions, LastOnLevel->boolVal);
	}
	if (SUCCEEDED(Module->GetValue(XML_SA_VARIANTSELECTION, &Option))) {
		SA_USQ_SETOPTION_VARIANTSELECTION(RockerOptions, Option.lVal);
	} else {
		Option = SA_ROCKEROPTION_US_UNDEFINED;
	}
	if ((Option.lVal == SA_ROCKEROPTION_US21) || (Option.lVal == SA_ROCKEROPTION_US22T)) {
		SA_USQ_SETOPTION_TALLROCKER(RockerOptions, TRUE);
	}
	char data[16];
	StringCchPrintfA(data, SIZEOF_ARRAY(data), "%02X%02X%02X", MAKE_MDID(MDID_CORE_COMMANDS, MDID_CORE_COMMAND_SETREGISTERVALUES), SA_REG_ROCKEROPTIONS, RockerOptions);
	Module->SetValueEx(SVCC_NOTIFY | SVCC_FORCE, XML_UPB_UPBNETWORKMESSAGE, &CComVariant(data));
}

static const BYTE PremiseButtonStates[] = {SYS_BUTTON_STATE_DOUBLETAP, SYS_BUTTON_STATE_RELEASE, SYS_BUTTON_STATE_HOLD, SYS_BUTTON_STATE_PRESS};

void PopulateRockerActions(
	IN IPremiseObject* Module,
	IN const WCHAR* RegisterData,
	IN int Actions
)
{
	CComPtr<IPremiseObjectCollection> Buttons;
	if (SUCCEEDED(Module->GetObjectsByType(CComVariant(XML_UPBSA_SCHEMA_ROOT XML_SYS_PATH_SEPARATOR XML_SA_KEYPADBUTTON), collectionTypeNoRecurse, &Buttons))) {
		long EnumButton = 0;
		CComPtr<IPremiseObject> Button;
		for (; SUCCEEDED(Buttons->GetItemEx(EnumButton, &Button)); EnumButton++, Button = NULL) {
			CComVariant ButtonID;
			if (SUCCEEDED(Button->GetValue(XML_SA_BUTTONID, &ButtonID)) && (ButtonID.lVal > 0) && (ButtonID.lVal <= Actions / 2)) {
				CComPtr<IPremiseObjectCollection> RockerActions;
				if (SUCCEEDED(Button->GetObjectsByType(CComVariant(XML_UPBSA_SCHEMA_ROOT XML_SYS_PATH_SEPARATOR XML_SA_ROCKERACTION), collectionTypeNoRecurse, &RockerActions))) {
					long EnumRockerAction = 0;
					CComPtr<IPremiseObject> RockerAction;
					for (; SUCCEEDED(RockerActions->GetItemEx(EnumRockerAction, &RockerAction)); EnumRockerAction++, RockerAction = NULL) {
						CComVariant ButtonState;
						if (SUCCEEDED(RockerAction->GetValue(XML_SYS_BUTTONSTATE, &ButtonState))) {
							int Offset = 2 * 2 * SA_REG_ROCKERACTIONENTRY * (ButtonID.lVal - 1);
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
								Offset += 2 * SA_REG_ROCKERACTIONENTRY;
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

void PopulateCmReceiveComponents(
	IN IPremiseObject* Module,
	IN const WCHAR* RegisterData
)
{
	//
	// Enumerate each relay, filling in the corresponding Receive
	// Component data from the registers. The identifiers are 1-based.
	//
	for (long RelayID = 1; RelayID <= SA_CM_RELAYS; RelayID++) {
		CComPtr<IPremiseObjectCollection> Relays;
		CComPtr<IPremiseObject> Relay;
		if (SUCCEEDED(Module->GetObjectsByTypeAndPropertyValue(XML_UPBSA_SCHEMA_ROOT XML_SYS_PATH_SEPARATOR XML_SA_RELAYCONTACT, XML_SA_RELAYID, CComVariant(RelayID), collectionTypeNoRecurse, &Relays)) && SUCCEEDED(Relays->GetItemEx(0, &Relay))) {
			for (int ComponentID = 1; ComponentID <= SA_REG_CM_RELAYCOMPONENTS; ComponentID++) {
				BYTE LinkID;
				RegisterData += parsehex_(RegisterData, LinkID);
				BYTE State;
				RegisterData += parsehex_(RegisterData, State);
				RegisterData += 2;
				CComPtr<IPremiseObjectCollection> Components;
				CComPtr<IPremiseObject> Component;
				if (SUCCEEDED(Relay->GetObjectsByTypeAndPropertyValue(XML_UPBSA_SCHEMA_ROOT XML_SYS_PATH_SEPARATOR XML_SA_RELAYRECEIVECOMPONENT, XML_UPB_COMPONENTID, CComVariant(ComponentID), collectionTypeNoRecurse, &Components)) && SUCCEEDED(Components->GetItemEx(0, &Component))) {
					Component->SetValueEx(SVCC_DRIVER | SVCC_NOTIFY, XML_UPB_LINKID, &CComVariant(LinkID));
					Component->SetValueEx(SVCC_DRIVER | SVCC_NOTIFY, XML_SYS_STATE, &CComVariant(State != 0));
					Component = NULL;
				}
				Components = NULL;
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
		for (int ComponentID = 1; ComponentID <= SA_REG_RECEIVECOMPONENTS_BYTES / SA_REG_RECEIVECOMPONENTENTRY; ComponentID++) {
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
				Component = NULL;
			}
			ComponentCol = NULL;
		}
	}
}

void PopulateCmTransmitComponents(
	IN IPremiseObject* Module,
	IN const WCHAR* RegisterData
)
{
	CComPtr<IPremiseObjectCollection> Inputs;
	if (SUCCEEDED(Module->GetObjectsByType(CComVariant(XML_UPBSA_SCHEMA_ROOT XML_SYS_PATH_SEPARATOR XML_SA_INPUTCONTACT), collectionTypeNoRecurse, &Inputs))) {
		long EnumInput = 0;
		CComPtr<IPremiseObject> Input;
		for (; SUCCEEDED(Inputs->GetItemEx(EnumInput, &Input)); EnumInput++, Input = NULL) {
			CComVariant InputID;
			CComPtr<IPremiseObjectCollection> Components;
			if (SUCCEEDED(Input->GetValue(XML_SA_INPUTID, &InputID)) && (InputID.lVal > 0) && (InputID.lVal <= SA_CM_INPUTS) && SUCCEEDED(Input->GetObjectsByType(CComVariant(XML_UPBSA_SCHEMA_ROOT XML_SYS_PATH_SEPARATOR XML_SA_INPUTTRANSMITCOMPONENT), collectionTypeNoRecurse, &Components))) {
				long EnumComponent = 0;
				CComPtr<IPremiseObject> Component;
				for (; SUCCEEDED(Components->GetItemEx(EnumComponent, &Component)); EnumComponent++, Component = NULL) {
					CComVariant CommandID;
					if (SUCCEEDED(Component->GetValue(XML_UPB_COMMANDID, &CommandID)) && (CommandID.lVal > 0) && (CommandID.lVal <= SA_REG_CM_TRANSMITCOMPONENTS_BYTES / SA_REG_CM_TXCOMPONENTENTRY / SA_CM_INPUTS)) {
						int Offset;
						Offset = 2 * SA_REG_CM_TXCOMPONENTENTRY * (InputID.lVal - 1) * (CommandID.lVal - 1);
						BYTE LinkID;
						parsehex_(RegisterData + Offset, LinkID);
						Offset += 2;
						Component->SetValueEx(SVCC_DRIVER | SVCC_NOTIFY, XML_UPB_LINKID, &CComVariant(LinkID));
						WCHAR Command[2 * SA_REG_TXCOMMANDENTRY];
						StringCchCopyW(Command, SIZEOF_ARRAY(Command), RegisterData + Offset);
						Component->SetValueEx(SVCC_DRIVER | SVCC_NOTIFY, XML_UPB_COMMAND, &CComVariant(Command));
					}
				}
			}
			Components = NULL;
		}
	}
}

void PopulateUsqTransmitComponents(
	IN IPremiseObject* Module,
	IN const WCHAR* RegisterData1,
	IN const WCHAR* RegisterData2
)
{
	CComPtr<IPremiseObjectCollection> ButtonCol;
	if (SUCCEEDED(Module->GetObjectsByType(CComVariant(XML_UPBSA_SCHEMA_ROOT XML_SYS_PATH_SEPARATOR XML_SA_KEYPADBUTTON), collectionTypeNoRecurse, &ButtonCol))) {
		long EnumButton = 0;
		CComPtr<IPremiseObject> Button;
		for (; SUCCEEDED(ButtonCol->GetItemEx(EnumButton, &Button)); EnumButton++, Button = NULL) {
			CComVariant ButtonID;
			CComPtr<IPremiseObjectCollection> ComponentCol;
			CComPtr<IPremiseObject> Component;
			if (SUCCEEDED(Button->GetValue(XML_SA_BUTTONID, &ButtonID)) && (ButtonID.lVal > 0) && (ButtonID.lVal <= SA_USQ_BUTTONS) && SUCCEEDED(Button->GetObjectsByType(CComVariant(XML_UPB_SCHEMA_ROOT XML_SYS_PATH_SEPARATOR XML_UPB_TRANSMITCOMPONENT), collectionTypeNoRecurse, &ComponentCol)) && SUCCEEDED(ComponentCol->GetItemEx(0, &Component))) {
				const WCHAR* RegisterData;
				int Offset;
				//
				// The transmit components are split between the first 2 buttons, and the remaining
				// buttons.
				//
				if (ButtonID.lVal <= 2) {
					RegisterData = RegisterData1;
					Offset = 2 * SA_REG_TXCOMPONENTENTRY * (ButtonID.lVal - 1);
				} else {
					RegisterData = RegisterData2;
					Offset = 2 * SA_REG_TXCOMPONENTENTRY * (ButtonID.lVal - 3);
				}
				BYTE LinkID;
				parsehex_(RegisterData + Offset, LinkID);
				Offset += 2;
				Component->SetValueEx(SVCC_DRIVER | SVCC_NOTIFY, XML_UPB_LINKID, &CComVariant(LinkID));
				CComPtr<IPremiseObjectCollection> CommandIDs;
				if (SUCCEEDED(Component->GetObjectsByType(CComVariant(XML_UPBSA_SCHEMA_ROOT XML_SYS_PATH_SEPARATOR XML_UPB_COMMANDID), collectionTypeNoRecurse, &CommandIDs))) {
					long EnumCommandID = 0;
					CComPtr<IPremiseObject> CommandID;
					for (; SUCCEEDED(CommandIDs->GetItemEx(EnumCommandID, &CommandID)); EnumCommandID++, CommandID = NULL) {
						CComVariant ButtonState;
						if (SUCCEEDED(CommandID->GetValue(XML_SYS_BUTTONSTATE, &ButtonState)) && (ButtonState.lVal >= SYS_BUTTON_STATE_RELEASE) && (ButtonState.lVal <= SYS_BUTTON_STATE_DOUBLETAP)) {
							BYTE Toggle;
							parsehex_(RegisterData + Offset + 2 * PremiseButtonStates[ButtonState.lVal], Toggle);
							CommandID->SetValueEx(SVCC_DRIVER | SVCC_NOTIFY, XML_SA_ONTOGGLE, &CComVariant((Toggle & 0xf0) >> 4));
							CommandID->SetValueEx(SVCC_DRIVER | SVCC_NOTIFY, XML_SA_OFFTOGGLE, &CComVariant(Toggle & 0x0f));
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
		for (long CommandID = 0; CommandID < Commands; CommandID++) {
			CComPtr<IPremiseObjectCollection> ComponentCol;
			CComPtr<IPremiseObject> Component;
			if (SUCCEEDED(Components->GetObjectsByTypeAndPropertyValue(XML_UPB_SCHEMA_ROOT XML_SYS_PATH_SEPARATOR XML_UPB_TRANSMITCOMMAND, XML_UPB_COMMANDID, CComVariant(CommandID), collectionTypeNoRecurse, &ComponentCol)) && SUCCEEDED(ComponentCol->GetItemEx(0, &Component))) {
				WCHAR Command[2 * SA_REG_TXCOMMANDENTRY + 1];
				StringCchCopyW(Command, SIZEOF_ARRAY(Command), RegisterData);
				Component->SetValueEx(SVCC_DRIVER | SVCC_NOTIFY, XML_UPB_COMMAND, &CComVariant(Command));
			}
			ComponentCol = NULL;
			Component = NULL;
			RegisterData += 2 * SA_REG_TXCOMMANDENTRY;
		}
	}
}

void PopulateCmInitialRelayState(
	IN IPremiseObject* Module,
	IN const WCHAR* RegisterData
)
{
	CComPtr<IPremiseObjectCollection> Relays;
	if (SUCCEEDED(Module->GetObjectsByType(CComVariant(XML_UPBSA_SCHEMA_ROOT XML_SYS_PATH_SEPARATOR XML_SA_RELAYCONTACT), collectionTypeNoRecurse, &Relays))) {
		BYTE InitialStates;
		parsehex_(RegisterData, InitialStates);
		long EnumRelay = 0;
		CComPtr<IPremiseObject> Relay;
		for (; SUCCEEDED(Relays->GetItemEx(EnumRelay, &Relay)); EnumRelay++, Relay = NULL) {
			CComVariant RelayID;
			if (SUCCEEDED(Relay->GetValue(XML_SA_RELAYID, &RelayID)) && (RelayID.lVal > 0) && (RelayID.lVal <= SA_CM_RELAYS)) {
				Relay->SetValueEx(SVCC_DRIVER | SVCC_NOTIFY, XML_SA_INITIALSTATE, &CComVariant(SA_CM_CONTACTSTATE(InitialStates, RelayID.lVal)));
			}
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
	Module->SetValueEx(SVCC_DRIVER | SVCC_NOTIFY, XML_UPB_LEDMODE, &CComVariant(SA_MODULE_LEDMODE(LedOptions)));
	Module->SetValueEx(SVCC_DRIVER | SVCC_NOTIFY, XML_UPB_LEDONCOLOR, &CComVariant(SA_MODULE_LEDONCOLOR(LedOptions)));
	Module->SetValueEx(SVCC_DRIVER | SVCC_NOTIFY, XML_UPB_LEDOFFCOLOR, &CComVariant(SA_MODULE_LEDOFFCOLOR(LedOptions)));
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
		SA_MODULE_SETOPTION_LEDMODE(LedOptions, LedMode->lVal);
	}
	//
	// The CM module uses random registers.
	//
	BYTE RegLedOptions;
	if (IsObjectOfExplicitType(Module, XML_UPBSA_SCHEMA_ROOT XML_SYS_PATH_SEPARATOR XML_SA_CM01)) {
		RegLedOptions = SA_REG_CM_LEDOPTIONS;
	} else {
		RegLedOptions = SA_REG_LEDOPTIONS;
		if (!LedOnColor && SUCCEEDED(Module->GetValue(XML_UPB_LEDONCOLOR, &Option))) {
			LedOnColor = &Option;
		}
		if (LedOnColor) {
			SA_MODULE_SETOPTION_LEDONCOLOR(LedOptions, LedOnColor->lVal);
		}
		if (!LedOffColor && SUCCEEDED(Module->GetValue(XML_UPB_LEDOFFCOLOR, &Option))) {
			LedOffColor = &Option;
		}
		if (LedOffColor) {
			SA_MODULE_SETOPTION_LEDOFFCOLOR(LedOptions, LedOffColor->lVal);
		}
	}
	char data[16];
	StringCchPrintfA(data, SIZEOF_ARRAY(data), "%02X%02X%02X", MAKE_MDID(MDID_CORE_COMMANDS, MDID_CORE_COMMAND_SETREGISTERVALUES), RegLedOptions, LedOptions);
	Module->SetValueEx(SVCC_NOTIFY | SVCC_FORCE, XML_UPB_UPBNETWORKMESSAGE, &CComVariant(data));
}

void PopulateDimmerOptions(
	IN IPremiseObject* Module,
	IN const WCHAR* RegisterData
)
{
	BYTE DimmerOptions;
	parsehex_(RegisterData, DimmerOptions);
	Module->SetValueEx(SVCC_DRIVER | SVCC_NOTIFY, XML_SA_LOADTYPE, &CComVariant(SA_SWITCH_LOADTYPE(DimmerOptions)));
	Module->SetValueEx(SVCC_DRIVER | SVCC_NOTIFY, XML_SA_REPORTSTATE, &CComVariant(SA_SWITCH_REPORTSTATE(DimmerOptions)));
	Module->SetValueEx(SVCC_DRIVER | SVCC_NOTIFY, XML_SA_TRIGGERSWITCH, &CComVariant(SA_SWITCH_TRIGGERSWITCH(DimmerOptions)));
	Module->SetValueEx(SVCC_DRIVER | SVCC_NOTIFY, XML_SA_TRIGGERLASTLEVEL, &CComVariant(SA_SWITCH_TRIGGERLASTLEVEL(DimmerOptions)));
	Module->SetValueEx(SVCC_DRIVER | SVCC_NOTIFY, XML_UPB_DEFAULTFADERATE, &CComVariant(SA_SWITCH_DEFAULTFADERATE(DimmerOptions)));
}

void SetDimmerOptions(
	IN IPremiseObject* Module,
	IN VARIANT* LoadType OPTIONAL,
	IN VARIANT* ReportState OPTIONAL,
	IN VARIANT* TriggerSwitch OPTIONAL,
	IN VARIANT* TriggerLastLevel OPTIONAL,
	IN VARIANT* DefaultFadeRate OPTIONAL
)
{
	BYTE DimmerOptions = 0;
	CComVariant Option;
	if (IsObjectOfExplicitType(Module, XML_UPBSA_SCHEMA_ROOT XML_SYS_PATH_SEPARATOR XML_SA_DIMMER)) {
		if (!LoadType && SUCCEEDED(Module->GetValue(XML_SA_LOADTYPE, &Option))) {
			LoadType = &Option;
		}
		if (LoadType) {
			SA_SWITCH_SETOPTION_LOADTYPE(DimmerOptions, LoadType->lVal);
		}
		if (!DefaultFadeRate && SUCCEEDED(Module->GetValue(XML_UPB_DEFAULTFADERATE, &Option))) {
			DefaultFadeRate = &Option;
		}
		if (DefaultFadeRate) {
			SA_SWITCH_SETOPTION_DEFAULTFADERATE(DimmerOptions, DefaultFadeRate->lVal);
		}
	} else {
		SA_SWITCH_SETOPTION_LOADTYPE(DimmerOptions, 0);
		SA_SWITCH_SETOPTION_DEFAULTFADERATE(DimmerOptions, UPB_SWITCH_FADERATE_SNAP);
	}
	if (!ReportState && SUCCEEDED(Module->GetValue(XML_SA_REPORTSTATE, &Option))) {
		ReportState = &Option;
	}
	if (ReportState) {
		SA_SWITCH_SETOPTION_REPORTSTATE(DimmerOptions, ReportState->boolVal);
	}
	if (!TriggerSwitch && SUCCEEDED(Module->GetValue(XML_SA_TRIGGERSWITCH, &Option))) {
		TriggerSwitch = &Option;
	}
	if (TriggerSwitch) {
		SA_SWITCH_SETOPTION_TRIGGERSWITCH(DimmerOptions, TriggerSwitch->boolVal);
	}
	if (!TriggerLastLevel && SUCCEEDED(Module->GetValue(XML_SA_TRIGGERLASTLEVEL, &Option))) {
		TriggerLastLevel = &Option;
	}
	if (TriggerLastLevel) {
		SA_SWITCH_SETOPTION_TRIGGERLASTLEVEL(DimmerOptions, TriggerLastLevel->boolVal);
	}
	char data[16];
	StringCchPrintfA(data, SIZEOF_ARRAY(data), "%02X%02X%02X", MAKE_MDID(MDID_CORE_COMMANDS, MDID_CORE_COMMAND_SETREGISTERVALUES), SA_REG_DIMMEROPTIONS, DimmerOptions);
	Module->SetValueEx(SVCC_NOTIFY | SVCC_FORCE, XML_UPB_UPBNETWORKMESSAGE, &CComVariant(data));
}

void PopulateTransmissionOptions(
	IN IPremiseObject* Module,
	IN const WCHAR* RegisterData
)
{
	BYTE TransmissionOptions;
	parsehex_(RegisterData, TransmissionOptions);
	Module->SetValueEx(SVCC_DRIVER | SVCC_NOTIFY, XML_UPB_EVENTPACKETTYPE, &CComVariant(SA_MODULE_EVENTPACKETTYPE(TransmissionOptions)));
	Module->SetValueEx(SVCC_DRIVER | SVCC_NOTIFY, XML_UPB_ACKMESSAGEREQUEST, &CComVariant(SA_MODULE_ACKMESSAGEREQUEST(TransmissionOptions)));
	Module->SetValueEx(SVCC_DRIVER | SVCC_NOTIFY, XML_UPB_IDPULSEREQUEST, &CComVariant(SA_MODULE_IDPULSEREQUEST(TransmissionOptions)));
	Module->SetValueEx(SVCC_DRIVER | SVCC_NOTIFY, XML_UPB_ACKPULSEREQUEST, &CComVariant(SA_MODULE_ACKPULSEREQUEST(TransmissionOptions)));
	Module->SetValueEx(SVCC_DRIVER | SVCC_NOTIFY, XML_UPB_TRANSMISSIONCOUNT, &CComVariant(SA_MODULE_TRANSMISSIONCOUNT(TransmissionOptions)));
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
		SA_MODULE_SETOPTION_EVENTPACKETTYPE(TransmissionOptions, EventPacketType->boolVal);
	}
	if (!AckMessageRequest && SUCCEEDED(Module->GetValue(XML_UPB_ACKMESSAGEREQUEST, &Option))) {
		AckMessageRequest = &Option;
	}
	if (AckMessageRequest) {
		SA_MODULE_SETOPTION_ACKMESSAGEREQUEST(TransmissionOptions, AckMessageRequest->boolVal);
	}
	if (!IdPulseRequest && SUCCEEDED(Module->GetValue(XML_UPB_IDPULSEREQUEST, &Option))) {
		IdPulseRequest = &Option;
	}
	if (IdPulseRequest) {
		SA_MODULE_SETOPTION_IDPULSEREQUEST(TransmissionOptions, IdPulseRequest->boolVal);
	}
	if (!AckPulseRequest && SUCCEEDED(Module->GetValue(XML_UPB_ACKPULSEREQUEST, &Option))) {
		AckPulseRequest = &Option;
	}
	if (AckPulseRequest) {
		SA_MODULE_SETOPTION_ACKPULSEREQUEST(TransmissionOptions, AckPulseRequest->boolVal);
	}
	if (!TransmissionCount && SUCCEEDED(Module->GetValue(XML_UPB_TRANSMISSIONCOUNT, &Option))) {
		TransmissionCount = &Option;
	}
	if (TransmissionCount) {
		SA_MODULE_SETOPTION_TRANSMISSIONCOUNT(TransmissionOptions, TransmissionCount->lVal);
	}
	//
	// The CM module uses random registers.
	//
	BYTE RegTxControl;
	if (IsObjectOfExplicitType(Module, XML_UPBSA_SCHEMA_ROOT XML_SYS_PATH_SEPARATOR XML_SA_CM01)) {
		RegTxControl = SA_REG_CM_TXCONTROL;
	} else {
		RegTxControl = SA_REG_TXCONTROL;
	}
	char data[16];
	StringCchPrintfA(data, SIZEOF_ARRAY(data), "%02X%02X%02X", MAKE_MDID(MDID_CORE_COMMANDS, MDID_CORE_COMMAND_SETREGISTERVALUES), RegTxControl, TransmissionOptions);
	Module->SetValueEx(SVCC_NOTIFY | SVCC_FORCE, XML_UPB_UPBNETWORKMESSAGE, &CComVariant(data));
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
	if (SUCCEEDED(Module->GetObjectsByTypeAndPropertyValue(ButtonClass, XML_SA_BUTTONID, CComVariant(DestinationID), collectionTypeNoRecurse, &Buttons)) && SUCCEEDED(Buttons->GetItemEx(0, &Button))) {
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

void PopulateCmDeviceState(
	IN IPremiseObject* Module,
	IN const WCHAR* MessageData
)
{
	BYTE InputStates;
	parsehex_(MessageData, InputStates);
	CComPtr<IPremiseObjectCollection> Inputs;
	if (SUCCEEDED(Module->GetObjectsByType(CComVariant(XML_UPBSA_SCHEMA_ROOT XML_SYS_PATH_SEPARATOR XML_SA_INPUTCONTACT), collectionTypeNoRecurse, &Inputs))) {
		long EnumInput = 0;
		CComPtr<IPremiseObject> Input;
		for (; SUCCEEDED(Inputs->GetItemEx(EnumInput, &Input)); EnumInput++, Input = NULL) {
			CComVariant InputID;
			CComVariant Invert;
			if (SUCCEEDED(Input->GetValue(XML_SA_INPUTID, &InputID)) && (InputID.lVal > 0) && (InputID.lVal <= SA_CM_INPUTS) && SUCCEEDED(Input->GetValue(XML_SYS_INVERT, &Invert))) {
				BOOL DeviceContactState = SA_CM_CONTACTSTATE(InputStates, InputID.lVal);
				Input->SetValueEx(SVCC_DRIVER | SVCC_NOTIFY, XML_SYS_STATE, &CComVariant(Invert.boolVal ? !DeviceContactState : DeviceContactState));
			}
		}
	}
	BYTE RelayStates;
	parsehex_(MessageData, RelayStates);
	CComPtr<IPremiseObjectCollection> Relays;
	if (SUCCEEDED(Module->GetObjectsByType(CComVariant(XML_UPBSA_SCHEMA_ROOT XML_SYS_PATH_SEPARATOR XML_SA_RELAYCONTACT), collectionTypeNoRecurse, &Relays))) {
		long EnumRelay = 0;
		CComPtr<IPremiseObject> Relay;
		for (; SUCCEEDED(Relays->GetItemEx(EnumRelay, &Relay)); EnumRelay++, Relay = NULL) {
			CComVariant RelayID;
			CComVariant Invert;
			if (SUCCEEDED(Relay->GetValue(XML_SA_RELAYID, &RelayID)) && (RelayID.lVal > 0) && (RelayID.lVal <= SA_CM_RELAYS) && SUCCEEDED(Relay->GetValue(XML_SYS_INVERT, &Invert))) {
				BOOL DeviceContactState = SA_CM_CONTACTSTATE(RelayStates, RelayID.lVal);
				Relay->SetValueEx(SVCC_DRIVER | SVCC_NOTIFY, XML_SYS_STATE, &CComVariant(Invert.boolVal ? !DeviceContactState : DeviceContactState));
				//!!if it is found to be closed (non-zero), should a timer be started now?
			}
		}
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
	if (SUCCEEDED(Action->GetValue(XML_SYS_BUTTONSTATE, &ButtonState)) && (ButtonState.lVal >= SYS_BUTTON_STATE_RELEASE) && (ButtonState.lVal <= SYS_BUTTON_STATE_DOUBLETAP) && SUCCEEDED(Action->get_Parent(&Button)) && SUCCEEDED(Button->GetValue(XML_SA_BUTTONID, &ButtonID)) && (ButtonID.lVal > 0) && (ButtonID.lVal <= Buttons) && SUCCEEDED(Button->get_Parent(&Module))) {
		char data[16];
		StringCchPrintfA(data, SIZEOF_ARRAY(data), "%02X%02X%02X", MAKE_MDID(MDID_CORE_COMMANDS, MDID_CORE_COMMAND_SETREGISTERVALUES), SA_REG_ROCKERACTIONS + Offset + SA_REG_ROCKERACTIONENTRY * (ButtonID.lVal - 1), Item);
		Module->SetValueEx(SVCC_NOTIFY | SVCC_FORCE, XML_UPB_UPBNETWORKMESSAGE, &CComVariant(data));
	}
}

void SetReceiveComponentItem(
	IN IPremiseObject* Component,
	IN BYTE Item,
	IN int Offset
)
{
	CComVariant ComponentID;
	CComPtr<IPremiseObject> Components;
	CComPtr<IPremiseObject> Module;
	if (SUCCEEDED(Component->GetValue(XML_UPB_COMPONENTID, &ComponentID)) && (ComponentID.lVal > 0) && SUCCEEDED(Component->get_Parent(&Components)) && SUCCEEDED(Components->get_Parent(&Module))) {
		if ((IsObjectOfExplicitType(Module, XML_UPBSA_SCHEMA_ROOT XML_SYS_PATH_SEPARATOR XML_SA_CM01) && (ComponentID.lVal <= SA_REG_CM_RELAYCOMPONENTS)) || (ComponentID.lVal <= SA_REG_RECEIVECOMPONENTS_BYTES / SA_REG_RECEIVECOMPONENTENTRY)) {
			char data[16];
			StringCchPrintfA(data, SIZEOF_ARRAY(data), "%02X%02X%02X", MAKE_MDID(MDID_CORE_COMMANDS, MDID_CORE_COMMAND_SETREGISTERVALUES), SA_REG_RECEIVECOMPONENTS + Offset + SA_REG_RECEIVECOMPONENTENTRY * (ComponentID.lVal - 1), Item);
			Module->SetValueEx(SVCC_NOTIFY | SVCC_FORCE, XML_UPB_UPBNETWORKMESSAGE, &CComVariant(data));
		}
	}
}

void SetTransmitComponentLinkID(
	IN IPremiseObject* Component,
	IN BYTE LinkID
)
{
	CComPtr<IPremiseObject> Container;
	CComPtr<IPremiseObject> Module;
	if (SUCCEEDED(Component->get_Parent(&Container)) && SUCCEEDED(Container->get_Parent(&Module))) {
		if (IsObjectOfExplicitType(Module, XML_UPBSA_SCHEMA_ROOT XML_SYS_PATH_SEPARATOR XML_SA_USQ)) {
			CComVariant ButtonID;
			if (SUCCEEDED(Container->GetValue(XML_SA_BUTTONID, &ButtonID)) && (ButtonID.ulVal <= SA_USQ_BUTTONS)) {
				//
				// The Transmit Components are split.
				//
				BYTE Register;
				if (ButtonID.ulVal <= 2) {
					Register = SA_REG_TRANSMITCOMPONENTS;
				} else {
					Register = SA_REG_USQ_TRANSMITCOMPONENTS;
					ButtonID.ulVal -= 2;
				}
				char data[16];
				StringCchPrintfA(data, SIZEOF_ARRAY(data), "%02X%02X%02X", MAKE_MDID(MDID_CORE_COMMANDS, MDID_CORE_COMMAND_SETREGISTERVALUES), Register + (ButtonID.ulVal - 1) * SA_REG_TXCOMPONENTENTRY, LinkID);
				Module->SetValueEx(SVCC_NOTIFY | SVCC_FORCE, XML_UPB_UPBNETWORKMESSAGE, &CComVariant(data));
			}
		} else if (IsObjectOfExplicitType(Module, XML_UPBSA_SCHEMA_ROOT XML_SYS_PATH_SEPARATOR XML_SA_CM01)) {
			CComVariant CommandID;
			CComVariant InputID;
			if (SUCCEEDED(Component->GetValue(XML_UPB_COMMANDID, &CommandID)) && (CommandID.ulVal < 2) && SUCCEEDED(Container->GetValue(XML_SA_INPUTID, &InputID)) && (InputID.ulVal <= SA_CM_INPUTS)) {
				char data[16];
				StringCchPrintfA(data, SIZEOF_ARRAY(data), "%02X%02X%02X", MAKE_MDID(MDID_CORE_COMMANDS, MDID_CORE_COMMAND_SETREGISTERVALUES), SA_REG_CM_TRANSMITCOMPONENTS + (CommandID.ulVal + InputID.ulVal - 1) * SA_REG_CM_TXCOMPONENTENTRY, LinkID);
				Module->SetValueEx(SVCC_NOTIFY | SVCC_FORCE, XML_UPB_UPBNETWORKMESSAGE, &CComVariant(data));
			}
		}
	}
}

void VirtualizeCmTransmitCommands(
	IN IPremiseObject* Module
)
{
	//
	// Each TransmitCommand with a CommandID that matches an input
	// state must have the Command string set to the Extended
	// Command Set with the Virtualized command, followed by the
	// input state that matches the CommandID, followed by 00. The
	// LinkID is then set to the InputID. Note that this is different
	// than a normal device, in that all the pieces of the command are
	// gathered under a single TransmitCommand object, following the
	// design of this particular piece of hardware.
	//
	CComPtr<IPremiseObjectCollection> Inputs;
	if (SUCCEEDED(Module->GetObjectsByType(CComVariant(XML_UPBSA_SCHEMA_ROOT XML_SYS_PATH_SEPARATOR XML_SA_INPUTCONTACT), collectionTypeNoRecurse, &Inputs))) {
		long EnumInput = 0;
		CComPtr<IPremiseObject> Input;
		for (; SUCCEEDED(Inputs->GetItemEx(EnumInput, &Input)); EnumInput++, Input = NULL) {
			CComVariant InputID;
			CComPtr<IPremiseObjectCollection> Components;
			if (SUCCEEDED(Input->GetValue(XML_SA_INPUTID, &InputID)) && (InputID.lVal > 0) && (InputID.lVal <= SA_CM_INPUTS) && SUCCEEDED(Input->GetObjectsByTypeAndPropertyValue(CComVariant(XML_UPBSA_SCHEMA_ROOT XML_SYS_PATH_SEPARATOR XML_SA_INPUTTRANSMITCOMPONENT), collectionTypeNoRecurse, &Components))) {
				long EnumComponent = 0;
				CComPtr<IPremiseObject> Component;
				for (; SUCCEEDED(Components->GetItemEx(EnumComponent, &Component)); EnumComponent++, Component = NULL) {
					CComVariant CommandID;
					if (SUCCEEDED(Component->GetValue(XML_UPB_COMMANDID, &CommandID)) && (CommandID.lVal >= SA_CM_INPUT_MESSAGE_CLOSE) && (CommandID.lVal <= SA_CM_INPUT_MESSAGE_OPEN)) {
						Component->SetValueEx(SVCC_DRIVER | SVCC_NOTIFY, XML_UPB_LINKID, &InputID);
						char data[16];
						StringCchPrintfA(data, SIZEOF_ARRAY(data), "%02X%02X00", MAKE_MDID(MDID_EXTENDED_MESSAGE_SET, VIRTUALIZED_MESSAGE_SET), CommandID.lVal);
						Component->SetValueEx(SVCC_DRIVER | SVCC_NOTIFY, XML_UPB_COMMAND, &CComVariant(data));
					}
				}
			}
			Components = NULL;
		}
	}
	//
	// Collect the register data into a single write so that I/O is
	// reduced. The UPB driver will split the write up into maximum
	// lengths.
	//
	char RegisterData[2 * (1 + 1 + SA_REG_CM_TXCOMPONENTENTRY * SA_CM_INPUTS) + 1];
	char* DataPointer;
	size_t DataRemaining;
	StringCchPrintfExA(RegisterData, SIZEOF_ARRAY(RegisterData), &DataPointer, &DataRemaining, 0, "%02X%02X", MAKE_MDID(MDID_CORE_COMMANDS, MDID_CORE_COMMAND_SETREGISTERVALUES), RegisterStart);
	for (int i = 1; i <= SA_CM_INPUTS; i++ ) {
//!!for CLOSE to OPEN for each Input: LID MDID State 00
		StringCchPrintfExA(DataPointer, DataRemaining, &DataPointer, &DataRemaining, 0, "%02X%02X%02X00", MAKE_MDID(MDID_EXTENDED_MESSAGE_SET, VIRTUALIZED_MESSAGE_SET), i);
//!!
	}
	Module->SetValueEx(SVCC_NOTIFY | SVCC_FORCE, XML_UPB_UPBNETWORKMESSAGE, &CComVariant(RegisterData));
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
	char RegisterData[2 * (1 + 1 + (SYS_BUTTON_STATE_DOUBLETAP + 1) * SA_REG_TXCOMMANDENTRY) + 1];
	char* DataPointer;
	size_t DataRemaining;
	StringCchPrintfExA(RegisterData, SIZEOF_ARRAY(RegisterData), &DataPointer, &DataRemaining, 0, "%02X%02X", MAKE_MDID(MDID_CORE_COMMANDS, MDID_CORE_COMMAND_SETREGISTERVALUES), RegisterStart);
	for (int i = SYS_BUTTON_STATE_RELEASE; i <= SYS_BUTTON_STATE_DOUBLETAP; i++ ) {
		StringCchPrintfExA(DataPointer, DataRemaining, &DataPointer, &DataRemaining, 0, "%02X%02X00", MAKE_MDID(MDID_EXTENDED_MESSAGE_SET, VIRTUALIZED_MESSAGE_SET), i);
	}
	Module->SetValueEx(SVCC_NOTIFY | SVCC_FORCE, XML_UPB_UPBNETWORKMESSAGE, &CComVariant(RegisterData));
}

void VirtualizeUsqTransmitComponents(
	IN IPremiseObject* Module
)
{
	//
	// Set the LinkID in each TransmitComponent to the ButtonID in
	// which it is contained. Also set the OnToggle and OffToggle
	// CommandID to match the ButtonState.
	//
	CComPtr<IPremiseObjectCollection> Buttons;
	if (SUCCEEDED(Module->GetObjectsByType(CComVariant(XML_UPBSA_SCHEMA_ROOT XML_SYS_PATH_SEPARATOR XML_SA_KEYPADBUTTON), collectionTypeNoRecurse, &Buttons))) {
		long EnumButton = 0;
		CComPtr<IPremiseObject> Button;
		for (; SUCCEEDED(Buttons->GetItemEx(EnumButton, &Button)); EnumButton++, Button = NULL) {
			CComVariant ButtonID;
			CComPtr<IPremiseObjectCollection> ComponentCol;
			CComPtr<IPremiseObject> Component;
			if (SUCCEEDED(Button->GetValue(XML_SA_BUTTONID, &ButtonID)) && (ButtonID.lVal > 0) && (ButtonID.lVal <= SA_USQ_BUTTONS) && SUCCEEDED(Button->GetObjectsByType(CComVariant(XML_UPB_SCHEMA_ROOT XML_SYS_PATH_SEPARATOR XML_UPB_TRANSMITCOMPONENT), collectionTypeNoRecurse, &ComponentCol)) && SUCCEEDED(ComponentCol->GetItemEx(0, &Component))) {
				Component->SetValueEx(SVCC_DRIVER | SVCC_NOTIFY, XML_UPB_LINKID, &ButtonID);
				CComPtr<IPremiseObjectCollection> CommandIDs;
				if (SUCCEEDED(Component->GetObjectsByType(CComVariant(XML_UPBSA_SCHEMA_ROOT XML_SYS_PATH_SEPARATOR XML_UPB_COMMANDID), collectionTypeNoRecurse, &CommandIDs))) {
					long EnumCommandID = 0;
					CComPtr<IPremiseObject> CommandID;
					for (; SUCCEEDED(CommandIDs->GetItemEx(EnumCommandID, &CommandID)); EnumCommandID++, CommandID = NULL) {
						CComVariant ButtonState;
						if (SUCCEEDED(CommandID->GetValue(XML_SYS_BUTTONSTATE, &ButtonState)) && (ButtonState.lVal >= SYS_BUTTON_STATE_RELEASE) && (ButtonState.lVal <= SYS_BUTTON_STATE_DOUBLETAP)) {
							CommandID->SetValueEx(SVCC_DRIVER | SVCC_NOTIFY, XML_SA_ONTOGGLE, &CComVariant(ButtonState.lVal));
							CommandID->SetValueEx(SVCC_DRIVER | SVCC_NOTIFY, XML_SA_OFFTOGGLE, &CComVariant(ButtonState.lVal));
						}
					}
				}
			}
			ComponentCol = NULL;
			Component = NULL;
		}
	}
	//
	// Collect the register data into two writes so that I/O is
	// reduced. The UPB driver will split the write up into maximum
	// lengths.
	//
	size_t DataRemaining = 2 * (1 + 1 + SA_USQ_BUTTONS * SA_REG_TXCOMPONENTENTRY) + 1;
	char* RegisterData = new char[DataRemaining];
	if (RegisterData) {
		char* DataPointer;
		StringCchPrintfExA(RegisterData, DataRemaining, &DataPointer, &DataRemaining, 0, "%02X%02X", MAKE_MDID(MDID_CORE_COMMANDS, MDID_CORE_COMMAND_SETREGISTERVALUES), SA_REG_TRANSMITCOMPONENTS);
		for (int i = 1; i <= 2; i++ ) {
			StringCchPrintfExA(DataPointer, DataRemaining, &DataPointer, &DataRemaining, 0, "%02X%02X%02X%02X%02X", i, SYS_BUTTON_STATE_PRESS << 4 | SYS_BUTTON_STATE_PRESS, SYS_BUTTON_STATE_DOUBLETAP << 4 | SYS_BUTTON_STATE_DOUBLETAP, SYS_BUTTON_STATE_HOLD << 4 | SYS_BUTTON_STATE_HOLD, SYS_BUTTON_STATE_RELEASE << 4 | SYS_BUTTON_STATE_RELEASE);
		}
		Module->SetValueEx(SVCC_NOTIFY | SVCC_FORCE, XML_UPB_UPBNETWORKMESSAGE, &CComVariant(RegisterData));
		StringCchPrintfExA(RegisterData, DataRemaining, &DataPointer, &DataRemaining, 0, "%02X%02X", MAKE_MDID(MDID_CORE_COMMANDS, MDID_CORE_COMMAND_SETREGISTERVALUES), SA_REG_USQ_TRANSMITCOMPONENTS);
		for (int i = 3; i <= SA_USQ_BUTTONS; i++ ) {
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
	SetDimmerOptions(Module, NULL, &CComVariant(TRUE), NULL, NULL, NULL);
}

void VirtualizeCm(
	IN IPremiseObject* Module
)
{
	//
	// Enable sending of Link packets.
	//
	SetTransmissionOptions(Module, &CComVariant(TRUE), NULL, NULL, NULL, NULL);
	VirtualizeCmTransmitCommands(Module);
}

void VirtualizeUsq(
	IN IPremiseObject* Module
)
{
	//
	// Enable ReportState. Triggering is not available on this unit, so just set them to FALSE.
	//
	SetDimmerOptions(Module, NULL, &CComVariant(TRUE), &CComVariant(FALSE), &CComVariant(FALSE), NULL);
	//
	// Enable sending of Link packets.
	//
	SetTransmissionOptions(Module, &CComVariant(TRUE), NULL, NULL, NULL, NULL);
	VirtualizeUsqTransmitComponents(Module);
	VirtualizeTransmitCommands(Module, SA_REG_TRANSMITCOMMANDS);
}

void UpdateSwitchReceiveComponentResults(
	IN IPremiseObject* Module,
	IN BYTE did,
	IN BOOL Status
)
{
	//
	// If activating the link, then collect the ReceiveComponent
	// information to determine what Brightness.
	//
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

void UpdateCmReceiveComponentResults(
	IN IPremiseObject* Module,
	IN BYTE did,
	IN BOOL Status,
	IN BOOL ForceState
)
{
	//
	// If activating the link, then collect the ReceiveComponent
	// information to determine what Relay state.
	//
	CComPtr<IPremiseObjectCollection> Components;
	if (SUCCEEDED(Module->GetObjectsByTypeAndPropertyValue(XML_UPB_SCHEMA_ROOT XML_SYS_PATH_SEPARATOR XML_UPB_RECEIVECOMPONENT, XML_UPB_LINKID, CComVariant(did), collectionTypeRecurse, &Components))) {
		long EnumComponent = 0;
		CComPtr<IPremiseObject> Component;
		for (; SUCCEEDED(Components->GetItemEx(EnumComponent, &Component)); EnumComponent++, Component = NULL) {
			CComPtr<IPremiseObject> Relay;
			CComVariant Invert;
			if (SUCCEEDED(Component->get_Parent(&Relay)) && SUCCEEDED(Relay->GetValue(XML_SYS_INVERT, &Invert))) {
				CComVariant State;
				if (ForceState) {
					//
					// The state is being forced to whatever is passed
					// in, not what is in the Link entry. This is used
					// for a Goto command.
					//
					State.boolVal = Status == TRUE;
				} else if (Status) {
					//
					// If the Link is being activated, then follow what the
					// Receive Component says.
					//
					if (FAILED(Component->GetValue(XML_SYS_STATE, &State))) {
						return;
					}
				} else {
					//
					// Else the Link is being deactivated, which means
					// it is always set to Open.
					//
					State.boolVal = FALSE;
				}
				Relay->SetValueEx(SVCC_DRIVER | SVCC_NOTIFY, XML_SYS_BRIGHTNESS, &CComVariant(Invert.boolVal ? !State.boolVal : State.boolVal));
				//!!if it is found to be closed (non-zero), start a timer?
			}
		}
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
	if (SUCCEEDED(Module->GetObjectsByTypeAndPropertyValue(XML_UPBSA_SCHEMA_ROOT XML_SYS_PATH_SEPARATOR XML_UPB_COMMANDID, XML_SA_ONTOGGLE, CommandID, collectionTypeRecurse, &CommandIDs)) && SUCCEEDED(CommandIDs->GetItemEx(0, &ToggleCommand)) && SUCCEEDED(ToggleCommand->get_Parent(&Component)) && SUCCEEDED(Component->GetValue(XML_UPB_LINKID, &LinkID)) && (LinkID.bVal == did) && SUCCEEDED(ToggleCommand->GetValue(XML_SYS_BUTTONSTATE, &ButtonState))) {
		*State = ButtonState.lVal;
		return Component->get_Parent(Button);
	}
	CommandIDs = NULL;
	ToggleCommand = NULL;
	Component = NULL;
	ButtonState = NULL;
	if (SUCCEEDED(Module->GetObjectsByTypeAndPropertyValue(XML_UPBSA_SCHEMA_ROOT XML_SYS_PATH_SEPARATOR XML_UPB_COMMANDID, XML_SA_OFFTOGGLE, CommandID, collectionTypeRecurse, &CommandIDs)) && SUCCEEDED(CommandIDs->GetItemEx(0, &ToggleCommand)) && SUCCEEDED(ToggleCommand->get_Parent(&Component)) && SUCCEEDED(Component->GetValue(XML_UPB_LINKID, &LinkID)) && (LinkID.bVal == did) && SUCCEEDED(ToggleCommand->GetValue(XML_SYS_BUTTONSTATE, &ButtonState))) {
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
			if (SUCCEEDED(Component->GetValue(XML_UPB_COMPONENTID, &ComponentID)) && (ComponentID.lVal > 0) && (ComponentID.lVal <= SA_REG_RECEIVECOMPONENTS_BYTES / SA_REG_RECEIVECOMPONENTENTRY)) {
				Component->SetValueEx(SVCC_DRIVER | SVCC_NOTIFY, XML_UPB_LASTONLEVEL, &CComVariant(FALSE));
				Component->SetValueEx(SVCC_DRIVER | SVCC_NOTIFY, XML_UPB_LIGHTLEVEL, &Brightness);
				//
				// Only update the cache, without writing to the device
				// again.
				//
				WCHAR data[16];
				StringCchPrintfW(data, SIZEOF_ARRAY(data), L"%c%02X%02X%02X", UPB_MESSAGE_CACHE_ONLY, MAKE_MDID(MDID_CORE_COMMANDS, MDID_CORE_COMMAND_SETREGISTERVALUES), SA_REG_RECEIVECOMPONENTS + 1 + SA_REG_RECEIVECOMPONENTENTRY * (ComponentID.lVal - 1), static_cast<BYTE>(static_cast<long>(Brightness.dblVal * 100 + 0.5)));
				Module->SetValueEx(SVCC_NOTIFY | SVCC_FORCE, XML_UPB_UPBNETWORKMESSAGE, &CComVariant(data));
			}
		}
	}
}

void StoreCmState(
	IN IPremiseObject* Module,
	IN BYTE LinkID
)
{
	CComPtr<IPremiseObjectCollection> Components;
	if (SUCCEEDED(Module->GetObjectsByTypeAndPropertyValue(XML_UPB_SCHEMA_ROOT XML_SYS_PATH_SEPARATOR XML_UPB_RECEIVECOMPONENT, XML_UPB_LINKID, CComVariant(LinkID), collectionTypeRecurse, &Components))) {
		long EnumComponent = 0;
		CComPtr<IPremiseObject> Component;
		for (; SUCCEEDED(Components->GetItemEx(EnumComponent, &Component)); EnumComponent++, Component = NULL) {
			//
			// Ensure the entry is within range.
			//
			CComVariant ComponentID;
			CComPtr<IPremiseObject> Relay;
			CComVariant Invert;
			CComVariant State;
			CComVariant RelayID;
			if (SUCCEEDED(Component->GetValue(XML_UPB_COMPONENTID, &ComponentID)) && (ComponentID.lVal > 0) && (ComponentID.lVal <= SA_REG_CM_RECEIVECOMPONENTS_BYTES / SA_REG_RECEIVECOMPONENTENTRY) && SUCCEEDED(Component->get_Parent(&Relay)) && SUCCEEDED(Relay->GetValue(XML_SYS_INVERT, &Invert)) && SUCCEEDED(Relay->GetValue(XML_SA_RELAYID, &RelayID)) && (RelayID.lVal > 0) && (RelayID.lVal <= SA_CM_RELAYS) && SUCCEEDED(Relay->GetValue(XML_SYS_STATE, &State))) {
				Component->SetValueEx(SVCC_DRIVER | SVCC_NOTIFY, XML_SYS_STATE, &CComVariant(Invert.boolVal ? !State.boolVal : State.boolVal));
				//
				// Only update the cache, without writing to the device
				// again.
				//
				WCHAR data[16];
				StringCchPrintfW(data, SIZEOF_ARRAY(data), L"%c%02X%02X%02X", UPB_MESSAGE_CACHE_ONLY, MAKE_MDID(MDID_CORE_COMMANDS, MDID_CORE_COMMAND_SETREGISTERVALUES), SA_REG_RECEIVECOMPONENTS + 1 + SA_REG_RECEIVECOMPONENTENTRY * (SA_REG_CM_RELAYCOMPONENTS * (RelayID.lVal - 1) + (ComponentID.lVal - 1)), State.boolVal);
				Module->SetValueEx(SVCC_NOTIFY | SVCC_FORCE, XML_UPB_UPBNETWORKMESSAGE, &CComVariant(data));
			}
		}
	}
}

void OnUsqNetworkData(
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
			PopulateVirtualizedButtonState(Module, XML_UPBSA_SCHEMA_ROOT XML_SYS_PATH_SEPARATOR XML_SA_KEYPADBUTTON, MessageData, did);
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
				if (SUCCEEDED(Module->GetValue(XML_SA_LOADTYPE, &LoadType))) {
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

void OnSwitchNetworkData(
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
	case UPB_NETWORKDATA_BROADCAST:
		//
		// The Link and Direct messages are fairly mutually exclusive, so there should not be
		// any need to differentiate between the two.
		//
		switch (mdid) {
		case MAKE_MDID(MDID_CORE_REPORTS, MDID_DEVICE_CORE_REPORT_DEVICESTATE):
			PopulateBrightness(Module, MessageData);
			break;
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
				if (SUCCEEDED(Module->GetValue(XML_SA_LOADTYPE, &LoadType))) {
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

void PopulateCmRelayState(
	IN IPremiseObject* Module,
	IN BYTE did,
	IN WCHAR* MessageData
)
{
	//
	// The proposed new relay state.
	//
	BYTE Status;
	MessageData += parsehex_(MessageData, Status);
	//
	// Skip unused Goto parameter.
	//
	MessageData += 2;
	if (*MessageData) {
		//
		// This is a Direct message, and contains a Channel parameter
		// which indicates the relay to which the message applies, zero
		// for both.
		//
		BYTE Channel;
		parsehex_(MessageData, Channel);
		BYTE FirstRelay;
		BYTE LastRelay;
		if (!Channel) {
			FirstRelay = 1;
			LastRelay = SA_CM_RELAYS;
		} else {
			FirstRelay = Channel;
			LastRelay = Channel;
		}
		CComPtr<IPremiseObjectCollection> Relays;
		if (SUCCEEDED(Module->GetObjectsByType(CComVariant(XML_UPBSA_SCHEMA_ROOT XML_SYS_PATH_SEPARATOR XML_SA_RELAYCONTACT), collectionTypeNoRecurse, &Relays))) {
			long EnumRelay = 0;
			CComPtr<IPremiseObject> Relay;
			for (; SUCCEEDED(Relays->GetItemEx(EnumRelay, &Relay)); EnumRelay++, Relay = NULL) {
				CComVariant RelayID;
				CComVariant Invert;
				if (SUCCEEDED(Relay->GetValue(XML_SA_RELAYID, &RelayID)) && (RelayID.lVal >= FirstRelay) && (RelayID.lVal <= LastRelay) && SUCCEEDED(Relay->GetValue(XML_SYS_INVERT, &Invert))) {
					Relay->SetValueEx(SVCC_DRIVER | SVCC_NOTIFY, XML_SYS_STATE, &CComVariant(Invert.boolVal ? !Status : Status));
					//!!if it is found to be closed (non-zero), should a timer be started now?
				}
			}
		}
	} else {
		//
		// This is a Link message, and the message applies to whatever
		// Receive Component contains the the Link ID, and overrides
		// the setting for that Receive Component.
		//
		UpdateCmReceiveComponentResults(Module, did, Status, TRUE);
	}
}

void OnCmNetworkData(
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
//!!			PopulateVirtualizedInputState(Module, MessageData, did);
			break;
		case MAKE_MDID(MDID_DEVICE_CONTROL_COMMANDS, MDID_DEVICE_CONTROL_COMMAND_ACTIVATELINK):
		case MAKE_MDID(MDID_DEVICE_CONTROL_COMMANDS, MDID_DEVICE_CONTROL_COMMAND_DEACTIVATELINK):
		case MAKE_MDID(MDID_DEVICE_CONTROL_COMMANDS, MDID_DEVICE_CONTROL_COMMAND_GOTO):
//!!different data structure?
//!!			UpdateTransmitComponentResults(Module, did, mdid, MessageData - 2);
			break;
		case MAKE_MDID(MDID_CORE_REPORTS, MDID_DEVICE_CORE_REPORT_DEVICESTATE):
			PopulateCmDeviceState(Module, MessageData);
			break;
		}
		break;
	case UPB_NETWORKDATA_BROADCAST:
		//
		// The Link and Direct messages are fairly mutually exclusive, so there should not be
		// any need to differentiate between the two, except for Goto.
		//
		switch (mdid) {
		case MAKE_MDID(MDID_DEVICE_CONTROL_COMMANDS, MDID_DEVICE_CONTROL_COMMAND_ACTIVATELINK):
			UpdateCmReceiveComponentResults(Module, did, TRUE, FALSE);
			break;
		case MAKE_MDID(MDID_DEVICE_CONTROL_COMMANDS, MDID_DEVICE_CONTROL_COMMAND_DEACTIVATELINK):
			UpdateCmReceiveComponentResults(Module, did, FALSE, FALSE);
			break;
		case MAKE_MDID(MDID_DEVICE_CONTROL_COMMANDS, MDID_DEVICE_CONTROL_COMMAND_GOTO):
			PopulateCmRelayState(Module, did, MessageData);
			break;
		case MAKE_MDID(MDID_DEVICE_CONTROL_COMMANDS, MDID_DEVICE_CONTROL_COMMAND_STORESTATE):
			StoreCmState(Module, did);
			break;
		}
		break;
	}
}

HRESULT STDMETHODCALLTYPE CupbSA::OnUpbNetworkDataChanged(
	IN IPremiseObject* Object,
	IN VARIANT* newValue
)
{
	//
	// Ensure that this is one of the root module objects, and not
	// something added underneath.
	//
	if (newValue->bstrVal[0] && IsObjectOfExplicitType(Object, XML_UPBSA_SCHEMA_ROOT XML_SYS_PATH_SEPARATOR XML_SA_UPBSABASE)) {
		if (IsObjectOfExplicitType(Object, XML_UPBSA_SCHEMA_ROOT XML_SYS_PATH_SEPARATOR XML_SA_USQ)) {
			OnUsqNetworkData(Object, newValue->bstrVal);
		} else if (IsObjectOfExplicitType(Object, XML_UPBSA_SCHEMA_ROOT XML_SYS_PATH_SEPARATOR XML_SA_CM01)) {
			OnCmNetworkData(Object, newValue->bstrVal);
		} else if (IsObjectOfExplicitType(Object, XML_UPBSA_SCHEMA_ROOT XML_SYS_PATH_SEPARATOR XML_SA_SWITCH)) {
			OnSwitchNetworkData(Object, newValue->bstrVal);
		}
	}
	return S_OK;
}

HRESULT STDMETHODCALLTYPE CupbSA::OnDeviceInitializedChanged(
	IN IPremiseObject* Object,
	IN VARIANT* newValue
)
{
	CComVariant SetupRegisterCache;
	if (newValue->boolVal && IsObjectOfExplicitType(Object, XML_UPBSA_SCHEMA_ROOT XML_SYS_PATH_SEPARATOR XML_SA_UPBSABASE) && SUCCEEDED(Object->GetValue(XML_UPB_SETUPREGISTERCACHE, &SetupRegisterCache)) && (lstrlenW(SetupRegisterCache.bstrVal) == 2 * UPB_MODULE_REGISTERS)) {
		if (IsObjectOfExplicitType(Object, XML_UPBSA_SCHEMA_ROOT XML_SYS_PATH_SEPARATOR XML_SA_USQ)) {
			CComVariant VariantSelection;
			if (FAILED(Object->GetValue(XML_SA_VARIANTSELECTION, &VariantSelection))) {
				VariantSelection = SA_ROCKEROPTION_US_UNDEFINED;
			}
			BYTE RockerOptions;
			parsehex_(&SetupRegisterCache.bstrVal[2 * SA_REG_ROCKEROPTIONS], RockerOptions);
			ReconfigUsqButtons(Object, VariantSelection.bVal, SA_USQ_VARIANTSELECTION(RockerOptions));
			PopulateUsqRockerOptions(Object, &SetupRegisterCache.bstrVal[2 * SA_REG_ROCKEROPTIONS]);
			PopulateSwitchReceiveComponents(Object, &SetupRegisterCache.bstrVal[2 * SA_REG_RECEIVECOMPONENTS]);
			PopulateUsqTransmitComponents(Object, &SetupRegisterCache.bstrVal[2 * SA_REG_TRANSMITCOMPONENTS], &SetupRegisterCache.bstrVal[2 * SA_REG_USQ_TRANSMITCOMPONENTS]);
			PopulateRockerActions(Object, &SetupRegisterCache.bstrVal[2 * SA_REG_ROCKERACTIONS], SA_REG_ROCKERACTIONS_BYTES / SA_REG_ROCKERACTIONENTRY);
			PopulateSwitchLedOptions(Object, &SetupRegisterCache.bstrVal[2 * SA_REG_LEDOPTIONS]);
			PopulateDimmerOptions(Object, &SetupRegisterCache.bstrVal[2 * SA_REG_DIMMEROPTIONS]);
			PopulateTransmissionOptions(Object, &SetupRegisterCache.bstrVal[2 * SA_REG_TXCONTROL]);
			PopulateTransmitCommands(Object, &SetupRegisterCache.bstrVal[2 * SA_REG_TRANSMITCOMMANDS], SA_REG_TRANSMITCOMMANDS_BYTES / SA_REG_TXCOMMANDENTRY);
			TransmitNetworkPacket(Object, MAKE_MDID(MDID_DEVICE_CONTROL_COMMANDS, MDID_DEVICE_CONTROL_COMMAND_REPORTSTATE));
		} else if (IsObjectOfExplicitType(Object, XML_UPBSA_SCHEMA_ROOT XML_SYS_PATH_SEPARATOR XML_SA_CM01)) {
			PopulateCmReceiveComponents(Object, &SetupRegisterCache.bstrVal[2 * SA_REG_RECEIVECOMPONENTS]);
			PopulateCmTransmitComponents(Object, &SetupRegisterCache.bstrVal[2 * SA_REG_CM_TRANSMITCOMPONENTS]);
			PopulateCmInitialRelayState(Object, &SetupRegisterCache.bstrVal[2 * SA_REG_CM_INITIALRELAYSTATE]);
			PopulateTransmissionOptions(Object, &SetupRegisterCache.bstrVal[2 * SA_REG_CM_TXCONTROL]);
			//
			// This does not have a multi-color LED, but just ignore
			// the property change errors.
			//
			PopulateSwitchLedOptions(Object, &SetupRegisterCache.bstrVal[2 * SA_REG_CM_LEDOPTIONS]);
			TransmitNetworkPacket(Object, MAKE_MDID(MDID_DEVICE_CONTROL_COMMANDS, MDID_DEVICE_CONTROL_COMMAND_REPORTSTATE));
		} else if (IsObjectOfExplicitType(Object, XML_UPBSA_SCHEMA_ROOT XML_SYS_PATH_SEPARATOR XML_SA_SWITCH)) {
			PopulateSwitchReceiveComponents(Object, &SetupRegisterCache.bstrVal[2 * SA_REG_RECEIVECOMPONENTS]);
			PopulateSwitchLedOptions(Object, &SetupRegisterCache.bstrVal[2 * SA_REG_LEDOPTIONS]);
			PopulateDimmerOptions(Object, &SetupRegisterCache.bstrVal[2 * SA_REG_DIMMEROPTIONS]);
			TransmitNetworkPacket(Object, MAKE_MDID(MDID_DEVICE_CONTROL_COMMANDS, MDID_DEVICE_CONTROL_COMMAND_REPORTSTATE));
		}
	}
	return S_OK;
}

HRESULT STDMETHODCALLTYPE CupbSA::OnLedModeChanged(
	IN IPremiseObject* Object,
	IN VARIANT* newValue
)
{
	if (IsObjectOfExplicitType(Object, XML_UPBSA_SCHEMA_ROOT XML_SYS_PATH_SEPARATOR XML_SA_UPBSABASE)) {
		SetSwitchLedOptions(Object, newValue, NULL, NULL);
	}
	return S_OK;
}

HRESULT STDMETHODCALLTYPE CupbSA::OnLedOnColorChanged(
	IN IPremiseObject* Object,
	IN VARIANT* newValue
)
{
	if (IsObjectOfExplicitType(Object, XML_UPBSA_SCHEMA_ROOT XML_SYS_PATH_SEPARATOR XML_SA_UPBSABASE)) {
		SetSwitchLedOptions(Object, NULL, newValue, NULL);
	}
	return S_OK;
}

HRESULT STDMETHODCALLTYPE CupbSA::OnLedOffColorChanged(
	IN IPremiseObject* Object,
	IN VARIANT* newValue
)
{
	if (IsObjectOfExplicitType(Object, XML_UPBSA_SCHEMA_ROOT XML_SYS_PATH_SEPARATOR XML_SA_UPBSABASE)) {
		SetSwitchLedOptions(Object, NULL, NULL, newValue);
	}
	return S_OK;
}

HRESULT STDMETHODCALLTYPE CupbSA::OnEventPacketTypeChanged(
	IN IPremiseObject* Object,
	IN VARIANT* newValue
)
{
	if (IsObjectOfExplicitType(Object, XML_UPBSA_SCHEMA_ROOT XML_SYS_PATH_SEPARATOR XML_SA_UPBSABASE)) {
		SetTransmissionOptions(Object, newValue, NULL, NULL, NULL, NULL);
	}
	return S_OK;
}

HRESULT STDMETHODCALLTYPE CupbSA::OnAckMessageRequestChanged(
	IN IPremiseObject* Object,
	IN VARIANT* newValue
)
{
	if (IsObjectOfExplicitType(Object, XML_UPBSA_SCHEMA_ROOT XML_SYS_PATH_SEPARATOR XML_SA_UPBSABASE)) {
		SetTransmissionOptions(Object, NULL, newValue, NULL, NULL, NULL);
	}
	return S_OK;
}

HRESULT STDMETHODCALLTYPE CupbSA::OnIdPulseRequestChanged(
	IN IPremiseObject* Object,
	IN VARIANT* newValue
)
{
	if (IsObjectOfExplicitType(Object, XML_UPBSA_SCHEMA_ROOT XML_SYS_PATH_SEPARATOR XML_SA_UPBSABASE)) {
		SetTransmissionOptions(Object, NULL, NULL, newValue, NULL, NULL);
	}
	return S_OK;
}

HRESULT STDMETHODCALLTYPE CupbSA::OnAckPulseRequestChanged(
	IN IPremiseObject* Object,
	IN VARIANT* newValue
)
{
	if (IsObjectOfExplicitType(Object, XML_UPBSA_SCHEMA_ROOT XML_SYS_PATH_SEPARATOR XML_SA_UPBSABASE)) {
		SetTransmissionOptions(Object, NULL, NULL, NULL, newValue, NULL);
	}
	return S_OK;
}

HRESULT STDMETHODCALLTYPE CupbSA::OnTransmissionCountChanged(
	IN IPremiseObject* Object,
	IN VARIANT* newValue
)
{
	if (IsObjectOfExplicitType(Object, XML_UPBSA_SCHEMA_ROOT XML_SYS_PATH_SEPARATOR XML_SA_UPBSABASE)) {
		SetTransmissionOptions(Object, NULL, NULL, NULL, NULL, newValue);
	}
	return S_OK;
}

HRESULT STDMETHODCALLTYPE CupbSA::OnLoadTypeChanged(
	IN IPremiseObject* Object,
	IN VARIANT* newValue
)
{
	if (IsObjectOfExplicitType(Object, XML_UPBSA_SCHEMA_ROOT XML_SYS_PATH_SEPARATOR XML_SA_UPBSABASE)) {
		SetDimmerOptions(Object, newValue, NULL, NULL, NULL, NULL);
	}
	return S_OK;
}

HRESULT STDMETHODCALLTYPE CupbSA::OnReportStateChanged(
	IN IPremiseObject* Object,
	IN VARIANT* newValue
)
{
	if (IsObjectOfExplicitType(Object, XML_UPBSA_SCHEMA_ROOT XML_SYS_PATH_SEPARATOR XML_SA_UPBSABASE)) {
		SetDimmerOptions(Object, NULL, newValue, NULL, NULL, NULL);
	}
	return S_OK;
}

HRESULT STDMETHODCALLTYPE CupbSA::OnTriggerSwitchChanged(
	IN IPremiseObject* Object,
	IN VARIANT* newValue
)
{
	if (IsObjectOfExplicitType(Object, XML_UPBSA_SCHEMA_ROOT XML_SYS_PATH_SEPARATOR XML_SA_TRIGGERINGSWITCH)) {
		SetDimmerOptions(Object, NULL, NULL, newValue, NULL, NULL);
	}
	return S_OK;
}

HRESULT STDMETHODCALLTYPE CupbSA::OnTriggerLastLevelChanged(
	IN IPremiseObject* Object,
	IN VARIANT* newValue
)
{
	if (IsObjectOfExplicitType(Object, XML_UPBSA_SCHEMA_ROOT XML_SYS_PATH_SEPARATOR XML_SA_TRIGGERINGSWITCH)) {
		SetDimmerOptions(Object, NULL, NULL, NULL, newValue, NULL);
	}
	return S_OK;
}

HRESULT STDMETHODCALLTYPE CupbSA::OnDefaultFadeRateChanged(
	IN IPremiseObject* Object,
	IN VARIANT* newValue
)
{
	if (IsObjectOfExplicitType(Object, XML_UPBSA_SCHEMA_ROOT XML_SYS_PATH_SEPARATOR XML_SA_SWITCH)) {
		SetDimmerOptions(Object, NULL, NULL, NULL, NULL, newValue);
	}
	return S_OK;
}

HRESULT STDMETHODCALLTYPE CupbSA::OnFadeRateChanged(
	IN IPremiseObject* Object,
	IN VARIANT* newValue
)
{
	if (IsObjectOfExplicitType(Object, XML_UPB_SCHEMA_ROOT XML_SYS_PATH_SEPARATOR XML_UPB_SWITCHRECEIVECOMPONENT)) {
		BYTE FadeRate = static_cast<BYTE>(newValue->ulVal);
		if (FadeRate > UPB_SWITCH_FADERATE_1HOUR) {
			FadeRate = UPB_SWITCH_FADERATE_DEFAULT;
		}
		SetReceiveComponentItem(Object, FadeRate, 2);
	} else if (IsObjectOfExplicitType(Object, XML_UPBSA_SCHEMA_ROOT XML_SYS_PATH_SEPARATOR XML_SA_ROCKERACTION)) {
		BYTE FadeRate = static_cast<BYTE>(newValue->ulVal);
		if (FadeRate > UPB_SWITCH_FADERATE_1HOUR) {
			FadeRate = UPB_SWITCH_FADERATE_DEFAULT;
		}
		SetRockerActionItem(Object, FadeRate, 2, 1);
	}
	return S_OK;
}

HRESULT STDMETHODCALLTYPE CupbSA::OnLightLevelChanged(
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
			SetReceiveComponentItem(Object, static_cast<BYTE>(static_cast<long>(newValue->dblVal * 100 + 0.5)), 1);
		}
	} else if (IsObjectOfExplicitType(Object, XML_UPBSA_SCHEMA_ROOT XML_SYS_PATH_SEPARATOR XML_SA_ROCKERACTION)) {
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
		if (SUCCEEDED(Object->GetValue(XML_SYS_BUTTONSTATE, &ButtonState)) && (ButtonState.lVal == SYS_BUTTON_STATE_PRESS) && SUCCEEDED(Object->get_Parent(&Button)) && SUCCEEDED(Button->GetValue(XML_SA_BUTTONID, &ButtonID)) && (ButtonID.lVal == 1) && SUCCEEDED(Button->get_Parent(&Module))) {
			Module->SetValueEx(SVCC_DRIVER | SVCC_NOTIFY, XML_SYS_PRESETDIM, newValue);
		}
	}
	return S_OK;
}

HRESULT STDMETHODCALLTYPE CupbSA::OnLastOnLevelChanged(
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
		SetReceiveComponentItem(Object, Level, 1);
	} else if (IsObjectOfExplicitType(Object, XML_UPBSA_SCHEMA_ROOT XML_SYS_PATH_SEPARATOR XML_SA_USQ)) {
		SetUsqRockerOptions(Object, NULL, NULL, newValue);
	}
	return S_OK;
}

HRESULT STDMETHODCALLTYPE CupbSA::OnPresetDimChanged(
	IN IPremiseObject* Object,
	IN VARIANT* newValue
)
{
	if (IsObjectOfExplicitType(Object, XML_UPBSA_SCHEMA_ROOT XML_SYS_PATH_SEPARATOR XML_SA_USQ)) {
		//
		// Just pass it on to the top Press Rocker Action.
		//
		CComPtr<IPremiseObjectCollection> Buttons;
		CComPtr<IPremiseObject> Button;
		CComPtr<IPremiseObjectCollection> Actions;
		CComPtr<IPremiseObject> Action;
		if (SUCCEEDED(Object->GetObjectsByTypeAndPropertyValue(XML_UPBSA_SCHEMA_ROOT XML_SYS_PATH_SEPARATOR XML_SA_KEYPADBUTTON, XML_SA_BUTTONID, CComVariant(1), collectionTypeNoRecurse, &Buttons)) && SUCCEEDED(Buttons->GetItemEx(0, &Button)) && SUCCEEDED(Button->GetObjectsByTypeAndPropertyValue(XML_UPBSA_SCHEMA_ROOT XML_SYS_PATH_SEPARATOR XML_SA_ROCKERACTION, XML_SYS_BUTTONSTATE, CComVariant(SYS_BUTTON_STATE_PRESS), collectionTypeNoRecurse, &Actions)) && SUCCEEDED(Actions->GetItemEx(0, &Action))) {
			Action->SetValueEx(SVCC_NOTIFY, XML_UPB_LIGHTLEVEL, newValue);
		}
	}
	return S_OK;
}

HRESULT STDMETHODCALLTYPE CupbSA::OnLinkIdChanged(
	IN IPremiseObject* Object,
	IN VARIANT* newValue
)
{
	if (IsObjectOfExplicitType(Object, XML_UPB_SCHEMA_ROOT XML_SYS_PATH_SEPARATOR XML_UPB_TRANSMITCOMPONENT)) {
		SetTransmitComponentLinkID(Object, static_cast<BYTE>(newValue->ulVal));
	} else if (IsObjectOfExplicitType(Object, XML_UPB_SCHEMA_ROOT XML_SYS_PATH_SEPARATOR XML_UPB_SWITCHRECEIVECOMPONENT)) {
		SetReceiveComponentItem(Object, static_cast<BYTE>(newValue->ulVal), 0);
	}
	return S_OK;
}

HRESULT STDMETHODCALLTYPE CupbSA::OnCommandChanged(
	IN IPremiseObject* Object,
	IN VARIANT* newValue
)
{
	if (IsObjectOfExplicitType(Object, XML_UPB_SCHEMA_ROOT XML_SYS_PATH_SEPARATOR XML_UPB_TRANSMITCOMMAND) && (lstrlenW(newValue->bstrVal) == 2 * SA_REG_TXCOMMANDENTRY)) {
		CComPtr<IPremiseObject> Container;
		CComPtr<IPremiseObject> Module;
		CComVariant CommandID;
		if (SUCCEEDED(Object->GetValue(XML_UPB_COMMANDID, &CommandID)) && SUCCEEDED(Object->get_Parent(&Container)) && SUCCEEDED(Container->get_Parent(&Module))) {
			if (IsObjectOfExplicitType(Module, XML_UPBSA_SCHEMA_ROOT XML_SYS_PATH_SEPARATOR XML_SA_USQ)) {
				if (CommandID.ulVal < 15) {
					char data[16];
					StringCchPrintfA(data, SIZEOF_ARRAY(data), "%02X%02X%S", MAKE_MDID(MDID_CORE_COMMANDS, MDID_CORE_COMMAND_SETREGISTERVALUES), SA_REG_TRANSMITCOMMANDS + CommandID.ulVal * SA_REG_TXCOMMANDENTRY, newValue->bstrVal);
					Module->SetValueEx(SVCC_NOTIFY | SVCC_FORCE, XML_UPB_UPBNETWORKMESSAGE, &CComVariant(data));
				}
			} else if (IsObjectOfExplicitType(Module, XML_UPBSA_SCHEMA_ROOT XML_SYS_PATH_SEPARATOR XML_SA_CM01)) {
				CComVariant InputID;
				if (SUCCEEDED(Container->GetValue(XML_SA_INPUTID, &InputID)) && (InputID.ulVal <= SA_CM_INPUTS) && (CommandID.ulVal < 2)) {
					char data[16];
					StringCchPrintfA(data, SIZEOF_ARRAY(data), "%02X%02X%S", MAKE_MDID(MDID_CORE_COMMANDS, MDID_CORE_COMMAND_SETREGISTERVALUES),   SA_REG_CM_TRANSMITCOMPONENTS + 1 + (CommandID.ulVal + InputID.ulVal - 1) * SA_REG_CM_TXCOMPONENTENTRY, newValue->bstrVal);
					Module->SetValueEx(SVCC_NOTIFY | SVCC_FORCE, XML_UPB_UPBNETWORKMESSAGE, &CComVariant(data));
					
				}
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
	if (IsObjectOfExplicitType(Object, XML_UPBSA_SCHEMA_ROOT XML_SYS_PATH_SEPARATOR XML_UPB_COMMANDID)) {
		CComPtr<IPremiseObject> TransmitComponent;
		CComPtr<IPremiseObject> Button;
		CComPtr<IPremiseObject> Module;
		if (SUCCEEDED(Object->get_Parent(&TransmitComponent)) && SUCCEEDED(TransmitComponent->get_Parent(&Button)) && SUCCEEDED(Button->get_Parent(&Module)) && IsObjectOfExplicitType(Module, XML_UPBSA_SCHEMA_ROOT XML_SYS_PATH_SEPARATOR XML_SA_USQ)) {
			CComVariant ButtonState;
			CComVariant ButtonID;
			CComVariant OtherToggle;
			if (SUCCEEDED(Object->GetValue(XML_SYS_BUTTONSTATE, &ButtonState)) && (ButtonState.lVal >= SYS_BUTTON_STATE_RELEASE) && (ButtonState.lVal <= SYS_BUTTON_STATE_DOUBLETAP) && SUCCEEDED(Button->GetValue(XML_SA_BUTTONID, &ButtonID)) && (ButtonID.ulVal <= SA_USQ_BUTTONS) && SUCCEEDED(Object->GetValue(ToggleType ? XML_SA_ONTOGGLE : XML_SA_OFFTOGGLE, &OtherToggle))) {
				//
				// The Transmit Components are split.
				//
				BYTE Register;
				if (ButtonID.ulVal <= 2) {
					Register = SA_REG_TRANSMITCOMPONENTS;
				} else {
					Register = SA_REG_USQ_TRANSMITCOMPONENTS;
					ButtonID.ulVal -= 2;
				}
				BYTE Toggle;
				if (ToggleType) {
					Toggle = (newValue->bVal << 4) + (OtherToggle.bVal & 0x0f);
				} else {
					Toggle = (OtherToggle.bVal << 4) + (newValue->bVal & 0x0f);
				}
				char data[16];
				StringCchPrintfA(data, SIZEOF_ARRAY(data), "%02X%02X%02X", MAKE_MDID(MDID_CORE_COMMANDS, MDID_CORE_COMMAND_SETREGISTERVALUES), Register + (ButtonID.ulVal - 1) * SA_REG_TXCOMPONENTENTRY + PremiseButtonStates[ButtonState.ulVal] + 1, Toggle);
				Module->SetValueEx(SVCC_NOTIFY | SVCC_FORCE, XML_UPB_UPBNETWORKMESSAGE, &CComVariant(data));
			}
		}
	}
	return S_OK;
}

HRESULT STDMETHODCALLTYPE CupbSA::OnOnToggleChanged(
	IN IPremiseObject* Object,
	IN VARIANT* newValue
)
{
	return OnToggleChanged(Object, 0, newValue);
}

HRESULT STDMETHODCALLTYPE CupbSA::OnOffToggleChanged(
	IN IPremiseObject* Object,
	IN VARIANT* newValue
)
{
	return OnToggleChanged(Object, 1, newValue);
}

HRESULT STDMETHODCALLTYPE CupbSA::OnBrightnessChanged(
	IN IPremiseObject* Object,
	IN VARIANT* newValue
)
{
	if (IsObjectOfExplicitType(Object, XML_UPBSA_SCHEMA_ROOT XML_SYS_PATH_SEPARATOR XML_SA_SWITCH)) {
		BYTE Brightness = static_cast<BYTE>(static_cast<long>(newValue->dblVal * 100 + 0.5));
		char Data[16];
		StringCchPrintfA(Data, sizeof(Data), "%02X", Brightness);
		TransmitNetworkPacket(Object, MAKE_MDID(MDID_DEVICE_CONTROL_COMMANDS, MDID_DEVICE_CONTROL_COMMAND_GOTO), Data);
		Object->SetValueEx(SVCC_DRIVER | SVCC_NOTIFY, XML_SYS_POWERSTATE, &CComVariant(Brightness));
	}
	return S_OK;
}

HRESULT STDMETHODCALLTYPE CupbSA::OnPowerStateChanged(
	IN IPremiseObject* Object,
	IN VARIANT* newValue
)
{
	if (IsObjectOfExplicitType(Object, XML_UPBSA_SCHEMA_ROOT XML_SYS_PATH_SEPARATOR XML_SA_SWITCH)) {
		char Data[16];
		StringCchPrintfA(Data, sizeof(Data), "%02X%02X", newValue->boolVal ? 100 : 0, UPB_SWITCH_FADERATE_SNAP);
		TransmitNetworkPacket(Object, MAKE_MDID(MDID_DEVICE_CONTROL_COMMANDS, MDID_DEVICE_CONTROL_COMMAND_GOTO), Data);
		Object->SetValueEx(SVCC_DRIVER | SVCC_NOTIFY, XML_SYS_BRIGHTNESS, &CComVariant(newValue->boolVal ? 1 : 0));
	}
	return S_OK;
}

HRESULT STDMETHODCALLTYPE CupbSA::OnStateChanged(
	IN IPremiseObject* Object,
	IN VARIANT* newValue
)
{
	if (IsObjectOfExplicitType(Object, XML_UPBSA_SCHEMA_ROOT XML_SYS_PATH_SEPARATOR XML_SA_RELAYCONTACT)) {
		CComVariant RelayID;
		CComVariant Invert;
		CComPtr<IPremiseObject> Module;
		if (SUCCEEDED(Object->GetValue(XML_SA_RELAYID, &RelayID)) && (RelayID.lVal > 0) && (RelayID.lVal <= SA_CM_RELAYS) && SUCCEEDED(Object->GetValue(XML_SYS_INVERT, &Invert)) && SUCCEEDED(Object->get_Parent(&Module))) {
			BOOL State = Invert.boolVal ? !newValue->boolVal : newValue->boolVal;
			char data[16];
			StringCchPrintfA(data, SIZEOF_ARRAY(data), "%02X%02X00%02X", MAKE_MDID(MDID_DEVICE_CONTROL_COMMANDS, MDID_DEVICE_CONTROL_COMMAND_GOTO), State ? 1 : 0, RelayID.lVal);
			Module->SetValueEx(SVCC_NOTIFY | SVCC_FORCE, XML_UPB_UPBNETWORKMESSAGE, &CComVariant(data));
			//!!if it is closed (non-zero), should a timer be started now?
		}
	} else if (IsObjectOfExplicitType(Object, XML_UPBSA_SCHEMA_ROOT XML_SYS_PATH_SEPARATOR XML_SA_RELAYRECEIVECOMPONENT)) {
		SetReceiveComponentItem(Object, static_cast<BYTE>(newValue->boolVal), 1);
	}
	return S_OK;
}

HRESULT STDMETHODCALLTYPE CupbSA::OnInvertChanged(
	IN IPremiseObject* Object,
	IN VARIANT* newValue
)
{
	CComVariant State;
	if ((IsObjectOfExplicitType(Object, XML_UPBSA_SCHEMA_ROOT XML_SYS_PATH_SEPARATOR XML_SA_RELAYCONTACT) || IsObjectOfExplicitType(Object, XML_UPBSA_SCHEMA_ROOT XML_SYS_PATH_SEPARATOR XML_SA_INPUTCONTACT)) && SUCCEEDED(Object->GetValue(XML_SYS_STATE, &State))) {
		State.boolVal = !State.boolVal;
		Object->SetValueEx(SVCC_DRIVER | SVCC_NOTIFY, XML_SYS_STATE, &State);
	}
	return S_OK;
}

HRESULT STDMETHODCALLTYPE CupbSA::OnAutoResetChanged(
	IN IPremiseObject* Object,
	IN VARIANT* newValue
	)
{
	if (IsObjectOfExplicitType(Object, XML_UPBSA_SCHEMA_ROOT XML_SYS_PATH_SEPARATOR XML_SA_RELAYCONTACT)) {
		//!!cancel or set the timer if any relay is set TRUE (or FALSE on Invert)
	}
	return S_OK;
}

HRESULT STDMETHODCALLTYPE CupbSA::OnInitialStateChanged(
	IN IPremiseObject* Object,
	IN VARIANT* newValue
)
{
	CComPtr<IPremiseObject> Module;
	CComPtr<IPremiseObjectCollection> Relays;
	CComVariant ChangedRelayID;
	//
	// Get all the other initial states for the Relays, as they all must be set in a single register.
	//
	if (IsObjectOfExplicitType(Object, XML_UPBSA_SCHEMA_ROOT XML_SYS_PATH_SEPARATOR XML_SA_RELAYCONTACT) && SUCCEEDED(Object->GetValue(XML_SA_RELAYID, &ChangedRelayID)) && (ChangedRelayID.lVal > 0) && (ChangedRelayID.lVal <= SA_CM_RELAYS) && SUCCEEDED(Object->get_Parent(&Module)) && IsObjectOfExplicitType(Module, XML_UPBSA_SCHEMA_ROOT XML_SYS_PATH_SEPARATOR XML_SA_CM01) && SUCCEEDED(Module->GetObjectsByType(CComVariant(XML_UPBSA_SCHEMA_ROOT XML_SYS_PATH_SEPARATOR XML_SA_RELAYCONTACT), collectionTypeNoRecurse, &Relays))) {
		BYTE InitialStates;
		SA_CM_SETOPTION_INITIALRELAYSTATE(InitialStates, ChangedRelayID.lVal, newValue->boolVal);
		long EnumRelay = 0;
		CComPtr<IPremiseObject> Relay;
		for (; SUCCEEDED(Relays->GetItemEx(EnumRelay, &Relay)); EnumRelay++, Relay = NULL) {
			CComVariant RelayID;
			CComVariant InitialState;
			if (SUCCEEDED(Relay->GetValue(XML_SA_RELAYID, &RelayID)) && (RelayID.lVal > 0) && (RelayID.lVal <= SA_CM_RELAYS) && (RelayID.lVal != ChangedRelayID.lVal) && SUCCEEDED(Relay->GetValue(XML_SA_INITIALSTATE, &InitialState))) {
				SA_CM_SETOPTION_INITIALRELAYSTATE(InitialStates, RelayID.lVal, InitialState.boolVal);
			}
		}
		char data[16];
		StringCchPrintfA(data, SIZEOF_ARRAY(data), "%02X%02X%02X", MAKE_MDID(MDID_CORE_COMMANDS, MDID_CORE_COMMAND_SETREGISTERVALUES), SA_REG_CM_INITIALRELAYSTATE, InitialStates);
		Module->SetValueEx(SVCC_NOTIFY | SVCC_FORCE, XML_UPB_UPBNETWORKMESSAGE, &CComVariant(data));
	}
	return S_OK;
}

HRESULT STDMETHODCALLTYPE CupbSA::OnVirtualizeChanged(
	IN IPremiseObject* Object,
	IN VARIANT* newValue
)
{
	if (newValue->boolVal) {
		if (IsObjectOfExplicitType(Object, XML_UPBSA_SCHEMA_ROOT XML_SYS_PATH_SEPARATOR XML_SA_USQ)) {
			VirtualizeUsq(Object);
		} else if (IsObjectOfExplicitType(Object, XML_UPBSA_SCHEMA_ROOT XML_SYS_PATH_SEPARATOR XML_SA_CM01)) {
			VirtualizeCm(Object);
		} else if (IsObjectOfExplicitType(Object, XML_UPBSA_SCHEMA_ROOT XML_SYS_PATH_SEPARATOR XML_SA_SWITCH)) {
			VirtualizeSwitch(Object);
		}
	}
	return S_OK;
}

HRESULT STDMETHODCALLTYPE CupbSA::OnLocalLoadConnectChanged(
	IN IPremiseObject* Object,
	IN VARIANT* newValue
)
{
	if (IsObjectOfExplicitType(Object, XML_UPBSA_SCHEMA_ROOT XML_SYS_PATH_SEPARATOR XML_SA_USQ)) {
		SetUsqRockerOptions(Object, newValue, NULL, NULL);
	}
	return S_OK;
}

HRESULT STDMETHODCALLTYPE CupbSA::OnLocalLoadControlChanged(
	IN IPremiseObject* Object,
	IN VARIANT* newValue
)
{
	if (IsObjectOfExplicitType(Object, XML_UPBSA_SCHEMA_ROOT XML_SYS_PATH_SEPARATOR XML_SA_USQ)) {
		SetUsqRockerOptions(Object, NULL, newValue, NULL);
	}
	return S_OK;
}

HRESULT STDMETHODCALLTYPE CupbSA::OnVariantSelectionChanged(
	IN IPremiseObject* Object,
	IN VARIANT* newValue
)
{
	CComVariant SetupRegisterCache;
	if (IsObjectOfExplicitType(Object, XML_UPBSA_SCHEMA_ROOT XML_SYS_PATH_SEPARATOR XML_SA_USQ) && SUCCEEDED(Object->GetValue(XML_UPB_SETUPREGISTERCACHE, &SetupRegisterCache)) && (lstrlenW(SetupRegisterCache.bstrVal) == 2 * UPB_MODULE_REGISTERS)) {
		BYTE RockerOptions;
		parsehex_(&SetupRegisterCache.bstrVal[2 * SA_REG_ROCKEROPTIONS], RockerOptions);
		ReconfigUsqButtons(Object, SA_USQ_VARIANTSELECTION(RockerOptions), newValue->bVal);
		PopulateUsqTransmitComponents(Object, &SetupRegisterCache.bstrVal[2 * SA_REG_TRANSMITCOMPONENTS], &SetupRegisterCache.bstrVal[2 * SA_REG_USQ_TRANSMITCOMPONENTS]);
		PopulateRockerActions(Object, &SetupRegisterCache.bstrVal[2 * SA_REG_ROCKERACTIONS], SA_REG_ROCKERACTIONS_BYTES / SA_REG_ROCKERACTIONENTRY);
	}
	return S_OK;
}

HRESULT STDMETHODCALLTYPE CupbSA::OnUpbNetworkIdleChanged(
	IN IPremiseObject* Object,
	IN VARIANT* newValue
)
{
	if (newValue->boolVal) {
		if (IsObjectOfExplicitType(Object, XML_UPBSA_SCHEMA_ROOT XML_SYS_PATH_SEPARATOR XML_SA_SWITCH) || IsObjectOfExplicitType(Object, XML_UPBSA_SCHEMA_ROOT XML_SYS_PATH_SEPARATOR XML_SA_CM01)) {
			TransmitNetworkPacket(Object, MAKE_MDID(MDID_DEVICE_CONTROL_COMMANDS, MDID_DEVICE_CONTROL_COMMAND_REPORTSTATE));
		}
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
HRESULT STDMETHODCALLTYPE CupbSA::OnPropertyChanged(
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
		if (SUCCEEDED(Object->GetAncestorByType(XML_UPBSA_SCHEMA_ROOT XML_SYS_PATH_SEPARATOR XML_SA_UPBSABASE, &Ancestor)) && SUCCEEDED(Property->GetValue(XML_SYS_NAME, &PropertyName))) {
			if (!lstrcmpW(PropertyName.bstrVal, XML_UPB_UPBNETWORKDATA)) {
				return OnUpbNetworkDataChanged(Object, NewValue);
			} else if (!lstrcmpW(PropertyName.bstrVal, XML_SYS_BRIGHTNESS)) {
				return OnBrightnessChanged(Object, NewValue);
			} else if (!lstrcmpW(PropertyName.bstrVal, XML_SYS_POWERSTATE)) {
				return OnPowerStateChanged(Object, NewValue);
			} else if (!lstrcmpW(PropertyName.bstrVal, XML_SYS_STATE)) {
				return OnStateChanged(Object, NewValue);
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
			} else if (!lstrcmpW(PropertyName.bstrVal, XML_SA_LOADTYPE)) {
				return OnLoadTypeChanged(Object, NewValue);
			} else if (!lstrcmpW(PropertyName.bstrVal, XML_SA_REPORTSTATE)) {
				return OnReportStateChanged(Object, NewValue);
			} else if (!lstrcmpW(PropertyName.bstrVal, XML_SA_TRIGGERSWITCH)) {
				return OnTriggerSwitchChanged(Object, NewValue);
			} else if (!lstrcmpW(PropertyName.bstrVal, XML_SA_TRIGGERLASTLEVEL)) {
				return OnTriggerLastLevelChanged(Object, NewValue);
			} else if (!lstrcmpW(PropertyName.bstrVal, XML_UPB_DEFAULTFADERATE)) {
				return OnDefaultFadeRateChanged(Object, NewValue);
			} else if (!lstrcmpW(PropertyName.bstrVal, XML_UPB_FADERATE)) {
				return OnFadeRateChanged(Object, NewValue);
			} else if (!lstrcmpW(PropertyName.bstrVal, XML_UPB_LIGHTLEVEL)) {
				return OnLightLevelChanged(Object, NewValue);
			} else if (!lstrcmpW(PropertyName.bstrVal, XML_UPB_LASTONLEVEL)) {
				return OnLastOnLevelChanged(Object, NewValue);
			} else if (!lstrcmpW(PropertyName.bstrVal, XML_UPB_LINKID)) {
				return OnLinkIdChanged(Object, NewValue);
			} else if (!lstrcmpW(PropertyName.bstrVal, XML_UPB_COMMAND)) {
				return OnCommandChanged(Object, NewValue);
			} else if (!lstrcmpW(PropertyName.bstrVal, XML_SA_ONTOGGLE)) {
				return OnOnToggleChanged(Object, NewValue);
			} else if (!lstrcmpW(PropertyName.bstrVal, XML_SA_OFFTOGGLE)) {
				return OnOffToggleChanged(Object, NewValue);
			} else if (!lstrcmpW(PropertyName.bstrVal, XML_UPB_VIRTUALIZE)) {
				return OnVirtualizeChanged(Object, NewValue);
			} else if (!lstrcmpW(PropertyName.bstrVal, XML_SYS_PRESETDIM)) {
				return OnPresetDimChanged(Object, NewValue);
			} else if (!lstrcmpW(PropertyName.bstrVal, XML_SYS_INVERT)) {
				return OnInvertChanged(Object, NewValue);
			} else if (!lstrcmpW(PropertyName.bstrVal, XML_SYS_AUTORESET)) {
				return OnAutoResetChanged(Object, NewValue);
			} else if (!lstrcmpW(PropertyName.bstrVal, XML_SA_INITIALSTATE)) {
				return OnInitialStateChanged(Object, NewValue);
			} else if (!lstrcmpW(PropertyName.bstrVal, XML_SA_LOCALLOADCONNECT)) {
				return OnLocalLoadConnectChanged(Object, NewValue);
			} else if (!lstrcmpW(PropertyName.bstrVal, XML_SA_LOCALLOADCONTROL)) {
				return OnLocalLoadControlChanged(Object, NewValue);
			} else if (!lstrcmpW(PropertyName.bstrVal, XML_SA_VARIANTSELECTION)) {
				return OnVariantSelectionChanged(Object, NewValue);
			} else if (!lstrcmpW(PropertyName.bstrVal, XML_UPB_UPBNETWORKIDLE)) {
				return OnUpbNetworkIdleChanged(Object, NewValue);
			}
		}
	}
	return E_NOTIMPL;
}
