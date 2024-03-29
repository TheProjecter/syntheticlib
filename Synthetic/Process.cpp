/*
	This file is part of the Synthetic library.
	Synthetic is a little library for writing custom gamecheats etc

	Synthetic is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	Synthetic is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with Synthetic.  If not, see <http://www.gnu.org/licenses/>.

	Copyright (C) [2010] [Ethon <Ethon@list.ru>]
*/

#include "System.hpp"

#if defined(SYNTHETIC_ISWINDOWS)

//C++ header files:
#include <algorithm>

//Synthetic Header files:
#include "Process.hpp"
#include "SysObjectIterator.hpp"
#include "SmartType.hpp"
#include "Auxiliary.hpp"

using namespace std;
using namespace Synthetic;

/**********************************************************************
***********************************************************************
************************* PUBLIC FREE FUNCTIONS ***********************
***********************************************************************
**********************************************************************/

pid_t Process::getProcessByForegroundWindow()
{
	//Get a handle to the foreground window
	HWND windowHandle = GetForegroundWindow();
	if(!windowHandle)
		return 0;

	//Resolve the handle to its associated ID and return it
	DWORD pid;
	GetWindowThreadProcessId(windowHandle, &pid);
	return pid;
}

pid_t Process::getProcessByWindowName(const wstring& windowName)
{
	//Get a handle to the specified window
	HWND windowHandle = FindWindowW(NULL, windowName.c_str());
	if(!windowHandle)
		return 0;

	//Resolve the handle to its associated ID and return it
	DWORD pid;
	GetWindowThreadProcessId(windowHandle, &pid);
	return pid;
}

pid_t Process::getProcessByName(wstring processName)
{
	//Convert given name to lowercase
	transform(	processName.begin(),
					processName.end(),
					processName.begin(),
					towlower);

	//Iterate the complete process-list
	pid_t id = 0;
	for_each(ProcessIterator(0), ProcessIterator(), [&](PROCESSENTRY32W cur)
	{
		//Convert current name to lowercase
		wstring curName(cur.szExeFile);
		transform(	curName.begin(),
						curName.end(),
						curName.begin(),
						tolower);

		//Compare
		if(curName == processName)
			id = cur.th32ProcessID;
	});

	return id;
}

pid_t Process::getProcessByWindowHandle(HWND windowHandle)
{
	//Resolve the handle to its associated ID and return it
	DWORD pid;
	GetWindowThreadProcessId(windowHandle, &pid);
	return pid;
}

pid_t Process::getCurrentProcess()
{
	return GetCurrentProcessId();
}

size_t Process::getProcessListByName(wstring processName, std::vector<pid_t>& dest)
{
	size_t previousSize = dest.size();

	//Convert given name to lowercase
	transform(	processName.begin(),
					processName.end(),
					processName.begin(),
					towlower);


	//Iterate the complete process-list
	for(ProcessIterator it(0); it != ProcessIterator(); ++it)
	{
		//Convert current name to lowercase
		wstring currentProcess(it->szExeFile);
		transform(	currentProcess.begin(),
						currentProcess.end(),
						currentProcess.begin(),
						towlower);

		//Compare
		if(currentProcess == processName)
			dest.push_back(it->th32ProcessID);
	}

	return dest.size() - previousSize;
}

size_t Process::getProcessList(std::vector<pid_t>& dest)
{
	size_t previousSize = dest.size();

	//Iterate the complete process-list
	for(ProcessIterator it(0); it != ProcessIterator(); ++it)
		dest.push_back(it->th32ProcessID);

	return dest.size() - previousSize;
}

/**********************************************************************
***********************************************************************
************************ PUBLIC MEMBER FUNCTIONS **********************
***********************************************************************
**********************************************************************/

Process::Process() : handle_(NULL), id_(0)
{
	addDebugPrivileges_();
}

Process::Process(pid_t pid) : handle_(NULL)
{
	addDebugPrivileges_();
	open(pid);
}

Process::Process(const Process& proc)
{
	close();
	addDebugPrivileges_();

	//Duplicate handle to avoid it getting invalid if the
	//original objects destructor gets called
	if(proc.handle_)
		handle_ = Aux::duplicateHandleLocal(proc.handle_);

	id_ = proc.id_;
}

Process::~Process()
{
	close(); 
}

ptr_t Process::operator[](ptr_t address) const
{
	return readMemory<ptr_t>(address);
}

HANDLE Process::getHandle() const
{
	return handle_;
}

pid_t Process::getId() const
{
	return id_;
}

pid_t Process::createProcessAndOpen(	const wstring& applicationName,
														const wstring& commandLine,
														const wstring directory,
														bool suspended,
														dword_t waitingTime)
{
	STARTUPINFO startupInfo;
	PROCESS_INFORMATION processInfo;

	ZeroMemory(&startupInfo, sizeof(startupInfo));
	ZeroMemory(&processInfo, sizeof(processInfo));
	startupInfo.cb = sizeof(startupInfo);

	//Prepare Arguments
	wstring formattedArguments(	L"\"" + applicationName +
											L"\" " + commandLine);

	const wchar_t* appDirectory = (directory == L"") ? NULL : directory.c_str();

	DWORD creationFlags;
	creationFlags = suspended ? CREATE_SUSPENDED : CREATE_DEFAULT_ERROR_MODE;

	BOOL ec = CreateProcessW(	applicationName.c_str(),
										&formattedArguments[0],
										NULL,
										NULL,
										false,
										creationFlags,
										NULL,
										appDirectory,
										&startupInfo,
										&processInfo);

	if(!ec)
	{
		throw WinException(	"Process::createProcessAndOpen()",
									"CreateProcessW()",
									GetLastError());
	}

	id_ = processInfo.dwProcessId;
	handle_ = processInfo.hProcess;
	CloseHandle(processInfo.hThread);

	if(waitingTime)
	{
		if(WaitForSingleObject(processInfo.hThread, waitingTime) == WAIT_FAILED)
		{
			throw WinException(	"Process::createProcessAndOpen()",
										"WaitForSingleObject()",
										GetLastError());
		}
	}
	
	return id_;
}

void Process::open(pid_t pid)
{
	close();

	DWORD desiredAccess =	PROCESS_QUERY_INFORMATION	|
									PROCESS_CREATE_THREAD		|
									PROCESS_VM_OPERATION			|
									PROCESS_VM_READ				|
									PROCESS_VM_WRITE				|
									PROCESS_SUSPEND_RESUME		|
									PROCESS_TERMINATE				|
									SYNCHRONIZE;

	handle_ = OpenProcess(desiredAccess, false, pid);
	if(handle_ == NULL)
	{
		throw WinException(	"Process::open()",
									"OpenProcess()",
									GetLastError());
	}

	id_ = pid;
}

void Process::close()
{
	if(handle_ != NULL && handle_ != INVALID_HANDLE_VALUE)
	{
		CloseHandle(handle_);
		handle_ = NULL;
	}
}

void Process::terminate(dword_t exitCode)
{
	if(!TerminateProcess(handle_, exitCode))
	{
		throw WinException(	"Process::terminate()",
									"TerminateProcess()",
									GetLastError());
	}

	close();
}

/**********************************************************************
***********************************************************************
************************ PRIVATE MEMBER FUNCTIONS *********************
***********************************************************************
**********************************************************************/

void Process::addDebugPrivileges_() const
{
	HANDLE hToken = NULL;
	if(!OpenProcessToken(	GetCurrentProcess(),
									TOKEN_ADJUST_PRIVILEGES,
									&hToken))
	{
		throw WinException(	"Process::AddDebugPrivileges_()",
									"OpenProcessToken()",
									GetLastError());
	}

	SmartHandle ensureClosure(hToken);

	TOKEN_PRIVILEGES tp;
	if(!LookupPrivilegeValueW(	NULL,
										L"SeDebugPrivilege",
										&tp.Privileges[0].Luid))
	{
		throw WinException(	"Process::AddDebugPrivileges_()",
									"LookupPrivilegeValueA()",
									GetLastError());
	}

	tp.PrivilegeCount = 1;
	tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;

	if(!AdjustTokenPrivileges(	hToken,
										FALSE,
										&tp,
										0,
										reinterpret_cast<PTOKEN_PRIVILEGES>(NULL),
										0))
	{
		throw WinException(	"Process::AddDebugPrivileges_()",
									"AdjustTokenPrivileges()",
									GetLastError());
	}
}

#endif //defined(SYNTHETIC_ISWINDOWS)

/******************
******* EOF *******
******************/
