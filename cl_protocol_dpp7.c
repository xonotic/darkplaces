#include "quakedef.h"
#include "cl_protocol_ext.h"

protocol_netmsg_t netmsg_dpext_svc =
{
	.size = 62,
	.msg = {NETMSG_DPEXT_SVC}
};