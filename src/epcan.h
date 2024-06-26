#ifndef _H_EPCAN_
#define _H_EPCAN_

#define EPCAN_ID(node_ID, func)		(1024U | (node_ID) << 5 | (func))

#define EPCAN_GET_NODE(ID)		(((ID) >> 5) & 31U)
#define EPCAN_GET_FUNC(ID)		((ID) & 31U)

/* Network functions.
 * */
#define EPCAN_ID_NET_SURVEY		EPCAN_ID(31U, 31U)
#define EPCAN_ID_NET_ASSIGN		EPCAN_ID(31U, 29U)

/* Lower IDs is for DATA streaming.
 * */
#define	EPCAN_ID_NODE_BASE		EPCAN_ID(0U, 0U)

#define EPCAN_FILTER_MATCH		EPCAN_ID(31U, 31U)
#define EPCAN_FILTER_NETWORK		EPCAN_ID(31U, 0U)

/* The maximal number of nodes in the network.
 * */
#define EPCAN_NODES_MAX			30

enum {
	EPCAN_NODE_REQ			= 0,
	EPCAN_NODE_ACK,
	EPCAN_NODE_RX,				/* receive from remote node */
	EPCAN_NODE_TX,				/* send to remote node */
	EPCAN_NODE_GET,				/* request to GET register data */
	EPCAN_NODE_SET,				/* request to SET register data */
	EPCAN_NODE_DATA				/* reply to the GET request */
};

enum {
	EPCAN_REQ_NOTHING		= 0,
	EPCAN_REQ_FLOW_TX_PAUSE			/* node flow control */
};

enum {
	EPCAN_ACK_NOTHING		= 0,
	EPCAN_ACK_NETWORK_REPLY			/* reply to network survey */
};

enum {
	EPCAN_LOG_DISABLED		= 0,
	EPCAN_LOG_FILTERED,
	EPCAN_LOG_PROMISCUOUS
};

enum {
	EPCAN_PIPE_DISABLED		= 0,
	EPCAN_PIPE_INCOMING,
	EPCAN_PIPE_OUTGOING_REGULAR,
	EPCAN_PIPE_OUTGOING_INJECTED
};

enum {
	EPCAN_PAYLOAD_FLOAT		= 0,
	EPCAN_PAYLOAD_INT_16
};

enum {
	EPCAN_PIPE_MAX			= 4,
};

typedef struct {

	int		MODE;		/* mode of EP */
	int		ID;		/* CAN ID of the endpoint (EP) */
	int		clock_ID;	/* CAN ID used as clock */

	float		reg_DATA;	/* actual DATA */
	int		reg_ID;		/* linked register ID */

	int		PAYLOAD;	/* packet payload type */
	int		STARTUP;	/* motor startup behaviour */
	int		ACTIVE;

	int		rate;		/* transfer rate */
	float		range[2];	/* natural data range */

	int		tx_clock;
	int		tx_flag;
}
epcan_pipe_t;

typedef struct {

	int			node_ID;	/* EPCAN node ID */
	int			log_MODE;
	int			timeout_EP;

	epcan_pipe_t		ep[EPCAN_PIPE_MAX];
}
epcan_t;

extern epcan_t			net;

void EPCAN_pipe_REGULAR();

void EPCAN_putc(int c);

void EPCAN_startup();
void EPCAN_bind();

#endif /* _H_EPCAN_ */

