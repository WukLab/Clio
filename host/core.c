/*
 * Copyright (c) 2020 Wuklab, UCSD. All rights reserved.
 */

#include <uapi/vregion.h>
#include <uapi/compiler.h>
#include <uapi/list.h>
#include <uapi/err.h>

#include "net/net.h"

int main (int argc, char **argv)
{
	struct session_net *ses;

	ses = init_net();
	if (!ses) {
		printf("Fail to init network layer.\n");
		return 0;
	}
	return 0;
}
