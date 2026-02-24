#pragma once
#include <Windows.h>
#include <optional>
#include <winsvc.h>
#include "DataType.h"

namespace winsvc {
	struct Service {
		Service(SC_HANDLE service) : service(service) {}
		Service(SC_HANDLE service, unsigned long tagId) : service(service), tagId(tagId) {}

		~Service() {
			CloseServiceHandle(service);
		}

		void changeConfig(
			std::optional<std::wstring> displayName,
			std::optional<ServiceType> serviceType,
			std::optional<StartType> startType,
			std::optional<ErrorControl> actionInCaseOfError,
			std::optional<std::wstring> quotedBinaryPathWithArgs,
			std::optional<std::wstring> loadOrderGroup,
			std::optional<std::vector<std::wstring>> dependencies,
			std::optional<std::wstring> username,
			std::optional<std::wstring> password
		) {
			std::wstring depStr;
			if (dependencies.has_value()) {
				for (const auto& d : *dependencies) {
					depStr += d;
					depStr += L'\0';
				}
				depStr += L'\0';
			}

			BOOL wasChanged = ChangeServiceConfigW(
				service,
				serviceType ? (DWORD)*serviceType : SERVICE_NO_CHANGE,
				startType ? (DWORD)*startType : SERVICE_NO_CHANGE,
				actionInCaseOfError ? (DWORD)*actionInCaseOfError : SERVICE_NO_CHANGE,
				quotedBinaryPathWithArgs ? quotedBinaryPathWithArgs->c_str() : nullptr,
				loadOrderGroup ? loadOrderGroup->c_str() : nullptr,
				&tagId,
				dependencies ? depStr.c_str() : nullptr,
				username ? username->c_str() : nullptr,
				password ? password->c_str() : nullptr,
				displayName ? displayName->c_str() : nullptr
			);

			if (!wasChanged)
				throwLastError("Failed to change service config");
		}

		void setDelayedAutoStart(bool value) {
			SERVICE_DELAYED_AUTO_START_INFO info{};
			info.fDelayedAutostart = value ? TRUE : FALSE;

			BOOL ok = ChangeServiceConfig2W(service, SERVICE_CONFIG_DELAYED_AUTO_START_INFO, &info);
			if (!ok)
				throwLastError("Failed to set delayed auto start");
		}

		void setDescription(std::wstring description) {
			SERVICE_DESCRIPTIONW desc{};
			desc.lpDescription = description.data();

			BOOL ok = ChangeServiceConfig2W(service, SERVICE_CONFIG_DESCRIPTION, &desc);
			if (!ok)
				throwLastError("Failed to set description");
		}

		void setFailureActions(
			unsigned long resetPeriod,
			std::optional<std::wstring> rebootMessage,
			std::optional<std::wstring> command,
			std::vector<FailureAction> actions
		) {
			std::vector<SC_ACTION> scActions;
			scActions.reserve(actions.size());
			for (const auto& a : actions) {
				SC_ACTION sca{};
				sca.Type = (SC_ACTION_TYPE)(int)a.type;
				sca.Delay = a.delayMs;
				scActions.push_back(sca);
			}

			SERVICE_FAILURE_ACTIONSW sfa{};
			sfa.dwResetPeriod = resetPeriod;
			sfa.lpRebootMsg = rebootMessage ? rebootMessage->data() : nullptr;
			sfa.lpCommand = command ? command->data() : nullptr;
			sfa.cActions = static_cast<DWORD>(scActions.size());
			sfa.lpsaActions = scActions.empty() ? nullptr : scActions.data();

			BOOL ok = ChangeServiceConfig2W(service, SERVICE_CONFIG_FAILURE_ACTIONS, &sfa);
			if (!ok)
				throwLastError("Failed to set failure actions");
		}

		void setShouldAlsoRunFailureActionsOnProcessReturningNonZero(bool value) {
			SERVICE_FAILURE_ACTIONS_FLAG flag{};
			flag.fFailureActionsOnNonCrashFailures = value ? TRUE : FALSE;

			BOOL ok = ChangeServiceConfig2W(service, SERVICE_CONFIG_FAILURE_ACTIONS_FLAG, &flag);
			if (!ok)
				throwLastError("Failed to set failure actions flag");
		}

		void setPreferredNode(unsigned short node, bool shouldDelete) {
			SERVICE_PREFERRED_NODE_INFO info{};
			info.usPreferredNode = node;
			info.fDelete = shouldDelete ? TRUE : FALSE;

			BOOL ok = ChangeServiceConfig2W(service, SERVICE_CONFIG_PREFERRED_NODE, &info);
			if (!ok)
				throwLastError("Failed to set preferred node");
		}

		void setPreshutdownTimeout(unsigned long milliseconds) {
			SERVICE_PRESHUTDOWN_INFO info{};
			info.dwPreshutdownTimeout = milliseconds;

			BOOL ok = ChangeServiceConfig2W(service, SERVICE_CONFIG_PRESHUTDOWN_INFO, &info);
			if (!ok)
				throwLastError("Failed to set preshutdown timeout");
		}

		void setRequiredPrivileges(std::vector<std::wstring> privileges) {
			std::wstring multiSz;
			for (const auto& p : privileges) {
				multiSz += p;
				multiSz += L'\0';
			}
			multiSz += L'\0';

			SERVICE_REQUIRED_PRIVILEGES_INFOW info{};
			info.pmszRequiredPrivileges = multiSz.data();

			BOOL ok = ChangeServiceConfig2W(service, SERVICE_CONFIG_REQUIRED_PRIVILEGES_INFO, &info);
			if (!ok)
				throwLastError("Failed to set required privileges");
		}

		void setSecurityIdentifierType(SecurityIdType type) {
			SERVICE_SID_INFO info{};
			info.dwServiceSidType = (DWORD)type;

			BOOL ok = ChangeServiceConfig2W(service, SERVICE_CONFIG_SERVICE_SID_INFO, &info);
			if (!ok)
				throwLastError("Failed to set security identifier type");
		}

		void setTriggers(std::vector<Trigger> triggers) {
			// Build flat storage for SERVICE_TRIGGER_SPECIFIC_DATA_ITEM arrays
			// and SERVICE_TRIGGER structs, then wire them together.
			struct TriggerStorage {
				std::vector<SERVICE_TRIGGER_SPECIFIC_DATA_ITEM> items;
				std::vector<std::vector<BYTE>> itemData; // backing byte buffers
			};

			std::vector<TriggerStorage> storage(triggers.size());
			std::vector<SERVICE_TRIGGER> st(triggers.size());

			for (size_t i = 0; i < triggers.size(); ++i) {
				const Trigger& t = triggers[i];
				TriggerStorage& stor = storage[i];

				for (const auto& di : t.items) {
					SERVICE_TRIGGER_SPECIFIC_DATA_ITEM item{};
					item.dwDataType = (DWORD)di.type;

					// Store raw bytes from the string data
					std::vector<BYTE> bytes(di.data.begin(), di.data.end());
					stor.itemData.push_back(std::move(bytes));
					item.cbData = static_cast<DWORD>(stor.itemData.back().size());
					item.pData = stor.itemData.back().data();
					stor.items.push_back(item);
				}

				st[i].dwTriggerType = (DWORD)t.type;
				st[i].dwAction = (DWORD)t.action;
				st[i].pTriggerSubtype = const_cast<GUID*>(&t.subtype.value);
				st[i].cDataItems = static_cast<DWORD>(stor.items.size());
				st[i].pDataItems = stor.items.empty() ? nullptr : stor.items.data();
			}

			SERVICE_TRIGGER_INFO info{};
			info.cTriggers = static_cast<DWORD>(st.size());
			info.pTriggers = st.empty() ? nullptr : st.data();
			info.pReserved = nullptr;

			BOOL ok = ChangeServiceConfig2W(service, SERVICE_CONFIG_TRIGGER_INFO, &info);
			if (!ok)
				throwLastError("Failed to set triggers");
		}

		void setLaunchProtection(ServiceLaunchProtection protection) {
			SERVICE_LAUNCH_PROTECTED_INFO info{};
			info.dwLaunchProtected = (DWORD)protection;

			BOOL ok = ChangeServiceConfig2W(service, SERVICE_CONFIG_LAUNCH_PROTECTED, &info);
			if (!ok)
				throwLastError("Failed to set launch protection");
		}

		ServiceStatus controlService(ServiceControl control) {
			SERVICE_STATUS ss{};
			BOOL ok = ControlService(service, (DWORD)control, &ss);
			if (!ok)
				throwLastError("Failed to control service");
			return fromServiceStatus(ss);
		}

		ServiceStatus controlServiceEx(ServiceControl control, DWORD infoLevel = SERVICE_CONTROL_STATUS_REASON_INFO, void* pControlParams = nullptr) {
			SERVICE_STATUS_PROCESS ssp{};
			BOOL ok = ControlServiceExW(service, (DWORD)control, infoLevel, pControlParams);
			if (!ok)
				throwLastError("Failed to control service (ex)");

			return queryServiceStatus();
		}

		void setServiceObjectSecurity(SECURITY_INFORMATION securityInformation, PSECURITY_DESCRIPTOR pSecurityDescriptor) {
			BOOL ok = SetServiceObjectSecurity(service, securityInformation, pSecurityDescriptor);
			if (!ok)
				throwLastError("Failed to set service object security");
		}

		std::vector<ENUM_SERVICE_STATUSW> enumDependentServices(DWORD serviceState = SERVICE_STATE_ALL) {
			DWORD bytesNeeded = 0;
			DWORD servicesReturned = 0;

			// First call to get required buffer size
			EnumDependentServicesW(service, serviceState, nullptr, 0, &bytesNeeded, &servicesReturned);

			if (bytesNeeded == 0)
				return {};

			std::vector<BYTE> buffer(bytesNeeded);
			BOOL ok = EnumDependentServicesW(
				service, serviceState,
				reinterpret_cast<LPENUM_SERVICE_STATUSW>(buffer.data()),
				bytesNeeded, &bytesNeeded, &servicesReturned
			);
			if (!ok)
				throwLastError("Failed to enumerate dependent services");

			auto* entries = reinterpret_cast<LPENUM_SERVICE_STATUSW>(buffer.data());
			return std::vector<ENUM_SERVICE_STATUSW>(entries, entries + servicesReturned);
		}

		ServiceStatus queryServiceStatus() {
			SERVICE_STATUS_PROCESS ssp{};
			DWORD bytesNeeded = 0;
			BOOL ok = QueryServiceStatusEx(service, SC_STATUS_PROCESS_INFO,
				reinterpret_cast<LPBYTE>(&ssp), sizeof(ssp), &bytesNeeded);
			if (!ok)
				throwLastError("Failed to query service status");

			ServiceStatus result{};
			result.serviceType = (ServiceType)ssp.dwServiceType;
			result.currentState = (ServiceState)ssp.dwCurrentState;
			result.controlsAccepted.value = (int)ssp.dwControlsAccepted;
			result.dwWin32ExitCode = ssp.dwWin32ExitCode;
			result.dwServiceSpecificExitCode = ssp.dwServiceSpecificExitCode;
			result.dwCheckPoint = ssp.dwCheckPoint;
			result.dwWaitHint = ssp.dwWaitHint;
			return result;
		}

		// Registers a handler for service control notifications.
		// The handler must be a function of type LPHANDLER_FUNCTION_EX.
		SERVICE_STATUS_HANDLE registerCtrlHandlerEx(
			const std::wstring& serviceName,
			LPHANDLER_FUNCTION_EX handler,
			LPVOID context = nullptr
		) {
			SERVICE_STATUS_HANDLE h = RegisterServiceCtrlHandlerExW(serviceName.c_str(), handler, context);
			if (!h)
				throwLastError("Failed to register service control handler");
			return h;
		}

		void Delete() {
			BOOL wasDeleted = DeleteService(service);

			if (!wasDeleted) {
				unsigned long error = GetLastError();
				throw std::exception("Unable to delete service");
			}
		}

		void start(std::vector<std::wstring> args = {}) {
			std::vector<LPCWSTR> argv;
			argv.reserve(args.size());
			for (const auto& a : args)
				argv.push_back(a.c_str());

			BOOL ok = StartServiceW(service, static_cast<DWORD>(argv.size()), argv.empty() ? nullptr : argv.data());
			if (!ok)
				throwLastError("Failed to start service");
		}

		void stop() {
			controlService(ServiceControl::stop);
		}

		ServiceConfig queryConfig() {
			DWORD bytesNeeded = 0;
			QueryServiceConfigW(service, nullptr, 0, &bytesNeeded);

			std::vector<BYTE> buffer(bytesNeeded);
			auto* config = reinterpret_cast<QUERY_SERVICE_CONFIGW*>(buffer.data());

			BOOL ok = QueryServiceConfigW(service, config, bytesNeeded, &bytesNeeded);
			if (!ok)
				throwLastError("Failed to query service config");

			return ServiceConfig{
				(ServiceType)config->dwServiceType,
				(StartType)config->dwStartType,
				(ErrorControl)config->dwErrorControl,
				config->lpBinaryPathName,
				config->lpLoadOrderGroup,
				config->dwTagId,
				config->lpDependencies,
				config->lpServiceStartName,
				config->lpDisplayName
			};
		}

		std::wstring queryDescription() {
			auto buffer = queryConfig2Raw(SERVICE_CONFIG_DESCRIPTION);
			auto* info = reinterpret_cast<SERVICE_DESCRIPTIONW*>(buffer.data());
			return info->lpDescription ? info->lpDescription : L"";
		}

		bool queryDelayedAutoStart() {
			auto buffer = queryConfig2Raw(SERVICE_CONFIG_DELAYED_AUTO_START_INFO);
			auto* info = reinterpret_cast<SERVICE_DELAYED_AUTO_START_INFO*>(buffer.data());
			return info->fDelayedAutostart != FALSE;
		}

		ServiceLaunchProtection queryLaunchProtection() {
			auto buffer = queryConfig2Raw(SERVICE_CONFIG_LAUNCH_PROTECTED);
			auto* info = reinterpret_cast<SERVICE_LAUNCH_PROTECTED_INFO*>(buffer.data());
			return (ServiceLaunchProtection)info->dwLaunchProtected;
		}

		SecurityIdType querySecurityIdentifierType() {
			auto buffer = queryConfig2Raw(SERVICE_CONFIG_SERVICE_SID_INFO);
			auto* info = reinterpret_cast<SERVICE_SID_INFO*>(buffer.data());
			return (SecurityIdType)info->dwServiceSidType;
		}

		unsigned long queryPreshutdownTimeout() {
			auto buffer = queryConfig2Raw(SERVICE_CONFIG_PRESHUTDOWN_INFO);
			auto* info = reinterpret_cast<SERVICE_PRESHUTDOWN_INFO*>(buffer.data());
			return info->dwPreshutdownTimeout;
		}

		bool queryFailureActionsOnNonCrashFailures() {
			auto buffer = queryConfig2Raw(SERVICE_CONFIG_FAILURE_ACTIONS_FLAG);
			auto* info = reinterpret_cast<SERVICE_FAILURE_ACTIONS_FLAG*>(buffer.data());
			return info->fFailureActionsOnNonCrashFailures != FALSE;
		}

		FailureActionsConfig queryFailureActions() {
			auto buffer = queryConfig2Raw(SERVICE_CONFIG_FAILURE_ACTIONS);
			auto* info = reinterpret_cast<SERVICE_FAILURE_ACTIONSW*>(buffer.data());

			FailureActionsConfig result{};
			result.resetPeriod = info->dwResetPeriod;
			result.rebootMessage = info->lpRebootMsg ? info->lpRebootMsg : L"";
			result.command = info->lpCommand ? info->lpCommand : L"";

			for (DWORD i = 0; i < info->cActions; ++i)
				result.actions.push_back({ (ActionType)info->lpsaActions[i].Type, info->lpsaActions[i].Delay });

			return result;
		}

		std::vector<std::wstring> queryRequiredPrivileges() {
			auto buffer = queryConfig2Raw(SERVICE_CONFIG_REQUIRED_PRIVILEGES_INFO);
			auto* info = reinterpret_cast<SERVICE_REQUIRED_PRIVILEGES_INFOW*>(buffer.data());

			std::vector<std::wstring> result;
			if (!info->pmszRequiredPrivileges)
				return result;

			const wchar_t* p = info->pmszRequiredPrivileges;
			while (*p) {
				result.emplace_back(p);
				p += result.back().size() + 1;
			}
			return result;
		}

		std::vector<BYTE> queryObjectSecurity(SECURITY_INFORMATION securityInformation) {
			DWORD bytesNeeded = 0;
			QueryServiceObjectSecurity(service, securityInformation, nullptr, 0, &bytesNeeded);

			std::vector<BYTE> buffer(bytesNeeded);
			BOOL ok = QueryServiceObjectSecurity(service, securityInformation,
				reinterpret_cast<PSECURITY_DESCRIPTOR>(buffer.data()), bytesNeeded, &bytesNeeded);
			if (!ok)
				throwLastError("Failed to query service object security");

			return buffer;
		}

		void setServiceStatus(SERVICE_STATUS_HANDLE handle, ServiceStatus status) {
			SERVICE_STATUS ss{};
			ss.dwServiceType = (DWORD)status.serviceType;
			ss.dwCurrentState = (DWORD)status.currentState;
			ss.dwControlsAccepted = (DWORD)status.controlsAccepted.value;
			ss.dwWin32ExitCode = status.dwWin32ExitCode;
			ss.dwServiceSpecificExitCode = status.dwServiceSpecificExitCode;
			ss.dwCheckPoint = status.dwCheckPoint;
			ss.dwWaitHint = status.dwWaitHint;

			BOOL ok = SetServiceStatus(handle, &ss);
			if (!ok)
				throwLastError("Failed to set service status");
		}

		void notifyServiceStatusChange(DWORD notifyMask, SERVICE_NOTIFYW& notifyBuffer) {
			// notifyBuffer.dwVersion must be set to SERVICE_NOTIFY_STATUS_CHANGE by the caller.
			// notifyBuffer.pfnNotifyCallback must point to a valid callback.
			DWORD result = NotifyServiceStatusChangeW(service, notifyMask, &notifyBuffer);
			if (result != ERROR_SUCCESS)
				throw std::system_error(std::error_code(static_cast<int>(result), std::system_category()),
					"Failed to register service status change notification");
		}

	private:
		[[noreturn]] static void throwLastError(const char* msg) {
			unsigned long error = GetLastError();
			throw std::system_error(
				std::error_code(static_cast<int>(error), std::system_category()),
				msg
			);
		}

		static ServiceStatus fromServiceStatus(const SERVICE_STATUS& ss) {
			ServiceStatus result{};
			result.serviceType = (ServiceType)ss.dwServiceType;
			result.currentState = (ServiceState)ss.dwCurrentState;
			result.controlsAccepted.value = (int)ss.dwControlsAccepted;
			result.dwWin32ExitCode = ss.dwWin32ExitCode;
			result.dwServiceSpecificExitCode = ss.dwServiceSpecificExitCode;
			result.dwCheckPoint = ss.dwCheckPoint;
			result.dwWaitHint = ss.dwWaitHint;
			return result;
		}

		std::vector<BYTE> queryConfig2Raw(DWORD infoLevel) {
			DWORD bytesNeeded = 0;
			QueryServiceConfig2W(service, infoLevel, nullptr, 0, &bytesNeeded);

			std::vector<BYTE> buffer(bytesNeeded);
			BOOL ok = QueryServiceConfig2W(service, infoLevel, buffer.data(), bytesNeeded, &bytesNeeded);
			if (!ok)
				throwLastError("Failed to query service config2");

			return buffer;
		}


		SC_HANDLE service;
		unsigned long tagId = 0;
	};
}