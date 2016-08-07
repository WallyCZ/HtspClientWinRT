#include "common.h"

enum
{
	O32_LITTLE_ENDIAN = 0x03020100ul,
	O32_BIG_ENDIAN = 0x00010203ul,
	O32_PDP_ENDIAN = 0x01000302ul
};
static const union { unsigned char bytes[4]; uint32_t value; } o32_host_order = { { 0, 1, 2, 3 } };

int64_t endian64(int64_t v)
{
	if (o32_host_order.value == O32_BIG_ENDIAN)
		return v;

	int64_t res = 0;

	char *vp = (char*)&v;
	char *rp = (char*)&res;

	rp[0] = vp[7];
	rp[1] = vp[6];
	rp[2] = vp[5];
	rp[3] = vp[4];
	rp[4] = vp[3];
	rp[5] = vp[2];
	rp[6] = vp[1];
	rp[7] = vp[0];

	return res;
}

uint32_t htonl(uint32_t v)
{
	if (o32_host_order.value == O32_BIG_ENDIAN)
		return v;

	uint32_t res = 0;

	char *vp = (char*)&v;
	char *rp = (char*)&res;

	rp[0] = vp[3];
	rp[1] = vp[2];
	rp[2] = vp[1];
	rp[3] = vp[0];

	return res;
}

uint32_t ntohl(uint32_t v)
{
	if (o32_host_order.value == O32_BIG_ENDIAN)
		return v;

	uint32_t res = 0;

	char *vp = (char*)&v;
	char *rp = (char*)&res;

	rp[0] = vp[3];
	rp[1] = vp[2];
	rp[2] = vp[1];
	rp[3] = vp[0];

	return res;
}
