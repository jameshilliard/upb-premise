#ifndef __UPBCONTROLLER_H_
#define __UPBCONTROLLER_H_

#include "resource.h"
#include "upbCommon.h"

#define NOBREAK
#define	SIZEOF_ARRAY(a) (sizeof(a)/sizeof(a[0]))

/*
	Byte align all structures as they are used to communicate with h/w.
*/
#pragma pack(push)
#pragma pack(1)

/*
	Common register locations as defined in the UPB documentation.
*/
#define UPB_REG_NETWORKID 0x00
#define UPB_REG_MODULEID 0x01
#define UPB_REG_PASSWORD 0x02
#define UPB_REG_UPBOPTIONS 0x04
#define UPB_REG_UPBVERSION 0x05
#define UPB_REG_MANUFACTURERID 0x06
#define UPB_REG_PRODUCTID 0x08
#define UPB_REG_FIRMWAREVERSION 0x0A
#define UPB_REG_SERIALNUMBER 0x0C
#define UPB_REG_NETWORKNAME 0x10
#define UPB_REG_ROOMNAME 0x20
#define UPB_REG_DEVICENAME 0x30
#define UPB_REG_RESERVED1 0x40
#define UPB_REG_PIMOPTIONS 0x70
#define UPB_REG_RESERVED2 0x71
#define UPB_REG_SIGNALSTRENGTH 0xF9
#define UPB_REG_NOISEFLOOR 0xFA
#define UPB_REG_NOISECOUNTS 0xFB

#define UPB_REG_NETWORKID_BYTES 0x01
#define UPB_REG_MODULEID_BYTES 0x01
#define UPB_REG_PASSWORD_BYTES 0x02
#define UPB_REG_UPBOPTIONS_BYTES 0x01
#define UPB_REG_UPBVERSION_BYTES 0x01
#define UPB_REG_MANUFACTURERID_BYTES 0x02
#define UPB_REG_PRODUCTID_BYTES 0x02
#define UPB_REG_FIRMWAREVERSION_BYTES 0x02
#define UPB_REG_SERIALNUMBER_BYTES 0x04
#define UPB_REG_NETWORKNAME_BYTES 0x10
#define UPB_REG_ROOMNAME_BYTES 0x10
#define UPB_REG_DEVICENAME_BYTES 0x10
#define UPB_REG_RESERVED1_BYTES 0x30
#define UPB_REG_PIMOPTIONS_BYTES 0x01
#define UPB_REG_RESERVED2_BYTES 0x08
#define UPB_REG_SIGNALSTRENGTH_BYTES 0x01
#define UPB_REG_NOISEFLOOR_BYTES 0x01
#define UPB_REG_NOISECOUNTS_BYTES 0x05

#define UPB_REG_PIM_OPTION_NO_IDLE_PULSE_REPORTS 0x01
#define UPB_REG_PIM_OPTION_MESSAGE_MODE 0x02

/*
	Bit rate in bits/second.
*/
#define UPB_BIT_RATE 240
#define UPB_TYPICAL_PAYLOAD_BYTES 3
#define UPB_TYPICAL_MESSAGE_BYTES (1 + sizeof(UPB_PACKETHEADER) + UPB_TYPICAL_PAYLOAD_BYTES + 1)
#define UPB_TYPICAL_DELAY_MILLISECONDS (1000.0f * UPB_TYPICAL_MESSAGE_BYTES * 8 / UPB_BIT_RATE)
#define UPB_BIT_PAIR_MILLISECONDS (1000.0f * 2 / UPB_BIT_RATE)

#define UPB_PIM_ACK_TIMEOUT 5000
#define UPB_DEVICE_ACK_TIMEOUT 3000
#define UPB_MAX_DEVICE_TIMEOUTS 3

#define UPB_MESSAGE 'U'
#define UPB_PIM_ACCEPT 'A'
#define UPB_PIM_BUSY 'B'
#define UPB_PIM_ERROR 'E'
#define UPB_PIM_REGISTERS 'R'
#define UPB_TRANSMISSION_ACK 'K'
#define UPB_TRANSMISSION_NAK 'N'

#define UPB_MESSAGE_PIMREPORT 'P'
#define UPB_MESSAGE_START 'X'
#define UPB_MESSAGE_SYNC 'R'
#define UPB_MESSAGE_DATA_0 '0'
#define UPB_MESSAGE_DATA_1 '1'
#define UPB_MESSAGE_DATA_2 '2'
#define UPB_MESSAGE_DATA_3 '3'
#define UPB_MESSAGE_ACK 'A'
#define UPB_MESSAGE_NAK 'N'
#define UPB_MESSAGE_DROP 'D'
#define UPB_MESSAGE_IDLE '-'
#define UPB_MESSAGE_TRANSMITTED 'T'

#define UPB_NETWORK_TRANSMIT 0x14
#define UPB_PIM_READ 0x12
#define UPB_PIM_WRITE 0x17

#define INVALID_UNIQUE_ID 0
#define LARGEST_UNIQUE_ID 0xffff

#define INITIAL_PIM_REG_QUERY_BASE UPB_REG_UPBOPTIONS
#define INITIAL_PIM_REG_QUERY_BYTES sizeof(INITIAL_PIM_REG_REPORT)
/*
	The structure that maps to the set of registers initially requested
	from the PIM. The initial query starts at
	INITIAL_PIM_REG_QUERY_BASE, and is INITIAL_PIM_REG_QUERY_BYTES
	bytes in length. The PIM does not use the Network ID, Module ID, or
	Password registers, so they are ignored.

	UpbOptions - The UPB_REG_UPBOPTIONS register. This contains the
		Idle Pulse Reports, and Message Mode options as defined in the
		UPB documentation.
	UpbVersion - The UPB_REG_UPBVERSION register. UPB version
		compatibility.
	ManufacturerID - The UPB_REG_MANUFACTURERID register. The
		manufacturer is used to locate a driver that supports this
		device.
	ProductID - The UPB_REG_PRODUCTID register. The product is used to
		scan the supported modules within a manufacturer's driver.
	FirmwareVersion - The UPB_REG_FIRMWAREVERSION register. The
		firmware version is used to narrow down which product version
		to use within a manufacturer's driver.
*/
typedef struct {
	BYTE UpbOptions;
	BYTE UpbVersion;
	USHORT ManufacturerID;
	USHORT ProductID;
	USHORT FirmwareVersion;
} INITIAL_PIM_REG_REPORT;
#define INITIAL_MODULE_REG_QUERY_BASE UPB_REG_NETWORKID
#define INITIAL_MODULE_REG_QUERY_BYTES sizeof(INITIAL_MODULE_REG_REPORT)
typedef struct {
	BYTE NetworkID;
	BYTE ModuleID;
	USHORT Password;
	BYTE UpbOptions;
	BYTE UpbVersion;
	USHORT ManufacturerID;
	USHORT ProductID;
	USHORT FirmwareVersion;
	BYTE SerialNumber[4];
} INITIAL_MODULE_REG_REPORT;
typedef struct {
	USHORT Identifier;
	BYTE SignalStrength;
	BYTE NoiseLevel;
	USHORT upbChecksum;
	USHORT SetupChecksum;
	BYTE SetupRegisterCount;
	BYTE Diagnostics[8];
} DEVICE_SIGNATURE_REPORT;
typedef struct {
	BYTE RamRegister;
	BYTE TimeoutSeconds;
} DEVICE_SETUPTIME_REPORT;

#define PIM_REGISTER_READ_BYTES 8

#define PULSE_REPORT_BYTES 3

#define UPB_MESSAGE_TYPE 0
#define UPB_MESSAGE_PIMREPORT_TYPE 1
#define PULSE_MODE_ENERGY 1
#define PULSE_MODE_SEQUENCE 2

#define UPB_MESSAGE_TERMINATOR '\r'

typedef enum {
	MessageTransmitState_None,
	MessageTransmitState_PimAccept,
	MessageTransmitState_Transmitted
} MessageTransmitStates;

#define MESSAGE_TYPE_NORMAL 0x00
#define MESSAGE_TYPE_QUERYMODULEBASEREGISTERS 0x01
#define MESSAGE_TYPE_QUERYNEWMODULESIGNATURE 0x02
#define MESSAGE_TYPE_QUERYMODULESIGNATURE 0x03
#define MESSAGE_TYPE_TRYPASSWORD 0x04
#define MESSAGE_TYPE_GUESSPASSWORD 0x05
#define MESSAGE_TYPE_NETWORKMODULEENUM 0x06
#define MESSAGE_TYPE_NETWORKIDENUM 0x07
#define MESSAGE_TYPE_PIMREAD 0x08
#define MESSAGE_TYPE_PIMWRITE 0x09

#define MESSAGE_TYPE_CLIENT 0x10
#define FILTER_CLIENT_FLAG(MessageType) ((MessageType) & 0x0f)

#define RESPONSE_TYPE_NONE 0x00
#define RESPONSE_TYPE_PIMACCEPT 0x20
#define RESPONSE_TYPE_PIMDATAACK 0x40
#define RESPONSE_TYPE_TRANSMITACK 0x60
#define RESPONSE_TYPE_ACK 0x80
#define RESPONSE_TYPE_DATAACK 0xa0

#define CREATE_CMD(UniqueID, MessageType, ResponseType, mdid) (((UniqueID) << 16) | (((MessageType) | (ResponseType)) << 8) | mdid)
#define UNIQUEID_FROM_CMD(cmd) static_cast<USHORT>((cmd) >> 16)
#define MESSAGETYPE_FROM_CMD(cmd) (((cmd) >> 8) & 0x1f)
#define RESPONSETYPE_FROM_CMD(cmd) (((cmd) >> 8) & 0xe0)
#define MDID_FROM_CMD(cmd) ((cmd) & 0xff)

#define UPB_HISTORYQUEUEITEMS 32

#define	UPB_PULSEENERGYCOUNTMAX 32

#define PACKETHEADER_REP_NONREPEATER 0
#define PACKETHEADER_REP_LOWREPEAT 1
#define PACKETHEADER_REP_MEDIUMREPEAT 2
#define PACKETHEADER_REP_HIGHREPEAT 3

#define PACKETHEADER_REQ_ACKPULSE 1
#define PACKETHEADER_REQ_IDPULSE 2
#define PACKETHEADER_REQ_ACKMESSAGE 4

#define PACKETHEADER_TRANSMITCOUNT_1 0
#define PACKETHEADER_TRANSMITCOUNT_2 1
#define PACKETHEADER_TRANSMITCOUNT_3 2
#define PACKETHEADER_TRANSMITCOUNT_4 3

#define PACKETHEADER_TRANSMITSEQUENCE_1 0
#define PACKETHEADER_TRANSMITSEQUENCE_2 1
#define PACKETHEADER_TRANSMITSEQUENCE_3 2
#define PACKETHEADER_TRANSMITSEQUENCE_4 3

#define PACKETHEADER_LINKBIT 0x80
//
// Link Bit : 1
// Repeater Request : 2
// Packet Length : 5
// Reserved : 1
// Acknowledgement Request : 3
// Transmit Count : 2
// Transmit Sequence : 2
//
typedef struct {
	BYTE ControlWord[2];
	BYTE NetworkID;
	BYTE DestinationID;
	BYTE SourceID;
} UPB_PACKETHEADER;

#define PACKETDATALEN(PacketHeader) (((PacketHeader)->ControlWord[0] & 0x1f) - sizeof(UPB_PACKETHEADER))

#define NETWORKIDLE_FREQUENCY 10000

typedef enum {
	DuplicateAddressMode_None = 0,
	DuplicateAddressMode_FlushQueue = 1,
	DuplicateAddressMode_QueryUnpopulatedNetwork = 2,
	DuplicateAddressMode_CreateMap = 3,
	DuplicateAddressMode_MoveModules = 4
} DuplicateAddressMode;

#define DEFAULT_PASSWORD_TRY -1L

#define DEFAULT_NETWORK_ENUMERATION -1L

#define UNINITIALIZED_CHECKSUM_DIFFERENCE -1L

#define UPB_VERSION_FIRST 1
#define UPB_VERSION_QUICKNETWORKENUM 2
#define UPB_VERSION_DEFAULT UPB_VERSION_FIRST

/*
UPB_NETWORK_MAXADDRESSES - The maximum number of device slots on a network, which
	is the default address identifier plus one, as addresses are zero-based.
UPB_NETWORK_MAXNETWORKS - The maximum number of network slots, which
	is the default network identifier plus one, as addresses are zero-based.
*/
#define UPB_NETWORK_MAXADDRESSES (DEFAULT_DEVICEID + 1)
#define UPB_NETWORK_MAXNETWORKS (DEFAULT_NETWORKID + 1)

#pragma pack(pop) /* pack(1) */

#define XML_UPB_UNKNOWNPIM L"upbUnknownPIM"
#define XML_UPB_UNKNOWNMODULE L"upbUnknownModule"

#define XML_UPB_UPBMANUFACTURERID L"upbManufacturerID"
#define XML_UPB_UPBPRODUCTIDS L"upbProductIDs"

#define XML_UPB_NETWORKPASSWORD L"NetworkPassword"
#define XML_UPB_UPBOPTIONS L"UpbOptions"
#define XML_UPB_UPBVERSION L"UpbVersion"
#define XML_UPB_PULSESTRENGTH L"PulseStrength"
#define XML_UPB_UNKNOWN L"Unknown"
#define XML_UPB_FIRMWAREVERSION L"FirmwareVersion"
#define XML_UPB_PROBEPASSWORD L"ProbePassword"
#define XML_UPB_NETWORKNAME L"NetworkName"
#define XML_UPB_ROOMNAME L"RoomName"
#define XML_UPB_DEVICENAME L"DeviceName"
#define XML_UPB_RESET L"Reset"
#define XML_UPB_FORCENETWORKENUM L"ForceNetworkEnum"
#define XML_UPB_UPBCHECKSUM L"upbChecksum"
#define XML_UPB_SETUPCHECKSUM L"SetupChecksum"
#define XML_UPB_UPBCHECKSUMDIFFERENCE L"upbChecksumDifference"
#define XML_UPB_PASSWORDPROBE L"PasswordProbe"
#define XML_UPB_UPBUNIQUEID L"upbUniqueID"
#define XML_UPB_NEXTUNIQUEID L"NextUniqueID"
#define XML_UPB_NETWORKENUMID L"NetworkEnumID"
#define XML_UPB_SETUPREGISTERCOUNT L"SetupRegisterCount"
#define XML_UPB_POLLNETWORK L"PollNetwork"
#define XML_UPB_MANUFACTURERNAME L"ManufacturerName"
#define XML_UPB_PRODUCTNAME L"ProductName"
#define XML_UPB_RESOLVEDUPLICATEADDRESS L"ResolveDuplicateAddress"
#define XML_UPB_LASTPASSWORDSET L"LastPasswordSet"
#define XML_UPB_COMPONENT L"Component"
#define XML_UPB_LINKNAME L"LinkName"

#define XML_SYS_DESCRIPTION L"Description"
#define XML_SYS_DISPLAYNAME L"DisplayName"
#define XML_SYS_NETWORK L"Network"
#define XML_SYS_ENABLELOGGING L"EnableLogging"

// CupbController
class ATL_NO_VTABLE CupbController : 
	public CComObjectRootEx<CComMultiThreadModel>,
	public CComCoClass<CupbController>,
	public CPremiseBufferedPortDevice
{
private:
/*
	m_IdleTickCount - Keeps track of the last time the network had inbound or outbound traffic.
		This is used to determine if a module should be allowed to poll its associated device
		for current status.
	m_IdleEnumCount - Keeps track of which module was last allowed to poll its associated
		device's current status.
	m_UpbVersion - The current UPB Version that network communications will conform to.
		Although the value is acquired from the XML_UPB_UPBVERSION property, it is limited to
		the UPB Version reported by the PIM.
	m_PulseData - The current pulse data received from the network. This is used to build an
		incoming network packet.
	m_PulseDataPos - The current position in the m_PulseData array.
	m_DeviceTimeouts - Tracks the number of times the device being communicated with has timed
		out so that retries are limited.
	m_BufferedWriteDelay - Tracks any delay that an outbound message should first create before
		transmitting. This is based on the number of pulse data packets received when in the
		middle of an inbound message. That in itself is based on a proposed average size of
		messages that would be broadcast by a device.
	m_FirstPasswordUniqueID - Keeps track of which module is attempting to discover its
		associated device's password, else it is set to INVALID_UNIQUE_ID. This serializes
		password discovery on modules.
	m_PulseEnergy - Current summed pulse energy, which is divided to come up with an average
		pulse strength over time for the XML_UPB_PULSESTRENGTH property.
	m_NetworkModuleEnumCount - Contains the current count of outstanding ACK/NAKs that are to be
		expected from enumeration of a network identifier, or zero when none have been received
		yet. This is initially set to UPB_NETWORK_MAXADDRESSES, and decremented for each
		ACK/NAK so that the end of the series is known, and so that it is known which module
		address a specific ACK/NAK represents.
	m_NetworkIDAckCount - Contains the current count of outstanding ACK/NAKs that are to be
		expected from enumeration of all networks when complying to UPB Version
		UPB_VERSION_QUICKNETWORKENUM or higher. This is initially set to
		UPB_NETWORK_MAXNETWORKS, and decremented for each ACK/NAK so that the end of the series
		is known, and so that it is known which network address a specific ACK/NAK represents.
	m_NetworkIDEnumCount - When enumerating all network identifiers using the UPB Version
		UPB_VERSION_FIRST method, this tracks the current network being enumerated so that
		enumeration can be interleaved with device discovery, such that devices start appearing
		as soon as possible. It also allows the enumeration to be easily interrupted.
	m_PulseEnergyCount - The count of the number of packets that have reported a pulse energy
		so that the XML_UPB_PULSESTRENGTH property can be updated.
	m_PulseDataSequence - Contains the next expected sequence number in the inbound pulse data.
	m_PulseDataBitPos - Contains the last bit positions filled in the inbound pulse data.
	m_HistoryQueue - Contains a short history of inbound packets so that duplicates can be
		ignored.
	m_HistoryQueueOldest - Current oldest packet that should be dropped off the history queue.
	m_CurrentNetworkID - The network identifier of the current outbound message, if any.
	m_CurrentSourceID - The module identifier of the current outbound message, if any.
	m_MessageTransmitState - The current state of transmitting a message. This is tracked so
		that failures can be handled based on suspected problems with PIM communication, or
		device communication.
	m_UnpopulatedNetworkID - Contains the unpopulated network identifier found when attempting
		to resolve module identifier conflicts.
	m_DuplicateNetworkID - Contains the network identifier on which the potential addressing
		conflict is occurring.
	m_DuplicateSourceID - Contains the module identifier on which the potential addressing
		conflict is occurring.
	m_DuplicateUniqueID - Contains the unique identifier for the module that has a potential
		addressing conflict.
	m_dam - Contains the duplicate addressing resolution mode.
	m_NetworkMap - Contains a bitmap of the m_DuplicateNetworkID specifying populated module
		addresses.
	m_UnpopulatedNetworkMap - Contains a bitmap of the m_UnpopulatedNetworkID specifying newly
		populated module addresses after modules have been told to re-address themselves.
*/
	ULONG m_IdleTickCount;
	long m_IdleEnumCount;
	long m_UpbVersion;
	BYTE m_PulseData[32];
	int m_PulseDataPos;
	int m_DeviceTimeouts;
	float m_BufferedWriteDelay;
	USHORT m_FirstPasswordUniqueID;
	USHORT m_PulseEnergy;
	USHORT m_NetworkModuleEnumCount;
	USHORT m_NetworkIDAckCount;
	BYTE m_NetworkIDEnumCount;
	BYTE m_PulseEnergyCount;
	BYTE m_PulseDataSequence;
	BYTE m_PulseDataBitPos;
	UPB_PACKETHEADER m_HistoryQueue[UPB_HISTORYQUEUEITEMS];
	int m_HistoryQueueOldest;
	BYTE m_CurrentNetworkID;
	BYTE m_CurrentSourceID;
	MessageTransmitStates m_MessageTransmitState;
	BYTE m_UnpopulatedNetworkID;
	BYTE m_DuplicateNetworkID;
	BYTE m_DuplicateSourceID;
	USHORT m_DuplicateUniqueID;
	DuplicateAddressMode m_dam;
	BYTE m_NetworkMap[UPB_NETWORK_MAXADDRESSES/8];
	BYTE m_UnpopulatedNetworkMap[UPB_NETWORK_MAXADDRESSES/8];

	void SetUpbPassword(IN IPremiseObject* Module, IN USHORT Password);
	void ResetAddressResolution();
	void ResetAllInitializedStates();
	HRESULT FindInitializedUpbModule(IN BYTE NetworkID, IN BYTE ModuleID, OUT IPremiseObject** Module);
	HRESULT FindInitializedUpbModule(IN USHORT UniqueID, OUT IPremiseObject** Module);
	HRESULT FindInitializedPIM(OUT IPremiseObject** PIM);
	HRESULT FindUpbModule(IN INITIAL_MODULE_REG_REPORT* Report, OUT IPremiseObject** Module);
	HRESULT FindUpbModule(IN BYTE NetworkID, IN BYTE ModuleID, OUT IPremiseObject** Module);
	HRESULT FindUpbModule(IN USHORT UniqueID, OUT IPremiseObject** Module);
	HRESULT LocateUpbManufacturer(IN USHORT ManufacturerID, IN USHORT ProductID, IN USHORT FirmwareVersion, OUT BSTR* SchemaPath);
	HRESULT LocateUpbManufacturerName(IN IPremiseObject* Module, IN USHORT ManufacturerID);
	HRESULT TransmitNetworkPacket(IN BOOL LinkBit, IN BYTE RepeaterRequest, IN BYTE AckRequest, IN BYTE TransmitCount, IN BYTE TransmitSequence, IN BYTE NetworkID, IN BYTE DestinationID, IN BYTE MDID, IN LPCSTR Data OPTIONAL, IN UINT_PTR cmd OPTIONAL);
	HRESULT ReadModuleRegisters(IN BYTE NetworkID, IN BYTE ModuleID, IN BYTE RegisterStart, IN BYTE Registers, IN USHORT UniqueID, IN BYTE MessageType);
	HRESULT ReadPIMRegisters(IN BYTE RegisterStart, IN BYTE Registers, IN USHORT UniqueID = INVALID_UNIQUE_ID OPTIONAL);
	HRESULT WritePIMRegisters(IN LPCSTR Registers, IN USHORT UniqueID);
	void PulseDataReset();
	void PulseDataAdd(IN LPCSTR DataLine);
	inline const BYTE* PulseDataGet();
	HRESULT TryUpbPassword(IN IPremiseObject* Module, IN ULONG FirstTryPassword = DEFAULT_PASSWORD_TRY OPTIONAL);
	void QueueNextPasswordTry(IN ULONG FirstTryPassword = DEFAULT_PASSWORD_TRY OPTIONAL);
	void InitPhaseQueryModuleSignature(IN const UPB_PACKETHEADER* PacketHeader, IN const BYTE* RegisterData, IN USHORT UniqueID, IN BYTE MessageType);
	void StartNetworkEnumeration();
	void InitPhaseQueryModuleBaseRegisters(IN const BYTE* RegisterData, IN USHORT UniqueID);
	void HandleTryPassword(IN const BYTE* MessageData, IN USHORT UniqueID, IN BYTE TryType);
	HRESULT ValidateUpbDataMessage(const UPB_PACKETHEADER* PacketHeader);
	void SendDataToClient(IN IPremiseObject* Module, IN BYTE DataType, IN const BYTE* MessageData = NULL OPTIONAL, IN int MessageLength = 0 OPTIONAL);
	void BroadcastLinkPacket(IN BYTE nid, IN BYTE sid, IN BYTE did, IN BYTE mdid, IN char* LinkPacket, IN BOOL SendToPIM);
	void SetCurrentPacketDestination(IN BYTE NetworkID = GLOBAL_NETWORKID OPTIONAL, IN BYTE ModuleID = BROADCAST_DEVICEID OPTIONAL);
	BOOL CheckCurrentPacketDestination(IN BYTE NetworkID, IN BYTE ModuleID);
	void FindUnpopulatedNetwork(IN IPremiseObject* Module);
	void MoveModules(IN BYTE NetworkID, IN BOOL AckReceived);
	void SetNetworkMapBit(IN BYTE ModuleID);
	void EnumerateUnpopulatedNetwork();
	void SetUnpopulatedNetworkMapBit(IN BYTE ModuleID);
	void SetNewModuleAddresses();
	void HandleUpbDataMessage(IN UINT_PTR cmd);
	void HandleUpbAckMessage(IN BOOL AckReceived, IN UINT_PTR cmd);
	HRESULT GenerateUniqueID(OUT CComVariant* UniqueID);
	HRESULT LocatePimDevice(IN const INITIAL_PIM_REG_REPORT* Report, OUT IPremiseObject** PIM);
	HRESULT CreateOrReplaceUpbObject(IN IPremiseObject* CurrentObject, IN USHORT ManufacturerID, IN USHORT ProductID, IN USHORT FirmwareVersion, IN BSTR DefaultSchema, OUT IPremiseObject** NewObject);
	HRESULT SetupPimDevice(IN const INITIAL_PIM_REG_REPORT* Report, OUT IPremiseObject** PIM);
	void InitPhaseQueryPimSettings(IN LPCSTR RegisterData);
	void HandlePimRegisters(IN LPCSTR RegisterData, IN USHORT UniqueID);
	void UpdatePulseEnergy(IN char PulseEnergy, IN UINT_PTR cmd OPTIONAL);
	BOOL StartSetupMode(IN USHORT UniqueID, IN BYTE NetworkID, IN BYTE ModuleID);
	void StopSetupMode(IN USHORT UniqueID, IN BYTE NetworkID, IN BYTE ModuleID);
	BOOL SetWriteEnable(IN IPremiseObject* Object, IN USHORT UniqueID, IN BYTE NetworkID, IN BYTE ModuleID);
	void SetWriteProtect(IN USHORT UniqueID, IN BYTE NetworkID, IN BYTE ModuleID);
	HRESULT OnTextNameChanged(IN IPremiseObject* Object, IN const VARIANT* newValue, IN BYTE RegisterBase);
	void InitializeData();

public:
	DECLARE_NO_REGISTRY()
	DECLARE_NOT_AGGREGATABLE(CupbController)
	DECLARE_PROTECT_FINAL_CONSTRUCT()

BEGIN_COM_MAP(CupbController)
	COM_INTERFACE_ENTRY(IPremiseNotify)
	COM_INTERFACE_ENTRY(IObjectWithSite)
	COM_INTERFACE_ENTRY(IPremisePortCallback)
END_COM_MAP()

	CupbController() :
		m_PulseEnergy(0),
		m_PulseEnergyCount(0),
		m_FirstPasswordUniqueID(INVALID_UNIQUE_ID)
	{
	}

	// CPremiseSubscriber overrides
	HRESULT OnBrokerDetach();
	HRESULT OnBrokerAttach();

	// IPremiseNotifyImpl overrides
	HRESULT STDMETHODCALLTYPE OnPropertyChanged(IN long subscriptionID, IN long transactionID, IN long propagationID, IN long controlCode, IN IPremiseObject* Object, IN IPremiseObject* Property, IN VARIANT* NewValue, IN VARIANT* OldValue);

	//CPremiseBufferedPortDevice pure virtuals/overrides
	HRESULT OnConfigurePort(IN IPremiseObject* Port);
	void ProcessLine(IN LPCSTR psz);
	HRESULT OnDeviceState(IN DEVICE_STATE ps);
	bool OnPing();
	void OnHeartbeat();
	bool OnAckTimeout();
	void OnBufferedWrite(IN _command& cmd);

	HRESULT STDMETHODCALLTYPE OnNetworkPasswordChanged(IN IPremiseObject* Object, IN VARIANT* NewValue, IN VARIANT* OldValue);
	HRESULT STDMETHODCALLTYPE OnUpbLinkDataChanged(IN IPremiseObject* Object, IN VARIANT* NewValue, IN VARIANT* OldValue);
	HRESULT STDMETHODCALLTYPE OnUpbOptionsChanged(IN IPremiseObject* Object, IN VARIANT* NewValue, IN VARIANT* OldValue);
	HRESULT STDMETHODCALLTYPE OnNetworkIdChanged(IN IPremiseObject* Object, IN VARIANT* NewValue, IN VARIANT* OldValue);
	HRESULT STDMETHODCALLTYPE OnModuleIdChanged(IN IPremiseObject* Object, IN VARIANT* NewValue, IN VARIANT* OldValue);
	HRESULT STDMETHODCALLTYPE OnUpbNetworkMessageChanged(IN IPremiseObject* Object, IN VARIANT* NewValue, IN VARIANT* OldValue);
	HRESULT STDMETHODCALLTYPE OnNetworkNameChanged(IN IPremiseObject* Object, IN VARIANT* NewValue, IN VARIANT* OldValue);
	HRESULT STDMETHODCALLTYPE OnRoomNameChanged(IN IPremiseObject* Object, IN VARIANT* NewValue, IN VARIANT* OldValue);
	HRESULT STDMETHODCALLTYPE OnDeviceNameChanged(IN IPremiseObject* Object, IN VARIANT* NewValue, IN VARIANT* OldValue);
	HRESULT STDMETHODCALLTYPE OnResetChanged(IN IPremiseObject* Object, IN VARIANT* NewValue, IN VARIANT* OldValue);
	HRESULT STDMETHODCALLTYPE OnForceNetworkEnumChanged(IN IPremiseObject* Object, IN VARIANT* NewValue, IN VARIANT* OldValue);
	HRESULT STDMETHODCALLTYPE OnResolveDuplicateAddressChanged(IN IPremiseObject* Object, IN VARIANT* NewValue, IN VARIANT* OldValue);
	HRESULT STDMETHODCALLTYPE OnUpbVersionChanged(IN IPremiseObject* Object, IN VARIANT* NewValue, IN VARIANT* OldValue);
};

#endif //__UPBCONTROLLER_H_
