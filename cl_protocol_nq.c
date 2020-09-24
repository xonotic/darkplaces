#include "quakedef.h"
#include "protocol.h"
#include "cl_protocol_basenq.h"

protocol_netmsg_t netmsg_nq_svc =
{
	.size = 34,
	.msg = {NETMSG_BASENQ_SVC}
};
