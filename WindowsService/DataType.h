#pragma once
#include <Windows.h>
#include <winsvc.h>

namespace winsvc {
	enum class DataType : unsigned long {
		binary = SERVICE_TRIGGER_DATA_TYPE_BINARY,
		string = SERVICE_TRIGGER_DATA_TYPE_STRING,
		level = SERVICE_TRIGGER_DATA_TYPE_LEVEL,
		keywordAny = SERVICE_TRIGGER_DATA_TYPE_KEYWORD_ANY,
		keywordAll = SERVICE_TRIGGER_DATA_TYPE_KEYWORD_ALL
	};

	enum class TriggerAction : unsigned long {
		startService = SERVICE_TRIGGER_ACTION_SERVICE_START,
		stopService = SERVICE_TRIGGER_ACTION_SERVICE_STOP
	};

	enum class TriggerType : unsigned long {
		deviceInterfaceArrival = SERVICE_TRIGGER_TYPE_DEVICE_INTERFACE_ARRIVAL,
		domainJoin = SERVICE_TRIGGER_TYPE_DOMAIN_JOIN,
		firewallPortEvent = SERVICE_TRIGGER_TYPE_FIREWALL_PORT_EVENT,
		groupPolicy = SERVICE_TRIGGER_TYPE_GROUP_POLICY,
		ipAddressAvailability = SERVICE_TRIGGER_TYPE_IP_ADDRESS_AVAILABILITY,
		networkEndpoint = SERVICE_TRIGGER_TYPE_NETWORK_ENDPOINT,
		custom = SERVICE_TRIGGER_TYPE_CUSTOM
	};

	enum class StartType : unsigned long {
		autoStart = SERVICE_AUTO_START,
		bootStart = SERVICE_BOOT_START,
		onDemandStart = SERVICE_DEMAND_START,
		disabled = SERVICE_DISABLED,
		systemStart = SERVICE_SYSTEM_START
	};

	enum class ServiceType : unsigned long {
		adapter = SERVICE_ADAPTER,
		filesystemDriver = SERVICE_FILE_SYSTEM_DRIVER,
		kernelDriver = SERVICE_KERNEL_DRIVER,
		recognizerDriver = SERVICE_RECOGNIZER_DRIVER,
		standaloneProcess = SERVICE_WIN32_OWN_PROCESS,
		sharedProcess = SERVICE_WIN32_SHARE_PROCESS,
		userOwnProcess = SERVICE_USER_OWN_PROCESS,
		userSharedProcess = SERVICE_USER_SHARE_PROCESS
	};

	enum class SecurityIdType : int {
		none = SERVICE_SID_TYPE_NONE,
		restricted = SERVICE_SID_TYPE_RESTRICTED,
		unrestricted = SERVICE_SID_TYPE_UNRESTRICTED
	};

	enum class ErrorControl {
		critical = SERVICE_ERROR_CRITICAL,
		severe = SERVICE_ERROR_SEVERE,
		normal = SERVICE_ERROR_NORMAL,
		ignore = SERVICE_ERROR_IGNORE
	};

	enum class ActionType : int {
		None = SC_ACTION_NONE,
		Reboot = SC_ACTION_REBOOT,
		RestartService = SC_ACTION_RESTART,
		RunCommand = SC_ACTION_RUN_COMMAND
	};

	struct AccessRights {
		AccessRights& withAllAccess() {
			value |= SC_MANAGER_ALL_ACCESS;
			return *this;
		}

		AccessRights& withCreateService() {
			value |= SC_MANAGER_CREATE_SERVICE;
			return *this;
		}

		AccessRights& withConnect() {
			value |= SC_MANAGER_CONNECT;
			return *this;
		}

		AccessRights& withEnumServices() {
			value |= SC_MANAGER_ENUMERATE_SERVICE;
			return *this;
		}

		AccessRights& withLockDb() {
			value |= SC_MANAGER_LOCK;
			return *this;
		}

		AccessRights& withModifyBootConfig() {
			value |= SC_MANAGER_MODIFY_BOOT_CONFIG;
			return *this;
		}

		AccessRights& withQueryLockStatus() {
			value |= SC_MANAGER_QUERY_LOCK_STATUS;
			return *this;
		}

		unsigned long getValue() {
			return value;
		}
	private:
		unsigned long value = 0;
	};

	struct FailureAction {
		ActionType type;
		unsigned long delayMs;
	};

	struct TriggerSubtype {
		inline static const GUID namedPipe = NAMED_PIPE_EVENT_GUID;
		inline static const GUID rpcInterface = RPC_INTERFACE_EVENT_GUID;
		inline static const GUID domainJoin = DOMAIN_JOIN_GUID;
		inline static const GUID domainLeave = DOMAIN_LEAVE_GUID;
		inline static const GUID firewallPortOpen = FIREWALL_PORT_OPEN_GUID;
		inline static const GUID firewallPortClose = FIREWALL_PORT_CLOSE_GUID;
		inline static const GUID machinePolicyChange = MACHINE_POLICY_PRESENT_GUID;
		inline static const GUID firstIpArrival = NETWORK_MANAGER_FIRST_IP_ADDRESS_ARRIVAL_GUID;
		inline static const GUID lastIpRemoval = NETWORK_MANAGER_LAST_IP_ADDRESS_REMOVAL_GUID;
		inline static const GUID userPolicyChange = USER_POLICY_PRESENT_GUID;

		TriggerSubtype(const std::wstring& guid)
		{
			HRESULT hr = CLSIDFromString(guid.c_str(), &value);

			if (FAILED(hr))
				throw std::runtime_error("Invalid GUID string");
		}

		GUID value;
	};

	struct DataItem {
		DataType type;
		std::string data;
	};

	struct Trigger {
		TriggerType type;
		TriggerAction action;
		TriggerSubtype subtype;
		std::vector<DataItem> items;
	};

	enum class ServiceLaunchProtection : int {
		none = SERVICE_LAUNCH_PROTECTED_NONE,
		windows = SERVICE_LAUNCH_PROTECTED_WINDOWS,
		windowsLight = SERVICE_LAUNCH_PROTECTED_WINDOWS_LIGHT,
		antimalwareLight = SERVICE_LAUNCH_PROTECTED_ANTIMALWARE_LIGHT
	};

	enum class ServiceControl : int {
		resume = SERVICE_CONTROL_CONTINUE,
		interrogate = SERVICE_CONTROL_INTERROGATE,
		bindNewComponent = SERVICE_CONTROL_NETBINDADD,
		disableBinding = SERVICE_CONTROL_NETBINDDISABLE,
		enableBinding = SERVICE_CONTROL_NETBINDENABLE,
		removeBinding = SERVICE_CONTROL_NETBINDREMOVE,
		changeParameter = SERVICE_CONTROL_PARAMCHANGE,
		pause = SERVICE_CONTROL_PAUSE,
		stop = SERVICE_CONTROL_STOP
	};

	enum ServiceState : int {
		stopped = SERVICE_STOPPED,
		staring = SERVICE_START_PENDING,
		stopping = SERVICE_STOP_PENDING,
		running = SERVICE_RUNNING,
		continuing = SERVICE_CONTINUE_PENDING,
		pausing = SERVICE_PAUSE_PENDING,
		paused = SERVICE_PAUSED
	};

	struct ControlsAccepted {
		ControlsAccepted& acceptNetbindChanges() {
			value |= SERVICE_ACCEPT_NETBINDCHANGE;
			return *this;
		}

		ControlsAccepted& acceptParameterChanges() {
			value |= SERVICE_ACCEPT_PARAMCHANGE;
			return *this;
		}
		ControlsAccepted& acceptPausesAndContinues() {
			value |= SERVICE_ACCEPT_PAUSE_CONTINUE;
			return *this;
		}
		ControlsAccepted& acceptPreshutdowns() {
			value |= SERVICE_ACCEPT_PRESHUTDOWN;
			return *this;
		}
		ControlsAccepted& acceptShutdowns() {
			value |= SERVICE_ACCEPT_SHUTDOWN;
			return *this;
		}

		ControlsAccepted& acceptStops() {
			value |= SERVICE_ACCEPT_STOP;
			return *this;
		}

		bool acceptsNetbindChanges() {
			return value & SERVICE_ACCEPT_NETBINDCHANGE;
		}

		bool acceptsParameterChanges() {
			return value & SERVICE_ACCEPT_PARAMCHANGE;
		}

		bool acceptsPausesAndContinues() {
			return value & SERVICE_ACCEPT_PAUSE_CONTINUE;
		}

		bool acceptsPreshutdowns() {
			return value & SERVICE_ACCEPT_PRESHUTDOWN;
		}

		bool acceptsShutdowns() {
			return value & SERVICE_ACCEPT_SHUTDOWN;
		}

		bool acceptsStops() {
			return value & SERVICE_ACCEPT_STOP;
		}

		int value;
	};
	struct ControlsAcceptedEx : ControlsAccepted {
		ControlsAccepted& acceptHardwareProfileChanges() {
			value |= SERVICE_ACCEPT_HARDWAREPROFILECHANGE;
			return *this;
		}

		ControlsAccepted& acceptPowerEvents() {
			value |= SERVICE_ACCEPT_POWEREVENT;
			return *this;
		}
		ControlsAccepted& acceptSessionStatusChanges() {
			value |= SERVICE_ACCEPT_SESSIONCHANGE;
			return *this;
		}
		ControlsAccepted& acceptTimeChanges() {
			value |= SERVICE_ACCEPT_TIMECHANGE;
			return *this;
		}
		ControlsAccepted& acceptTriggerEvents() {
			value |= SERVICE_ACCEPT_TRIGGEREVENT;
			return *this;
		}

		ControlsAccepted& acceptReboots() {
			value |= SERVICE_ACCEPT_HARDWAREPROFILECHANGE;
			return *this;
		}

		bool acceptsHardwareProfileChanges() {
			return value & SERVICE_ACCEPT_NETBINDCHANGE;
		}

		bool acceptsPowerEvents() {
			return value & SERVICE_ACCEPT_POWEREVENT;
		}

		bool acceptsSessionStatusChanges() {
			return value & SERVICE_ACCEPT_SESSIONCHANGE;
		}

		bool acceptsTimeChanges() {
			return value & SERVICE_ACCEPT_TIMECHANGE;
		}

		bool acceptsTriggerEvents() {
			return value & SERVICE_ACCEPT_TRIGGEREVENT;
		}

		bool acceptsReboots() {
			return value & SERVICE_ACCEPT_USER_LOGOFF;
		}
	};

	struct ServiceStatus {
		ServiceType serviceType;
		ServiceState currentState;
		ControlsAcceptedEx controlsAccepted;
		DWORD dwWin32ExitCode;
		DWORD dwServiceSpecificExitCode;
		DWORD dwCheckPoint;
		DWORD dwWaitHint;
	};

	struct ServiceConfig {
		ServiceType serviceType;
		StartType startType;
		ErrorControl errorControl;
		std::wstring binaryPathName;
		std::wstring loadOrderGroup;
		unsigned long tagId;
		std::wstring dependencies;
		std::wstring serviceStartName;
		std::wstring displayName;
	};

	struct FailureActionsConfig {
		unsigned long resetPeriod;
		std::wstring rebootMessage;
		std::wstring command;
		std::vector<FailureAction> actions;
	};
}