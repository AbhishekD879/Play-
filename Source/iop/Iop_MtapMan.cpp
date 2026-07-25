#include <assert.h>
#include "Iop_MtapMan.h"
#include "Log.h"

using namespace Iop;

#define LOG_NAME "iop_mtapman"

CMtapMan::CMtapMan()
{
	m_module901 = CSifModuleAdapter(std::bind(&CMtapMan::Invoke901, this,
	                                          std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4, std::placeholders::_5, std::placeholders::_6));
	m_module902 = CSifModuleAdapter(std::bind(&CMtapMan::Invoke902, this,
	                                          std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4, std::placeholders::_5, std::placeholders::_6));
	m_module903 = CSifModuleAdapter(std::bind(&CMtapMan::Invoke903, this,
	                                          std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4, std::placeholders::_5, std::placeholders::_6));
}

std::string CMtapMan::GetId() const
{
	return "mtapman";
}

std::string CMtapMan::GetFunctionName(unsigned int) const
{
	return "unknown";
}

void CMtapMan::RegisterSifModules(CSifMan& sif)
{
	sif.RegisterModule(MODULE_ID_1, &m_module901);
	sif.RegisterModule(MODULE_ID_2, &m_module902);
	sif.RegisterModule(MODULE_ID_3, &m_module903);
}

void CMtapMan::Invoke(CMIPS& context, unsigned int functionId)
{
	throw std::runtime_error("Not implemented.");
}

bool CMtapMan::Invoke901(uint32 method, uint32* args, uint32 argsSize, uint32* ret, uint32 retSize, uint8* ram)
{
	switch(method)
	{
	case 1:
		ret[1] = PortOpen(args[0]);
		break;
	default:
		CLog::GetInstance().Warn(LOG_NAME, "Unknown method invoked (0x%08X, 0x%08X).\r\n", 0x901, method);
		break;
	}
	return true;
}

bool CMtapMan::Invoke902(uint32 method, uint32* args, uint32 argsSize, uint32* ret, uint32 retSize, uint8* ram)
{
	switch(method)
	{
	//>>> PLAYSTATION-PORTFOLIO MULTITAP
	//MTAPSERV_PORT_CLOSE (0x80000902). ps2sdk's mtapPortClose does
	//sceSifCallRpc(&clientPortClose, 1, ...) with args[0] = port and reads
	//back mtapRpcBuffer[1] — so METHOD 1 on THIS module, result in ret[1].
	case 1:
		ret[1] = PortClose(args[0]);
		break;
	//<<< PLAYSTATION-PORTFOLIO MULTITAP
	default:
		CLog::GetInstance().Warn(LOG_NAME, "Unknown method invoked (0x%08X, 0x%08X).\r\n", 0x902, method);
		break;
	}
	return true;
}

bool CMtapMan::Invoke903(uint32 method, uint32* args, uint32 argsSize, uint32* ret, uint32 retSize, uint8* ram)
{
	switch(method)
	{
	//>>> PLAYSTATION-PORTFOLIO MULTITAP
	//★ MTAPSERV_GET_CONNECTION (0x80000903) — this is the call a game makes to
	//ask "is a multitap attached to this port?". It is a SEPARATE SIF module,
	//not a method on 0x901; wiring it there left this switch empty, so the
	//question went unanswered and every game concluded there was no tap.
	case 1:
		ret[1] = GetConnection(args[0]);
		break;
	//<<< PLAYSTATION-PORTFOLIO MULTITAP
	default:
		CLog::GetInstance().Warn(LOG_NAME, "Unknown method invoked (0x%08X, 0x%08X).\r\n", 0x903, method);
		break;
	}
	return true;
}

//>>> PLAYSTATION-PORTFOLIO MULTITAP
//libmtap.h contract: "1 on success; !1 on failure". Upstream returned 0
//unconditionally — a deliberate stub telling every game there is no tap. We now
//answer honestly per port, so a port with no tap still behaves exactly as before.
uint32 CMtapMan::PortOpen(uint32 port)
{
	bool enabled = Multitap::IsEnabled(port);
	CLog::GetInstance().Print(LOG_NAME, "PortOpen(port = %d) = %d;\r\n", port, enabled ? 1 : 0);
	return enabled ? 1 : 0;
}

uint32 CMtapMan::PortClose(uint32 port)
{
	bool enabled = Multitap::IsEnabled(port);
	CLog::GetInstance().Print(LOG_NAME, "PortClose(port = %d) = %d;\r\n", port, enabled ? 1 : 0);
	return enabled ? 1 : 0;
}

//mtapGetConnection: 1 when a tap is attached to an opened port.
uint32 CMtapMan::GetConnection(uint32 port)
{
	bool enabled = Multitap::IsEnabled(port);
	CLog::GetInstance().Print(LOG_NAME, "GetConnection(port = %d) = %d;\r\n", port, enabled ? 1 : 0);
	return enabled ? 1 : 0;
}

//Slots available on the port: 4 with a tap, 1 without.
uint32 CMtapMan::GetSlotNumber(uint32 port)
{
	uint32 slots = Multitap::IsEnabled(port) ? Multitap::MAX_SLOTS : 1;
	CLog::GetInstance().Print(LOG_NAME, "GetSlotNumber(port = %d) = %d;\r\n", port, slots);
	return slots;
}
//<<< PLAYSTATION-PORTFOLIO MULTITAP
