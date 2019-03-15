#include "stdafx.h"
#include <stdio.h>
#include <stdlib.h>
#include <IPTypes.h>
#include <iphlpapi.h>
#define STRSAFE_LIB
#include <strsafe.h>
#include "upb.h"
#include "upbController.h"

/*
	Template for a function to parse two character hex bytes of the specified type. This does
	not validate the data, nor the length of the string passed in.

		lpsz - The string containing hex number pairs representing bytes.
		t - The place in which to put the binary result of the parsed hex data. This also
			determines the size of the data parsed.

	Return - The number of characters parsed from the string.
*/
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
	Template for a function to parse two character hex bytes of the specified type. This does
	not validate the data, nor the length of the string passed in.

		lpsz - The string containing hex number pairs representing bytes.
		t - The place in which to put the binary result of the parsed hex data. This also
			determines the size of the data parsed.

	Return - The number of characters parsed from the string.
*/
template <class T> static int parsehex_(
	IN const char* lpsz,
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
	Parses two character hex bytes into a USHORT. This validates the string coming
	in, and produces an error on invalid data. The data must be at least 4 hex digits in length.

		Data - The string containing hex number pairs representing bytes.
		Number - The place in which to put the binary result of the parsed hex data.

	Return - A pointer to the next place in the Data string after the number parsed, else NULL
		on an error.
*/
BSTR HexStringToShort(
	IN BSTR Data,
	OUT USHORT* Number
)
{
	int digits = 0;
	for (*Number = 0; isxdigit(*Data); Data++) {
		digits++;
		*Number = (*Number * 16) + (isdigit(*Data) ? (*Data - '0') : (10 + (*Data | 0x20) - 'a'));
	}
	return digits == (2 * sizeof(*Number)) ? Data : NULL;
}

/*
	Converts binary data into two character hex bytes, placing them into the specified string.
	This does not terminate the string, and therefore is used to insert hex data into a string.

		HexString - The place in which to put the hex digits.
		ByteData - The binary source data.
		DataLen - The number of bytes contained in the ByteData binary data.
*/
void BytesToHex(
	OUT WCHAR* HexString,
	IN const BYTE* ByteData,
	IN int DataLen
)
{
	for (int i = 0; i < DataLen; i++) {
		HexString[2 * i] = NibToHex(ByteData[i] >> 4);
		HexString[2 * i + 1] = NibToHex(ByteData[i] & 0xf);		
	}
}

/*
	Create the string format of the firmware version number in the form	of "x.y" where "x"
	represents the high byte of the version in a 1 or 2 digit hex format, and "y"
	represents the low byte in a 2 digit hex format.

		FirmwareVersion - The firmware version to return as a string.
		FirmwareVersionString - The place in which to put an allocated buffer with the
			string format of the firmware version. This must be freed by the caller.

	Return - S_OK if the buffer was allocated and filled, else E_FAIL.
*/
HRESULT MakeFirmwareVersionString(
	IN USHORT FirmwareVersion,
	OUT BSTR* FirmwareVersionString
)
{
	WCHAR s[16];
	StringCchPrintfW(s, SIZEOF_ARRAY(s), L"%X.%02X", FirmwareVersion >> 8, FirmwareVersion & 0xff);
	return (*FirmwareVersionString = SysAllocString(s)) ? S_OK : E_FAIL;
}

/*
	Indicate if the device has the initialized property set or not, which is assumed to
	indicate if the device has actually been located and initialized.

		Module - The module being tested.

	Return - TRUE if initialized, else FALSE.
*/
BOOL IsDeviceInitialized(
	IN IPremiseObject* Module
)
{
	CComVariant DeviceInitialized;
	return SUCCEEDED(Module->GetValue(XML_UPB_UPBDEVICEINITIALIZED, &DeviceInitialized)) && DeviceInitialized.boolVal;
}

/*
	Obtain the common set of identifiers used for communicating with the hardware device.

		Module - The module whose identifiers are to be returned.
		NetworkID - The place in which to put the network identifier for this module.
		ModuleID - The place in which to put the network-specific module identifier.
		UniqueID - The place in which to put the globally unique identifier for this
			module.

	Return - S_OK if the identifiers were obtained, else an error.
*/
HRESULT GetModuleIdentifiers(
	IN IPremiseObject* Module,
	OUT BYTE& NetworkID,
	OUT BYTE& ModuleID,
	OUT USHORT& UniqueID
)
{
	CComVariant ThisNetworkID;
	CComVariant ThisModuleID;
	CComVariant ThisUniqueID;
	if (SUCCEEDED(Module->GetValue(XML_UPB_NETWORKID, &ThisNetworkID)) && SUCCEEDED(Module->GetValue(XML_UPB_MODULEID, &ThisModuleID)) && SUCCEEDED(Module->GetValue(XML_UPB_UPBUNIQUEID, &ThisUniqueID))) {
		NetworkID = ThisNetworkID.bVal;
		ModuleID = ThisModuleID.bVal;
		UniqueID = ThisUniqueID.uiVal;
		return S_OK;
	}
	return E_FAIL;
}

/*
	Read the device's register cache property. If the cache does not exist, or there is a
	problem detected with it, a new zeroed cache is generated and returned.

		Module - The module whose cache is to be returned.
		SetupRegisterCache - The place in which to return the cache data.

	Return - S_OK if the cache is returned, else an error on a memory failure.
*/
HRESULT ReadSetupRegisterCache(
	IN IPremiseObject* Module,
	OUT VARIANT* SetupRegisterCache
)
{
	if (FAILED(Module->GetValue(XML_UPB_SETUPREGISTERCACHE, SetupRegisterCache)) || (lstrlenW(SetupRegisterCache->bstrVal) < 2 * UPB_MODULE_REGISTERS)) {
		//
		// If the cache is broken in any way, generate a blank zero-filled one. Delete any
		// string that might have been obtained from the query.
		//
		VariantClear(SetupRegisterCache);
		//
		// All device caches are the same size, which covers the maximum number of
		// registers available. The extra hex digits are just zero-filled and ignored.
		// Initialized the string to zeroes and NULL terminate.
		//
		SetupRegisterCache->bstrVal = SysAllocStringLen(NULL, 2 * UPB_MODULE_REGISTERS + 1);
		if (!SetupRegisterCache->bstrVal) {
			return E_OUTOFMEMORY;
		}
		SetupRegisterCache->vt = VT_BSTR;
		for (int i = 0; i < 2 * UPB_MODULE_REGISTERS; i++) {
			SetupRegisterCache->bstrVal[i] = '0';
		}
		SetupRegisterCache->bstrVal[2 * UPB_MODULE_REGISTERS] = 0;
	}
	return S_OK;
}

/*
	Sets the UPB password property on a module using the four hex digit string format to
	duplicate the UPStart displayed format.

		Module - The module whose password property is to be updated.
		Password - The password to set.
*/
void CupbController::SetUpbPassword(
	IN IPremiseObject* Module,
	IN USHORT Password
)
{
	//
	// Store the last password set in order to try out on any new modules that later
	// appear as a first guess. Since most modules end up using the same password, this shortens
	// the guess time.
	//
	m_spSite->SetValueEx(SVCC_DRIVER, XML_UPB_LASTPASSWORDSET, &CComVariant(Password));
	//
	// The password is stored as a four digit hex string so that it resembles what is
	// displayed in UPStart.
	//
	char PasswordString[16];
	StringCchPrintfA(PasswordString, SIZEOF_ARRAY(PasswordString), "%04X", Password);
	Module->SetValueEx(SVCC_DRIVER | SVCC_NOTIFY, XML_UPB_NETWORKPASSWORD, &CComVariant(PasswordString));
	//
	// Update the cache at the same time.
	//
	CComVariant SetupRegisterCache;
	if (SUCCEEDED(ReadSetupRegisterCache(Module, &SetupRegisterCache))) {
		//
		// The conversion function is byte-based, so the ordering needs to be reversed.
		//
		Password = ntohs(Password);
		BytesToHex(&SetupRegisterCache.bstrVal[2 * UPB_REG_PASSWORD], reinterpret_cast<BYTE*>(&Password), sizeof(Password));
		Module->SetValueEx(SVCC_DRIVER, XML_UPB_SETUPREGISTERCACHE, &SetupRegisterCache);
	}
}

/*
	Read the UPB password property on the specified module, if it is set, and return the
	password in binary format.

		Module - The module whose password is to be returned.
		Password - The place in which to return the password.

	Return - TRUE if the password is returned, else FALSE if the property could not be read, or
		if the password string was invalid or zero length (never been set on the module).
*/
BOOL GetUpbPassword(
	IN IPremiseObject* Module,
	OUT USHORT* Password
)
{
	CComVariant TextPassword;
	BSTR Pos;
	return SUCCEEDED(Module->GetValue(XML_UPB_NETWORKPASSWORD, &TextPassword)) && (Pos = HexStringToShort(TextPassword.bstrVal, Password)) && !*Pos;
}

/*
	Overrides CPremiseSubscriber::OnBrokerDetach.

	Return - S_OK.
*/
HRESULT CupbController::OnBrokerDetach(
)
{
	return S_OK;
}

/*
	Overrides CPremiseSubscriber::OnBrokerAttach.

	Return - S_OK.
*/
HRESULT CupbController::OnBrokerAttach(
)
{
	//
	// Cache the UPB Version protocol that the driver will comply to.
	//
	CComVariant UpbVersion;
	if (FAILED(m_spSite->GetValue(XML_UPB_UPBVERSION, &UpbVersion)) || (UpbVersion.lVal < UPB_VERSION_FIRST)) {
		UpbVersion = UPB_VERSION_DEFAULT;
	}
	m_UpbVersion = UpbVersion.lVal;
	ResetAddressResolution();
	//
	// Initialize the network idle counters used in OnHeartbeat.
	//
	m_IdleTickCount = GetTickCount();
	m_IdleEnumCount = 0;
	return S_OK;
}

/*
	Implements CPremiseBufferedPortDeviceBase::OnConfigurePort. Handles configuring the port
	object properties as it is being opened.

		Port - The port being opened.

	Return - S_OK.
*/
HRESULT CupbController::OnConfigurePort(
	IN IPremiseObject* Port
)
{
	Port->SetValue(L"Baud", &CComVariant(SERIAL_4800));
	Port->SetValue(L"DataBits", &CComVariant(SERIAL_DATABITS_8));
	Port->SetValue(L"Parity", &CComVariant(SERIAL_PARITY_NONE));
	Port->SetValue(L"StopBits", &CComVariant(SERIAL_STOPBITS_1));
	Port->SetValue(L"FlowControl", &CComVariant(SERIAL_FLOWCONTROL_NONE));
	Port->SetValue(L"RTS", &CComVariant(SERIAL_RTS_ENABLE));
	Port->SetValue(L"DTR", &CComVariant(SERIAL_DTR_ENABLE));
	Port->SetValue(L"CTS", &CComVariant(FALSE));
	Port->SetValue(L"DSR", &CComVariant(FALSE));
	return S_OK;
}

/*
	Resets duplicate address mode, effectively turning it off. This might be done at the end of
	running duplicate address resolution, or when resetting the port after a serious
	communications error.
*/
void CupbController::ResetAddressResolution(
)
{
	m_UnpopulatedNetworkID = GLOBAL_NETWORKID;
	m_DuplicateNetworkID = GLOBAL_NETWORKID;
	m_DuplicateSourceID = BROADCAST_DEVICEID;
	m_DuplicateUniqueID = INVALID_UNIQUE_ID;
	m_dam = DuplicateAddressMode_None;
}

/*
	Resets the UpbDeviceInitialized property state to FALSE for all objects in the tree. This
	also includes resetting the UpbChecksumDifference so that the module does not get put in the
	queue for password discovery if the password property has not been set until the module
	actually has read all the register data.
*/
void CupbController::ResetAllInitializedStates(
)
{
	CComPtr<IPremiseObjectCollection> Objects;
	if (SUCCEEDED(m_spSite->GetObjectsByPropertyValue(XML_UPB_UPBDEVICEINITIALIZED, CComVariant(TRUE), FALSE, &Objects))) {
		long EnumDevice = 0;
		CComPtr<IPremiseObject> ThisObject;
		for (; SUCCEEDED(Objects->GetItemEx(EnumDevice, &ThisObject)); EnumDevice++, ThisObject = NULL) {
			ThisObject->SetValueEx(SVCC_DRIVER | SVCC_NOTIFY, XML_UPB_UPBDEVICEINITIALIZED, &CComVariant(FALSE));
			ThisObject->SetValueEx(SVCC_DRIVER, XML_UPB_UPBCHECKSUMDIFFERENCE, &CComVariant(UNINITIALIZED_CHECKSUM_DIFFERENCE));
		}
	}
}

/*
	Calculate the checksum of a UPB message based on the given binary data. This is a 2's
	compliment truncated to 8 bits.

		Message - The binary message data.
		MessageLen - The number of bytes in the message.

	Return - The message checksum.
*/
BYTE CalculateChecksum(
	IN const BYTE* Message,
	IN int MessageLen
)
{
	int Checksum = 0;
	for (int i = 0; i < MessageLen; i++) {
		Checksum -= Message[i];
	}
	return static_cast<BYTE>(Checksum);
}

/*
	Calculate the checksum of a UPB message based on the given hex digit string. This is a 2's
	compliment truncated to 8 bits.

		Message - The hex digit message data.

	Return - The message checksum.
*/
BYTE CalculateChecksum(
	IN LPCSTR Message
)
{
	int Checksum = 0;
	for (; *Message;) {
		BYTE c;
		Message += parsehex(Message, c);
		Checksum -= c;
	}
	return static_cast<BYTE>(Checksum);
}

/*
	Find an initialized module within the tree based on the network and module identifiers.

		NetworkID - The network identifier of the module being searched for.
		ModuleID - The module identifier of the module being searched for.
		Module - The place in which to put the Premise Object interface to the module.

	Return - S_OK if the module was found, else an error.
*/
HRESULT CupbController::FindInitializedUpbModule(
	IN BYTE NetworkID,
	IN BYTE ModuleID,
	OUT IPremiseObject** Module
)
{
	//
	// Create a collection based on the ModuleID first, as it is likely to be smaller.
	//
	CComPtr<IPremiseObjectCollection> Modules;
	if (SUCCEEDED(m_spSite->GetObjectsByTypeAndPropertyValue(XML_UPB_SCHEMA_ROOT XML_SYS_PATH_SEPARATOR XML_UPB_MODULE, XML_UPB_MODULEID, CComVariant(ModuleID), collectionTypeNoRecurse, &Modules))) {
		long EnumDevice = 0;
		CComPtr<IPremiseObject> ThisModule;
		//
		// Then enumerate each item within the collection to find one that has the correct
		// NetworkID and is initialized.
		//
		for (; SUCCEEDED(Modules->GetItemEx(EnumDevice, &ThisModule)); EnumDevice++, ThisModule = NULL) {
			CComVariant ThisNetworkID;
			if (SUCCEEDED(ThisModule->GetValue(XML_UPB_NETWORKID, &ThisNetworkID)) && (ThisNetworkID.bVal == NetworkID) && IsDeviceInitialized(ThisModule)) {
				return ThisModule->QueryInterface(Module);
			}
		}
	}
	return E_FAIL;
}

/*
	Find an initialized module within the tree based on the unique identifier.

		UniqueID - The unique identifier of the module being searched for.
		Module - The place in which to put the Premise Object interface to the module.

	Return - S_OK if the module was found, else an error.
*/
HRESULT CupbController::FindInitializedUpbModule(
	IN USHORT UniqueID,
	OUT IPremiseObject** Module
)
{
	CComPtr<IPremiseObjectCollection> Modules;
	CComPtr<IPremiseObject> ThisModule;
	//
	// Do a check for INVALID_UNIQUE_ID, as this is used in cases where the identifier may not
	// be valid.
	//
	// Only check the first item in the collection, since the UniqueID is supposed to be unique.
	//
	if ((UniqueID != INVALID_UNIQUE_ID) && SUCCEEDED(m_spSite->GetObjectsByPropertyValue(XML_UPB_UPBUNIQUEID, CComVariant(UniqueID), FALSE, &Modules)) && SUCCEEDED(Modules->GetItemEx(0, &ThisModule)) && IsDeviceInitialized(ThisModule)) {
		return ThisModule->QueryInterface(Module);
	}
	return E_FAIL;
}

/*
	Find the first initialized PIM within the tree.

		PIM - The place in which to put the Premise Object interface to the PIM.

	Return - S_OK if the PIM was found, else an error.
*/
HRESULT CupbController::FindInitializedPIM(
	OUT IPremiseObject** PIM
)
{
	CComPtr<IPremiseObjectCollection> PIMs;
	CComPtr<IPremiseObject> ThisPIM;
	//
	// Return the first initialized PIM found in the collection created.
	//
	if (SUCCEEDED(m_spSite->GetObjectsByTypeAndPropertyValue(XML_UPB_SCHEMA_ROOT XML_SYS_PATH_SEPARATOR XML_UPB_PIM, XML_UPB_UPBDEVICEINITIALIZED, CComVariant(TRUE), collectionTypeNoRecurse, &PIMs)) && SUCCEEDED(PIMs->GetItemEx(0, &ThisPIM))) {
		return ThisPIM->QueryInterface(PIM);
	}
	return E_FAIL;
}

/*
	Find a module within the tree based on the base registers read from the hardware device.
	This is initially done before the module is known and initialized, and allows detailed
	comparison with the manufacturer and product identifiers, along with the UPB conforming
	version just as an extra check.

		Report - The report containing the first 16 registers from a module.
		Module - The place in which to put the Premise Object interface to the module.

	Return - S_OK if the module was found, else an error.
*/
HRESULT CupbController::FindUpbModule(
	IN INITIAL_MODULE_REG_REPORT* Report,
	OUT IPremiseObject** Module
)
{
	//
	// Create a collection based on the ModuleID first, as it is likely to be smaller.
	//
	CComPtr<IPremiseObjectCollection> Modules;
	if (SUCCEEDED(m_spSite->GetObjectsByTypeAndPropertyValue(XML_UPB_SCHEMA_ROOT XML_SYS_PATH_SEPARATOR XML_UPB_MODULE, XML_UPB_MODULEID, CComVariant(Report->ModuleID), collectionTypeNoRecurse, &Modules))) {
		long EnumDevice = 0;
		CComPtr<IPremiseObject> ThisModule;
		for (; SUCCEEDED(Modules->GetItemEx(EnumDevice, &ThisModule)); EnumDevice++, ThisModule = NULL) {
			CComVariant NetworkID;
			CComVariant ManufacturerID;
			CComVariant ProductID;
			CComVariant UpbVersion;
			//
			// Perform the detailed check to determine if this module represents the exact same
			// type of hardware device that the registers contain information for.
			//
			if (SUCCEEDED(ThisModule->GetValue(XML_UPB_NETWORKID, &NetworkID)) && (NetworkID.bVal == Report->NetworkID) && SUCCEEDED(ThisModule->GetValue(XML_UPB_MANUFACTURERID, &ManufacturerID)) && (ManufacturerID.bVal == Report->ManufacturerID) && SUCCEEDED(ThisModule->GetValue(XML_UPB_PRODUCTID, &ProductID)) && (ProductID.bVal == Report->ProductID) && SUCCEEDED(ThisModule->GetValue(XML_UPB_UPBVERSION, &UpbVersion)) && (UpbVersion.bVal == Report->UpbVersion)) {
				return ThisModule->QueryInterface(Module);
			}
		}
	}
	return E_FAIL;
}

/*
	Find a module within the tree based on the network and module identifiers.

		NetworkID - The network identifier of the module being searched for.
		ModuleID - The module identifier of the module being searched for.
		Module - The place in which to put the Premise Object interface to the module.

	Return - S_OK if the module was found, else an error.
*/
HRESULT CupbController::FindUpbModule(
	IN BYTE NetworkID,
	IN BYTE ModuleID,
	OUT IPremiseObject** Module
)
{
	//
	// Create a collection based on the ModuleID first, as it is likely to be smaller.
	//
	CComPtr<IPremiseObjectCollection> Modules;
	if (SUCCEEDED(m_spSite->GetObjectsByTypeAndPropertyValue(XML_UPB_SCHEMA_ROOT XML_SYS_PATH_SEPARATOR XML_UPB_MODULE, XML_UPB_MODULEID, CComVariant(ModuleID), collectionTypeNoRecurse, &Modules))) {
		long EnumDevice = 0;
		CComPtr<IPremiseObject> ThisModule;
		//
		// Then enumerate each item within the collection to find one that has the correct
		// NetworkID.
		//
		for (; SUCCEEDED(Modules->GetItemEx(EnumDevice, &ThisModule)); EnumDevice++, ThisModule = NULL) {
			CComVariant ThisNetworkID;
			if (SUCCEEDED(ThisModule->GetValue(XML_UPB_NETWORKID, &ThisNetworkID)) && (ThisNetworkID.bVal == NetworkID)) {
				return ThisModule->QueryInterface(Module);
			}
		}
	}
	return E_FAIL;
}

/*
	Find a module within the tree based on the unique identifier.

		UniqueID - The unique identifier of the module being searched for.
		Module - The place in which to put the Premise Object interface to the module.

	Return - S_OK if the module was found, else an error.
*/
HRESULT CupbController::FindUpbModule(
	IN USHORT UniqueID,
	OUT IPremiseObject** Module
)
{
	CComPtr<IPremiseObjectCollection> Modules;
	CComPtr<IPremiseObject> ThisModule;
	//
	// Do a check for INVALID_UNIQUE_ID, as this is used in cases where the identifier may not
	// be valid.
	//
	if ((UniqueID != INVALID_UNIQUE_ID) && SUCCEEDED(m_spSite->GetObjectsByPropertyValue(XML_UPB_UPBUNIQUEID, CComVariant(UniqueID), FALSE, &Modules)) && SUCCEEDED(Modules->GetItemEx(0, &ThisModule))) {
		return ThisModule->QueryInterface(Module);
	}
	return E_FAIL;
}

/*
	Parses decimal string bytes into a USHORT. This validates the string coming
	in, and produces an error on invalid data. The data must be at least 1 digit in length. Does
	not check for an overflow.

		Data - The string containing decimal number at least one digit in length.
		Number - The place in which to put the binary result of the parsed decimal data.

	Return - A pointer to the next place in the Data string after the number parsed, else NULL
		on an error.
*/
BSTR StringToShort(
	IN BSTR Data,
	OUT USHORT* Number
)
{
	int digits = 0;
	for (*Number = 0; isdigit(*Data); Data++) {
		digits++;
		*Number = *Number * 10 + (*Data - '0');
	}
	return digits ? Data : NULL;
}

/*
	Parses hex string bytes of the form "x{x}.yy" into a USHORT. This validates the string
	coming in, and produces an error on invalid data. The string may contain leading whitespace.

		Data - The string containing hex number.
		Number - The place in which to put the binary result of the parsed hex data.

	Return - A pointer to the next place in the Data string after the number parsed, else NULL
		on an error.
*/
BSTR HexXdotYToShort(
	IN BSTR Data,
	OUT USHORT& Number
)
{
	while (iswspace(*Data)) {
		Data++;
	}
	if (!isxdigit(*Data)) {
		return NULL;
	}
	Number = HexCharToByte(*Data);
	if (*(++Data) != L'.') {
		if (!isxdigit(*Data)) {
			return NULL;
		}
		Number = (Number << 4) + HexCharToByte(*Data);
		if (*Data != L'.') {
			return NULL;
		}
	}
	if (!isxdigit(*(++Data))) {
		return NULL;
	}
	Number = (Number << 4) + HexCharToByte(*Data);
	if (!isxdigit(*(++Data))) {
		return NULL;
	}
	Number = (Number << 4) + HexCharToByte(*Data);
	return Data + 1;
}

/*
	Find the product class within the given string of products based on the product identifier
	and optionally the firmware version. Returns the first match found, if any. This is used to
	search the upbProductIDs string of a manufacturer's UPB driver in order to locate a
	supporting class for a product found on the network.

		ProductIDs - The string containing the list of products to search.
		ProductID - The product identifier to search for in the string.
		FirmwareVersion - The firmware version to compare to if the firmware version is
			specified in the string.
		ProductClass - The place in which to return a buffer containing the product class name.

	Return - S_OK if the product class name was found, else an error.
*/
HRESULT FindProductClass(
	IN BSTR ProductIDs,
	IN USHORT ProductID,
	IN USHORT FirmwareVersion,
	OUT BSTR* ProductClass
)
{
	//
	// The list consists of a Product ID, optionally followed by supported Firmware Versions in
	// parenthesis, followed by the tree-relative name of the class that contains the code to
	// support this product.
	//
	// [ProductID] {([FirmwareVersion]{-[FirmwareVersion]}{,...})} [ProductClass]
	//
	while (*ProductIDs) {
		//
		// Find the ProductID.
		//
		while (iswspace(*ProductIDs)) {
			ProductIDs++;
		}
		USHORT ThisProductID;
		ProductIDs = StringToShort(ProductIDs, &ThisProductID);
		if (!ProductIDs || !iswspace(*ProductIDs)) {
			break;
		}
		while (iswspace(*ProductIDs)) {
			ProductIDs++;
		}
		//
		// Find the optional FirmwareVersion.
		//
		if (*ProductIDs == '(') {
			ProductIDs++;
			BOOL FoundVersion = FALSE;
			//
			// Loop until the end of the list of firmware versions is found.
			//
			while (*ProductIDs != ')') {
				if (!*ProductIDs) {
					return E_FAIL;
				}
				//
				// If the matching firmware version has not already been found, and this is a
				// matching product, then actually parse the version to compare with. Else just
				// skip to the end.
				//
				if (!FoundVersion && (ThisProductID == ProductID)) {
					USHORT StartRange;
					while (iswspace(*ProductIDs)) {
						ProductIDs++;
					}
					//
					// The start of range is optional and defaults to zero.
					//
					if (*ProductIDs == '-') {
						StartRange = 0;
					} else {
						ProductIDs = HexXdotYToShort(ProductIDs, StartRange);
						if (!ProductIDs) {
							return E_FAIL;
						}
					}
					USHORT EndRange;
					//
					// If there is no range, then default to the start of range.
					//
					if (*ProductIDs == '-') {
						ProductIDs++;
						while (iswspace(*ProductIDs)) {
							ProductIDs++;
						}
						//
						// The actual end of range is optional, and defaults to FFFF if not
						// explicitly stated.
						//
						if ((*ProductIDs == ',') || (*ProductIDs == ')')) {
							EndRange = 0xffff;
						} else {
							ProductIDs = HexXdotYToShort(ProductIDs, EndRange);
							if (!ProductIDs) {
								return E_FAIL;
							}
						}
					} else {
						EndRange = StartRange;
					}
					//
					// The range must not be negative.
					//
					if (EndRange > StartRange) {
						return E_FAIL;
					}
					//
					// Check for an actual match.
					//
					if ((FirmwareVersion >= StartRange) && (FirmwareVersion <= EndRange)) {
						FoundVersion = TRUE;
					}
					//
					// If this range was terminated by the start of the next range, then ensure
					// the string points to the beginning of the next range.
					//
					if (*ProductIDs == ',') {
						ProductIDs++;
					}
				} else {
					ProductIDs++;
				}
			}
			//
			// The firmware version did not match, so make sure the product identifier
			// comparison will fail.
			//
			if (!FoundVersion) {
				ThisProductID = !ProductID;
			}
			//
			// Skip the closing parenthesis, and any whitespace.
			//
			ProductIDs++;
			while (iswspace(*ProductIDs)) {
				ProductIDs++;
			}
		}
		// Find the ProductClass.
		//
		BSTR start = ProductIDs;
		while (*ProductIDs && !iswspace(*ProductIDs)) {
			ProductIDs++;
		}
		if (ThisProductID == ProductID) {
			CComBSTR _ProductClass(ProductIDs - start, start);
			*ProductClass = _ProductClass.Detach();
			return S_OK;
		}
	}
	return E_FAIL;
}

/*
	Locates an installed UPB manufacturer driver for a specified product, returning the schema
	path to the class that supports the hardware device.

		ManufacturerID - The manufacturer identifier of the hardware device whose supporting
			class is to be located. This is used in searching for matching upbManufacturerID
			properties on installed drivers.
		ProductID - The manufacturer-specific product identifier of the hardware device whose
			supporting class code is to be located.
		FirmwareVersion - The firmware version of the hardware device, which is optionally
			specified by the manufacturer's driver to separate classes for various versions.
		SchemaPath - The place in which to return a buffer containing the schema path to the
			class that supports the manufacturer's product.

	Return - S_OK if the supporting class was found, else an error.
*/
HRESULT CupbController::LocateUpbManufacturer(
	IN USHORT ManufacturerID,
	IN USHORT ProductID,
	IN USHORT FirmwareVersion,
	OUT BSTR* SchemaPath
)
{
	//
	// Enumerate the installed devices in order to find the set that is
	// labelled with the specified Manufacturer ID. Then enumerate that
	// set to locate one that supports the specified Product ID.
	//
	CComPtr<IPremiseObject> Devices;
	CComPtr<IPremiseObjectCollection> Manufacturers;
	if (SUCCEEDED(m_spSite->GetObject(XML_SYS_DEVICES_ROOT, &Devices)) && SUCCEEDED(Devices->GetObjectsByPropertyValue(XML_UPB_UPBMANUFACTURERID, CComVariant(ManufacturerID), FALSE, &Manufacturers))) {
		long EnumDevice = 0;
		CComPtr<IPremiseObject> Device;
		//
		// A manufacturer may have multiple drivers installed supporting their products, so
		// enumerate each one looking for the product identifier.
		//
		for (; SUCCEEDED(Manufacturers->GetItemEx(EnumDevice, &Device)); EnumDevice++, Device = NULL) {
			CComVariant ProductIDs;
			if (SUCCEEDED(Device->GetValue(XML_UPB_UPBPRODUCTIDS, &ProductIDs))) {
				CComBSTR ProductClass;
				if (SUCCEEDED(FindProductClass(ProductIDs.bstrVal, ProductID, FirmwareVersion, &ProductClass))) {
					CComPtr<IPremiseObject> DeviceClass;
					CComPtr<IPremiseObject> Library;
					CComBSTR LibraryPath;
					//
					// Construct the full schema path by prepending the path to the parent class
					// of the driver's class.
					//
					if (SUCCEEDED(Device->get_Class(&DeviceClass)) && SUCCEEDED(DeviceClass->get_Parent(&Library)) && SUCCEEDED(Library->get_Path(&LibraryPath)) && SUCCEEDED(LibraryPath.Append(XML_SYS_PATH_SEPARATOR)) && SUCCEEDED(LibraryPath.Append(ProductClass))) {
						*SchemaPath = LibraryPath.Detach();
						return S_OK;
					}
				}
			}
			ProductIDs = NULL;
		}
	}
	*SchemaPath = NULL;
	return E_FAIL;
}

/*
	Locates an installed UPB manufacturer driver for a specified manufacturer
	identifier, setting the name on the specified device.

		Module - The module who ManufacturerName is to be set.

		ManufacturerID - The manufacturer identifier to be located. This is used in
			searching for matching upbManufacturerID properties on installed drivers.

	Return - S_OK if the name was found and the property was set, else an error.
*/
HRESULT CupbController::LocateUpbManufacturerName(
	IN IPremiseObject* Module,
	IN USHORT ManufacturerID
)
{
	//
	// Enumerate the installed devices in order to find the set that is
	// labelled with the specified Manufacturer ID.
	//
	CComPtr<IPremiseObject> Devices;
	CComPtr<IPremiseObjectCollection> Manufacturers;
	CComPtr<IPremiseObject> Device;
	CComPtr<IPremiseObject> DeviceClass;
	CComPtr<IPremiseObject> LibraryClass;
	CComVariant LibraryDescription;
	//
	// Obtain the parent of the driver class (the manufacturer's root schema
	// object). This is used as the manufacturer name.
	//
	if (SUCCEEDED(m_spSite->GetObject(XML_SYS_DEVICES_ROOT, &Devices)) && SUCCEEDED(Devices->GetObjectsByPropertyValue(XML_UPB_UPBMANUFACTURERID, CComVariant(ManufacturerID), FALSE, &Manufacturers)) && SUCCEEDED(Manufacturers->GetItemEx(0, &Device)) && SUCCEEDED(Device->get_Class(&DeviceClass)) && SUCCEEDED(DeviceClass->get_Parent(&LibraryClass)) && SUCCEEDED(LibraryClass->GetValue(XML_SYS_DESCRIPTION, &LibraryDescription))) {
		return Module->SetValueEx(SVCC_DRIVER | SVCC_NOTIFY, XML_UPB_MANUFACTURERNAME, &LibraryDescription);
	}
	return E_FAIL;
}

HRESULT CupbController::TransmitNetworkPacket(
	IN BOOL LinkBit,
	IN BYTE RepeaterRequest,
	IN BYTE AckRequest,
	IN BYTE TransmitCount,
	IN BYTE TransmitSequence,
	IN BYTE NetworkID,
	IN BYTE DestinationID,
	IN BYTE MDID,
	IN LPCSTR Data OPTIONAL,
	IN UINT_PTR cmd
)
{
	m_IdleTickCount = GetTickCount();
	int DataLen = Data ? strlen(Data) / 2 : 0;
	char TransmitMessage[64];
	StringCchPrintfA(TransmitMessage, sizeof(TransmitMessage), "%c%02X%02X%02X%02X%02X%02X%s", UPB_NETWORK_TRANSMIT, (LinkBit << 7) | (RepeaterRequest << 5) | (sizeof(UPB_PACKETHEADER) + 1 + DataLen + 1), (AckRequest << 4) | (TransmitCount << 2) | TransmitSequence, NetworkID, DestinationID, DEFAULT_DEVICEID, MDID, Data ? Data : "");
	BYTE Checksum = CalculateChecksum(&TransmitMessage[1]);
	StringCchPrintfA(&TransmitMessage[strlen(TransmitMessage)], sizeof(TransmitMessage) - strlen(TransmitMessage), "%02X%c", Checksum, UPB_MESSAGE_TERMINATOR);
	return SendBufferedCommandWithTimeout(TransmitMessage, (RESPONSETYPE_FROM_CMD(cmd) == RESPONSE_TYPE_DATAACK) ? UPB_DEVICE_ACK_TIMEOUT : UPB_PIM_ACK_TIMEOUT, cmd);
}

HRESULT CupbController::ReadModuleRegisters(
	IN BYTE NetworkID,
	IN BYTE ModuleID,
	IN BYTE RegisterStart,
	IN BYTE Registers,
	IN USHORT UniqueID,
	IN BYTE MessageType
)
{
	char r[16];
	StringCchPrintfA(r, SIZEOF_ARRAY(r), "%02X%02X", RegisterStart, Registers);
	return TransmitNetworkPacket(FALSE, PACKETHEADER_REP_NONREPEATER, PACKETHEADER_REQ_ACKPULSE, PACKETHEADER_TRANSMITCOUNT_1, PACKETHEADER_TRANSMITSEQUENCE_1, NetworkID, ModuleID, MAKE_MDID(MDID_CORE_COMMANDS, MDID_CORE_COMMAND_GETREGISTERVALUES), r, CREATE_CMD(UniqueID, MessageType, RESPONSE_TYPE_DATAACK, MAKE_MDID(MDID_CORE_REPORTS, MDID_DEVICE_CORE_REPORT_REGISTERVALUES)));
}

HRESULT CupbController::ReadPIMRegisters(
	IN BYTE RegisterStart,
	IN BYTE Registers,
	IN USHORT UniqueID OPTIONAL
)
{
	m_IdleTickCount = GetTickCount();
	BYTE BinaryMessage[] = {RegisterStart, Registers};
	BYTE Checksum = CalculateChecksum(BinaryMessage, sizeof(BinaryMessage));
	char ReadMessage[PIM_REGISTER_READ_BYTES+1];
	StringCchPrintfA(ReadMessage, SIZEOF_ARRAY(ReadMessage), "%c%02X%02X%02X%c", UPB_PIM_READ, RegisterStart, Registers, Checksum, UPB_MESSAGE_TERMINATOR);
	return SendBufferedCommandWithTimeout(ReadMessage, UPB_PIM_ACK_TIMEOUT, CREATE_CMD(UniqueID, MESSAGE_TYPE_PIMREAD, RESPONSE_TYPE_PIMDATAACK, 0));
}

HRESULT CupbController::WritePIMRegisters(
	IN LPCSTR Registers,
	IN USHORT UniqueID
)
{
	m_IdleTickCount = GetTickCount();
	BYTE Checksum = CalculateChecksum(Registers);
	int WriteMessageLen = 1 + strlen(Registers) + 2 + 1 + 1;
	char* WriteMessage = new char[WriteMessageLen];
	StringCchPrintfA(WriteMessage, WriteMessageLen, "%c%s%02X%c", UPB_PIM_WRITE, Registers, Checksum, UPB_MESSAGE_TERMINATOR);
	HRESULT hr = SendBufferedCommandWithTimeout(WriteMessage, UPB_PIM_ACK_TIMEOUT, CREATE_CMD(UniqueID, MESSAGE_TYPE_PIMWRITE, RESPONSE_TYPE_PIMACCEPT, 0));
	delete [] WriteMessage;
	return hr;
}

void CupbController::PulseDataReset(
)
{
	m_PulseDataPos = 0;
	*m_PulseData = 0;
	m_PulseDataBitPos = 8;
	m_PulseDataSequence = 0;
}

void CupbController::PulseDataAdd(
	IN LPCSTR DataLine
)
{
	if (strlen(DataLine) == PULSE_REPORT_BYTES) {
		if (HexCharToByte(DataLine[PULSE_MODE_SEQUENCE]) == m_PulseDataSequence) {
			m_PulseDataBitPos -= 2;
			m_PulseData[m_PulseDataPos] |= ((DataLine[UPB_MESSAGE_TYPE] - '0') << m_PulseDataBitPos);
			m_PulseDataSequence++;
			if (m_PulseDataSequence > 0xf) {
				m_PulseDataSequence = 0;
			}
			if (!m_PulseDataBitPos) {
				m_PulseDataPos++;
				if (m_PulseDataPos == sizeof(m_PulseData)) {
					PulseDataReset();
				} else {
					m_PulseData[m_PulseDataPos] = 0;
					m_PulseDataBitPos = 8;
				}
			}
		} else {
			PulseDataReset();
		}
	} else {
		PulseDataReset();
	}
}

inline const BYTE* CupbController::PulseDataGet(
)
{
	return m_PulseData;
}

HRESULT CupbController::TryUpbPassword(
	IN IPremiseObject* Module,
	IN ULONG FirstTryPassword OPTIONAL
)
{
	//
	// If there is a duplicate address to deal with, stop the probing by returning
	// success.
	//
	if (m_dam != DuplicateAddressMode_None) {
		return S_OK;
	}
	//
	// If a password was set while probing was occuring, then just exit with success.
	//
	USHORT CurrentPassword;
	if (GetUpbPassword(Module, &CurrentPassword)) {
		Module->SetValueEx(SVCC_NOTIFY, XML_UPB_UPBDEVICEINITIALIZED, &CComVariant(TRUE));
		return S_OK;
	}
	CComVariant Password;
	if (SUCCEEDED(Module->GetValue(XML_UPB_PASSWORDPROBE, &Password))) {
		//
		// If a password to try has been passed in, use that.
		//
		if (FirstTryPassword != DEFAULT_PASSWORD_TRY) {
			Password = static_cast<USHORT>(FirstTryPassword);
		//
		// At the start of a search this is set to zero.
		//
		} else if (!Password.uiVal) {
			CComVariant upbChecksumDifference;
			//
			// Obtain the bounds of the search, which is the difference
			// between the checksum of the setup registers, and the
			// checksum that could be constructed without the 2 bytes
			// for the password.
			//
			if (SUCCEEDED(Module->GetValue(XML_UPB_UPBCHECKSUMDIFFERENCE, &upbChecksumDifference))) {
				//
				// Split the value into two bytes, and move a bit
				// from the lower to the upper byte with each iteration
				// of the search. Start with the lower byte filled as
				// much as possible, since number chosen are more
				// likely to be purely decimal digits, rather than fully
				// exploit hexidecimal digits.
				//
				if (upbChecksumDifference.uiVal > 0xff) {
					Password.uiVal = ((upbChecksumDifference.uiVal - 0xff) << 8) | 0xff;
				} else {
					Password.uiVal = upbChecksumDifference.uiVal;
				}
			} else {
				return E_FAIL;
			}
		} else {
			//
			// Shift the digit from the lower to the upper byte.
			//
			Password.uiVal = (Password.uiVal + 0x0100) - 1;
		}
		//
		// Save one pass by checking for the end condition and assuming
		// that it is correct, since there are no other possibilities.
		//
		if ((FirstTryPassword == DEFAULT_PASSWORD_TRY) && (!(Password.uiVal & 0xff) || ((Password.uiVal & 0xff00) == 0xff00))) {
			SetUpbPassword(Module, Password.uiVal);
			Module->SetValueEx(SVCC_NOTIFY, XML_UPB_UPBDEVICEINITIALIZED, &CComVariant(TRUE));
			return S_OK;
		} else {
			//
			// Store the latest attempt, and try to enter Setup Mode on the
			// device using that password. The ACK will generate a query
			// for the time left in Setup Mode, which will be non-zero if
			// this worked.
			//
			Module->SetValueEx(SVCC_DRIVER, XML_UPB_PASSWORDPROBE, &Password);
			BYTE NetworkID;
			BYTE ModuleID;
			USHORT UniqueID;
			if (SUCCEEDED(GetModuleIdentifiers(Module, NetworkID, ModuleID, UniqueID))) {
				char r[16];
				StringCchPrintfA(r, SIZEOF_ARRAY(r), "%04X", Password.uiVal);
				TransmitNetworkPacket(FALSE, PACKETHEADER_REP_NONREPEATER, PACKETHEADER_REQ_ACKPULSE, PACKETHEADER_TRANSMITCOUNT_1, PACKETHEADER_TRANSMITSEQUENCE_1, NetworkID, ModuleID, MAKE_MDID(MDID_CORE_COMMANDS, MDID_CORE_COMMAND_STARTSETUP), r, CREATE_CMD(UniqueID, (FirstTryPassword == DEFAULT_PASSWORD_TRY) ? MESSAGE_TYPE_TRYPASSWORD : MESSAGE_TYPE_GUESSPASSWORD, RESPONSE_TYPE_TRANSMITACK, MAKE_MDID(MDID_CORE_COMMANDS, MDID_CORE_COMMAND_STARTSETUP)));
			}
		}
	}
	return E_FAIL;
}

HRESULT UpToDateChecksum(
	IN IPremiseObject* Module
)
{
	CComVariant SetupChecksum;
	USHORT NetworkPassword;
	CComVariant SetupRegisterCache;
	CComVariant SetupRegisterCount;
	//
	// Can't succeed if password has not been set up.
	//
	if (GetUpbPassword(Module, &NetworkPassword) && SUCCEEDED(Module->GetValue(XML_UPB_SETUPCHECKSUM, &SetupChecksum)) && SUCCEEDED(ReadSetupRegisterCache(Module, &SetupRegisterCache)) && SUCCEEDED(Module->GetValue(XML_UPB_SETUPREGISTERCOUNT, &SetupRegisterCount))) {
		//
		// Sum the registers for this module, and compare with the checksum. Equality is
		// assumed to imply currency, though this could possibly not be true.
		//
		USHORT Checksum = 0;
		for (int i = 0; i < SetupRegisterCount.bVal; i++) {
			Checksum += (HexCharToByte(SetupRegisterCache.bstrVal[2 * i]) << 4) + HexCharToByte(SetupRegisterCache.bstrVal[2 * i + 1]);
		}
		if (Checksum == SetupChecksum.uiVal) {
			return S_OK;
		}
	}
	return E_FAIL;
}

USHORT CalcChecksumDifference(
	IN IPremiseObject* Module
)
{
	CComVariant upbChecksum;
	USHORT upbChecksumDifference;
	if (SUCCEEDED(Module->GetValue(XML_UPB_UPBCHECKSUM, &upbChecksum))) {
		CComVariant SetupRegisterCache;
		USHORT Checksum = 0;
		if (SUCCEEDED(ReadSetupRegisterCache(Module, &SetupRegisterCache))) {
			//
			// Obtain the sum of the lower common registers.
			//
			for (int i = 0; i < UPB_REG_RESERVED1; i++) {
				//
				// Always skip the password registers in case the device was in Setup Mode or
				// Write Enabled when the registers were read.
				//
				if ((i >= UPB_REG_PASSWORD + UPB_REG_PASSWORD_BYTES) || (i < UPB_REG_PASSWORD)) {
					Checksum += (HexCharToByte(SetupRegisterCache.bstrVal[2 * i]) << 4) + HexCharToByte(SetupRegisterCache.bstrVal[2 * i + 1]);
				}
			}
		}
		//
		// Create the difference between the lower common register sum, and the reported
		// checksum for those registers. The difference is the sum of the two bytes for
		// the password.
		//
		upbChecksumDifference = upbChecksum.uiVal - Checksum;
		Module->SetValueEx(SVCC_DRIVER, XML_UPB_UPBCHECKSUMDIFFERENCE, &CComVariant(upbChecksumDifference));
	} else {
		upbChecksumDifference = 0;
	}
	return upbChecksumDifference;
}

void CupbController::QueueNextPasswordTry(
	IN ULONG FirstTryPassword OPTIONAL
)
{
	//
	// Stop all password probing if there is a duplicate device address to deal with.
	//
	if (m_dam == DuplicateAddressMode_None) {
		//
		// Trying passwords is serialized with respect to normal property changes for the sake
		// of efficiency when trying duplicate passwords out on other modules. An ACK timeout
		// however could restart another password loop since it is on a separate thread.
		// However, normal data processing should not be occurring if an ACK timeout was
		// triggered. So no specific locking mechanism is used here.
		//
		CComPtr<IPremiseObjectCollection> Modules;
		if ((m_FirstPasswordUniqueID == INVALID_UNIQUE_ID) && SUCCEEDED(m_spSite->GetObjectsByTypeAndPropertyValue(XML_UPB_SCHEMA_ROOT XML_SYS_PATH_SEPARATOR XML_UPB_MODULE, XML_UPB_NETWORKPASSWORD, CComVariant(""), collectionTypeNoRecurse, &Modules))) {
			long EnumDevice = 0;
			CComPtr<IPremiseObject> Module;
			for (; SUCCEEDED(Modules->GetItemEx(EnumDevice, &Module)); EnumDevice++, Module = NULL) {
				CComVariant upbChecksumDifference;
				BYTE NetworkID;
				BYTE ModuleID;
				USHORT UniqueID;
				if (SUCCEEDED(Module->GetValue(XML_UPB_UPBCHECKSUMDIFFERENCE, &upbChecksumDifference)) && (upbChecksumDifference.lVal != UNINITIALIZED_CHECKSUM_DIFFERENCE) && SUCCEEDED(GetModuleIdentifiers(Module, NetworkID, ModuleID, UniqueID))) {
					Module->SetValueEx(SVCC_DRIVER, XML_UPB_PASSWORDPROBE, &CComVariant(0L));
					//
					// No need to stop Setup Mode here first, because if setup mode was
					// currently running, then the password would be known for the device
					// already.
					//
					m_FirstPasswordUniqueID = UniqueID;
					if (SUCCEEDED(TryUpbPassword(Module, FirstTryPassword))) {
						m_FirstPasswordUniqueID = INVALID_UNIQUE_ID;
					} else {
						break;
					}
				}
			}
		}
	}
}

void CupbController::InitPhaseQueryModuleSignature(
	const UPB_PACKETHEADER* PacketHeader,
	IN const BYTE* RegisterData,
	IN USHORT UniqueID,
	IN BYTE MessageType
)
{
	CComPtr<IPremiseObject> Module;
	if (SUCCEEDED(FindUpbModule(UniqueID, &Module))) {
		//
		// Update the checksum and such in order to use in checking for an up to date
		// cache.
		//
		DEVICE_SIGNATURE_REPORT Report = *reinterpret_cast<const DEVICE_SIGNATURE_REPORT*>(RegisterData);
		Report.upbChecksum = ntohs(Report.upbChecksum);
		Module->SetValueEx(SVCC_DRIVER, XML_UPB_UPBCHECKSUM, &CComVariant(Report.upbChecksum));
		Report.SetupChecksum = ntohs(Report.SetupChecksum);
		Module->SetValueEx(SVCC_DRIVER, XML_UPB_SETUPCHECKSUM, &CComVariant(Report.SetupChecksum));
		Module->SetValueEx(SVCC_DRIVER, XML_UPB_SETUPREGISTERCOUNT, &CComVariant(Report.SetupRegisterCount));
		switch (MessageType) {
		case MESSAGE_TYPE_QUERYNEWMODULESIGNATURE:
			//
			// This is a new module, so the base registers have already been read. Continue with
			// the rest.
			//
			for (int i = UPB_REG_NETWORKNAME; i < Report.SetupRegisterCount; i += UPB_MODULE_MAXREGISTERIO) {
				ReadModuleRegisters(PacketHeader->NetworkID, PacketHeader->SourceID, static_cast<BYTE>(i), static_cast<BYTE>(min(UPB_MODULE_MAXREGISTERIO, Report.SetupRegisterCount - i)), UniqueID, MESSAGE_TYPE_NORMAL);
			}
			break;
		case MESSAGE_TYPE_QUERYMODULESIGNATURE:
			//
			// If the register cache is up to date, then initialization is done. Else the
			// complete set of registers needs to be read.
			//
			if (SUCCEEDED(UpToDateChecksum(Module))) {
				Module->SetValueEx(SVCC_NOTIFY, XML_UPB_UPBDEVICEINITIALIZED, &CComVariant(TRUE));
			} else {
				Module->SetValueEx(SVCC_NOTIFY, XML_UPB_UPBDEVICEINITIALIZED, &CComVariant(FALSE));
				Module->SetValueEx(SVCC_DRIVER, XML_UPB_UPBCHECKSUMDIFFERENCE, &CComVariant(UNINITIALIZED_CHECKSUM_DIFFERENCE));
				//
				// Only queue up a single read to the device, which will then determine if
				// this is indeed the same device, or some other device using the same
				// address.
				//
				ReadModuleRegisters(PacketHeader->NetworkID, PacketHeader->SourceID, INITIAL_MODULE_REG_QUERY_BASE, INITIAL_MODULE_REG_QUERY_BYTES, UniqueID, MESSAGE_TYPE_NORMAL);
			}
			break;
		}
	}
}

void CupbController::InitPhaseQueryModuleBaseRegisters(
	IN const BYTE* RegisterData,
	IN USHORT UniqueID
)
{
	//
	// Skip the register start byte of the message, and just access the data.
	//
	INITIAL_MODULE_REG_REPORT Report = *reinterpret_cast<const INITIAL_MODULE_REG_REPORT*>(RegisterData + 1);
	Report.Password = ntohs(Report.Password);
	Report.ManufacturerID = ntohs(Report.ManufacturerID);
	Report.ProductID = ntohs(Report.ProductID);
	Report.FirmwareVersion = ntohs(Report.FirmwareVersion);
	CComPtr<IPremiseObject> CurrentModule;
	//
	// If the data was queried using a unique identifier, locate it by that tag, else do
	// a search based on the contents of the report.
	//
	if (FAILED(FindUpbModule(UniqueID, &CurrentModule))) {
		FindUpbModule(&Report, &CurrentModule);
	}
	//
	// Create a new module if one does not exist, or if the current module is based on
	// the default class, and a manufacturer-specific class exists.
	//
	CComPtr<IPremiseObject> NewModule;
	if (SUCCEEDED(CreateOrReplaceUpbObject(CurrentModule, Report.ManufacturerID, Report.ProductID, Report.FirmwareVersion, XML_UPB_SCHEMA_ROOT XML_SYS_PATH_SEPARATOR XML_UPB_UNKNOWNMODULE, &NewModule))) {
		//
		// Set all the basic values in case this is a new module, or a replacement for a
		// default class. Also ensure the options and version stamp are up to date. Note
		// that if the firmware of a hardware device is updated and needs to be supported
		// by a different module class, the current module must be manually deleted.
		//
		NewModule->SetValueEx(SVCC_DRIVER | SVCC_NOTIFY, XML_UPB_NETWORKID, &CComVariant(Report.NetworkID));
		NewModule->SetValueEx(SVCC_DRIVER | SVCC_NOTIFY, XML_UPB_MODULEID, &CComVariant(Report.ModuleID));
		NewModule->SetValueEx(SVCC_DRIVER | SVCC_NOTIFY, XML_UPB_UPBOPTIONS, &CComVariant(Report.UpbOptions));
		NewModule->SetValueEx(SVCC_DRIVER | SVCC_NOTIFY, XML_UPB_UPBVERSION, &CComVariant(Report.UpbVersion));
		CComBSTR FirmwareVersion;
		if (SUCCEEDED(MakeFirmwareVersionString(Report.FirmwareVersion, &FirmwareVersion))) {
			NewModule->SetValueEx(SVCC_DRIVER | SVCC_NOTIFY, XML_UPB_FIRMWAREVERSION, &CComVariant(FirmwareVersion));
			NewModule->SetValueEx(SVCC_DRIVER, XML_UPB_UPBFIRMWAREVERSION, &CComVariant(Report.FirmwareVersion));
		}
		CComVariant NewUniqueID;
		if (SUCCEEDED(NewModule->GetValue(XML_UPB_UPBUNIQUEID, &NewUniqueID))) {
			//
			// If the password is not 0000, then it was actually read, as the device was in
			// Setup Mode. If not, then either the password really is 0000, or it could not be
			// read. The password will be extracted in that case. If the module already
			// existed, then the password likely has been found.
			//
			USHORT NetworkPassword;
			if (!Report.Password && GetUpbPassword(NewModule, &NetworkPassword)) {
				Report.Password = NetworkPassword;
			}
			CComVariant SetupRegisterCache;
			if (SUCCEEDED(ReadSetupRegisterCache(NewModule, &SetupRegisterCache))) {
				BytesToHex(SetupRegisterCache.bstrVal, reinterpret_cast<BYTE*>(&Report), sizeof(Report));
				NewModule->SetValueEx(SVCC_DRIVER, XML_UPB_SETUPREGISTERCACHE, &SetupRegisterCache);
			}
			//
			// Make sure the Password property is set correctly. This will later allow
			// initialization to skip password discovery.
			//
			if (Report.Password) {
				SetUpbPassword(NewModule, Report.Password);
			}
			//
			// Request the device signature to check for the register checksum. Since this
			// function handles the response for the base registers, use the NewModuleSignature
			// message type so that the base registers are not queried a second time.
			//
			TransmitNetworkPacket(FALSE, PACKETHEADER_REP_NONREPEATER, PACKETHEADER_REQ_ACKPULSE, PACKETHEADER_TRANSMITCOUNT_1, PACKETHEADER_TRANSMITSEQUENCE_1, Report.NetworkID, Report.ModuleID, MAKE_MDID(MDID_CORE_COMMANDS, MDID_CORE_COMMAND_GETDEVICESIGNATURE), NULL, CREATE_CMD(NewUniqueID.uiVal, MESSAGE_TYPE_QUERYNEWMODULESIGNATURE, RESPONSE_TYPE_DATAACK, MAKE_MDID(MDID_CORE_REPORTS, MDID_DEVICE_CORE_REPORT_DEVICESIGNATURE)));
		}
	}
}

void CupbController::HandleTryPassword(
	IN const BYTE* MessageData,
	IN USHORT UniqueID,
	IN BYTE TryType
)
{
	CComPtr<IPremiseObject> Module;
	CComVariant NetworkID;
	CComVariant ModuleID;
	if (SUCCEEDED(FindUpbModule(UniqueID, &Module)) && SUCCEEDED(Module->GetValue(XML_UPB_NETWORKID, &NetworkID)) && SUCCEEDED(Module->GetValue(XML_UPB_MODULEID, &ModuleID))) {
		CComVariant Password;
		const DEVICE_SETUPTIME_REPORT* SetupTime = reinterpret_cast<const DEVICE_SETUPTIME_REPORT*>(MessageData);
		if (SetupTime->TimeoutSeconds) {
			//
			// The password worked, because the module is now in Setup Mode. Set the password
			// on the module and set it to the initialized state. The next module can now try
			// a password. The first one tried will be a guess based on the password just
			// found.
			//
			if (SUCCEEDED(Module->GetValue(XML_UPB_PASSWORDPROBE, &Password))) {
				SetUpbPassword(Module, Password.uiVal);
			} else {
				Password = DEFAULT_PASSWORD_TRY;
			}
			TransmitNetworkPacket(FALSE, PACKETHEADER_REP_NONREPEATER, PACKETHEADER_REQ_ACKPULSE, PACKETHEADER_TRANSMITCOUNT_1, PACKETHEADER_TRANSMITSEQUENCE_1, NetworkID.bVal, ModuleID.bVal, MAKE_MDID(MDID_CORE_COMMANDS, MDID_CORE_COMMAND_STOPSETUP), NULL, CREATE_CMD(UniqueID, MESSAGE_TYPE_NORMAL, RESPONSE_TYPE_TRANSMITACK, MAKE_MDID(MDID_CORE_COMMANDS, MDID_CORE_COMMAND_STOPSETUP)));
			Module->SetValueEx(SVCC_NOTIFY, XML_UPB_UPBDEVICEINITIALIZED, &CComVariant(TRUE));
			m_FirstPasswordUniqueID = INVALID_UNIQUE_ID;
		} else {
			//
			// Try the next password. If the current try was a first time guess, then
			// start the probing from the beginning. Else continue continue probing.
			//
			if (TryType == MESSAGE_TYPE_GUESSPASSWORD) {
				Module->SetValueEx(SVCC_DRIVER, XML_UPB_PASSWORDPROBE, &CComVariant(0L));
				Password = DEFAULT_PASSWORD_TRY;
			}
			//
			// The next try might succeed immediately because there are no other possible
			// choices. In that case, queue up the next module, if any, with the password
			// that just succeeded as the first guess.
			//
			if (SUCCEEDED(TryUpbPassword(Module))) {
				m_FirstPasswordUniqueID = INVALID_UNIQUE_ID;
				Password = 0L;
				if (!GetUpbPassword(Module, &Password.uiVal)) {
					Password = DEFAULT_PASSWORD_TRY;
				}
			} else {
				Password = DEFAULT_PASSWORD_TRY;
			}
		}
		QueueNextPasswordTry(Password.ulVal);
	}
}

/*
	This structure defines the messages that should receive a response other than just an
	Ack on transmission to the PIM, and specifically what the response packet is. If
	there is some custom message or extended message set that also expects some response
	packet, there is no way to tell, since the message sets are not defined in an
	extensible manner. Such messages must use the UPB_MESSAGE_CUSTOM_RESPONSE token to indicate
	a custom response.
*/
typedef struct {
	BYTE mdidCommand;
	BYTE mdidResponse;
} MESSAGE_MAP;

static MESSAGE_MAP MessageMap[] = {
	{MAKE_MDID(MDID_CORE_COMMANDS, MDID_CORE_COMMAND_GETSETUPTIME), MAKE_MDID(MDID_CORE_REPORTS, MDID_DEVICE_CORE_REPORT_SETUPTIME)},
	{MAKE_MDID(MDID_DEVICE_CONTROL_COMMANDS, MDID_DEVICE_CONTROL_COMMAND_REPORTSTATE), MAKE_MDID(MDID_CORE_REPORTS, MDID_DEVICE_CORE_REPORT_DEVICESTATE)},
	{MAKE_MDID(MDID_CORE_COMMANDS, MDID_CORE_COMMAND_GETDEVICESTATUS), MAKE_MDID(MDID_CORE_REPORTS, MDID_DEVICE_CORE_REPORT_DEVICESTATUS)},
	{MAKE_MDID(MDID_CORE_COMMANDS, MDID_CORE_COMMAND_GETSIGNALSTRENGTH), MAKE_MDID(MDID_CORE_REPORTS, MDID_DEVICE_CORE_REPORT_SIGNALSTRENGTH)},
	{MAKE_MDID(MDID_CORE_COMMANDS, MDID_CORE_COMMAND_GETNOISELEVEL), MAKE_MDID(MDID_CORE_REPORTS, MDID_DEVICE_CORE_REPORT_NOISELEVEL)},
	{MAKE_MDID(MDID_CORE_COMMANDS, MDID_CORE_COMMAND_GETDEVICESIGNATURE), MAKE_MDID(MDID_CORE_REPORTS, MDID_DEVICE_CORE_REPORT_DEVICESIGNATURE)},
	{MAKE_MDID(MDID_CORE_COMMANDS, MDID_CORE_COMMAND_GETREGISTERVALUES), MAKE_MDID(MDID_CORE_REPORTS, MDID_DEVICE_CORE_REPORT_REGISTERVALUES)}
};

HRESULT CupbController::ValidateUpbDataMessage(
	const UPB_PACKETHEADER* PacketHeader
)
{
	//
	// The message must:
	// 1. have stopped on an even byte;
	// 2. be at least 2 bytes longer than the size of the PacketHeader
	//    to include at least a byte for the message id and checksum
	//    (the driver does not understand packets with no message id);
	// 3. not have the reserved bit set in the PacketHeader;
	// 4. be the same length as what is stated in the PacketHeader;
	// 5. contain a checksum in the last byte for the rest of the
	//    message data.
	//
	if ((m_PulseDataBitPos < 8) || (m_PulseDataPos <= sizeof(UPB_PACKETHEADER) + 1) || (PacketHeader->ControlWord[1] & 0x80) || (m_PulseDataPos != (PacketHeader->ControlWord[0] & 0x1f)) || (CalculateChecksum(reinterpret_cast<const BYTE*>(PacketHeader), m_PulseDataPos - 1) != reinterpret_cast<const BYTE*>(PacketHeader)[m_PulseDataPos - 1])) {
		return E_FAIL;
	}
	//
	// If this packet has a non-zero Transmit Count, it needs to be
	// checked as a duplicate.
	//
	if (PacketHeader->ControlWord[1] & 0x0c) {
		for (int i = 0; i < UPB_HISTORYQUEUEITEMS; i++ ) {
			if ((m_HistoryQueue[i].ControlWord[0] == PacketHeader->ControlWord[0]) && ((m_HistoryQueue[i].ControlWord[1] & 0xfc) == (PacketHeader->ControlWord[1] & 0xfc)) && !memcmp(&m_HistoryQueue[i].NetworkID, &PacketHeader->NetworkID, 3 * sizeof(BYTE))) {
				//
				// If the Transmit Sequence is <= to the one found,
				// then replace it and let the packet through, as
				// it is unrelated.
				//
				if (PacketHeader->ControlWord[1] <= m_HistoryQueue[i].ControlWord[1]) {
					m_HistoryQueue[i].ControlWord[1] = PacketHeader->ControlWord[1];
					return S_OK;
				}
				//
				// If this is the last resend of the same packet,
				// then remove the previous one from the history
				// queue so any new ones won't be confused with
				// this old one.
				//
				if ((PacketHeader->ControlWord[1] & 0x03) == (PacketHeader->ControlWord[1] & 0x0c)) {
					ZeroMemory(&m_HistoryQueue[i], sizeof(m_HistoryQueue[0]));
				} else if ((m_HistoryQueue[i].ControlWord[1] & 0x03) < (PacketHeader->ControlWord[1] & 0x03)) {
					//
					// Replace an older item with the newer one.
					//
					//
					m_HistoryQueue[i].ControlWord[1] = PacketHeader->ControlWord[1];
				}
				return E_FAIL;
			}
		}
		//
		// Compare the Transmit Sequence to the Transmit Count.
		//
		if ((PacketHeader->ControlWord[1] & 0x03) < (PacketHeader->ControlWord[1] & 0x0c)) {
			//
			// The item was not on the queue, so add it in case a duplicate
			// is received later.
			//
			m_HistoryQueue[m_HistoryQueueOldest] = *PacketHeader;
			m_HistoryQueueOldest++;
			if (m_HistoryQueueOldest == UPB_HISTORYQUEUEITEMS) {
				m_HistoryQueueOldest = 0;
			}
		}
	}
	return S_OK;
}

void CupbController::SendDataToClient(
	IN IPremiseObject* Module,
	IN BYTE DataType,
	IN const BYTE* MessageData OPTIONAL,
	IN int MessageLength OPTIONAL
)
{
	char HexData[sizeof(m_PulseData) * 2 + 2];
	HexData[0] = DataType;
	//
	// Register data from the PIM is already in a hex format.
	//
	if (DataType == UPB_NETWORKDATA_PIMREGISTERS) {
		if (MessageData) {
			StringCchCopyNA(&HexData[1], sizeof(HexData) - 1, reinterpret_cast<const char*>(MessageData), MessageLength);
		}
		HexData[1 + MessageLength] = 0;
	} else if (MessageLength) {
		//
		// Add the type of message, Direct, or Link, then skip the
		// header and remove the checksum. The message will then only
		// contain the nid did sid {mdid} {data}
		//
		HexData[1] = (reinterpret_cast<const UPB_PACKETHEADER*>(MessageData)->ControlWord[0] & PACKETHEADER_LINKBIT) ? UPB_MESSAGE_TYPE_LINK : UPB_MESSAGE_TYPE_DIRECT;
		MessageLength--;
		for (int i = 2; i < MessageLength; i++) {
			HexData[2 * (i - 1)] = static_cast<char>(NibToHex(MessageData[i] >> 4));
			HexData[2 * (i - 1) + 1] = static_cast<char>(NibToHex(MessageData[i] & 0x0f));
		}
		HexData[2 * (MessageLength - 1)] = 0;
	} else {
		HexData[1] = 0;
	}
	Module->SetValueEx(SVCC_NOTIFY | SVCC_FORCE, XML_UPB_UPBNETWORKDATA, &CComVariant(HexData));
}

void PopulateRegisterNameValue(
	IN IPremiseObject* Module,
	IN WCHAR* ValueName,
	IN const BYTE* MessageData
)
{
	//
	// The devices tend to have random data in the registers, so truncate at the first
	// non-printable character.
	//
	char Name[UPB_MODULE_MAXREGISTERIO + 1];
	memcpy(Name, MessageData, sizeof(Name) - 1);
	Name[sizeof(Name) - 1] = 0;
	int i;
	for (i = 0; isgraph(Name[i]); i++) {
	}
	Name[i] = 0;
	Module->SetValueEx(SVCC_DRIVER | SVCC_NOTIFY, ValueName, &CComVariant(Name));
}

void CupbController::BroadcastLinkPacket(
	IN BYTE nid,
	IN BYTE sid,
	IN BYTE did,
	IN BYTE mdid,
	IN char* LinkPacket,
	IN BOOL SendToPIM
)
{
	char ClientMessage[2 + 2 + 2 * (1 + 1 + 1 + UPB_MODULE_MAXREGISTERIO) + 1];
	StringCchPrintfA(ClientMessage, sizeof(ClientMessage), "%c%c%02X%02X%02X%02X%s", UPB_NETWORKDATA_BROADCAST, UPB_MESSAGE_TYPE_LINK, nid, did, sid, mdid, LinkPacket);
	//
	// Notify all modules with corresponding LinkID entries in their
	// Receive Components of this network message. If a module has for
	// some reason multiple Receive Components with the same LinkID,
	// then it will indeed receive multiple notifications.
	//
	CComPtr<IPremiseObjectCollection> Components;
	if (SUCCEEDED(m_spSite->GetObjectsByTypeAndPropertyValue(XML_UPB_SCHEMA_ROOT XML_SYS_PATH_SEPARATOR XML_UPB_RECEIVECOMPONENT, XML_UPB_LINKID, CComVariant(did), collectionTypeRecurse, &Components))) {
		long EnumComponent = 0;
		CComPtr<IPremiseObject> Component;
		for (; SUCCEEDED(Components->GetItemEx(EnumComponent, &Component)); EnumComponent++, Component = NULL) {
			CComPtr<IPremiseObject> Module;
			if (SUCCEEDED(Component->GetAncestorByType(XML_UPB_SCHEMA_ROOT XML_SYS_PATH_SEPARATOR XML_UPB_MODULE, &Module)) && IsDeviceInitialized(Module)) {
				//
				// The global network ID will match all modules. Note
				// that the code does not handle the Write Enabled or
				// Setup Mode network identifiers.
				//
				CComVariant NetworkID;
				//
				// Exclude the source of the message in case it has the
				// same LinkID.
				//
				CComVariant ModuleID;
				if ((nid == GLOBAL_NETWORKID) || (SUCCEEDED(Module->GetValue(XML_UPB_NETWORKID, &NetworkID)) && (NetworkID.bVal == nid) && ((sid == BROADCAST_DEVICEID) || (SUCCEEDED(Module->GetValue(XML_UPB_MODULEID, &ModuleID)) && (sid != ModuleID.bVal))))) {
					//
					// If the link command requires a password, do not
					// send it on if that password is missing or
					// incorrect.
					//
					if (mdid == MAKE_MDID(MDID_DEVICE_CONTROL_COMMANDS, MDID_DEVICE_CONTROL_COMMAND_STORESTATE)) {
						CComVariant NetworkPassword;
						char Password[32];
						if (FAILED(Module->GetValue(XML_UPB_NETWORKPASSWORD, &NetworkPassword)) || !WideCharToMultiByte(CP_ACP, 0, NetworkPassword.bstrVal, lstrlenW(NetworkPassword.bstrVal) + 1, Password, sizeof(Password), NULL, NULL) || lstrcmp(Password, LinkPacket)) {
							continue;
						}
					}
					Module->SetValueEx(SVCC_NOTIFY | SVCC_FORCE, XML_UPB_UPBNETWORKDATA, &CComVariant(ClientMessage));
				}
			}
		}
	}
	//
	// Pass on the link message to the PIM if needed.
	//
	CComPtr<IPremiseObject> PIM;
	if (SendToPIM && SUCCEEDED(FindInitializedPIM(&PIM))) {
		PIM->SetValueEx(SVCC_NOTIFY | SVCC_FORCE, XML_UPB_UPBNETWORKDATA, &CComVariant(ClientMessage));
	}
}

void CupbController::SetCurrentPacketDestination(
	IN BYTE NetworkID OPTIONAL,
	IN BYTE ModuleID OPTIONAL
)
{
	m_CurrentNetworkID = NetworkID;
	m_CurrentSourceID = ModuleID;
}

BOOL CupbController::CheckCurrentPacketDestination(
	IN BYTE NetworkID,
	IN BYTE ModuleID
)
{
	return (NetworkID == m_CurrentNetworkID) && (ModuleID == m_CurrentSourceID);
}

void CupbController::FindUnpopulatedNetwork(
	IN IPremiseObject* Module
)
{
	BYTE NetworkID;
	BYTE ModuleID;
	USHORT UniqueID;
	//
	// Do a safety check on the NID/MID pair to ensure that they are real before
	// trying to resolve duplicate addresses.
	//
	if ((m_dam == DuplicateAddressMode_None) && SUCCEEDED(GetModuleIdentifiers(Module, NetworkID, ModuleID, UniqueID)) && (NetworkID != GLOBAL_NETWORKID) && (ModuleID != BROADCAST_DEVICEID)) {
		//
		// Set Duplicate Device Address mode. This also stops current network enumeration and
		// allows it to be flushed.
		//
		m_DuplicateNetworkID = NetworkID;
		m_DuplicateSourceID = ModuleID;
		m_DuplicateUniqueID = UniqueID;
		m_dam = DuplicateAddressMode_FlushQueue;
		//
		// Turn everything off to avoid random changes.
		//
		ResetAllInitializedStates();
		//
		// Start enumeration of networks to look for an unpopulated one.
		//
		MoveModules(GLOBAL_NETWORKID, TRUE);
	}
}

void CupbController::MoveModules(
	IN BYTE NetworkID,
	IN BOOL AckReceived
)
{
	if (AckReceived) {
		//
		// This is an occuppied network, so move to the next.
		//
		if (++NetworkID == m_DuplicateNetworkID) {
			NetworkID++;
		}
		if (NetworkID < RESERVED1_NETWORKID) {
			//
			// Network enumeration does not use a UPB_VERSION_QUICKNETWORKENUM conformant
			// query.
			//
			TransmitNetworkPacket(FALSE, PACKETHEADER_REP_NONREPEATER, PACKETHEADER_REQ_ACKPULSE, PACKETHEADER_TRANSMITCOUNT_1, PACKETHEADER_TRANSMITSEQUENCE_1, NetworkID, BROADCAST_DEVICEID, MAKE_MDID(MDID_CORE_COMMANDS, MDID_CORE_COMMAND_NULL), NULL, CREATE_CMD(NetworkID, MESSAGE_TYPE_NETWORKIDENUM, RESPONSE_TYPE_TRANSMITACK, MAKE_MDID(MDID_CORE_COMMANDS, MDID_CORE_COMMAND_NULL)));
			return;
		}
	} else {
		//
		// An unoccuppied network was found. Move the devices to this network.
		//
		m_dam = DuplicateAddressMode_CreateMap;
		m_UnpopulatedNetworkID = NetworkID;
		if (StartSetupMode(m_DuplicateUniqueID, m_DuplicateNetworkID, m_DuplicateSourceID)) {
			char r[16];
			//
			// Move all responding devices to a temporary network. This should at least move
			// the one device that the Module node was created for.
			//
			StringCchPrintfA(r, SIZEOF_ARRAY(r), "%02X%02X", UPB_REG_NETWORKID, NetworkID);
			TransmitNetworkPacket(FALSE, PACKETHEADER_REP_NONREPEATER, PACKETHEADER_REQ_ACKPULSE, PACKETHEADER_TRANSMITCOUNT_1, PACKETHEADER_TRANSMITSEQUENCE_1, m_DuplicateNetworkID, m_DuplicateSourceID, MAKE_MDID(MDID_CORE_COMMANDS, MDID_CORE_COMMAND_SETREGISTERVALUES), r, CREATE_CMD(m_DuplicateUniqueID, MESSAGE_TYPE_NORMAL, RESPONSE_TYPE_TRANSMITACK, MAKE_MDID(MDID_CORE_COMMANDS, MDID_CORE_COMMAND_SETREGISTERVALUES)));
			//
			// Change the addresses for all the moved devices.
			//
			TransmitNetworkPacket(FALSE, PACKETHEADER_REP_NONREPEATER, PACKETHEADER_REQ_ACKPULSE, PACKETHEADER_TRANSMITCOUNT_1, PACKETHEADER_TRANSMITSEQUENCE_1, m_UnpopulatedNetworkID, BROADCAST_DEVICEID, MAKE_MDID(MDID_CORE_COMMANDS, MDID_CORE_COMMAND_AUTOADDRESS), NULL, CREATE_CMD(0, MESSAGE_TYPE_NORMAL, RESPONSE_TYPE_TRANSMITACK, MAKE_MDID(MDID_CORE_COMMANDS, MDID_CORE_COMMAND_AUTOADDRESS)));
			//
			// Create a map of the original network ID.
			//
			ZeroMemory(&m_NetworkMap, sizeof(m_NetworkMap));
			TransmitNetworkPacket(FALSE, PACKETHEADER_REP_NONREPEATER, PACKETHEADER_REQ_IDPULSE, PACKETHEADER_TRANSMITCOUNT_1, PACKETHEADER_TRANSMITSEQUENCE_1, m_DuplicateNetworkID, BROADCAST_DEVICEID, MAKE_MDID(MDID_CORE_COMMANDS, MDID_CORE_COMMAND_NULL), NULL, CREATE_CMD(m_DuplicateNetworkID, MESSAGE_TYPE_NETWORKMODULEENUM, RESPONSE_TYPE_ACK, MAKE_MDID(MDID_CORE_COMMANDS, MDID_CORE_COMMAND_NULL)));
			return;
		}
	}
	//
	// There are no free spots in any network. So leave the devices were they sit.
	//
	ResetAddressResolution();
	ReadPIMRegisters(INITIAL_PIM_REG_QUERY_BASE, INITIAL_PIM_REG_QUERY_BYTES);
}

void CupbController::SetNetworkMapBit(
	IN BYTE ModuleID
)
{
	//
	// Set the specified bit in the network map.
	//
	m_NetworkMap[ModuleID/8] |= (1 << (ModuleID % 8));
}

void CupbController::EnumerateUnpopulatedNetwork(
)
{
	m_dam = DuplicateAddressMode_MoveModules;
	//
	// Enumerate the modules in the temporary network in order to count them.
	//
	ZeroMemory(&m_UnpopulatedNetworkMap, sizeof(m_UnpopulatedNetworkMap));
	TransmitNetworkPacket(FALSE, PACKETHEADER_REP_NONREPEATER, PACKETHEADER_REQ_IDPULSE, PACKETHEADER_TRANSMITCOUNT_1, PACKETHEADER_TRANSMITSEQUENCE_1, m_UnpopulatedNetworkID, BROADCAST_DEVICEID, MAKE_MDID(MDID_CORE_COMMANDS, MDID_CORE_COMMAND_NULL), NULL, CREATE_CMD(m_UnpopulatedNetworkID, MESSAGE_TYPE_NETWORKMODULEENUM, RESPONSE_TYPE_ACK, MAKE_MDID(MDID_CORE_COMMANDS, MDID_CORE_COMMAND_NULL)));
}

void CupbController::SetUnpopulatedNetworkMapBit(
	IN BYTE ModuleID
)
{
	//
	// Set the specified bit in the network map.
	//
	m_UnpopulatedNetworkMap[ModuleID/8] |= (1 << (ModuleID % 8));
}

void CupbController::SetNewModuleAddresses(
)
{
	//
	// Count how many devices showed up on the unpopulated network. Also keep track of
	// the last one found in case there is only one of them.
	//
	int MovedModules = 0;
	int LastModuleFound = 0;
	int ModuleID;
	for (ModuleID = BROADCAST_DEVICEID + 1; ModuleID <= DEFAULT_DEVICEID; ModuleID++) {
		if (m_UnpopulatedNetworkMap[ModuleID/8] & (1 << (ModuleID % 8))) {
			MovedModules++;
			LastModuleFound = ModuleID;
		}
	}
	//
	// If there is only 1 device in the unpopulated network, and no longer any device at
	// the original Module ID position, then there presumably were no duplicates. So just
	// move the device back and exit.
	//
	if ((MovedModules == 1) && !(m_NetworkMap[m_DuplicateSourceID/8] & (1 << (m_DuplicateSourceID % 8)))) {
		char r[16];
		StringCchPrintfA(r, SIZEOF_ARRAY(r), "%02X%02X%02X", UPB_REG_NETWORKID, m_DuplicateNetworkID, m_DuplicateSourceID);
		TransmitNetworkPacket(FALSE, PACKETHEADER_REP_NONREPEATER, PACKETHEADER_REQ_ACKPULSE, PACKETHEADER_TRANSMITCOUNT_1, PACKETHEADER_TRANSMITSEQUENCE_1, m_UnpopulatedNetworkID, static_cast<BYTE>(LastModuleFound), MAKE_MDID(MDID_CORE_COMMANDS, MDID_CORE_COMMAND_SETREGISTERVALUES), r, CREATE_CMD(m_DuplicateUniqueID, MESSAGE_TYPE_NORMAL, RESPONSE_TYPE_TRANSMITACK, MAKE_MDID(MDID_CORE_COMMANDS, MDID_CORE_COMMAND_SETREGISTERVALUES)));
		StopSetupMode(m_DuplicateUniqueID, m_UnpopulatedNetworkID, static_cast<BYTE>(LastModuleFound));
	} else {
		//
		// Remove the original Premise object that represented the multiple device
		// addresses, as there is no way to determine which device this was actually
		// supposed to represent.
		//
		CComPtr<IPremiseObject> Module;
		if (SUCCEEDED(FindUpbModule(m_DuplicateUniqueID, &Module))) {
			Module->DeleteObject(NULL);
		}
		//
		// Enumerate the devices that were moved, and move each of them back to the
		// original network ID if there is room. When enumerating the original network to
		// look for a free space, always continue where the search was left off.
		//
		int NewModuleID = BROADCAST_DEVICEID + 1;
		for (ModuleID = BROADCAST_DEVICEID + 1; MovedModules; ModuleID++) {
			if (m_UnpopulatedNetworkMap[ModuleID/8] & (1 << (ModuleID % 8))) {
				MovedModules--;
				for (; NewModuleID < RESERVED1_DEVICEID; NewModuleID++) {
					//
					// If a free spot is found in the original network, assign the device
					// to that address, changing both the network and module IDs.
					//
					if (!(m_NetworkMap[NewModuleID/8] & (1 << (NewModuleID % 8)))) {
						char r[16];
						StringCchPrintfA(r, SIZEOF_ARRAY(r), "%02X%02X%02X", UPB_REG_NETWORKID, m_DuplicateNetworkID, NewModuleID);
						TransmitNetworkPacket(FALSE, PACKETHEADER_REP_NONREPEATER, PACKETHEADER_REQ_ACKPULSE, PACKETHEADER_TRANSMITCOUNT_1, PACKETHEADER_TRANSMITSEQUENCE_1, m_UnpopulatedNetworkID, static_cast<BYTE>(ModuleID), MAKE_MDID(MDID_CORE_COMMANDS, MDID_CORE_COMMAND_SETREGISTERVALUES), r, CREATE_CMD(0, MESSAGE_TYPE_NORMAL, RESPONSE_TYPE_TRANSMITACK, MAKE_MDID(MDID_CORE_COMMANDS, MDID_CORE_COMMAND_SETREGISTERVALUES)));
						NewModuleID++;
						break;
					}
				}
				//
				// Failed to find a free address at which to place the device. Quit
				// searching altogether and force an enumeration of the remainder of the
				// devices where they sit. This is OK to do before resetting address
				// resolution mode because the results won't be reported until this
				// thread returns.
				//
				if (NewModuleID == RESERVED1_DEVICEID) {
					TransmitNetworkPacket(FALSE, PACKETHEADER_REP_NONREPEATER, PACKETHEADER_REQ_IDPULSE, PACKETHEADER_TRANSMITCOUNT_1, PACKETHEADER_TRANSMITSEQUENCE_1, m_UnpopulatedNetworkID, BROADCAST_DEVICEID, MAKE_MDID(MDID_CORE_COMMANDS, MDID_CORE_COMMAND_NULL), NULL, CREATE_CMD(m_UnpopulatedNetworkID, MESSAGE_TYPE_NETWORKMODULEENUM, RESPONSE_TYPE_ACK, MAKE_MDID(MDID_CORE_COMMANDS, MDID_CORE_COMMAND_NULL)));
					break;
				}
			}
		}
	}
	//
	// Exit the address resolution mode and force an enumeration of the devices on the
	// original network.
	//
	m_spSite->SetValueEx(SVCC_DRIVER | SVCC_NOTIFY, XML_UPB_NETWORKENUMID, &CComVariant(m_DuplicateNetworkID));
	ResetAddressResolution();
	ReadPIMRegisters(INITIAL_PIM_REG_QUERY_BASE, INITIAL_PIM_REG_QUERY_BYTES);
}

void CupbController::HandleUpbDataMessage(
	IN UINT_PTR cmd
)
{
	const UPB_PACKETHEADER* PacketHeader = reinterpret_cast<const UPB_PACKETHEADER*>(PulseDataGet());
	if (SUCCEEDED(ValidateUpbDataMessage(PacketHeader))) {
		const BYTE* MessageData = reinterpret_cast<const BYTE*>(PacketHeader + 1);
		BYTE MessageDataID = *MessageData;
		MessageData += sizeof(MessageDataID);
		BYTE ResponseType = RESPONSETYPE_FROM_CMD(cmd);
		CComPtr<IPremiseObject> Module;
		//
		// See if it is worth looking into whether this is a response to an
		// outstanding data request, or just data generated by a
		// module. Requested data cannot be a link packet, and must be
		// sent from the device from which data was requested, be sent back to
		// the broadcast or global device identifier (the hardware is
		// not consistant as to which identifier it uses to respond to
		// a message from the global device identifier).
		//
		if (!(PacketHeader->ControlWord[0] & PACKETHEADER_LINKBIT) && CheckCurrentPacketDestination(PacketHeader->NetworkID, PacketHeader->SourceID) && ((PacketHeader->DestinationID == BROADCAST_DEVICEID) || (PacketHeader->DestinationID == DEFAULT_DEVICEID)) && (ResponseType == RESPONSE_TYPE_DATAACK)) {
			BYTE mdid = MDID_FROM_CMD(cmd);
			if (mdid == MessageDataID) {
				//
				// The message was received from the device, so reset the timeout counter in
				// case this device caused timeouts before the message was received.
				//
				m_DeviceTimeouts = 0;
				//
				// Zero this now before the next packet is sent so
				// that duplicate addressing is dealt with properly.
				//
				SetCurrentPacketDestination();
				SetAckReceived();
				BYTE MessageType = MESSAGETYPE_FROM_CMD(cmd);
				USHORT UniqueID = UNIQUEID_FROM_CMD(cmd);
				if (MessageType & MESSAGE_TYPE_CLIENT) {
					if (SUCCEEDED(FindInitializedUpbModule(UniqueID, &Module))) {
						SendDataToClient(Module, UPB_NETWORKDATA_DEVICERESPONSE, reinterpret_cast<const BYTE*>(PacketHeader), m_PulseDataPos);
					}
				} else {
					switch (MessageType) {
					case MESSAGE_TYPE_NORMAL:
						//
						// Allow queries to uninitialized modules to proceed so that
						// name data can be collected to create the password before
						// initialization by the client driver occurs.
						//
						if (SUCCEEDED(FindUpbModule(UniqueID, &Module)) && (MessageDataID == MAKE_MDID(MDID_CORE_REPORTS, MDID_DEVICE_CORE_REPORT_REGISTERVALUES))) {
							BYTE Register = *MessageData++;
							BYTE Registers = PACKETDATALEN(PacketHeader) - 3;
							//
							// Put data into cache. For base UPB registers update the
							// associated properties. A cache read is always
							// UPB_MODULE_MAXREGISTERIO bytes, unless it is the last
							// set of registers.
							//
							CComVariant SetupRegisterCache;
							CComVariant SetupRegisterCount;
							if (SUCCEEDED(ReadSetupRegisterCache(Module, &SetupRegisterCache)) && SUCCEEDED(Module->GetValue(XML_UPB_SETUPREGISTERCOUNT, &SetupRegisterCount))) {
								//
								// If at initialization time the register cache did not
								// match the checksum, then a read of the registers is
								// performed to determine first if this is actually the
								// same module.
								//
								if ((Register == INITIAL_MODULE_REG_QUERY_BASE) && (Registers == INITIAL_MODULE_REG_QUERY_BYTES)) {
									INITIAL_MODULE_REG_REPORT Report = *reinterpret_cast<const INITIAL_MODULE_REG_REPORT*>(MessageData);
									Report.Password = ntohs(Report.Password);
									Report.ManufacturerID = ntohs(Report.ManufacturerID);
									Report.ProductID = ntohs(Report.ProductID);
									//
									// Compare the device's Manufacturer and Product identifiers
									// to those contained in the module.
									//
									CComVariant ManufacturerID;
									CComVariant ProductID;
									if (SUCCEEDED(Module->GetValue(XML_UPB_MANUFACTURERID, &ManufacturerID)) && (ManufacturerID.bVal == Report.ManufacturerID) && SUCCEEDED(Module->GetValue(XML_UPB_PRODUCTID, &ProductID)) && (ProductID.bVal == Report.ProductID)) {
										//
										// It looks like the same device, so assume it is.
										// If the password is obviously visible, then use the
										// contents of the registers. Else determine if the
										// password has already been found in the past.
										//
										BytesToHex(&SetupRegisterCache.bstrVal[2 * Register], MessageData, Registers);
										if (Report.Password) {
											SetUpbPassword(Module, Report.Password);
										} else {
											USHORT Password;
											if (GetUpbPassword(Module, &Password)) {
												BytesToHex(&SetupRegisterCache.bstrVal[2 * UPB_REG_PASSWORD], reinterpret_cast<BYTE*>(&Password), sizeof(Password));
											}
										}
										Module->SetValueEx(SVCC_DRIVER, XML_UPB_SETUPREGISTERCACHE, &SetupRegisterCache);
										//
										// Read the remainder of the registers.
										//
										for (int i = UPB_REG_NETWORKNAME; i < SetupRegisterCount.bVal; i += UPB_MODULE_MAXREGISTERIO) {
											ReadModuleRegisters(PacketHeader->NetworkID, PacketHeader->SourceID, static_cast<BYTE>(i), static_cast<BYTE>(min(UPB_MODULE_MAXREGISTERIO, SetupRegisterCount.bVal - i)), UniqueID, MESSAGE_TYPE_NORMAL);
										}
									} else {
										//
										// This is a new device, so treat it as such.
										//
										InitPhaseQueryModuleBaseRegisters(MessageData - 1, INVALID_UNIQUE_ID);
									}
									break;
								}
								//
								// Since this is updating the cache, the register
								// query should never include the portion that covers
								// the password registers, unless the device is in
								// Setup Mode or Write Enabled.
								//
								BytesToHex(&SetupRegisterCache.bstrVal[2 * Register], MessageData, Registers);
								Module->SetValueEx(SVCC_DRIVER, XML_UPB_SETUPREGISTERCACHE, &SetupRegisterCache);
								switch (Register) {
								case UPB_REG_NETWORKNAME:
									PopulateRegisterNameValue(Module, XML_UPB_NETWORKNAME, MessageData);
									break;
								case UPB_REG_ROOMNAME:
									PopulateRegisterNameValue(Module, XML_UPB_ROOMNAME, MessageData);
									break;
								case UPB_REG_DEVICENAME:
									PopulateRegisterNameValue(Module, XML_UPB_DEVICENAME, MessageData);
									break;
								}
								//
								// If this is the last set of registers, then complete
								// initialization. If the password is already known, then
								// set upbDeviceInitialized.
								//
								if (Register + Registers == SetupRegisterCount.bVal) {
									USHORT ChecksumDifference = CalcChecksumDifference(Module);
									//
									// Take care of the	obvious end cases before attempting a
									// search.
									//
									USHORT Password;
									if (ChecksumDifference == 0x1fe) {
										//
										// If the checksum difference is 0xff + 0xff = 0x1fe,
										// then the password is known.
										//
										SetUpbPassword(Module, 0xffff);
										Module->SetValueEx(SVCC_NOTIFY, XML_UPB_UPBDEVICEINITIALIZED, &CComVariant(TRUE));
									} else if (!ChecksumDifference) {
										//
										// If the checksum difference is zero, then the
										// password is known.
										//
										SetUpbPassword(Module, 0);
										Module->SetValueEx(SVCC_NOTIFY, XML_UPB_UPBDEVICEINITIALIZED, &CComVariant(TRUE));
									} else if (GetUpbPassword(Module, &Password) && (ChecksumDifference == ((Password >> 8) + (Password & 0xff)))) {
										//
										// If the checksum difference is equal to the sum of
										// bytes of the currently set password, then leave the
										// current password, as it is already known.
										//
										Module->SetValueEx(SVCC_NOTIFY, XML_UPB_UPBDEVICEINITIALIZED, &CComVariant(TRUE));
									} else {
										//
										// Delete any current password, as it is wrong.
										//
										Module->SetValueEx(SVCC_DRIVER | SVCC_NOTIFY, XML_UPB_NETWORKPASSWORD, &CComVariant(L""));
									}
									//
									// If a password has been set in the past, use it
									// as the first time guess for the next module that needs a
									// password discovered.
									//
									CComVariant LastPasswordSet;
									if (SUCCEEDED(m_spSite->GetValue(XML_UPB_LASTPASSWORDSET, &LastPasswordSet)) && LastPasswordSet.uiVal) {
										QueueNextPasswordTry(LastPasswordSet.uiVal);
									} else {
										QueueNextPasswordTry();
									}
								}
							}
						}
						break;
					case MESSAGE_TYPE_QUERYMODULEBASEREGISTERS:
						InitPhaseQueryModuleBaseRegisters(MessageData, UniqueID);
						break;
					case MESSAGE_TYPE_QUERYNEWMODULESIGNATURE:
					case MESSAGE_TYPE_QUERYMODULESIGNATURE:
						InitPhaseQueryModuleSignature(PacketHeader, MessageData, UniqueID, MessageType);
						break;
					case MESSAGE_TYPE_TRYPASSWORD:
					case MESSAGE_TYPE_GUESSPASSWORD:
						HandleTryPassword(MessageData, UniqueID, MessageType);
						break;
					}
				}
				PulseDataReset();
				return;
			}
		}
		//
		// If this is not a Link Packet, and it is a Register Read, then it is likely a
		// response sent by a device with a duplicate NID/MID pair.
		//
		if (!(PacketHeader->ControlWord[0] & PACKETHEADER_LINKBIT) && (MessageDataID == MAKE_MDID(MDID_CORE_REPORTS, MDID_DEVICE_CORE_REPORT_REGISTERVALUES))) {
			USHORT Password;
			if (SUCCEEDED(FindUpbModule(UNIQUEID_FROM_CMD(cmd), &Module)) && GetUpbPassword(Module, &Password)) {
				FindUnpopulatedNetwork(Module);
			}
		} else {
			//
			// This is non-requested data originating from a module.
			// If it is a Link message, then it may need to be broadcast to
			// all modules with the corresponding network identifier and
			// LinkID in a receive component. The reserved virtualized
			// message set is an exception to this, and must also only be
			// sent on to the module whose hardware originated the message.
			//
			// Send the packet to the module whose hardware originated the
			// message so that it can update the state based on it current
			// configuration.
			//
			if (SUCCEEDED(FindInitializedUpbModule(PacketHeader->NetworkID, PacketHeader->SourceID, &Module))) {
				SendDataToClient(Module, UPB_NETWORKDATA_DEVICEMESSAGE, reinterpret_cast<const BYTE*>(PacketHeader), m_PulseDataPos);
			} else if (SUCCEEDED(FindUpbModule(PacketHeader->NetworkID, PacketHeader->SourceID, &Module))) {
				//
				// If the device exists, but was never initialized (because it might not have
				// been present), then initialize it now.
				//
				CComVariant UniqueID;
				if (SUCCEEDED(Module->GetValue(XML_UPB_UPBUNIQUEID, &UniqueID))) {
					TransmitNetworkPacket(FALSE, PACKETHEADER_REP_NONREPEATER, PACKETHEADER_REQ_ACKPULSE, PACKETHEADER_TRANSMITCOUNT_1, PACKETHEADER_TRANSMITSEQUENCE_1, PacketHeader->NetworkID, PacketHeader->SourceID, MAKE_MDID(MDID_CORE_COMMANDS, MDID_CORE_COMMAND_GETDEVICESIGNATURE), NULL, CREATE_CMD(UniqueID.uiVal, MESSAGE_TYPE_QUERYMODULESIGNATURE, RESPONSE_TYPE_DATAACK, MAKE_MDID(MDID_CORE_REPORTS, MDID_DEVICE_CORE_REPORT_DEVICESIGNATURE)));
				}
			} else {
				//
				// This appears to be a new device, so enumerate it.
				//
				ReadModuleRegisters(PacketHeader->NetworkID, PacketHeader->SourceID, INITIAL_MODULE_REG_QUERY_BASE, INITIAL_MODULE_REG_QUERY_BYTES, INVALID_UNIQUE_ID, MESSAGE_TYPE_QUERYMODULEBASEREGISTERS);
			}
			if (PacketHeader->ControlWord[0] & PACKETHEADER_LINKBIT) {
				//
				// Only broadcast normal Link messages to all other modules with a corresponding
				// Receive Component. The virtualized message set has already been sent above
				// to the module whose hardware device originated the message.
				//
				if (MessageDataID != MAKE_MDID(MDID_EXTENDED_MESSAGE_SET, VIRTUALIZED_MESSAGE_SET)) {
					//
					// The message needs to be in hex format. Skip the
					// header and get to the message data portion. Note
					// that this assumes link messages have an mdid.
					//
					char LinkData[64];
					int LinkDataLen = m_PulseDataPos - sizeof(PacketHeader) - 1;
					const BYTE* ByteData = reinterpret_cast<const BYTE*>(PacketHeader + 1) + 1;
					for (int i = 0; i < LinkDataLen; i++) {
						LinkData[2 * i] = static_cast<char>(NibToHex(ByteData[i] >> 4));
						LinkData[2 * i + 1] = static_cast<char>(NibToHex(ByteData[i] & 0xf));
					}
					LinkData[LinkDataLen] = 0;
					//
					// This will skip the source module, should it also
					// have the same Link ID in a receive component.
					//
					BroadcastLinkPacket(PacketHeader->NetworkID, PacketHeader->SourceID, PacketHeader->DestinationID, MessageDataID, LinkData, TRUE);
				}
			} else {
				//
				// A direct packet needs to be passed on to the destination
				// module.
				//
				CComPtr<IPremiseObject> DestinationModule;
				if (SUCCEEDED(FindInitializedUpbModule(PacketHeader->NetworkID, PacketHeader->DestinationID, &DestinationModule))) {
					SendDataToClient(DestinationModule, UPB_NETWORKDATA_DEVICEMESSAGE, reinterpret_cast<const BYTE*>(PacketHeader), m_PulseDataPos);
				}
			}
		}
	}
	PulseDataReset();
}

void CupbController::HandleUpbAckMessage(
	IN BOOL AckReceived,
	IN UINT_PTR cmd
)
{
	BYTE ResponseType = RESPONSETYPE_FROM_CMD(cmd);
	if (ResponseType == RESPONSE_TYPE_TRANSMITACK) {
		SetCurrentPacketDestination();
		SetAckReceived();
	}		
	CComPtr<IPremiseObject> Module;
	USHORT UniqueID = UNIQUEID_FROM_CMD(cmd);
	BYTE MessageType = MESSAGETYPE_FROM_CMD(cmd);
	if (MessageType & MESSAGE_TYPE_CLIENT) {
		if ((ResponseType == RESPONSE_TYPE_TRANSMITACK) && SUCCEEDED(FindInitializedUpbModule(UniqueID, &Module))) {
			SendDataToClient(Module, AckReceived ? UPB_NETWORKDATA_ACCEPT : UPB_NETWORKDATA_NAK);
		}
	} else {
		switch (MessageType) {
		case MESSAGE_TYPE_NORMAL:
		case MESSAGE_TYPE_QUERYMODULEBASEREGISTERS:
		case MESSAGE_TYPE_QUERYNEWMODULESIGNATURE:
		case MESSAGE_TYPE_QUERYMODULESIGNATURE:
			//
			// Nothing to do.
			//
			break;
		case MESSAGE_TYPE_TRYPASSWORD:
		case MESSAGE_TYPE_GUESSPASSWORD:
			if ((MDID_FROM_CMD(cmd) == MAKE_MDID(MDID_CORE_COMMANDS, MDID_CORE_COMMAND_STARTSETUP)) && SUCCEEDED(FindUpbModule(UniqueID, &Module))) {
				CComVariant NetworkID;
				CComVariant ModuleID;
				if (SUCCEEDED(Module->GetValue(XML_UPB_NETWORKID, &NetworkID)) && SUCCEEDED(Module->GetValue(XML_UPB_MODULEID, &ModuleID))) {
					TransmitNetworkPacket(FALSE, PACKETHEADER_REP_NONREPEATER, PACKETHEADER_REQ_ACKPULSE, PACKETHEADER_TRANSMITCOUNT_1, PACKETHEADER_TRANSMITSEQUENCE_1, NetworkID.bVal, ModuleID.bVal, MAKE_MDID(MDID_CORE_COMMANDS, MDID_CORE_COMMAND_GETSETUPTIME), NULL, CREATE_CMD(UniqueID, MessageType, RESPONSE_TYPE_DATAACK, MAKE_MDID(MDID_CORE_REPORTS, MDID_DEVICE_CORE_REPORT_SETUPTIME)));
				}
			}
			break;
		case MESSAGE_TYPE_NETWORKMODULEENUM:
			//
			// A NetworkID was completely enumerated, which returns
			// UPB_NETWORK_MAXADDRESSES ACK/NAK items, each of which represents a
			// ModuleID. If an ACK is received, then there is a module at that address,
			// and it is queried.
			//
			// Enumeration is performed serially, so only a single enumeration request
			// can possibly be running. The count is the current ModuleID.
			//
			if (!m_NetworkModuleEnumCount) {
				m_NetworkModuleEnumCount = UPB_NETWORK_MAXADDRESSES;
			}
			if (AckReceived) {
				if (m_dam == DuplicateAddressMode_None) {
					//
					// UNIQUEID_FROM_CMD(cmd) contains the NetworkID.
					//
					if (SUCCEEDED(FindUpbModule(static_cast<BYTE>(UniqueID), UPB_NETWORK_MAXADDRESSES - m_NetworkModuleEnumCount, &Module))) {
						//
						// The device exists.
						//
						CComVariant ThisUniqueID;
						if (SUCCEEDED(Module->GetValue(XML_UPB_UPBUNIQUEID, &ThisUniqueID))) {
							TransmitNetworkPacket(FALSE, PACKETHEADER_REP_NONREPEATER, PACKETHEADER_REQ_ACKPULSE, PACKETHEADER_TRANSMITCOUNT_1, PACKETHEADER_TRANSMITSEQUENCE_1, static_cast<BYTE>(UniqueID), UPB_NETWORK_MAXADDRESSES - m_NetworkModuleEnumCount, MAKE_MDID(MDID_CORE_COMMANDS, MDID_CORE_COMMAND_GETDEVICESIGNATURE), NULL, CREATE_CMD(ThisUniqueID.uiVal, MESSAGE_TYPE_QUERYMODULESIGNATURE, RESPONSE_TYPE_DATAACK, MAKE_MDID(MDID_CORE_REPORTS, MDID_DEVICE_CORE_REPORT_DEVICESIGNATURE)));
						}
					} else {
						//
						// This appears to be a new device, so enumerate it.
						//
						ReadModuleRegisters(static_cast<BYTE>(UniqueID), UPB_NETWORK_MAXADDRESSES - m_NetworkModuleEnumCount, INITIAL_MODULE_REG_QUERY_BASE, INITIAL_MODULE_REG_QUERY_BYTES, INVALID_UNIQUE_ID, MESSAGE_TYPE_QUERYMODULEBASEREGISTERS);
					}
				} else if (m_dam == DuplicateAddressMode_CreateMap) {
					//
					// Creating map of original network ID.
					//
					SetNetworkMapBit(UPB_NETWORK_MAXADDRESSES - m_NetworkModuleEnumCount);
				} else if (m_dam == DuplicateAddressMode_MoveModules) {
					SetUnpopulatedNetworkMapBit(UPB_NETWORK_MAXADDRESSES - m_NetworkModuleEnumCount);
				}
			}
			//
			// At the end of the UPB_NETWORK_MAXADDRESSES ACK/NAK items, the command is
			// complete.
			//
			if (!--m_NetworkModuleEnumCount) {
				if (m_dam == DuplicateAddressMode_CreateMap) {
					EnumerateUnpopulatedNetwork();
				} else if (m_dam == DuplicateAddressMode_MoveModules) {
					SetNewModuleAddresses();
				}					
				SetCurrentPacketDestination();
				SetAckReceived();
			}
			break;
		case MESSAGE_TYPE_NETWORKIDENUM:
			if (m_dam == DuplicateAddressMode_QueryUnpopulatedNetwork) {
				//
				// The queue has been flushed.
				//
				// Looking for an unpopulated network to move possible duplicates to.
				// UNIQUEID_FROM_CMD(cmd) contains the NetworkID.
				//
				MoveModules(static_cast<BYTE>(UniqueID), AckReceived);
				break;
			} else if (m_dam == DuplicateAddressMode_None) {
				//
				// Determine if this was a UPB_VERSION_QUICKNETWORKENUM compliant query,
				// or the older individual network query.
				//
				if (UniqueID == GLOBAL_NETWORKID) {
					//
					// All NetworkIDs were completely enumerated, which returns
					// UPB_NETWORK_MAXNETWORKS ACK/NAK items, each of which represents a
					// NetworkID. If an ACK is received, then there is at least one
					// module on that network identifier, and it is queried.
					//
					// Enumeration is performed serially, so only a single enumeration
					// request can possibly be running. The count is the current NetworkID.
					//
					if (!m_NetworkIDAckCount) {
						m_NetworkIDAckCount = UPB_NETWORK_MAXNETWORKS;
					}
					if (AckReceived) {
						//
						// At least one device exists, so enumerate that network
						// identifier.
						//
						TransmitNetworkPacket(FALSE, PACKETHEADER_REP_NONREPEATER, PACKETHEADER_REQ_IDPULSE, PACKETHEADER_TRANSMITCOUNT_1, PACKETHEADER_TRANSMITSEQUENCE_1, UPB_NETWORK_MAXNETWORKS - m_NetworkIDAckCount, BROADCAST_DEVICEID, MAKE_MDID(MDID_CORE_COMMANDS, MDID_CORE_COMMAND_NULL), NULL, CREATE_CMD(UPB_NETWORK_MAXNETWORKS - m_NetworkIDAckCount, MESSAGE_TYPE_NETWORKMODULEENUM, RESPONSE_TYPE_ACK, MAKE_MDID(MDID_CORE_COMMANDS, MDID_CORE_COMMAND_NULL)));
					}
					//
					// At the end of UPB_NETWORK_MAXNETWORKS ACK/NAK items, the command
					// is complete.
					//
					if (!--m_NetworkIDAckCount) {
						SetCurrentPacketDestination();
						SetAckReceived();
					}
				} else {
					//
					// A NetworkID was queried to determine if any devices
					// are on it at all. If any ACK occurs, then at least
					// one device is on that NetworkID, and so the whole
					// NetworkID is enumerated. The UNIQUEID_FROM_CMD(cmd)
					// contains the NetworkID to enumerate.
					//
					if (AckReceived) {
						TransmitNetworkPacket(FALSE, PACKETHEADER_REP_NONREPEATER, PACKETHEADER_REQ_IDPULSE, PACKETHEADER_TRANSMITCOUNT_1, PACKETHEADER_TRANSMITSEQUENCE_1, static_cast<BYTE>(UniqueID), BROADCAST_DEVICEID, MAKE_MDID(MDID_CORE_COMMANDS, MDID_CORE_COMMAND_NULL), NULL, CREATE_CMD(UniqueID, MESSAGE_TYPE_NETWORKMODULEENUM, RESPONSE_TYPE_ACK, MAKE_MDID(MDID_CORE_COMMANDS, MDID_CORE_COMMAND_NULL)));
					}
					//
					// Interleave the enumeration of Network ID's so that devices start being
					// added as soon as possible. Check that the count has not been reset
					// because of a communications problem.
					//
					if (m_NetworkIDEnumCount != GLOBAL_NETWORKID) {
						m_NetworkIDEnumCount++;
						//
						// Skip past the reserved network identifiers.
						//
						if (m_NetworkIDEnumCount == WRITEENABLED_NETWORKID) {
							m_NetworkIDEnumCount = DEFAULT_NETWORKID;
						}
						if (m_NetworkIDEnumCount != GLOBAL_NETWORKID) {
							TransmitNetworkPacket(FALSE, PACKETHEADER_REP_NONREPEATER, PACKETHEADER_REQ_ACKPULSE, PACKETHEADER_TRANSMITCOUNT_1, PACKETHEADER_TRANSMITSEQUENCE_1, m_NetworkIDEnumCount, BROADCAST_DEVICEID, MAKE_MDID(MDID_CORE_COMMANDS, MDID_CORE_COMMAND_NULL), NULL, CREATE_CMD(m_NetworkIDEnumCount, MESSAGE_TYPE_NETWORKIDENUM, RESPONSE_TYPE_TRANSMITACK, MAKE_MDID(MDID_CORE_COMMANDS, MDID_CORE_COMMAND_NULL)));
						}
					}
				}
			}
			break;
		}
	}
}

HRESULT CupbController::GenerateUniqueID(
	OUT CComVariant* UniqueID
)
{
	//
	// Obtain the last unique identifier + 1 that was handed out as the first guess for
	// the next unique identifier. Keep track of the total attempts at generating a
	// unique identifier so that the loop can quit in the unlikely event that there are
	// already 64K modules using up all the identifiers (which is more than the total
	// possible modules on all valid network identifiers).
	//
	if (SUCCEEDED(m_spSite->GetValue(XML_UPB_NEXTUNIQUEID, UniqueID))) {
		for (long IdTries = 0; IdTries <= LARGEST_UNIQUE_ID; IdTries++) {
			if (UniqueID->uiVal == INVALID_UNIQUE_ID) {
				UniqueID->uiVal++;
			}
			CComPtr<IPremiseObjectCollection> Modules;
			long UniqueIDCount;
			if (FAILED(m_spSite->GetObjectsByPropertyValue(XML_UPB_UPBUNIQUEID, *UniqueID, FALSE, &Modules)) || FAILED(Modules->get_Count(&UniqueIDCount))) {
				break;
			}
			if (UniqueIDCount) {
				UniqueID->uiVal++;
			} else {
				//
				// Store the next likely candidate for a unique identifier.
				//
				m_spSite->SetValueEx(SVCC_DRIVER, XML_UPB_NEXTUNIQUEID, &CComVariant(UniqueID->uiVal + 1));
				return S_OK;
			}
		}
	}
	return E_FAIL;
}

HRESULT CupbController::LocatePimDevice(
	IN const INITIAL_PIM_REG_REPORT* Report,
	OUT IPremiseObject** PIM
)
{
	CComPtr<IPremiseObjectCollection> PIMs;
	if (SUCCEEDED(m_spSite->GetObjectsByPropertyValue(XML_UPB_MANUFACTURERID, CComVariant(Report->ManufacturerID), FALSE, &PIMs))) {
		long EnumDevice = 0;
		CComPtr<IPremiseObject> ThisPIM;
		for (; SUCCEEDED(PIMs->GetItemEx(EnumDevice, &ThisPIM)); EnumDevice++, ThisPIM = NULL) {
			CComVariant ThisProductID;
			CComVariant ThisFirmwareVersion;
			if (SUCCEEDED(ThisPIM->GetValue(XML_UPB_PRODUCTID, &ThisProductID)) && (ThisProductID.uiVal == Report->ProductID) && SUCCEEDED(ThisPIM->GetValue(XML_UPB_UPBFIRMWAREVERSION, &ThisFirmwareVersion)) && (ThisFirmwareVersion.uiVal == Report->FirmwareVersion)) {
				return ThisPIM->QueryInterface(PIM);
			}
		}
	}
	return E_FAIL;
}

void SetObjectTypeLabels(
	IN IPremiseObject* Module
)
{
	CComPtr<IPremiseObject> ModuleClass;
	CComVariant ModuleDescription;
	//
	// Obtain the Description property of the class of this module, and the Description
	// property of the parent of that module class (the manufacturer's root schema
	// object). These are used as the product name and manufacturer name.
	//
	if (SUCCEEDED(Module->get_Class(&ModuleClass)) && SUCCEEDED(ModuleClass->GetValue(XML_SYS_DESCRIPTION, &ModuleDescription))) {
		Module->SetValueEx(SVCC_DRIVER | SVCC_NOTIFY, XML_UPB_PRODUCTNAME, &ModuleDescription);
		CComPtr<IPremiseObject> LibraryClass;
		CComVariant LibraryDescription;
		if (SUCCEEDED(ModuleClass->get_Parent(&LibraryClass)) && SUCCEEDED(LibraryClass->GetValue(XML_SYS_DESCRIPTION, &LibraryDescription))) {
			Module->SetValueEx(SVCC_DRIVER | SVCC_NOTIFY, XML_UPB_MANUFACTURERNAME, &LibraryDescription);
		}
	}
}

HRESULT CupbController::CreateOrReplaceUpbObject(
	IN IPremiseObject* CurrentObject,
	IN USHORT ManufacturerID,
	IN USHORT ProductID,
	IN USHORT FirmwareVersion,
	IN BSTR DefaultSchema,
	OUT IPremiseObject** NewObject
)
{
	//
	// If the object does not already exist, or it does exist but is one of the default
	// object classes, and not one created by a particular manufacturer, then attempt to
	// locate the manufacturer and create a hardware device-specific module for it.
	//
	if (!CurrentObject || IsObjectOfExplicitType(CurrentObject, DefaultSchema)) {
		CComBSTR SchemaPath;
		if (SUCCEEDED(LocateUpbManufacturer(ManufacturerID, ProductID, FirmwareVersion, &SchemaPath)) && SUCCEEDED(m_spSite->CreateObject(SchemaPath, NULL, NewObject))) {
			SetObjectTypeLabels(*NewObject);
			CComVariant UniqueID;
			//
			// If this hardware device-specific module is replacing a default object
			// class, then copy the basic information to the new module so that it need
			// not be queried again.
			//
			if (CurrentObject) {
				CComVariant OldProp;
				if (SUCCEEDED(CurrentObject->GetValue(XML_SYS_DESCRIPTION, &OldProp))) {
					(*NewObject)->SetValueEx(SVCC_DRIVER | SVCC_NOTIFY, XML_SYS_DESCRIPTION, &OldProp);
					OldProp = NULL;
				}
				if (SUCCEEDED(CurrentObject->GetValue(XML_SYS_DISPLAYNAME, &OldProp))) {
					(*NewObject)->SetValueEx(SVCC_DRIVER | SVCC_NOTIFY, XML_SYS_DISPLAYNAME, &OldProp);
					OldProp = NULL;
				}
				if (SUCCEEDED(CurrentObject->GetValue(XML_SYS_NAME, &OldProp))) {
					(*NewObject)->SetValueEx(SVCC_DRIVER | SVCC_NOTIFY, XML_SYS_NAME, &OldProp);
					OldProp = NULL;
				}
				if (SUCCEEDED(CurrentObject->GetValue(XML_UPB_NETWORKPASSWORD, &OldProp))) {
					(*NewObject)->SetValueEx(SVCC_DRIVER | SVCC_NOTIFY, XML_UPB_NETWORKPASSWORD, &OldProp);
					OldProp = NULL;
				}
				//
				// Copying the SetupRegisterCache will fail for a PIM,
				// which is fine.
				//
				if (SUCCEEDED(CurrentObject->GetValue(XML_UPB_SETUPREGISTERCACHE, &OldProp))) {
					(*NewObject)->SetValueEx(SVCC_DRIVER, XML_UPB_SETUPREGISTERCACHE, &OldProp);
					OldProp = NULL;
				}
				if ((SUCCEEDED(CurrentObject->GetValue(XML_UPB_UPBUNIQUEID, &UniqueID)) && UniqueID.uiVal) || SUCCEEDED(GenerateUniqueID(&UniqueID))) {
					(*NewObject)->SetValueEx(SVCC_DRIVER, XML_UPB_UPBUNIQUEID, &UniqueID);
				}
				CurrentObject->DeleteObject(NULL);
			} else if (SUCCEEDED(GenerateUniqueID(&UniqueID))){
				(*NewObject)->SetValueEx(SVCC_DRIVER, XML_UPB_UPBUNIQUEID, &UniqueID);
			}
		} else if (CurrentObject) {
			//
			// A default object already existed, but a manufacturer-specific class could
			// not be located. So just continue using the same object.
			//
			return CurrentObject->QueryInterface(NewObject);
		} else {
			//
			// No object previously existed, and a manufacturer-specific class could not
			// be located. Create a default version of the module.
			//
			if (SUCCEEDED(m_spSite->CreateObject(DefaultSchema, NULL, NewObject))) {
				//
				// The product is not supported, but there may be a manufacturer's
				// driver installed, so that the ManufacturerName can be filled in
				// correctly.
				//
				if (FAILED(LocateUpbManufacturerName(*NewObject, ManufacturerID))) {
					(*NewObject)->SetValueEx(SVCC_DRIVER | SVCC_NOTIFY, XML_UPB_MANUFACTURERNAME, &CComVariant(XML_UPB_UNKNOWN));
				}
				(*NewObject)->SetValueEx(SVCC_DRIVER | SVCC_NOTIFY, XML_UPB_PRODUCTNAME, &CComVariant(XML_UPB_UNKNOWN));
				CComVariant UniqueID;
				if (SUCCEEDED(GenerateUniqueID(&UniqueID))){
					(*NewObject)->SetValueEx(SVCC_DRIVER, XML_UPB_UPBUNIQUEID, &UniqueID);
				}
			} else {
				return E_FAIL;
			}
		}
		(*NewObject)->SetValueEx(SVCC_DRIVER | SVCC_NOTIFY, XML_UPB_MANUFACTURERID, &CComVariant(ManufacturerID));
		(*NewObject)->SetValueEx(SVCC_DRIVER | SVCC_NOTIFY, XML_UPB_PRODUCTID, &CComVariant(ProductID));
		return S_OK;
	}
	return CurrentObject->QueryInterface(NewObject);
}

HRESULT CupbController::SetupPimDevice(
	IN const INITIAL_PIM_REG_REPORT* Report,
	OUT IPremiseObject** PIM
)
{
	//
	// Locate the PIM, if there is one already to compare with
	// what is already there. If an Unknown object was already created,
	// then still look for a Manufacturer-specific driver, in case it was
	// later installed.
	//
	CComPtr<IPremiseObject> CurrentPim;
	LocatePimDevice(Report, &CurrentPim);
	CComPtr<IPremiseObject> NewPim;
	if (FAILED(CreateOrReplaceUpbObject(CurrentPim, Report->ManufacturerID, Report->ProductID, Report->FirmwareVersion, XML_UPB_SCHEMA_ROOT XML_SYS_PATH_SEPARATOR XML_UPB_UNKNOWNPIM, &NewPim))) {
		return E_FAIL;
	}
	NewPim->SetValueEx(SVCC_DRIVER | SVCC_NOTIFY, XML_UPB_UPBOPTIONS, &CComVariant(Report->UpbOptions));
	NewPim->SetValueEx(SVCC_DRIVER | SVCC_NOTIFY, XML_UPB_UPBVERSION, &CComVariant(Report->UpbVersion));
	CComBSTR FirmwareVersion;
	if (SUCCEEDED(MakeFirmwareVersionString(Report->FirmwareVersion, &FirmwareVersion))) {
		NewPim->SetValueEx(SVCC_DRIVER | SVCC_NOTIFY, XML_UPB_FIRMWAREVERSION, &CComVariant(FirmwareVersion));
		NewPim->SetValueEx(SVCC_DRIVER, XML_UPB_UPBFIRMWAREVERSION, &CComVariant(Report->FirmwareVersion));
	}
	return NewPim->QueryInterface(PIM);
}

void CupbController::StartNetworkEnumeration(
)
{
	//
	// Enumeration will confuse things to be started when duplicate address resolution is
	// running. Also, the address resolution will run enumeration when it is complete.
	//
	if (m_dam != DuplicateAddressMode_None) {
		return;
	}
	//
	// Obtain the items to enumerate. The modules which already exist are always
	// enumerated in one fashion or another. In addition, a specific network identifier
	// can also be enumerate by setting the XML_UPB_NETWORKENUMID to that value, or all
	// network identifiers can be enumerated by setting the same item to
	// GLOBAL_NETWORKID.
	//
	CComVariant NetworkEnumID;
	if (FAILED(m_spSite->GetValue(XML_UPB_NETWORKENUMID, &NetworkEnumID)) || (NetworkEnumID.lVal < GLOBAL_NETWORKID)) {
		NetworkEnumID = DEFAULT_NETWORK_ENUMERATION;
	}
	//
	// Reset back to the default value once that value is obtained.
	//
	m_spSite->SetValueEx(SVCC_DRIVER | SVCC_NOTIFY, XML_UPB_NETWORKENUMID, &CComVariant(DEFAULT_NETWORK_ENUMERATION));
	//
	// If the whole network is to be enumerated, then there is no need to check
	// individual existing devices and such that happens below.
	//
	if (NetworkEnumID.lVal == GLOBAL_NETWORKID) {
		//
		// Check the UPB Version compliance. If a quick global network enumeration can be
		// done, then do that instead of a separate requests on each network identifier.
		//
		if (m_UpbVersion >= UPB_VERSION_QUICKNETWORKENUM) {
			//
			// Send an ID pulse to the GLOBAL_NETWORKID, which will produce responses
			// from devices on each network identifier so a map can be built of all
			// occuppied network identifiers.
			//
			TransmitNetworkPacket(FALSE, PACKETHEADER_REP_NONREPEATER, PACKETHEADER_REQ_IDPULSE, PACKETHEADER_TRANSMITCOUNT_1, PACKETHEADER_TRANSMITSEQUENCE_1, GLOBAL_NETWORKID, BROADCAST_DEVICEID, MAKE_MDID(MDID_CORE_COMMANDS, MDID_CORE_COMMAND_NULL), NULL, CREATE_CMD(GLOBAL_NETWORKID, MESSAGE_TYPE_NETWORKIDENUM, RESPONSE_TYPE_ACK, MAKE_MDID(MDID_CORE_COMMANDS, MDID_CORE_COMMAND_NULL)));
		} else {
			//
			// The older method must be use. First use the shorter ACK pulse request to
			// see if anything at all exists, before bothering to request an ID pulse.
			//
			m_NetworkIDEnumCount = GLOBAL_NETWORKID + 1;
			TransmitNetworkPacket(FALSE, PACKETHEADER_REP_NONREPEATER, PACKETHEADER_REQ_ACKPULSE, PACKETHEADER_TRANSMITCOUNT_1, PACKETHEADER_TRANSMITSEQUENCE_1, m_NetworkIDEnumCount, BROADCAST_DEVICEID, MAKE_MDID(MDID_CORE_COMMANDS, MDID_CORE_COMMAND_NULL), NULL, CREATE_CMD(m_NetworkIDEnumCount, MESSAGE_TYPE_NETWORKIDENUM, RESPONSE_TYPE_TRANSMITACK, MAKE_MDID(MDID_CORE_COMMANDS, MDID_CORE_COMMAND_NULL)));
		}
	} else {
		//
		// Create a zero-initialized map of network identifiers. If a particular network
		// identifier is to be enumerated, then set the bit for that network identifier.
		//
		BYTE NetworkMap[UPB_NETWORK_MAXNETWORKS/8] = {0};
		//
		// If some set of network identifiers is to be enumerated in addition, determine what
		// they are, and build the network bitmap of those to be enumerated.
		//
		if (NetworkEnumID.lVal != DEFAULT_NETWORK_ENUMERATION) {
			//
			// Do not allow the reserved network identifiers to be enumerated, but allow the
			// Default Network Identifier, since some devices are initially set to that.
			//
			if ((NetworkEnumID.lVal >= WRITEENABLED_NETWORKID) && (NetworkEnumID.lVal != DEFAULT_NETWORKID)) {
				NetworkEnumID = DEFAULT_NETWORK_ENUMERATION;
			} else {
				//
				// Add this network to the set of network identifiers that should be
				// skipped when enumerating existing modules, as that whole network
				// identifier will already be enumerated.
				//
				NetworkMap[NetworkEnumID.lVal/8] |= (1 << (NetworkEnumID.lVal % 8));
			}
		}
		//
		// Enumerate the networks on which existing modules reside. This is so that there is
		// a quick turnaround as to which devices are present, rather than waiting for
		// individual responses.
		//
		CComPtr<IPremiseObjectCollection> Modules;
		if (SUCCEEDED(m_spSite->GetObjectsByType(CComVariant(XML_UPB_SCHEMA_ROOT XML_SYS_PATH_SEPARATOR XML_UPB_MODULE), collectionTypeNoRecurse, &Modules))) {
			long EnumDevice = 0;
			CComPtr<IPremiseObject> Module;
			for (; SUCCEEDED(Modules->GetItemEx(EnumDevice, &Module)); EnumDevice++, Module = NULL) {
				CComVariant NetworkID;
				//
				// Only enumerate the network on which this device resides if it won't also
				// be enumerated below. Then add this network identifier to the map of identifiers
				// that have been, or will be, enumerated. The next time through this loop
				// will skip any other devices with the same network identifier.
				//
				if (SUCCEEDED(Module->GetValue(XML_UPB_NETWORKID, &NetworkID)) && (NetworkID.bVal != GLOBAL_NETWORKID) && !(NetworkMap[NetworkID.bVal/8] & (1 << (NetworkID.bVal % 8)))) {
					NetworkMap[NetworkID.bVal/8] |= (1 << (NetworkID.bVal % 8));
					TransmitNetworkPacket(FALSE, PACKETHEADER_REP_NONREPEATER, PACKETHEADER_REQ_IDPULSE, PACKETHEADER_TRANSMITCOUNT_1, PACKETHEADER_TRANSMITSEQUENCE_1, NetworkID.bVal, BROADCAST_DEVICEID, MAKE_MDID(MDID_CORE_COMMANDS, MDID_CORE_COMMAND_NULL), NULL, CREATE_CMD(NetworkID.bVal, MESSAGE_TYPE_NETWORKMODULEENUM, RESPONSE_TYPE_ACK, MAKE_MDID(MDID_CORE_COMMANDS, MDID_CORE_COMMAND_NULL)));
				}
			}
		}
		//
		// Enumerate the specified network identifier, if any.
		//
		if (NetworkEnumID.lVal > GLOBAL_NETWORKID) {
			//
			// Request an ID pulse from each occuppied address on this network identifier.
			//
			TransmitNetworkPacket(FALSE, PACKETHEADER_REP_NONREPEATER, PACKETHEADER_REQ_IDPULSE, PACKETHEADER_TRANSMITCOUNT_1, PACKETHEADER_TRANSMITSEQUENCE_1, NetworkEnumID.bVal, BROADCAST_DEVICEID, MAKE_MDID(MDID_CORE_COMMANDS, MDID_CORE_COMMAND_NULL), NULL, CREATE_CMD(NetworkEnumID.bVal, MESSAGE_TYPE_NETWORKMODULEENUM, RESPONSE_TYPE_ACK, MAKE_MDID(MDID_CORE_COMMANDS, MDID_CORE_COMMAND_NULL)));
		}
	}
}

void CupbController::InitPhaseQueryPimSettings(
	IN LPCSTR RegisterData
)
{
	CComPtr<IPremiseObject> PIM;
	INITIAL_PIM_REG_REPORT Report;
	ParseHexString(RegisterData, reinterpret_cast<BYTE*>(&Report), sizeof(Report));
	Report.ManufacturerID = ntohs(Report.ManufacturerID);
	Report.ProductID = ntohs(Report.ProductID);
	Report.FirmwareVersion = ntohs(Report.FirmwareVersion);
	if (SUCCEEDED(SetupPimDevice(&Report, &PIM))) {
		CComVariant UniqueID;
		if (SUCCEEDED(PIM->GetValue(XML_UPB_UPBUNIQUEID, &UniqueID))) {
			//
			// Ensure the PIM is set to Pulse Mode, as Message Mode is
			// useless.
			//
			char r[16];
			StringCchPrintfA(r, SIZEOF_ARRAY(r), "%02X%02X", UPB_REG_PIMOPTIONS, UPB_REG_PIM_OPTION_NO_IDLE_PULSE_REPORTS);
			WritePIMRegisters(r, UniqueID.uiVal);
			//
			// Notify the manufacturer driver that the device is
			// initialized.
			//
			PIM->SetValueEx(SVCC_NOTIFY, XML_UPB_UPBDEVICEINITIALIZED, &CComVariant(TRUE));
			StartNetworkEnumeration();
		}
	}
}

void CupbController::HandlePimRegisters(
	IN LPCSTR RegisterData,
	IN USHORT UniqueID
)
{
	//
	// Extract the starting register address.
	//
	BYTE RegisterID;
	RegisterData += parsehex(RegisterData, RegisterID);
	//
	// There must be at minimum the register start address, plus one
	// register, plus the checksum. It must also contain an exact
	// number of registers.
	//
	// Only the initial query to find the PIM does not have a unique
	// identifier attached to the query.
	//
	if (UniqueID != INVALID_UNIQUE_ID) {
		//
		// There are only specific queries that are performed by this
		// driver.
		//
		CComPtr<IPremiseObject> PIM;
		if (SUCCEEDED(FindInitializedUpbModule(UniqueID, &PIM)) && (RegisterID == UPB_REG_UPBOPTIONS)) {
			BYTE UpbOptions;
			parsehex(RegisterData, UpbOptions);
			PIM->SetValueEx(SVCC_DRIVER | SVCC_NOTIFY, XML_UPB_UPBOPTIONS, &CComVariant(UpbOptions));
		}
	} else if (RegisterID == INITIAL_PIM_REG_QUERY_BASE) {
		InitPhaseQueryPimSettings(RegisterData);
	}
}

void CupbController::UpdatePulseEnergy(
	IN char PulseEnergy,
	IN UINT_PTR cmd OPTIONAL
)
{
	if (isHex(PulseEnergy)) {
		m_PulseEnergy += HexCharToByte(PulseEnergy);
		//
		// The running average dumps the data to the property every
		// UPB_PULSEENERGYCOUNTMAX pulses.
		//
		if (++m_PulseEnergyCount == UPB_PULSEENERGYCOUNTMAX) {
			CComPtr<IPremiseObject> PIM;
			//
			// Only a single PIM can be in the initialized state at one time under an
			// interface tree. Try to locate by the unique identifier if the packet has
			// such an identifier associated with it, else just find the initialized PIM.
			//
			if ((SUCCEEDED(FindInitializedUpbModule(UNIQUEID_FROM_CMD(cmd), &PIM)) && IsObjectOfExplicitType(PIM, XML_UPB_SCHEMA_ROOT XML_SYS_PATH_SEPARATOR XML_UPB_PIM)) || SUCCEEDED(FindInitializedPIM(&PIM))) {
				PIM->SetValueEx(SVCC_DRIVER | SVCC_NOTIFY, XML_UPB_PULSESTRENGTH, &CComVariant(static_cast<double>(m_PulseEnergy) / (15.0 * UPB_PULSEENERGYCOUNTMAX)));
			}
			m_PulseEnergy = 0;
		}
	}
}

/*
	Overrides CPremiseBufferedPortDeviceBase::ProcessLine. Handles a new single line of data
	from the PIM. Data is sent in text format with newline termination.

		DataLine - The new data received by the port.
*/
void CupbController::ProcessLine(
	IN LPCSTR DataLine
)
{
	UINT_PTR cmd = GetLastCmdSent();
	switch (DataLine[UPB_MESSAGE_TYPE]) {
	case UPB_MESSAGE_PIMREPORT:
		m_BufferedWriteDelay = 0;
		m_IdleTickCount = GetTickCount();
		if (strlen(DataLine) > UPB_MESSAGE_PIMREPORT_TYPE) {
			BYTE ResponseType = RESPONSETYPE_FROM_CMD(cmd);
			BYTE MessageType = MESSAGETYPE_FROM_CMD(cmd);
			switch (DataLine[UPB_MESSAGE_PIMREPORT_TYPE]) {
			case UPB_MESSAGE:
				//
				// The PIM is not run in this mode, so such messages
				// are ignored.
				//
				break;
			case UPB_PIM_BUSY:
			case UPB_PIM_ERROR:
				//
				// Messages are serialized, so the PIM will not be busy. Also, the
				// checksum is calculated correctly, and total length is verified, so no
				// error should ever occur, except when power is restored to the device, and a
				// message is only partially received by it.
				//
				ResendLastCmdImmediate();
				break;
			case UPB_PIM_ACCEPT:
				if (ResponseType == RESPONSE_TYPE_PIMACCEPT) {
					//
					// Only ACK actual PIM writes, not other commands, such as a broadcast,
					// which also generate a PA before also generating an ACK/NAK. Note that the
					// packet destination will not have been set in OnBufferedWrite for this
					// packet.
					//
					SetAckReceived();
					if (MessageType & MESSAGE_TYPE_CLIENT) {
						CComPtr<IPremiseObject> Module;
						if (SUCCEEDED(FindInitializedUpbModule(UNIQUEID_FROM_CMD(cmd), &Module))) {
							SendDataToClient(Module, UPB_PIM_ACCEPT);
						}
					}
				} else {
					m_MessageTransmitState = MessageTransmitState_PimAccept;
				}
				break;
			case UPB_PIM_REGISTERS:
				if (FILTER_CLIENT_FLAG(MessageType) == MESSAGE_TYPE_PIMREAD) {
					if (ResponseType == RESPONSE_TYPE_PIMDATAACK) {
						//
						// Note that the packet destination will not have been set in
						// OnBufferedWrite for this packet.
						//
						SetAckReceived();
					}
					USHORT UniqueID = UNIQUEID_FROM_CMD(cmd);
					if (MessageType & MESSAGE_TYPE_CLIENT) {
						CComPtr<IPremiseObject> Module;
						if (SUCCEEDED(FindInitializedUpbModule(UniqueID, &Module))) {
							SendDataToClient(Module, UPB_PIM_REGISTERS, reinterpret_cast<const BYTE*>(DataLine + UPB_MESSAGE_PIMREPORT_TYPE + 1), m_PulseDataPos - UPB_MESSAGE_PIMREPORT_TYPE - 1);
						}
					} else {
						HandlePimRegisters(DataLine + UPB_MESSAGE_PIMREPORT_TYPE + 1, UniqueID);
					}
				}
				break;
			case UPB_TRANSMISSION_ACK:
			case UPB_TRANSMISSION_NAK:
				{
					//
					// Ensure the PIM is set to Pulse Mode, as Message Mode is
					// useless.
					//
					char r[16];
					StringCchPrintfA(r, SIZEOF_ARRAY(r), "%02X%02X", UPB_REG_PIMOPTIONS, UPB_REG_PIM_OPTION_NO_IDLE_PULSE_REPORTS);
					WritePIMRegisters(r, INVALID_UNIQUE_ID);
					break;
				}
			}
		}
		break;
	case UPB_MESSAGE_DATA_0:
	case UPB_MESSAGE_DATA_1:
	case UPB_MESSAGE_DATA_2:
	case UPB_MESSAGE_DATA_3:
		m_BufferedWriteDelay -= UPB_BIT_PAIR_MILLISECONDS;
		m_IdleTickCount = GetTickCount();
		PulseDataAdd(DataLine);
		break;
	case UPB_MESSAGE_ACK:
		UpdatePulseEnergy(DataLine[PULSE_MODE_ENERGY], cmd);
		NOBREAK;
	case UPB_MESSAGE_NAK:
		m_BufferedWriteDelay = 0;
		m_IdleTickCount = GetTickCount();
		if (m_PulseDataPos) {
			//
			// This signals the end of message data. If there is data,
			// then an incoming message was received.
			//
			HandleUpbDataMessage(cmd);
		} else {
			//
			// Otherwise there is no actual data with the message, just
			// an ACK/NAK.
			//
			m_MessageTransmitState = MessageTransmitState_Transmitted;
			HandleUpbAckMessage(DataLine[UPB_MESSAGE_TYPE] == UPB_MESSAGE_ACK, cmd);
		}
		break;
	case UPB_MESSAGE_DROP:
		ResendLastCmdImmediate();
		break;
	case UPB_MESSAGE_IDLE:
		// Bus Idle
		m_BufferedWriteDelay = 0;
		break;
	case UPB_MESSAGE_START:
		// Received a Start Pulse
		//
		// If this is the first Start bit pair, then set the delay time for maximum in
		// millisconds. Else reduce it by one bit pair.
		//
		if (!m_BufferedWriteDelay) {
			m_BufferedWriteDelay = UPB_TYPICAL_DELAY_MILLISECONDS;
		} else {
			m_BufferedWriteDelay = UPB_TYPICAL_DELAY_MILLISECONDS - UPB_BIT_PAIR_MILLISECONDS;
		}
		UpdatePulseEnergy(DataLine[PULSE_MODE_ENERGY], cmd);
		break;
	case UPB_MESSAGE_SYNC:
		// Received a Sync Pulse
		m_BufferedWriteDelay -= UPB_BIT_PAIR_MILLISECONDS;
		m_IdleTickCount = GetTickCount();
		UpdatePulseEnergy(DataLine[PULSE_MODE_ENERGY], cmd);
		PulseDataReset();
		break;
	case UPB_MESSAGE_TRANSMITTED:
		// Transmitted a Pulse
		break;
	default:
		{
			//
			// Only restart if communication has already been established.
			//
			CComPtr<IPremiseObject> PIM;
			if (SUCCEEDED(FindInitializedPIM(&PIM))) {
				//
				// Garbage. Likely to have happened during opening of the port.
				//
				m_BufferedWriteDelay = 0;
				InitializeData();
				ReadPIMRegisters(INITIAL_PIM_REG_QUERY_BASE, INITIAL_PIM_REG_QUERY_BYTES);
			}
			break;
		}
	}
}

void CupbController::InitializeData(
)
{
	//
	// This will ensure that network messages generated by modules are rejected. The theory
	// being that all such notifications should be processed by the time the next incoming data
	// notification is received.
	//
	ResetAllInitializedStates();
	ClearBufferedCommands();
	SetCurrentPacketDestination();
	ResetAddressResolution();
	m_PulseEnergy = 0;
	m_PulseEnergyCount = 0;
	m_DeviceTimeouts = 0;
	PulseDataReset();
	m_NetworkModuleEnumCount = 0;
	m_NetworkIDEnumCount = GLOBAL_NETWORKID;
	m_NetworkIDAckCount = 0;
	m_FirstPasswordUniqueID = INVALID_UNIQUE_ID;
	m_MessageTransmitState = MessageTransmitState_None;
}

/*
	Implements CPremiseBufferedPortDeviceBase::OnDeviceState. Responds to state changes of the
	port, specifically after it has been opened, or when it is being closed.

		ps - The new port state.

	Return - S_OK.
*/
HRESULT CupbController::OnDeviceState(
	IN DEVICE_STATE ps
)
{
	switch (ps) {
	case PORT_OPENING:
	case PORT_OPENED:
		break;
	case DEVICE_INIT:
		//
		// The port has just completed the Open process.
		//
		{
			m_BufferedWriteDelay = 0;
			InitializeData();
			memset(m_HistoryQueue, 0, sizeof(m_HistoryQueue));
			m_HistoryQueueOldest = 0;
			//
			// Try to find the PIM at init time by sending out queries.
			// First determine the basic PIM information. This query is
			// sent without any Unique Identifier.
			//
			ReadPIMRegisters(INITIAL_PIM_REG_QUERY_BASE, INITIAL_PIM_REG_QUERY_BYTES);
			break;
		}
	case PORT_CLOSING:
		//
		// The port is about to close.
		//
		InitializeData();
		break;
	case PORT_CLOSED:
		break;
	}
	return S_OK;
}

/*
	Implements CPremiseBufferedPortDeviceBase::OnPing. Responds to the query sent during a Ping.

	Return - TRUE so that the port is not reset.
*/
bool CupbController::OnPing(
)
{
	return TRUE;
}

/*
	Overrides CPremiseBufferedPortDeviceBase::OnHeartbeat. This is used to optionally allow
	modules to poll their respective hardware devices.
*/
void CupbController::OnHeartbeat(
)
{
	//
	// First check if polling is turned on, and there is nothing outstanding in the queue to
	// send. Then check if the difference between the last time a packet was sent and now meets
	// or exceeds the minimum idle period. Wrapping of the count does not matter, as it is only
	// a difference. If the count wraps and happens to be within range of the old count, then a
	// single idle slot will be missed, which does not matter.
	//
	CComVariant PollNetwork;
	if (SUCCEEDED(m_spSite->GetValue(XML_UPB_POLLNETWORK, &PollNetwork)) && PollNetwork.boolVal && !m_commands.GetSize() && IsTimeElapsed(m_IdleTickCount, NETWORKIDLE_FREQUENCY)) {
		CComPtr<IPremiseObjectCollection> Modules;
		long EnumCount;
		//
		// Perform a round-robin on each module, returning to the PIM with each cycle.
		//
		if (SUCCEEDED(m_spSite->GetObjectsByTypeAndPropertyValue(XML_UPB_SCHEMA_ROOT XML_SYS_PATH_SEPARATOR XML_UPB_MODULE, XML_UPB_UPBDEVICEINITIALIZED, CComVariant(TRUE), collectionTypeNoRecurse, &Modules)) && SUCCEEDED(Modules->get_Count(&EnumCount)) && (m_IdleEnumCount < EnumCount)) {
			CComPtr<IPremiseObject> Module;
			if (SUCCEEDED(Modules->GetItemEx(m_IdleEnumCount, &Module))) {
				//
				// Notify the module that it can probe its hardware.
				//
				Module->SetValueEx(SVCC_NOTIFY, XML_UPB_UPBNETWORKIDLE, &CComVariant(TRUE));
			}
			m_IdleEnumCount++;
		} else {
			//
			// Probe the PIM instead.
			//
			m_IdleEnumCount = 0;
			CComPtr<IPremiseObject> PIM;
			CComVariant UniqueID;
			if (SUCCEEDED(FindInitializedPIM(&PIM)) && SUCCEEDED(PIM->GetValue(XML_UPB_UPBUNIQUEID, &UniqueID))) {
				ReadPIMRegisters(UPB_REG_UPBOPTIONS, UPB_REG_UPBOPTIONS_BYTES, UniqueID.bVal);
			}
		}
	}
}

/*
	Overrides CPremiseBufferedPortDeviceBase::OnAckTimeout. Determines what to do with a timeout
	based on the last known state. The action taken ranges from total re-initialization to
	merely resending the command.

	Return - FALSE so that the message is not ACK'd.
*/
bool CupbController::OnAckTimeout(
)
{
	if ((m_dam != DuplicateAddressMode_None) || (m_MessageTransmitState == MessageTransmitState_None)) {
		//
		// Either duplicate addressing was being resolved, which is too complicated to continue,
		// or the packet was never accepted by the PIM. In the latter case assumption is that
		// the connection no longer exists. In both cases enumeration must start from the beginning.
		//
		InitializeData();
		ReadPIMRegisters(INITIAL_PIM_REG_QUERY_BASE, INITIAL_PIM_REG_QUERY_BYTES);
	} else {
		switch (m_MessageTransmitState) {
		case MessageTransmitState_PimAccept:
			//
			// The packet was never transmitted by the PIM, or in the case of network enumeration,
			// not all of the ACK's may have been received. It may have been interrupted by incoming
			// data.
			//
			// If this was in the middle of receiving ack's for enumeration of a network
			// identifier, then the count must be reset.
			//
			m_NetworkModuleEnumCount = 0;
			m_NetworkIDAckCount = 0;
			//
			// The sequence count in the message is not incremented because the driver never
			// sets the repeat count in the first place.
			//
			ResendLastCmdImmediate();
			break;
		case MessageTransmitState_Transmitted:
			//
			// The device never replied to a request. Either the packet was lost, or the device is
			// no longer plugged in. Retry the message if a timeout threshold has not been reached.
			//
			m_DeviceTimeouts++;
			if (m_DeviceTimeouts == UPB_MAX_DEVICE_TIMEOUTS) {
				//
				// This device has had too many timeouts, so assume it is gone.
				//
				USHORT UniqueID = UNIQUEID_FROM_CMD(GetLastCmdSent());
				//
				// If the module does not exist yet, then it will just get dropped.
				//
				if (UniqueID != INVALID_UNIQUE_ID) {
					CComPtr<IPremiseObject> Module;
					if (SUCCEEDED(FindUpbModule(UniqueID, &Module))) {
						Module->SetValueEx(SVCC_NOTIFY, XML_UPB_UPBDEVICEINITIALIZED, &CComVariant(FALSE));
						//
						// Remove any other messages to this device from the queue, ignoring the
						// current one. The m_csBPD lock is already held.
						//
						for (int i = 1; i < m_commands.GetSize();) {
							if (UNIQUEID_FROM_CMD(m_commands[i].nCmdID) == UniqueID) {
								m_commands.RemoveAt(i);
							} else {
								i++;
							}
						}
						//
						// Restart password discovery, in case the device that failed to respond
						// happened to be the one doing this.
						//
						// Since the message timed out, there should be no ProcessLine handling
						// occurring, and thus, no second thread concurrently in
						// QueueNextPasswordTry. It is possible that the thread processing the
						// ACK had not yet called SetAckReceived before the timeout occurred,
						// but the delay before timeout presumably is long enough to avoid this.
						// Also, this is how the Premise base class is set up, so that there is
						// no synchronization between processing data and a timeout.
						//
						if (m_FirstPasswordUniqueID == UniqueID) {
							m_FirstPasswordUniqueID = INVALID_UNIQUE_ID;
							QueueNextPasswordTry();
						}
					}
				}
				//
				// Timeouts will not be reset until the next data message is processed, so it
				// must be reset here.
				//
				m_DeviceTimeouts = 0;
				SetCurrentPacketDestination();
				SetAckReceived();
			} else {
				//
				// The sequence count in the message is not incremented because the driver never
				// sets the repeat count in the first place.
				//
				ResendLastCmdImmediate();
			}
			break;
		}
	}
	//
	// Do not ack the last message.
	//
	return FALSE;
}

/*
	Overrides CPremiseBufferedPortDeviceBase::OnBufferedWrite. Keeps track of the network and
	destination identifiers on the actual command currently being sent, as it does not fit into
	the UINT_PTR space provided, and this allows it to be done without allocating and keeping
	track of memory. Also attempts to reduce packet collisions by delaying if a packet is in the
	middle of being received. Lastly, allows duplicate addressing resolution to know that the
	message queue has been flushed of any incoming ACK's before it sends a network enumeration
	query.

		cmd - The command object containing the data to be sent.
*/
void CupbController::OnBufferedWrite(
	IN _command& cmd
)
{
	m_MessageTransmitState = MessageTransmitState_None;
	//
	// To reduce collision with actively incoming data, delay output when a message has been
	// partially received. Data rate is 240 b/s. Basic message length is 7 bytes, plus payload.
	// Assume that anything a device would be reporting on its own is very short, no more than
	// the size of a standard Transmit entry, which is 3 bytes. This is a total of 10 bytes, or
	// a maximum of 80 bits (333 ms).
	//
	if (m_BufferedWriteDelay > 0) {
		Sleep(static_cast<int>(m_BufferedWriteDelay));
	}
	//
	// After delaying, reset the delay amount.
	//
	m_BufferedWriteDelay = 0;
	//
	// If this is a non-link data transmission on the network keep track of that NID/MID.
	// This includes broadcast data.
	//
	if (cmd.pstr[0] == UPB_NETWORK_TRANSMIT) {
		char* CommandString = reinterpret_cast<char*>(cmd.pstr + 1);
		BYTE ControlWord;
		CommandString += parsehex_(CommandString, ControlWord);
		if (!(ControlWord & PACKETHEADER_LINKBIT)) {
			CommandString += 2;
			BYTE NetworkID;
			CommandString += parsehex_(CommandString, NetworkID);
			BYTE DestinationID;
			parsehex_(CommandString, DestinationID);
			SetCurrentPacketDestination(NetworkID, DestinationID);
			//
			// If the queue is now flushed of any outstanding ACK's, then change the mode,
			// since it is now ready to receive ACK's from querying for unpopulated networks.
			//
			if ((m_dam == DuplicateAddressMode_FlushQueue) && (DestinationID == BROADCAST_DEVICEID)) {
				//
				// Check that this is an AckPulse request. This flag is in the second
				// byte of the control word.
				//
				parsehex_(reinterpret_cast<char*>(cmd.pstr + 3), ControlWord);
				if (ControlWord & (PACKETHEADER_REQ_ACKPULSE << 4)) {
					//
					// Check that this is indeed the first network identifier to be queried. It
					// could be an old MESSAGE_TYPE_NETWORKIDENUM message left over that needs to be
					// flushed. Note that if this is an old message, and it happens to also be
					// enumerating the first network identifier that duplicate addressing would,
					// then it just means a second message will be sent, which is fine.
					//
					BYTE FirstNetworkID = GLOBAL_NETWORKID + 1;
					//
					// If the source network happens to be the first network that would
					// be enumerated, then it would have been skipped.
					//
					if (FirstNetworkID == m_DuplicateNetworkID) {
						FirstNetworkID++;
					}
					if (FirstNetworkID == NetworkID) {
						//
						// Check that this is a NULL command, which is a query for ACK responses.
						//
						CommandString += 4;
						BYTE mdid;
						parsehex_(CommandString, mdid);
						if (mdid == MAKE_MDID(MDID_CORE_COMMANDS, MDID_CORE_COMMAND_NULL)) {
							//
							// Old requests have been flushed, so it is now safe to process
							// ACK's from a duplicate address unpopulated network search.
							//
							m_dam = DuplicateAddressMode_QueryUnpopulatedNetwork;
						}
					}
				}
			}
		}
	}
}

BOOL CupbController::StartSetupMode(
	IN USHORT UniqueID,
	IN BYTE NetworkID,
	IN BYTE ModuleID
)
{
	IPremiseObject* Module;
	USHORT Password;
	if (SUCCEEDED(FindUpbModule(UniqueID, &Module)) && GetUpbPassword(Module, &Password)) {
		char r[16];
		StringCchPrintfA(r, SIZEOF_ARRAY(r), "%04X", Password);
		return SUCCEEDED(TransmitNetworkPacket(FALSE, PACKETHEADER_REP_NONREPEATER, PACKETHEADER_REQ_ACKPULSE, PACKETHEADER_TRANSMITCOUNT_1, PACKETHEADER_TRANSMITSEQUENCE_1, NetworkID, ModuleID, MAKE_MDID(MDID_CORE_COMMANDS, MDID_CORE_COMMAND_STARTSETUP), r, CREATE_CMD(UniqueID, MESSAGE_TYPE_NORMAL, RESPONSE_TYPE_TRANSMITACK, MAKE_MDID(MDID_CORE_COMMANDS, MDID_CORE_COMMAND_STARTSETUP))));
	}
	return FALSE;
}

void CupbController::StopSetupMode(
	IN USHORT UniqueID,
	IN BYTE NetworkID,
	IN BYTE ModuleID
)
{
	TransmitNetworkPacket(FALSE, PACKETHEADER_REP_NONREPEATER, PACKETHEADER_REQ_ACKPULSE, PACKETHEADER_TRANSMITCOUNT_1, PACKETHEADER_TRANSMITSEQUENCE_1, NetworkID, ModuleID, MAKE_MDID(MDID_CORE_COMMANDS, MDID_CORE_COMMAND_STOPSETUP), NULL, CREATE_CMD(UniqueID, MESSAGE_TYPE_NORMAL, RESPONSE_TYPE_TRANSMITACK, MAKE_MDID(MDID_CORE_COMMANDS, MDID_CORE_COMMAND_STOPSETUP)));
}

BOOL CupbController::SetWriteEnable(
	IN IPremiseObject* Object,
	IN USHORT UniqueID,
	IN BYTE NetworkID,
	IN BYTE ModuleID
)
{
	USHORT Password;
	if (GetUpbPassword(Object, &Password)) {
		char r[16];
		StringCchPrintfA(r, SIZEOF_ARRAY(r), "%04X", Password);
		return SUCCEEDED(TransmitNetworkPacket(FALSE, PACKETHEADER_REP_NONREPEATER, PACKETHEADER_REQ_ACKPULSE, PACKETHEADER_TRANSMITCOUNT_1, PACKETHEADER_TRANSMITSEQUENCE_1, NetworkID, ModuleID, MAKE_MDID(MDID_CORE_COMMANDS, MDID_CORE_COMMAND_WRITEENABLE), r, CREATE_CMD(UniqueID, MESSAGE_TYPE_NORMAL, RESPONSE_TYPE_TRANSMITACK, MAKE_MDID(MDID_CORE_COMMANDS, MDID_CORE_COMMAND_WRITEENABLE))));
	}
	return FALSE;
}

void CupbController::SetWriteProtect(
	IN USHORT UniqueID,
	IN BYTE NetworkID,
	IN BYTE ModuleID
)
{
	TransmitNetworkPacket(FALSE, PACKETHEADER_REP_NONREPEATER, PACKETHEADER_REQ_ACKPULSE, PACKETHEADER_TRANSMITCOUNT_1, PACKETHEADER_TRANSMITSEQUENCE_1, NetworkID, ModuleID, MAKE_MDID(MDID_CORE_COMMANDS, MDID_CORE_COMMAND_WRITEPROTECT), NULL, CREATE_CMD(UniqueID, MESSAGE_TYPE_NORMAL, RESPONSE_TYPE_TRANSMITACK, MAKE_MDID(MDID_CORE_COMMANDS, MDID_CORE_COMMAND_WRITEPROTECT)));
}

HRESULT STDMETHODCALLTYPE CupbController::OnNetworkPasswordChanged(
	IN IPremiseObject* Object,
	IN VARIANT* NewValue,
	IN VARIANT* OldValue
)
{
	if (IsObjectOfExplicitType(Object, XML_UPB_SCHEMA_ROOT XML_SYS_PATH_SEPARATOR XML_UPB_MODULE)) {
		USHORT NewPassword;
		BSTR Pos = HexStringToShort(NewValue->bstrVal, &NewPassword);
		if (Pos && !*Pos) {
			if (IsDeviceInitialized(Object)) {
				BYTE NetworkID;
				BYTE ModuleID;
				USHORT UniqueID;
				if (SUCCEEDED(GetModuleIdentifiers(Object, NetworkID, ModuleID, UniqueID))) {
					if (SetWriteEnable(Object, UniqueID, NetworkID, ModuleID)) {
						char r[16];
						StringCchPrintfA(r, SIZEOF_ARRAY(r), "%02X%04X", UPB_REG_PASSWORD, NewPassword);
						TransmitNetworkPacket(FALSE, PACKETHEADER_REP_NONREPEATER, PACKETHEADER_REQ_ACKPULSE, PACKETHEADER_TRANSMITCOUNT_1, PACKETHEADER_TRANSMITSEQUENCE_1, NetworkID, ModuleID, MAKE_MDID(MDID_CORE_COMMANDS, MDID_CORE_COMMAND_SETREGISTERVALUES), r, CREATE_CMD(UniqueID, MESSAGE_TYPE_NORMAL, RESPONSE_TYPE_TRANSMITACK, MAKE_MDID(MDID_CORE_COMMANDS, MDID_CORE_COMMAND_SETREGISTERVALUES)));
						SetWriteProtect(UniqueID, NetworkID, ModuleID);
					}
					SetUpbPassword(Object, NewPassword);
				}
			}
		} else if (OldValue->bstrVal[0]) {
			Object->SetValueEx(SVCC_DRIVER | SVCC_NOTIFY, XML_UPB_NETWORKPASSWORD, OldValue);
		}
	}
	return S_OK;
}

HRESULT STDMETHODCALLTYPE CupbController::OnUpbOptionsChanged(
	IN IPremiseObject* Object,
	IN VARIANT* NewValue,
	IN VARIANT* OldValue
)
{
	if (IsDeviceInitialized(Object)) {
		CComVariant UniqueID;
		if (SUCCEEDED(Object->GetValue(XML_UPB_UPBUNIQUEID, &UniqueID))) {
			char r[16];
			CComVariant NetworkID;
			CComVariant ModuleID;
			StringCchPrintfA(r, SIZEOF_ARRAY(r), "%02X%02X", UPB_REG_UPBOPTIONS, NewValue->bVal);
			if (IsObjectOfExplicitType(Object, XML_UPB_SCHEMA_ROOT XML_SYS_PATH_SEPARATOR XML_UPB_PIM)) {
				WritePIMRegisters(r, UniqueID.uiVal);
			} else if (IsObjectOfExplicitType(Object, XML_UPB_SCHEMA_ROOT XML_SYS_PATH_SEPARATOR XML_UPB_MODULE) && SUCCEEDED(Object->GetValue(XML_UPB_NETWORKID, &NetworkID)) && SUCCEEDED(Object->GetValue(XML_UPB_MODULEID, &ModuleID))) {
				if (SetWriteEnable(Object, UniqueID.uiVal, NetworkID.bVal, ModuleID.bVal)) {
					TransmitNetworkPacket(FALSE, PACKETHEADER_REP_NONREPEATER, PACKETHEADER_REQ_ACKPULSE, PACKETHEADER_TRANSMITCOUNT_1, PACKETHEADER_TRANSMITSEQUENCE_1, NetworkID.bVal, ModuleID.bVal, MAKE_MDID(MDID_CORE_COMMANDS, MDID_CORE_COMMAND_SETREGISTERVALUES), r, CREATE_CMD(UniqueID.uiVal, MESSAGE_TYPE_NORMAL, RESPONSE_TYPE_TRANSMITACK, MAKE_MDID(MDID_CORE_COMMANDS, MDID_CORE_COMMAND_SETREGISTERVALUES)));
					SetWriteProtect(UniqueID.uiVal, NetworkID.bVal, ModuleID.bVal);
					CComVariant SetupRegisterCache;
					if (SUCCEEDED(ReadSetupRegisterCache(Object, &SetupRegisterCache))) {
						BytesToHex(&SetupRegisterCache.bstrVal[2 * UPB_REG_UPBOPTIONS], reinterpret_cast<BYTE*>(&NewValue->bVal), UPB_REG_UPBOPTIONS_BYTES);
						Object->SetValueEx(SVCC_DRIVER, XML_UPB_SETUPREGISTERCACHE, &SetupRegisterCache);
					}
				}
			}
		}
	}
	return S_OK;
}

HRESULT STDMETHODCALLTYPE CupbController::OnNetworkIdChanged(
	IN IPremiseObject* Object,
	IN VARIANT* NewValue,
	IN VARIANT* OldValue
)
{
	if (IsDeviceInitialized(Object)) {
		CComVariant ModuleID;
		CComVariant UniqueID;
		if (IsObjectOfExplicitType(Object, XML_UPB_SCHEMA_ROOT XML_SYS_PATH_SEPARATOR XML_UPB_MODULE) && SUCCEEDED(Object->GetValue(XML_UPB_MODULEID, &ModuleID)) && SUCCEEDED(Object->GetValue(XML_UPB_UPBUNIQUEID, &UniqueID))) {
			if (SetWriteEnable(Object, UniqueID.uiVal, OldValue->bVal, ModuleID.bVal)) {
				char r[16];
				StringCchPrintfA(r, SIZEOF_ARRAY(r), "%02X%02X", UPB_REG_NETWORKID, NewValue->bVal);
				TransmitNetworkPacket(FALSE, PACKETHEADER_REP_NONREPEATER, PACKETHEADER_REQ_ACKPULSE, PACKETHEADER_TRANSMITCOUNT_1, PACKETHEADER_TRANSMITSEQUENCE_1, OldValue->bVal, ModuleID.bVal, MAKE_MDID(MDID_CORE_COMMANDS, MDID_CORE_COMMAND_SETREGISTERVALUES), r, CREATE_CMD(UniqueID.uiVal, MESSAGE_TYPE_NORMAL, RESPONSE_TYPE_TRANSMITACK, MAKE_MDID(MDID_CORE_COMMANDS, MDID_CORE_COMMAND_SETREGISTERVALUES)));
				SetWriteProtect(UniqueID.uiVal, NewValue->bVal, ModuleID.bVal);
				CComVariant SetupRegisterCache;
				if (SUCCEEDED(ReadSetupRegisterCache(Object, &SetupRegisterCache))) {
					BytesToHex(&SetupRegisterCache.bstrVal[2 * UPB_REG_NETWORKID], reinterpret_cast<BYTE*>(&NewValue->bVal), UPB_REG_NETWORKID_BYTES);
					Object->SetValueEx(SVCC_DRIVER, XML_UPB_SETUPREGISTERCACHE, &SetupRegisterCache);
				}
			}
		}
	}
	return S_OK;
}

HRESULT STDMETHODCALLTYPE CupbController::OnModuleIdChanged(
	IN IPremiseObject* Object,
	IN VARIANT* NewValue,
	IN VARIANT* OldValue
)
{
	if (IsDeviceInitialized(Object)) {
		CComVariant NetworkID;
		CComVariant UniqueID;
		if (IsObjectOfExplicitType(Object, XML_UPB_SCHEMA_ROOT XML_SYS_PATH_SEPARATOR XML_UPB_MODULE) && SUCCEEDED(Object->GetValue(XML_UPB_NETWORKID, &NetworkID)) && SUCCEEDED(Object->GetValue(XML_UPB_UPBUNIQUEID, &UniqueID))) {
			if (SetWriteEnable(Object, UniqueID.uiVal, NetworkID.bVal, OldValue->bVal)) {
				char r[16];
				StringCchPrintfA(r, SIZEOF_ARRAY(r), "%02X%02X", UPB_REG_MODULEID, NewValue->bVal);
				TransmitNetworkPacket(FALSE, PACKETHEADER_REP_NONREPEATER, PACKETHEADER_REQ_ACKPULSE, PACKETHEADER_TRANSMITCOUNT_1, PACKETHEADER_TRANSMITSEQUENCE_1, NetworkID.bVal, OldValue->bVal, MAKE_MDID(MDID_CORE_COMMANDS, MDID_CORE_COMMAND_SETREGISTERVALUES), r, CREATE_CMD(UniqueID.uiVal, MESSAGE_TYPE_NORMAL, RESPONSE_TYPE_TRANSMITACK, MAKE_MDID(MDID_CORE_COMMANDS, MDID_CORE_COMMAND_SETREGISTERVALUES)));
				SetWriteProtect(UniqueID.uiVal, NetworkID.bVal, NewValue->bVal);
				CComVariant SetupRegisterCache;
				if (SUCCEEDED(ReadSetupRegisterCache(Object, &SetupRegisterCache))) {
					BytesToHex(&SetupRegisterCache.bstrVal[2 * UPB_REG_MODULEID], reinterpret_cast<BYTE*>(&NewValue->bVal), UPB_REG_MODULEID_BYTES);
					Object->SetValueEx(SVCC_DRIVER, XML_UPB_SETUPREGISTERCACHE, &SetupRegisterCache);
				}
			}
		}
	}
	return S_OK;
}

HRESULT CupbController::OnTextNameChanged(
	IN IPremiseObject* Object,
	IN const VARIANT* newValue,
	IN BYTE RegisterBase
)
{
	if (IsDeviceInitialized(Object)) {
		BYTE NetworkID;
		BYTE ModuleID;
		USHORT UniqueID;
		if (IsObjectOfExplicitType(Object, XML_UPB_SCHEMA_ROOT XML_SYS_PATH_SEPARATOR XML_UPB_MODULE) && SUCCEEDED(GetModuleIdentifiers(Object, NetworkID, ModuleID, UniqueID))) {
			if (SetWriteEnable(Object, UniqueID, NetworkID, ModuleID)) {
				char r[UPB_MODULE_MAXREGISTERIO + 32];
				StringCchPrintfA(r, sizeof(r), "%02X", RegisterBase);
				int ValueLen = lstrlenW(newValue->bstrVal);
				for (int i = 0; i < UPB_MODULE_MAXREGISTERIO; i++) {
					if (i < ValueLen) {
						r[2 + (2 * i)] = static_cast<char>(NibToHex(static_cast<BYTE>(newValue->bstrVal[i]) >> 4));
						r[2 + (2 * i) + 1] = static_cast<char>(NibToHex(static_cast<BYTE>(newValue->bstrVal[i]) & 0x0f));
					} else {
						r[2 + (2 * i)] = '0';
						r[2 + (2 * i) + 1] = '0';
					}
				}
				r[2 + 2 * UPB_MODULE_MAXREGISTERIO] = 0;
				TransmitNetworkPacket(FALSE, PACKETHEADER_REP_NONREPEATER, PACKETHEADER_REQ_ACKPULSE, PACKETHEADER_TRANSMITCOUNT_1, PACKETHEADER_TRANSMITSEQUENCE_1, NetworkID, ModuleID, MAKE_MDID(MDID_CORE_COMMANDS, MDID_CORE_COMMAND_SETREGISTERVALUES), r, CREATE_CMD(UniqueID, MESSAGE_TYPE_NORMAL, RESPONSE_TYPE_TRANSMITACK, MAKE_MDID(MDID_CORE_COMMANDS, MDID_CORE_COMMAND_SETREGISTERVALUES)));
				SetWriteProtect(UniqueID, NetworkID, ModuleID);
				CComVariant SetupRegisterCache;
				if (SUCCEEDED(ReadSetupRegisterCache(Object, &SetupRegisterCache))) {
					for (int i = 0; i < UPB_MODULE_MAXREGISTERIO; i++) {
						if (i < ValueLen) {
							SetupRegisterCache.bstrVal[2 * (RegisterBase + i)] = NibToHex(static_cast<BYTE>(newValue->bstrVal[i]) >> 4);
							SetupRegisterCache.bstrVal[2 * (RegisterBase + i) + 1] = NibToHex(static_cast<BYTE>(newValue->bstrVal[i]) & 0x0f);
						} else {
							SetupRegisterCache.bstrVal[2 * (RegisterBase + i)] = L'0';
							SetupRegisterCache.bstrVal[2 * (RegisterBase + i) + 1] = L'0';
						}
					}
					Object->SetValueEx(SVCC_DRIVER, XML_UPB_SETUPREGISTERCACHE, &SetupRegisterCache);
				}
			}
		}
	}
	return S_OK;
}

HRESULT STDMETHODCALLTYPE CupbController::OnNetworkNameChanged(
	IN IPremiseObject* Object,
	IN VARIANT* NewValue,
	IN VARIANT* OldValue
)
{
	return OnTextNameChanged(Object, NewValue, UPB_REG_NETWORKNAME);
}

HRESULT STDMETHODCALLTYPE CupbController::OnRoomNameChanged(
	IN IPremiseObject* Object,
	IN VARIANT* NewValue,
	IN VARIANT* OldValue
)
{
	return OnTextNameChanged(Object, NewValue, UPB_REG_ROOMNAME);
}

HRESULT STDMETHODCALLTYPE CupbController::OnDeviceNameChanged(
	IN IPremiseObject* Object,
	IN VARIANT* NewValue,
	IN VARIANT* OldValue
)
{
	return OnTextNameChanged(Object, NewValue, UPB_REG_DEVICENAME);
}

HRESULT STDMETHODCALLTYPE CupbController::OnResetChanged(
	IN IPremiseObject* Object,
	IN VARIANT* NewValue,
	IN VARIANT* OldValue
)
{
	if (NewValue->boolVal && IsDeviceInitialized(Object)) {
		USHORT Password;
		BYTE NetworkID;
		BYTE ModuleID;
		USHORT UniqueID;
		if (GetUpbPassword(Object, &Password) && SUCCEEDED(GetModuleIdentifiers(Object, NetworkID, ModuleID, UniqueID))) {
			char r[16];
			StringCchPrintfA(r, SIZEOF_ARRAY(r), "%04X", Password);
			TransmitNetworkPacket(FALSE, PACKETHEADER_REP_NONREPEATER, PACKETHEADER_REQ_ACKPULSE, PACKETHEADER_TRANSMITCOUNT_1, PACKETHEADER_TRANSMITSEQUENCE_1, NetworkID, ModuleID, MAKE_MDID(MDID_CORE_COMMANDS, MDID_CORE_COMMAND_RESETDEVICE), r, CREATE_CMD(UniqueID, MESSAGE_TYPE_NORMAL, RESPONSE_TYPE_TRANSMITACK, MAKE_MDID(MDID_CORE_COMMANDS, MDID_CORE_COMMAND_RESETDEVICE)));
		}
	}
	return S_OK;
}

HRESULT STDMETHODCALLTYPE CupbController::OnForceNetworkEnumChanged(
	IN IPremiseObject* Object,
	IN VARIANT* NewValue,
	IN VARIANT* OldValue
)
{
	if (NewValue->boolVal) {
		//
		// Ensure that there is an initialized PIM present, though it does not actually stop
		// any enumeration from happening. If the PIM needs to be initialized, it will also
		// start a network enumeration.
		//
		CComPtr<IPremiseObject> PIM;
		if (FAILED(FindInitializedPIM(&PIM))) {
			ReadPIMRegisters(INITIAL_PIM_REG_QUERY_BASE, INITIAL_PIM_REG_QUERY_BYTES);
		} else {
			StartNetworkEnumeration();
		}
	}
	return S_OK;
}

HRESULT STDMETHODCALLTYPE CupbController::OnUpbNetworkMessageChanged(
	IN IPremiseObject* Object,
	IN VARIANT* NewValue,
	IN VARIANT* OldValue
)
{
	int MessageLen = lstrlenW(NewValue->bstrVal);
	BYTE NetworkID;
	BYTE ModuleID;
	USHORT UniqueID;
	if (MessageLen && IsObjectOfExplicitType(Object, XML_UPB_SCHEMA_ROOT XML_SYS_PATH_SEPARATOR XML_UPB_MODULE) && SUCCEEDED(GetModuleIdentifiers(Object, NetworkID, ModuleID, UniqueID))) {
		//
		// Look for a custom mdid response type or cache only request.
		//
		BOOL CustomCommand = FALSE;
		if ((NewValue->bstrVal[0] == UPB_MESSAGE_CUSTOM_RESPONSE) || (NewValue->bstrVal[0] == UPB_MESSAGE_CACHE_ONLY)) {
			//
			// The length must be odd to include the leading custom response character, and must
			// be long enough to contain that plus the mdid response type, and the mdid of the
			// message itself. Or in the case of a cache request, the mdid, and register start.
			//
			if (!(MessageLen & 1) || (MessageLen < 5)) {
				SendDataToClient(Object, UPB_NETWORKDATA_NAK);
				return S_OK;
			}
			CustomCommand = TRUE;
		} else {
			//
			// The length must be even for the hex values, which also ensures that it contains
			// at least a leading mdid since zero length is checked above.
			//
			if (MessageLen & 1) {
				SendDataToClient(Object, UPB_NETWORKDATA_NAK);
				return S_OK;
			}
			CustomCommand = FALSE;
		}
		//
		// A device cannot send data until it has been initialized. This also allows old
		// messages to be flushed when re-initializing the network. The idea being that all
		// these notifications will have been processed by the time the devices are initialized
		// again. If not, then the device is initialized anyway.
		//
		CComVariant DeviceInitialized;
		if (!IsDeviceInitialized(Object)) {
			SendDataToClient(Object, UPB_NETWORKDATA_NAK);
			return S_OK;
		}
		char* TransmitMessage = new char[MessageLen + 1];
		if (!TransmitMessage) {
			SendDataToClient(Object, UPB_NETWORKDATA_NAK);
			return S_OK;
		}
		WideCharToMultiByte(CP_ACP, 0, NewValue->bstrVal, MessageLen + 1, TransmitMessage, MessageLen + 1, NULL, NULL);
 		//
		// Validate that the data is indeed a hex string.
		//
		for (int i = CustomCommand ? 1 : 0; i < MessageLen; i++) {
			if (!isHex(NewValue->bstrVal[i])) {
				delete [] TransmitMessage;
				SendDataToClient(Object, UPB_NETWORKDATA_NAK);
				return S_OK;
			}
		}
		char* ParsePoint = TransmitMessage;
		BYTE mdidResponse;
		BYTE ResponseType;
		//
		// If this message uses a custom command check the type of command.
		//
		if (CustomCommand) {
			ParsePoint++;
			MessageLen--;
			switch (NewValue->bstrVal[0]) {
			case UPB_MESSAGE_CUSTOM_RESPONSE:
				//
				// For a custom response the first hex pair is the mdid of
				// the custom response. Preload this into the mdidResponse.
				//
				ParsePoint += parsehex(ParsePoint, mdidResponse);
				MessageLen -= 2 * sizeof(mdidResponse);
				ResponseType = RESPONSE_TYPE_DATAACK;
				break;
			case UPB_MESSAGE_CACHE_ONLY:
				//
				// For cache only, there is no response type.
				//
				mdidResponse = 0;
				ResponseType = RESPONSE_TYPE_NONE;
				break;
			}
		}
		BYTE mdid;
		ParsePoint += parsehex(ParsePoint, mdid);
		//
		// Otherwise for a normal message, look for a response type.
		//
		if (!CustomCommand) {
			ResponseType = RESPONSE_TYPE_TRANSMITACK;
			mdidResponse = mdid;
			//
			// Determine the type of response from the device this message
			// should generate so that it is known whether a transmission
			// ACK fulfills the message, or if data will be returned, and
			// an ACK subsequent to that must be waited for.
			//
			for (int i = 0; i < SIZEOF_ARRAY(MessageMap); i++) {
				if (MessageMap[i].mdidCommand == mdid) {
					//
					// The cmd holds the mdid of the response for a Data Ack type of message.
					//
					mdidResponse = MessageMap[i].mdidResponse;
					ResponseType = RESPONSE_TYPE_DATAACK;
					break;
				}
			}
		}
		//
		// If this is a register write, then update the cache first.
		//
		CComVariant SetupRegisterCache;
		if ((mdid == MAKE_MDID(MDID_CORE_COMMANDS, MDID_CORE_COMMAND_SETREGISTERVALUES)) && SUCCEEDED(ReadSetupRegisterCache(Object, &SetupRegisterCache))) {
			//
			// Ensure there is actually register data to write.
			//
			BYTE RegisterStart;
			if (MessageLen <= 2 * (sizeof(mdid) + sizeof(RegisterStart))) {
				delete [] TransmitMessage;
				SendDataToClient(Object, UPB_NETWORKDATA_NAK);
				return S_OK;
			}
			ParsePoint += parsehex(ParsePoint, RegisterStart);
			MessageLen -= 2 * (sizeof(mdid) + sizeof(RegisterStart));
			//
			// Ensure the the registers written do not exceed those
			// available for the device, and that the registers handled
			// by this driver are not directly written to.
			//
			CComVariant SetupRegisterCount;
			if (FAILED(Object->GetValue(XML_UPB_SETUPREGISTERCOUNT, &SetupRegisterCount))) {
				SetupRegisterCount.bVal = UPB_REG_RESERVED1;
			}
			if (RegisterStart + MessageLen / 2 > SetupRegisterCount.bVal) {
				MessageLen = 2 * (SetupRegisterCount.bVal - RegisterStart);
			}
			if (RegisterStart < UPB_REG_RESERVED1) {
				//
				// If the entire area to write to is within the
				// reserved registers, then there is nothing to write.
				//
				if (MessageLen / 2 <= UPB_REG_RESERVED1 - RegisterStart) {
					delete [] TransmitMessage;
					SendDataToClient(Object, UPB_NETWORKDATA_NAK);
					return S_OK;
				}
				ParsePoint += 2 * (UPB_REG_RESERVED1 - RegisterStart);
				MessageLen -= 2 * (UPB_REG_RESERVED1 - RegisterStart);
				RegisterStart = UPB_REG_RESERVED1;
			}
			//
			// Skip write enable and actually writing if this is only an update of the cache.
			//
			if ((ResponseType == RESPONSE_TYPE_NONE) || SetWriteEnable(Object, UniqueID, NetworkID, ModuleID)) {
				//
				// Update the the register cache.
				//
				memcpy(&SetupRegisterCache.bstrVal[2 * static_cast<int>(RegisterStart)], &NewValue->bstrVal[ParsePoint - TransmitMessage], sizeof(WCHAR) * MessageLen);
				Object->SetValueEx(SVCC_DRIVER, XML_UPB_SETUPREGISTERCACHE, &SetupRegisterCache);
				if (ResponseType != RESPONSE_TYPE_NONE) {
					//
					// Split up long register writes into maximum packet sizes. This allows
					// the client driver to write an arbitrary number of registers, which
					// will be reduced to the minimum number of packets, including a single
					// write enable and disable.
					//
					BYTE MessageType = MESSAGE_TYPE_NORMAL;
					do {
						char RegisterData[2 + 2 * UPB_MODULE_MAXREGISTERIO + 1];
						int ChunkLen = min(MessageLen, 2 * UPB_MODULE_MAXREGISTERIO);
						StringCchPrintfA(RegisterData, SIZEOF_ARRAY(RegisterData), "%02X%*.*s", RegisterStart, ChunkLen, ChunkLen, ParsePoint);
						//
						// Pass on the ACK for the last chunk.
						//
						if (MessageLen - ChunkLen == 0) {
							MessageType |= MESSAGE_TYPE_CLIENT;
						}
						TransmitNetworkPacket(FALSE, PACKETHEADER_REP_NONREPEATER, PACKETHEADER_REQ_ACKPULSE, PACKETHEADER_TRANSMITCOUNT_1, PACKETHEADER_TRANSMITSEQUENCE_1, NetworkID, ModuleID, mdid, RegisterData, CREATE_CMD(UniqueID, MessageType, ResponseType, mdidResponse));
						ParsePoint += ChunkLen;
						MessageLen -= ChunkLen;
						RegisterStart += UPB_MODULE_MAXREGISTERIO;
					} while (MessageLen);
					SetWriteProtect(UniqueID, NetworkID, ModuleID);
				}
			}
		} else {
			TransmitNetworkPacket(FALSE, PACKETHEADER_REP_NONREPEATER, PACKETHEADER_REQ_ACKPULSE, PACKETHEADER_TRANSMITCOUNT_1, PACKETHEADER_TRANSMITSEQUENCE_1, NetworkID, ModuleID, mdid, ParsePoint, CREATE_CMD(UniqueID, MESSAGE_TYPE_CLIENT | MESSAGE_TYPE_NORMAL, ResponseType, mdidResponse));
		}
		delete [] TransmitMessage;
	}
	return S_OK;
}

HRESULT STDMETHODCALLTYPE CupbController::OnUpbLinkDataChanged(
	IN IPremiseObject* Object,
	IN VARIANT* NewValue,
	IN VARIANT* OldValue
)
{
	int MessageLen = lstrlenW(NewValue->bstrVal);
	//
	// The length must be large enough for at least the nid/did/mdid,
	// and an even length for the hex values. Note that the interface
	// requires an mdid, even though in theory it is not required in a
	// UPB message.
	//
	if ((MessageLen >= 2 * (1 + 1 + 1)) && !(MessageLen & 1) && (MessageLen <= 2 * (1 + 1 + 1 + UPB_MODULE_MAXREGISTERIO)) && IsObjectOfExplicitType(Object, XML_UPB_SCHEMA_ROOT XML_SYS_PATH_SEPARATOR XML_UPB_UPBINTERFACE)) {
		char TransmitMessage[2 * (1 + 1 + 1 + UPB_MODULE_MAXREGISTERIO) + 1];
		WideCharToMultiByte(CP_ACP, 0, NewValue->bstrVal, MessageLen + 1, TransmitMessage, sizeof(TransmitMessage), NULL, NULL);
		//
		// Validate that the data is indeed a hex string.
		//
		for (int i = 0; i < MessageLen; i++) {
			if (!isHex(NewValue->bstrVal[i])) {
				return S_OK;
			}
		}
		char* ParsePoint = TransmitMessage;
		BYTE nid;
		ParsePoint += parsehex(ParsePoint, nid);
		BYTE did;
		ParsePoint += parsehex(ParsePoint, did);
		if ((did == BROADCAST_DEVICEID) || (did >= RESERVED1_DEVICEID)) {
			return S_OK;
		}
		BYTE mdid;
		ParsePoint += parsehex(ParsePoint, mdid);
		//
		// Restrict the acceptable commands to the known link commands.
		//
		if ((mdid < MAKE_MDID(MDID_DEVICE_CONTROL_COMMANDS, MDID_DEVICE_CONTROL_COMMAND_ACTIVATELINK)) || (mdid > MAKE_MDID(MDID_DEVICE_CONTROL_COMMANDS, MDID_DEVICE_CONTROL_COMMAND_STORESTATE)) || (mdid == MAKE_MDID(MDID_DEVICE_CONTROL_COMMANDS, MDID_DEVICE_CONTROL_COMMAND_REPORTSTATE))) {
			return S_OK;
		}
		if (SUCCEEDED(TransmitNetworkPacket(TRUE, PACKETHEADER_REP_NONREPEATER, PACKETHEADER_REQ_ACKPULSE, PACKETHEADER_TRANSMITCOUNT_1, PACKETHEADER_TRANSMITSEQUENCE_1, nid, did, mdid, ParsePoint, CREATE_CMD(0, MESSAGE_TYPE_NORMAL, RESPONSE_TYPE_TRANSMITACK, mdid)))) {
			BroadcastLinkPacket(nid, BROADCAST_DEVICEID, did, mdid, ParsePoint, FALSE);
		}
	}
	return S_OK;
}

HRESULT STDMETHODCALLTYPE CupbController::OnResolveDuplicateAddressChanged(
	IN IPremiseObject* Object,
	IN VARIANT* NewValue,
	IN VARIANT* OldValue
)
{
	if (NewValue->boolVal && IsObjectOfExplicitType(Object, XML_UPB_SCHEMA_ROOT XML_SYS_PATH_SEPARATOR XML_UPB_MODULE)) {
		FindUnpopulatedNetwork(Object);
	}
	return S_OK;
}

HRESULT STDMETHODCALLTYPE CupbController::OnUpbVersionChanged(
	IN IPremiseObject* Object,
	IN VARIANT* NewValue,
	IN VARIANT* OldValue
)
{
	if ((NewValue->lVal >= UPB_VERSION_FIRST) && IsObjectOfExplicitType(Object, XML_UPB_SCHEMA_ROOT XML_SYS_PATH_SEPARATOR XML_UPB_UPBINTERFACE)) {
		m_UpbVersion = NewValue->lVal;
	}
	return S_OK;
}

/*
	Overrides IPremiseNotifyImpl::OnPropertyChanged in order to pass on notification of that
	change to the associated object if needed.

		subscriptionID - Unique identifier returned by notification subscription. Not used.
		transactionID - Unique transaction identifier. Not used.
		propagationID - Unique propagation identifier. Not used.
		controlCode - Not used.
		Object - The owner of the property that was changed.
		Property - The property that was changed.
		NewValue - The new value of the property.
		OldValue - The previous value of the property.

	Return - S_OK.
*/
HRESULT STDMETHODCALLTYPE CupbController::OnPropertyChanged(
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
	// Ignore changes that are made by this driver, and changes that have not yet been committed
	// or represent a reset on an autoreset property.
	//
	if (!(controlCode & (SVCC_DRIVER | SVCC_VOLATILE))) {
		CComVariant PropertyName;
		if (SUCCEEDED(Property->GetValue(XML_SYS_NAME, &PropertyName))) {
			if (!lstrcmpW(PropertyName.bstrVal, XML_UPB_UPBNETWORKMESSAGE)) {
				return OnUpbNetworkMessageChanged(Object, NewValue, OldValue);
			} else if (!lstrcmpW(PropertyName.bstrVal, XML_UPB_UPBLINKDATA)) {
				return OnUpbLinkDataChanged(Object, NewValue, OldValue);
			} else if (!lstrcmpW(PropertyName.bstrVal, XML_UPB_NETWORKID)) {
				return OnNetworkIdChanged(Object, NewValue, OldValue);
			} else if (!lstrcmpW(PropertyName.bstrVal, XML_UPB_MODULEID)) {
				return OnModuleIdChanged(Object, NewValue, OldValue);
			} else if (!lstrcmpW(PropertyName.bstrVal, XML_UPB_NETWORKPASSWORD)) {
				return OnNetworkPasswordChanged(Object, NewValue, OldValue);
			} else if (!lstrcmpW(PropertyName.bstrVal, XML_UPB_UPBOPTIONS)) {
				return OnUpbOptionsChanged(Object, NewValue, OldValue);
			} else if (!lstrcmpW(PropertyName.bstrVal, XML_UPB_NETWORKNAME)) {
				return OnNetworkNameChanged(Object, NewValue, OldValue);
			} else if (!lstrcmpW(PropertyName.bstrVal, XML_UPB_ROOMNAME)) {
				return OnRoomNameChanged(Object, NewValue, OldValue);
			} else if (!lstrcmpW(PropertyName.bstrVal, XML_UPB_DEVICENAME)) {
				return OnDeviceNameChanged(Object, NewValue, OldValue);
			} else if (!lstrcmpW(PropertyName.bstrVal, XML_UPB_RESET)) {
				return OnResetChanged(Object, NewValue, OldValue);
			} else if (!lstrcmpW(PropertyName.bstrVal, XML_UPB_FORCENETWORKENUM)) {
				return OnForceNetworkEnumChanged(Object, NewValue, OldValue);
			} else if (!lstrcmpW(PropertyName.bstrVal, XML_UPB_RESOLVEDUPLICATEADDRESS)) {
				return OnResolveDuplicateAddressChanged(Object, NewValue, OldValue);
			} else if (!lstrcmpW(PropertyName.bstrVal, XML_UPB_UPBVERSION)) {
				return OnUpbVersionChanged(Object, NewValue, OldValue);
			//Implemented in CPremiseBufferedPortDevice
			} else if (!lstrcmpW(PropertyName.bstrVal, XML_SYS_NETWORK)) {
				return OnNetworkChanged(Object, *NewValue);
			} else if (!lstrcmpW(PropertyName.bstrVal, XML_SYS_ENABLELOGGING)) {
				return OnLoggingChanged(Object, *NewValue);
			}
		}
	}
	return E_NOTIMPL;
}
