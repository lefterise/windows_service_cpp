#pragma once
#include <Windows.h>
#include <optional>
#include <winsvc.h>
#include <string>
#include <vector>
#include <sstream>
#include "Service.h"
//https://learn.microsoft.com/en-us/windows/win32/Services/services

namespace winsvc {
	struct ServiceControlManager {
		ServiceControlManager(std::wstring machineName, std::wstring databaseName, ManagerAccessRights desiredAccess)
			: controlManager(OpenSCManagerW(machineName.c_str(), databaseName.c_str(), desiredAccess.getValue()))
		{
			if (controlManager == nullptr) {
				unsigned long error = GetLastError();
				throw std::system_error(
					std::error_code(static_cast<int>(error), std::system_category()),
					"Unable to open service control manager"
				);
			}
		}

		ServiceControlManager(ManagerAccessRights desiredAccess)
			: controlManager(OpenSCManagerW(nullptr, nullptr, desiredAccess.getValue()))
		{
			if (controlManager == nullptr) {
				unsigned long error = GetLastError();
				throw std::system_error(
					std::error_code(static_cast<int>(error), std::system_category()),
					"Unable to open service control manager"
				);
			}
		}

		Service createService(
			std::wstring serviceName,
			std::optional<std::wstring> displayName,
			ServiceAccessRights desiredAccess,
			ServiceType serviceType,
			StartType startType,
			ErrorControl actionInCaseOfError,
			std::optional<std::wstring> quotedBinaryPathWithArgs,
			std::optional<std::wstring> loadOrderGroup,
			std::vector<std::wstring> dependencies,
			std::optional<std::wstring> username,
			std::optional<std::wstring> password) {

			if (serviceName.find(L'\\') != std::string::npos || serviceName.find(L'/') != std::string::npos) {
				throw std::exception("Slashes not allowed in service name");
			}

			if ((startType == StartType::bootStart || startType == StartType::systemStart)
				&& !(serviceType == ServiceType::filesystemDriver || serviceType == ServiceType::kernelDriver || serviceType == ServiceType::recognizerDriver)) {

				throw std::exception("Start type not compatible with this service type");
			}
			std::wstringstream dep;
			for (const auto& d : dependencies) {
				dep << d << L'\0';
			}
			dep << L'\0';

			unsigned long tagId;
			SC_HANDLE service = CreateServiceW(
				controlManager,
				serviceName.c_str(),
				displayName ? displayName->c_str() : nullptr,
				desiredAccess.getValue(),
				(unsigned long)serviceType,
				(unsigned long)startType,
				(unsigned long)actionInCaseOfError,
				quotedBinaryPathWithArgs ? quotedBinaryPathWithArgs->c_str() : nullptr,
				loadOrderGroup ? loadOrderGroup->c_str() : nullptr,
				&tagId,
				!dependencies.empty() ? dep.str().c_str() : nullptr,
				username ? username->c_str() : nullptr,
				password ? password->c_str() : nullptr
			);

			if (service == nullptr) {
				unsigned long error = GetLastError();
				throw std::system_error(
					std::error_code(static_cast<int>(error), std::system_category()),
					"Unable to create service"
				);
			}

			return Service(service, tagId);
		}

		std::optional<Service> openService(std::wstring serviceName, ServiceAccessRights desiredAccess) {
			SC_HANDLE service = OpenServiceW(
				controlManager,
				serviceName.c_str(),
				desiredAccess.getValue()
			);

			if (service == nullptr) {
				unsigned long error = GetLastError();
				
				if (error == ERROR_SERVICE_DOES_NOT_EXIST) {
					return std::nullopt;
				}

				throw std::system_error(
					std::error_code(static_cast<int>(error), std::system_category()),
					"Unable to open service"
				);
			}

			return Service(service);
		}

		std::wstring getServiceKeyName(std::wstring displayName) {
			DWORD charsNeeded = 0;
			GetServiceKeyNameW(controlManager, displayName.c_str(), nullptr, &charsNeeded);
			++charsNeeded;
			std::wstring serviceKeyName;
			serviceKeyName.resize(charsNeeded);
			BOOL success = GetServiceKeyNameW(controlManager, displayName.c_str(), serviceKeyName.data(), &charsNeeded);
			if (!success) {
				unsigned long error = GetLastError();
				throw std::system_error(
					std::error_code(static_cast<int>(error), std::system_category()),
					"Unable to get service name"
				);
			}
			serviceKeyName.resize(charsNeeded);
			return serviceKeyName;
		}

		std::wstring getServiceDisplayName(std::wstring keyName) {
			DWORD charsNeeded = 0;
			GetServiceDisplayNameW(controlManager, keyName.c_str(), nullptr, &charsNeeded);
			++charsNeeded;
			std::wstring serviceDisplayName;
			serviceDisplayName.resize(charsNeeded);
			BOOL success = GetServiceDisplayNameW(controlManager, keyName.c_str(), serviceDisplayName.data(), &charsNeeded);
			if (!success) {
				unsigned long error = GetLastError();
				throw std::system_error(
					std::error_code(static_cast<int>(error), std::system_category()),
					"Unable to get service name"
				);
			}
			serviceDisplayName.resize(charsNeeded);
			return serviceDisplayName;
		}

		std::vector<ServiceItem> getAllServices() {
			std::vector<ServiceItem> services;

			DWORD bytesNeeded = 0;
			DWORD servicesReturned = 0;
			DWORD resumeHandle = 0;

			//Get required buffer size
			EnumServicesStatusExW(
				controlManager,
				SC_ENUM_PROCESS_INFO,
				SERVICE_WIN32,
				SERVICE_STATE_ALL,
				nullptr,
				0,
				&bytesNeeded,
				&servicesReturned,
				&resumeHandle,
				nullptr);

			unsigned long error = GetLastError();
			if (error != ERROR_MORE_DATA)
			{
				throw std::system_error(
					std::error_code(static_cast<int>(error), std::system_category()),
					"Unable to enumerate services"
				);
			}

			std::vector<BYTE> buffer(bytesNeeded);

			//note this may need to run in a loop making use of the resume handle if more data is returned
			if (!EnumServicesStatusExW(
				controlManager,
				SC_ENUM_PROCESS_INFO,
				SERVICE_WIN32,
				SERVICE_STATE_ALL,
				buffer.data(),
				bytesNeeded,
				&bytesNeeded,
				&servicesReturned,
				&resumeHandle,
				nullptr))
			{
				unsigned long error = GetLastError();
				throw std::system_error(
					std::error_code(static_cast<int>(error), std::system_category()),
					"Unable to enumerate services"
				);
			}

			auto s = reinterpret_cast<ENUM_SERVICE_STATUS_PROCESSW*>(buffer.data());
			services.reserve(servicesReturned);
			for (DWORD i = 0; i < servicesReturned; ++i)
			{
				const auto& svc = s[i];
				const auto& ssp = svc.ServiceStatusProcess;

				services.push_back(
					{
						svc.lpServiceName,
						svc.lpDisplayName,
						ServiceStatus{
							.serviceType = (ServiceType)ssp.dwServiceType,
							.currentState = (ServiceState)ssp.dwCurrentState,
							.controlsAccepted = (int)ssp.dwControlsAccepted,
							.dwWin32ExitCode = ssp.dwWin32ExitCode,
							.dwServiceSpecificExitCode = ssp.dwServiceSpecificExitCode,
							.dwCheckPoint = ssp.dwCheckPoint,
							.dwWaitHint = ssp.dwWaitHint
						}
					}
				);
			}
			return services;
		}

		~ServiceControlManager() {
			CloseServiceHandle(controlManager);
		}

	private:
		SC_HANDLE controlManager = nullptr;
	};
}