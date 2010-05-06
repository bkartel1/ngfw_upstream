/* Copyright 2004 Jozsef Kadlecsik (kadlec@blackhole.kfki.hu)
 *
 * This program is free software; you can redistribute it and/or modify   
 * it under the terms of the GNU General Public License as published by   
 * the Free Software Foundation; either version 2 of the License, or      
 * (at your option) any later version.                                    
 *                                                                         
 * This program is distributed in the hope that it will be useful,        
 * but WITHOUT ANY WARRANTY; without even the implied warranty of         
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the          
 * GNU General Public License for more details.                           
 *                                                                         
 * You should have received a copy of the GNU General Public License      
 * along with this program; if not, write to the Free Software            
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */


#include <stdio.h>			/* *printf */
#include <string.h>			/* mem* */

#include "ipset.h"

#include <linux/netfilter_ipv4/ip_set_portmap.h>

#define BUFLEN 30;

#define OPT_CREATE_FROM    0x01U
#define OPT_CREATE_TO      0x02U

#define OPT_ADDDEL_PORT      0x01U

/* Initialize the create. */
static void
create_init(void *data UNUSED)
{
	DP("create INIT");
	/* Nothing */
}

/* Function which parses command options; returns true if it ate an option */
static int
create_parse(int c, char *argv[] UNUSED, void *data, unsigned *flags)
{
	struct ip_set_req_portmap_create *mydata = data;

	DP("create_parse");

	switch (c) {
	case '1':
		parse_port(optarg, &mydata->from);

		*flags |= OPT_CREATE_FROM;

		DP("--from %x (%s)", mydata->from,
		   port_tostring(mydata->from, 0));

		break;

	case '2':
		parse_port(optarg, &mydata->to);

		*flags |= OPT_CREATE_TO;

		DP("--to %x (%s)", mydata->to,
		   port_tostring(mydata->to, 0));

		break;

	default:
		return 0;
	}

	return 1;
}

/* Final check; exit if not ok. */
static void
create_final(void *data, unsigned int flags)
{
	struct ip_set_req_portmap_create *mydata = data;

	if (flags == 0) {
		exit_error(PARAMETER_PROBLEM,
			   "Need to specify --from and --to\n");
	} else {
		/* --from --to */
		if ((flags & OPT_CREATE_FROM) == 0
		    || (flags & OPT_CREATE_TO) == 0)
			exit_error(PARAMETER_PROBLEM,
				   "Need to specify both --from and --to\n");
	}

	DP("from : %x to: %x  diff: %d", mydata->from, mydata->to,
	   mydata->to - mydata->from);

	if (mydata->from > mydata->to)
		exit_error(PARAMETER_PROBLEM,
			   "From can't be lower than to.\n");

	if (mydata->to - mydata->from > MAX_RANGE)
		exit_error(PARAMETER_PROBLEM,
			   "Range too large. Max is %d ports in range\n",
			   MAX_RANGE+1);
}

/* Create commandline options */
static const struct option create_opts[] = {
	{.name = "from",	.has_arg = required_argument,	.val = '1'},
	{.name = "to",		.has_arg = required_argument,	.val = '2'},
	{NULL},
};

/* Add, del, test parser */
static ip_set_ip_t
adt_parser(int cmd UNUSED, const char *arg, void *data)
{
	struct ip_set_req_portmap *mydata = data;

	parse_port(arg, &mydata->ip);
	DP("%s", port_tostring(mydata->ip, 0));

	return 1;	
}

/*
 * Print and save
 */

static void
initheader(struct set *set, const void *data)
{
	const struct ip_set_req_portmap_create *header = data;
	struct ip_set_portmap *map = set->settype->header;

	memset(map, 0, sizeof(struct ip_set_portmap));
	map->first_ip = header->from;
	map->last_ip = header->to;
}

static void
printheader(struct set *set, unsigned options)
{
	struct ip_set_portmap *mysetdata = set->settype->header;

	printf(" from: %s", port_tostring(mysetdata->first_ip, options));
	printf(" to: %s\n", port_tostring(mysetdata->last_ip, options));
}

static void
printports_sorted(struct set *set, void *data,
		  size_t len UNUSED, unsigned options)
{
	struct ip_set_portmap *mysetdata = set->settype->header;
	u_int32_t addr = mysetdata->first_ip;

	DP("%u -- %u", mysetdata->first_ip, mysetdata->last_ip);
	while (addr <= mysetdata->last_ip) {
		if (test_bit(addr - mysetdata->first_ip, data))
			printf("%s\n", port_tostring(addr, options));
		addr++;
	}
}

static char *
binding_port_tostring(struct set *set UNUSED,
		      ip_set_ip_t ip, unsigned options)
{
	return port_tostring(ip, options);
}

static void
saveheader(struct set *set, unsigned options)
{
	struct ip_set_portmap *mysetdata = set->settype->header;

	printf("-N %s %s --from %s", 
	       set->name,
	       set->settype->typename,
	       port_tostring(mysetdata->first_ip, options));
	printf(" --to %s\n", 
	       port_tostring(mysetdata->last_ip, options));
}

static void
saveports(struct set *set, void *data,
	  size_t len UNUSED, unsigned options)
{
	struct ip_set_portmap *mysetdata = set->settype->header;
	u_int32_t addr = mysetdata->first_ip;

	while (addr <= mysetdata->last_ip) {
		if (test_bit(addr - mysetdata->first_ip, data))
			printf("-A %s %s\n",
			       set->name,
			       port_tostring(addr, options));
		addr++;
	}
}

static void usage(void)
{
	printf
	    ("-N set portmap --from PORT --to PORT\n"
	     "-A set PORT\n"
	     "-D set PORT\n"
	     "-T set PORT\n");
}

static struct settype settype_portmap = {
	.typename = SETTYPE_NAME,
	.protocol_version = IP_SET_PROTOCOL_VERSION,

	/* Create */
	.create_size = sizeof(struct ip_set_req_portmap_create),
	.create_init = &create_init,
	.create_parse = &create_parse,
	.create_final = &create_final,
	.create_opts = create_opts,

	/* Add/del/test */
	.adt_size = sizeof(struct ip_set_req_portmap),
	.adt_parser = &adt_parser,

	/* Printing */
	.header_size = sizeof(struct ip_set_portmap),
	.initheader = &initheader,
	.printheader = &printheader,
	.printips = &printports_sorted,	/* We only have sorted version */
	.printips_sorted = &printports_sorted,
	.saveheader = &saveheader,
	.saveips = &saveports,
	
	/* Bindings */
	.bindip_tostring = &binding_port_tostring,
	.bindip_parse = &parse_port,

	.usage = &usage,
};

CONSTRUCTOR(portmap)
{
	settype_register(&settype_portmap);

}
