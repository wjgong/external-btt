/*
 * Copyright 2014 Tieto Corporation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "btt_gatt_client.h"
#include "btt.h"
#include "btt_utils.h"

#define DEFAULT_TIME_SEC 3
#define LONG_TIME_SEC 10

extern int app_socket;

static void run_gatt_client_help(int argc, char **argv);
static void run_gatt_client_scan(int argc, char **argv);
static void run_gatt_client_register_client(int argc, char **argv);
static void run_gatt_client_un_register_client(int argc, char **argv);
static void run_gatt_client_connect(int argc, char **argv);
static void run_gatt_client_disconnect(int argc, char **argv);
static void run_gatt_client_read_remote_rssi(int argc, char **argv);
static void run_gatt_client_listen(int argc, char **argv);
static void run_gatt_client_set_adv_data_basic(int argc, char **argv);
static void run_gatt_client_set_adv_data(int argc, char **argv);
static void run_gatt_client_get_device_type(int argc, char **argv);
static void run_gatt_client_refresh(int argc, char **argv);
static void run_gatt_client_search_service(int argc, char **argv);
static void run_gatt_client_get_included_service(int argc, char **argv);
static void run_gatt_client_get_characteristic(int argc, char **argv);
static void run_gatt_client_get_descriptor(int argc, char **argv);
static void run_gatt_client_read_characteristic(int argc, char **argv);
static void run_gatt_client_read_descriptor(int argc, char **argv);
static void run_gatt_client_write_characteristic(int argc, char **argv);
static void run_gatt_client_execute_write(int argc, char **argv);
static void run_gatt_client_write_descriptor(int argc, char **argv);
static void run_gatt_client_reg_for_notification(int argc, char **argv);
static void run_gatt_client_dereg_for_notification(int argc, char **argv);
static void run_gatt_client_test_command(int argc, char **argv);
static void set_sock_rcv_time(unsigned int sec, unsigned int usec,
		int server_sock);
static bool send_by_socket(int server_sock, void *data, size_t len, int flags);
static bool process_send_to_daemon(enum btt_gatt_client_req_t type, void *data,
		int server_sock);
static void printf_service(btgatt_srvc_id_t srv);
static void printf_characteristic(btgatt_gatt_id_t cha, int char_prop);
static bool process_UUID_sscanf(char *src, uint8_t *dest);

static const struct extended_command gatt_client_commands[] = {
		{{ "help",							"",							run_gatt_client_help}, 1, MAX_ARGC},
		{{ "scan",							"<client_if> <start>", run_gatt_client_scan}, 3, 3},
		{{ "register_client",				"<16-bits UUID>", run_gatt_client_register_client}, 2, 2},
		{{ "unregister_client",				"<client_if>", run_gatt_client_un_register_client}, 2, 2},
		{{ "connect",						"<client_if> <BD_ADDR> <is_direct>", run_gatt_client_connect}, 4, 4},
		{{ "disconnect",					"<client_if> <BD_ADDR> <conn_id>", run_gatt_client_disconnect}, 4, 4},
		{{ "listen",						"<client_if> <start>", run_gatt_client_listen}, 3, 3},
		{{ "refresh",						"<client_if> <BD_ADDR>", run_gatt_client_refresh}, 3, 3},
		{{ "search_service",				"<conn_id> [UUID_filter]", run_gatt_client_search_service}, 2, 3},
		{{ "get_included_service",			"<conn_id> <UUID> <is_primary> <inst_id> [<UUID> <is_primary> <inst_id>]", run_gatt_client_get_included_service}, 5, 8},
		{{ "get_characteristic",			"<conn_id> <UUID> <is_primary> <inst_id> [<UUID> <inst_id>]", run_gatt_client_get_characteristic}, 5, 7},
		{{ "get_descriptor",				"<conn_id> <UUID> <is_primary> <inst_id> <UUID> <inst_id> [<UUID> <inst_id>]", run_gatt_client_get_descriptor}, 7, 9},
		{{ "read_characteristic",			"<conn_id> <UUID> <is_primary> <inst_id> <UUID> <inst_id> <auth_req>", run_gatt_client_read_characteristic}, 8, 8},
		{{ "write_characteristic",			"<conn_id> <UUID> <is_primary> <inst_id> <UUID> <inst_id> <write_type> <auth_req> <hex_value>", run_gatt_client_write_characteristic}, 10, 10},
		{{ "read_descriptor",				"<conn_id> <UUID> <is_primary> <inst_id> <UUID> <inst_id> <UUID> <inst_id> <auth_req>", run_gatt_client_read_descriptor}, 10, 10},
		{{ "write_descriptor",				"<conn_id> <UUID> <is_primary> <inst_id> <UUID> <inst_id> <UUID> <inst_id> <write_type> <auth_req> <hex_value>", run_gatt_client_write_descriptor}, 12, 12},
		{{ "execute_write",					"<conn_id> <execute>", run_gatt_client_execute_write}, 3, 3},
		{{ "register_for_notification",		"<client_if> <BD_ADDR> <UUID> <is_primary> <inst_id> <UUID> <inst_id>", run_gatt_client_reg_for_notification}, 8, 8},
		{{ "deregister_for_notification",	"<client_if> <BD_ADDR> <UUID> <is_primary> <inst_id> <UUID> <inst_id>", run_gatt_client_dereg_for_notification}, 8, 8},
		{{ "read_remote_rssi",				"<BD_ADDR> <client_if>", run_gatt_client_read_remote_rssi}, 3, 3},
		{{ "get_device_type",				"<BD_ADDR>", run_gatt_client_get_device_type}, 2, 2},
		{{ "set_adv_data_basic",			"<client_if> <set_scan_rsp> <include_name> <include_txpower> <min_interval> <max_interval> <appearance>", run_gatt_client_set_adv_data_basic}, 8, 8},
		{{ "set_adv_data",					"<client_if> <manuf_data> <service_data> <service_uuid>", run_gatt_client_set_adv_data}, 5, 5},
		{{ "test_command",					"<command> <BD_ADDR> <UUID> [u1] [u2] [u3] [u4] [u5]", run_gatt_client_test_command}, 4, 9},
};

#define GATT_CLIENT_SUPPORTED_COMMANDS sizeof(gatt_client_commands)/sizeof(struct extended_command)

void run_gatt_client_help(int argc, char **argv)
{
	print_commands_extended(gatt_client_commands,
			GATT_CLIENT_SUPPORTED_COMMANDS);
}

static void process_request(enum btt_gatt_client_req_t type, void *data,
		unsigned int recv_time_sec)
{
	int client_if = -1;

	errno = 0;

	set_sock_rcv_time(recv_time_sec, 0, app_socket);

	process_send_to_daemon(type, data, app_socket);
}

void run_gatt_client(int argc, char **argv)
{
	run_generic_extended(gatt_client_commands, GATT_CLIENT_SUPPORTED_COMMANDS,
			run_gatt_client_help, argc, argv);
}

static void run_gatt_client_scan(int argc, char **argv)
{
	struct btt_gatt_client_scan req;

	sscanf(argv[1], "%d", &req.client_if);
	sscanf(argv[2], "%u", &req.start);

	process_request(BTT_GATT_CLIENT_REQ_SCAN, &req, DEFAULT_TIME_SEC);
}

static void set_sock_rcv_time(unsigned int sec, unsigned int usec,
		int server_sock)
{
	struct timeval tv;

	tv.tv_sec = sec;
	tv.tv_usec = usec;
	setsockopt(server_sock, SOL_SOCKET, SO_RCVTIMEO,
			(char *) &tv, sizeof(struct timeval));
}

static bool send_by_socket(int server_sock, void *data, size_t len, int flags)
{
	if (send(server_sock, data, len, flags) == -1)
		return FALSE;

	return TRUE;
}

/* server_sock must be correct socket descriptor */
static bool process_send_to_daemon(enum btt_gatt_client_req_t type, void *data,
		int server_sock)
{
	switch (type) {
	case BTT_GATT_CLIENT_REQ_SCAN:
	{
		struct btt_gatt_client_scan *cmd_scan;

		FILL_MSG_P(data, cmd_scan, BTT_CMD_GATT_CLIENT_SCAN);

		if (!send_by_socket(server_sock, cmd_scan,
				sizeof(struct btt_gatt_client_scan), 0))
			return FALSE;

		break;
	}
	case BTT_GATT_CLIENT_REQ_REGISTER_CLIENT:
	{
		struct btt_gatt_client_register_client *register_client;

		FILL_MSG_P(data, register_client, BTT_CMD_GATT_CLIENT_REGISTER_CLIENT);

		if (!send_by_socket(server_sock, register_client,
				sizeof(struct btt_gatt_client_register_client), 0))
			return FALSE;

		break;
	}
	case BTT_GATT_CLIENT_REQ_UNREGISTER_CLIENT:
	{
		struct btt_gatt_client_unregister_client *unregister_client;

		FILL_MSG_P(data, unregister_client, BTT_CMD_GATT_CLIENT_UNREGISTER_CLIENT);

		if (!send_by_socket(server_sock, unregister_client,
				sizeof(struct btt_gatt_client_unregister_client), 0))
			return FALSE;

		break;
	}
	case BTT_GATT_CLIENT_REQ_CONNECT:
	{
		struct btt_gatt_client_connect *connect;

		FILL_MSG_P(data, connect, BTT_CMD_GATT_CLIENT_CONNECT);

		if (!send_by_socket(server_sock, connect,
				sizeof(struct btt_gatt_client_connect), 0))
			return FALSE;

		break;
	}
	case BTT_GATT_CLIENT_REQ_DISCONNECT:
	{
		struct btt_gatt_client_disconnect *disconnect;

		FILL_MSG_P(data, disconnect, BTT_CMD_GATT_CLIENT_DISCONNECT);

		if (!send_by_socket(server_sock, disconnect,
				sizeof(struct btt_gatt_client_disconnect), 0))
			return FALSE;

		break;
	}
	case BTT_GATT_CLIENT_REQ_READ_REMOTE_RSSI:
	{
		struct btt_gatt_client_read_remote_rssi *read_rssi;

		FILL_MSG_P(data, read_rssi, BTT_CMD_GATT_CLIENT_READ_REMOTE_RSSI);

		if (!send_by_socket(server_sock, read_rssi,
				sizeof(struct btt_gatt_client_read_remote_rssi), 0))
			return FALSE;

		break;
	}
	case BTT_GATT_CLIENT_REQ_LISTEN:
	{
		struct btt_gatt_client_listen *listen;

		FILL_MSG_P(data, listen, BTT_CMD_GATT_CLIENT_LISTEN);

		if (!send_by_socket(server_sock, listen,
				sizeof(struct btt_gatt_client_listen), 0))
			return FALSE;

		break;
	}
	case BTT_GATT_CLIENT_REQ_SET_ADV_DATA:
	{
		struct btt_gatt_client_set_adv_data *adv;

		FILL_MSG_P(data, adv, BTT_CMD_GATT_CLIENT_SET_ADV_DATA);

		if (!send_by_socket(server_sock, adv,
				sizeof(struct btt_gatt_client_set_adv_data), 0))
			return FALSE;

		break;
	}
	case BTT_GATT_CLIENT_REQ_GET_DEVICE_TYPE:
	{
		struct btt_gatt_client_get_device_type *get;

		FILL_MSG_P(data, get, BTT_CMD_GATT_CLIENT_GET_DEVICE_TYPE);

		if (!send_by_socket(server_sock, get,
				sizeof(struct btt_gatt_client_listen), 0))
			return FALSE;

		break;
	}
	case BTT_GATT_CLIENT_REQ_REFRESH:
	{
		struct btt_gatt_client_refresh *refresh;

		FILL_MSG_P(data, refresh, BTT_CMD_GATT_CLIENT_REFRESH);

		if (!send_by_socket(server_sock, refresh,
				sizeof(struct btt_gatt_client_refresh), 0))
			return FALSE;

		break;
	}
	case BTT_GATT_CLIENT_REQ_SEARCH_SERVICE:
	{
		struct btt_gatt_client_search_service *search;

		FILL_MSG_P(data, search, BTT_CMD_GATT_CLIENT_SEARCH_SERVICE);

		if (!send_by_socket(server_sock, search,
				sizeof(struct btt_gatt_client_search_service), 0))
			return FALSE;

		break;
	}
	case BTT_GATT_CLIENT_REQ_GET_INCLUDED_SERVICE:
	{
		struct btt_gatt_client_get_included_service *get;

		FILL_MSG_P(data, get, BTT_CMD_GATT_CLIENT_GET_INCLUDE_SERVICE);

		if (!send_by_socket(server_sock, get,
				sizeof(struct btt_gatt_client_get_included_service), 0))
			return FALSE;

		break;
	}
	case BTT_GATT_CLIENT_REQ_GET_CHARACTERISTIC:
	{
		struct btt_gatt_client_get_characteristic *get;

		FILL_MSG_P(data, get, BTT_CMD_GATT_CLIENT_GET_CHARACTERISTIC);

		if (!send_by_socket(server_sock, get,
				sizeof(struct btt_gatt_client_get_characteristic), 0))
			return FALSE;

		break;
	}
	case BTT_GATT_CLIENT_REQ_GET_DESCRIPTOR:
	{
		struct btt_gatt_client_get_descriptor *get;

		FILL_MSG_P(data, get, BTT_CMD_GATT_CLIENT_GET_DESCRIPTOR);

		if (!send_by_socket(server_sock, get,
				sizeof(struct btt_gatt_client_get_descriptor), 0))
			return FALSE;

		break;
	}
	case BTT_GATT_CLIENT_REQ_READ_CHARACTERISTIC:
	{
		struct btt_gatt_client_read_characteristic *read;

		FILL_MSG_P(data, read, BTT_CMD_GATT_CLIENT_READ_CHARACTERISTIC);

		if (!send_by_socket(server_sock, read,
				sizeof(struct btt_gatt_client_read_characteristic), 0))
			return FALSE;

		break;
	}
	case BTT_GATT_CLIENT_REQ_READ_DESCRIPTOR:
	{
		struct btt_gatt_client_read_descriptor *read;

		FILL_MSG_P(data, read, BTT_CMD_GATT_CLIENT_READ_DESCRIPTOR);

		if (!send_by_socket(server_sock, read,
				sizeof(struct btt_gatt_client_read_descriptor), 0))
			return FALSE;

		break;
	}
	case BTT_GATT_CLIENT_REQ_WRITE_CHARACTERISTIC:
	{
		struct btt_gatt_client_write_characteristic *write;

		FILL_MSG_P(data, write, BTT_CMD_GATT_CLIENT_WRITE_CHARACTERISTIC);

		if (!send_by_socket(server_sock, write,
				sizeof(struct btt_gatt_client_write_characteristic), 0))
			return FALSE;

		break;
	}
	case BTT_GATT_CLIENT_REQ_EXECUTE_WRITE:
	{
		struct btt_gatt_client_execute_write *exe;

		FILL_MSG_P(data, exe, BTT_CMD_GATT_CLIENT_EXECUTE_WRITE);

		if (!send_by_socket(server_sock, exe,
				sizeof(struct btt_gatt_client_execute_write), 0))
			return FALSE;

		break;
	}
	case BTT_GATT_CLIENT_REQ_WRITE_DESCRIPTOR:
	{
		struct btt_gatt_client_write_descriptor *write;

		FILL_MSG_P(data, write, BTT_CMD_GATT_CLIENT_WRITE_DESCRIPTOR);

		if (!send_by_socket(server_sock, write,
				sizeof(struct btt_gatt_client_write_descriptor), 0))
			return FALSE;

		break;
	}
	case BTT_GATT_CLIENT_REQ_REGISTER_FOR_NOTIFICATION:
	{
		struct btt_gatt_client_reg_for_notification *reg;

		FILL_MSG_P(data, reg, BTT_CMD_GATT_CLIENT_REGISTER_FOR_NOTIFICATION);

		if (!send_by_socket(server_sock, reg,
				sizeof(struct btt_gatt_client_reg_for_notification), 0))
			return FALSE;

		break;
	}
	case BTT_GATT_CLIENT_REQ_DEREGISTER_FOR_NOTIFICATION:
	{
		struct btt_gatt_client_dereg_for_notification *dereg;

		FILL_MSG_P(data, dereg,
				BTT_CMD_GATT_CLIENT_DEREGISTER_FOR_NOTIFICATION);

		if (!send_by_socket(server_sock, dereg,
				sizeof(struct btt_gatt_client_dereg_for_notification), 0))
			return FALSE;

		break;
	}
	case BTT_GATT_CLIENT_REQ_TEST_COMMAND:
	{
		struct btt_gatt_client_test_command *test;

		FILL_MSG_P(data, test, BTT_CMD_GATT_CLIENT_TEST_COMMAND);

		if (!send_by_socket(server_sock, test,
				sizeof(struct btt_gatt_client_test_command), 0))
			return FALSE;

		break;
	}
	default:
		BTT_LOG_S("ERROR: Unknown command - %d", type);
		return FALSE;
	}

	return TRUE;
}

void handle_gattc_cb(const struct btt_message *btt_cb)
{
	unsigned int i;
	uint8_t empty_BD_ADDR[BD_ADDR_LEN];
	char *buffer;

	errno = 0;
	memset(empty_BD_ADDR, 0, BD_ADDR_LEN);

	switch (btt_cb->command) {
	case BTT_GATT_CLIENT_CB_BT_STATUS:
	{
		struct btt_gatt_client_cb_bt_status stat;

		if (!RECV(&stat, app_socket)) {
			BTT_LOG_S("Error: incorrect size of received structure.\n");
			return;
		}

		BTT_LOG_S("\nGATTC: Request status: %s\n",
				bt_status_string[stat.status]);

		break;
	}
	case BTT_GATT_CLIENT_CB_SCAN_RESULT:
	{
		struct btt_gatt_client_cb_scan_result device;

		if (!RECV(&device, app_socket)) {
			BTT_LOG_S("Error: incorrect size of received structure.\n");
			return;
		}

		BTT_LOG_S("\nGATTC: Scan result.\n");

		if (memcmp(device.bd_addr, empty_BD_ADDR, BD_ADDR_LEN)) {
			print_bdaddr(device.bd_addr);
			BTT_LOG_S("%s, ", device.name);
			BTT_LOG_S("%s\n", discoverable_mode[device.discoverable_mode]);
		}

		break;
	}
	case BTT_GATT_CLIENT_CB_REGISTER_CLIENT:
	{
		struct btt_gatt_client_cb_register_client cb;

		if (!RECV(&cb, app_socket)) {
			BTT_LOG_S("Error: incorrect size of received structure.\n");
			return;
		}

		BTT_LOG_S("\nGATTC: Register client.\n");
		printf_UUID_128(cb.app_uuid.uu, FALSE, FALSE);
		BTT_LOG_S("Status: %s\n", (!cb.status) ? "OK" : "ERROR");
		BTT_LOG_S("Client interface: %d\n\n", cb.client_if);

		break;
	}
	case BTT_GATT_CLIENT_CB_CONNECT:
	{
		struct btt_gatt_client_cb_connect cb;

		if (!RECV(&cb, app_socket)) {
			BTT_LOG_S("Error: incorrect size of received structure.\n");
			return;
		}

		BTT_LOG_S("\nGATTC: Connect.\n");
		BTT_LOG_S("Address: ");
		print_bdaddr(cb.bda.address);
		BTT_LOG_S("\nConnection ID: %d\n", cb.conn_id);
		BTT_LOG_S("Status: %s\n", (!cb.status) ? "OK" : "ERROR");
		BTT_LOG_S("Client interface: %d\n\n", cb.client_if);

		break;
	}
	case BTT_GATT_CLIENT_CB_DISCONNECT:
	{
		struct btt_gatt_client_cb_disconnect cb;

		if (!RECV(&cb, app_socket)) {
			BTT_LOG_S("Error: incorrect size of received structure.\n");
			return;
		}

		BTT_LOG_S("\nGATTC: Disconnect.\n");
		BTT_LOG_S("Address: ");
		print_bdaddr(cb.bda.address);
		BTT_LOG_S("\nConnection ID: %d\n", cb.conn_id);
		BTT_LOG_S("Status: %s\n", (!cb.status) ? "OK" : "ERROR");
		BTT_LOG_S("Client interface: %d\n\n", cb.client_if);

		break;
	}
	case BTT_GATT_CLIENT_CB_READ_REMOTE_RSSI:
	{
		struct btt_gatt_client_cb_read_remote_rssi cb;

		if (!RECV(&cb, app_socket)) {
			BTT_LOG_S("Error: incorrect size of received structure.\n");
			return;
		}

		BTT_LOG_S("\nGATTC: Read remote RSSI.\n");
		BTT_LOG_S("Address: ");
		print_bdaddr(cb.addr.address);
		BTT_LOG_S("\nStatus: %s\n", (!cb.status) ? "OK" : "ERROR");
		BTT_LOG_S("RSSI: %d \n", cb.rssi);
		BTT_LOG_S("(higher RSSI level = stronger signal)\n\n");

		break;
	}
	case BTT_GATT_CLIENT_CB_LISTEN:
	{
		struct btt_gatt_client_cb_listen cb;

		if (!RECV(&cb, app_socket)) {
			BTT_LOG_S("Error: incorrect size of received structure.\n");
			return;
		}

		BTT_LOG_S("\nGATTC: Listen.\n");
		BTT_LOG_S("Status: %s\n", (!cb.status) ? "OK" : "ERROR");
		BTT_LOG_S("Client interface: %d\n\n", cb.server_if);

		break;
	}
	case BTT_GATT_CLIENT_CB_GET_DEVICE_TYPE:
	{
		struct btt_gatt_client_cb_get_device_type cb;

		if (!RECV(&cb, app_socket)) {
			BTT_LOG_S("Error: incorrect size of received structure.\n");
			return;
		}

		BTT_LOG_S("\nGATTC: Get device type.\n");
		BTT_LOG_S("Device type: ");

		switch (cb.type) {
		case BT_DEVICE_DEVTYPE_BREDR:
			BTT_LOG_S("BR/EDR\n");
			break;
		case BT_DEVICE_DEVTYPE_BLE:
			BTT_LOG_S("LE\n");
			break;
		case BT_DEVICE_DEVTYPE_DUAL:
			BTT_LOG_S("DUAL\n");
			break;
		default:
			BTT_LOG_S("Unknown type or error: %d\n", cb.type);
		}

		break;
	}
	case BTT_GATT_CLIENT_CB_SEARCH_RESULT:
	{
		struct btt_gatt_client_cb_search_result cb;

		if (!RECV(&cb, app_socket)) {
			BTT_LOG_S("Error: incorrect size of received structure.\n");
			return;
		}

		BTT_LOG_S("\nGATTC: Search result.\n");
		BTT_LOG_S("Connection Id: %d.\n", cb.conn_id);
		printf_service(cb.srvc_id);
		BTT_LOG_S("\n");

		break;
	}
	case BTT_GATT_CLIENT_CB_SEARCH_COMPLETE:
	{
		struct btt_gatt_client_cb_search_complete cb;

		if (!RECV(&cb, app_socket)) {
			BTT_LOG_S("Error: incorrect size of received structure.\n");
			return;
		}

		BTT_LOG_S("\nGATTC: Search complete.\n");
		BTT_LOG_S("Status: %s\n", (!cb.status) ? "OK" : "ERROR");
		BTT_LOG_S("Connection Id: %d.\n", cb.conn_id);

		break;
	}
	case BTT_GATT_CLIENT_CB_GET_INCLUDED_SERVICE:
	{
		struct btt_gatt_client_cb_get_included_service cb;

		if (!RECV(&cb, app_socket)) {
			BTT_LOG_S("Error: incorrect size of received structure.\n");
			return;
		}

		BTT_LOG_S("\nGATTC: Get included service.\n");
		BTT_LOG_S("Status: %s\n", (!cb.status) ? "OK" : "ERROR");
		BTT_LOG_S("Connection Id: %d.\n", cb.conn_id);

		if (!cb.status) {
			BTT_LOG_S("SERVICE: \n");
			printf_service(cb.srvc_id);
			BTT_LOG_S("\nINCLUDED SERVICE: \n");
			printf_service(cb.incl_srvc_id);
			BTT_LOG_S("\n");
		}

		break;
	}
	case BTT_GATT_CLIENT_CB_GET_CHARACTERISTIC:
	{
		struct btt_gatt_client_cb_get_characteristic cb;

		if (!RECV(&cb, app_socket)) {
			BTT_LOG_S("Error: incorrect size of received structure.\n");
			return;
		}

		BTT_LOG_S("\nGATTC: Get characteristic.\n");
		BTT_LOG_S("Status: %s\n", (!cb.status) ? "OK" : "ERROR");
		BTT_LOG_S("Connection Id: %d.\n", cb.conn_id);

		if (!cb.status) {
			BTT_LOG_S("SERVICE: \n");
			printf_service(cb.srvc_id);
			BTT_LOG_S("\nCHARACTERISTIC: \n");
			printf_characteristic(cb.char_id, cb.char_prop);
		}

		break;
	}
	case BTT_GATT_CLIENT_CB_GET_DESCRIPTOR:
	{
		struct btt_gatt_client_cb_get_descriptor cb;

		if (!RECV(&cb, app_socket)) {
			BTT_LOG_S("Error: incorrect size of received structure.\n");
			return;
		}

		BTT_LOG_S("\nGATTC: Get descriptor.\n");
		BTT_LOG_S("Status: %s\n", (!cb.status) ? "OK" : "ERROR");
		BTT_LOG_S("Connection Id: %d.\n", cb.conn_id);

		if (!cb.status) {
			BTT_LOG_S("SERVICE: \n");
			printf_service(cb.srvc_id);
			BTT_LOG_S("\nCHARACTERISTIC: \n");
			printf_characteristic(cb.char_id, 0);
			BTT_LOG_S("\nDESCRIPTOR: \n");
			printf_characteristic(cb.descr_id, 0);
		}

		break;
	}
	case BTT_GATT_CLIENT_CB_READ_CHARACTERISTIC:
	{
		struct btt_gatt_client_cb_read_characteristic cb;

		if (!RECV(&cb, app_socket)) {
			BTT_LOG_S("Error: incorrect size of received structure.\n");
			return;
		}

		BTT_LOG_S("\nGATTC: Read characteristic.\n");
		BTT_LOG_S("Status: %s\n", (!cb.status) ? "OK" : "ERROR");
		BTT_LOG_S("Connection Id: %d.\n", cb.conn_id);

		if (!cb.status) {
			BTT_LOG_S("SERVICE: \n");
			printf_service(cb.p_data.srvc_id);
			BTT_LOG_S("\nCHARACTERISTIC: \n");
			printf_characteristic(cb.p_data.char_id, 0);
			BTT_LOG_S("Unformatted value: ");

			for (i = 0; i < cb.p_data.value.len; i++)
				BTT_LOG_S("%.2X", cb.p_data.value.value[i]);

			BTT_LOG_S("\nValue type: %.4X\n", cb.p_data.value_type);
			BTT_LOG_S("Status: %.2X\n", cb.p_data.status);
		}

		break;
	}
	case BTT_GATT_CLIENT_CB_READ_DESCRIPTOR:
	{
		struct btt_gatt_client_cb_read_descriptor cb;

		if (!RECV(&cb, app_socket)) {
			BTT_LOG_S("Error: incorrect size of received structure.\n");
			return;
		}

		BTT_LOG_S("\nGATTC: Read descriptor.\n");
		BTT_LOG_S("Status: %s\n", (!cb.status) ? "OK" : "ERROR");
		BTT_LOG_S("Connection Id: %d.\n", cb.conn_id);

		if (!cb.status) {
			BTT_LOG_S("SERVICE: \n");
			printf_service(cb.p_data.srvc_id);
			BTT_LOG_S("\nCHARACTERISTIC: \n");
			printf_characteristic(cb.p_data.char_id, 0);
			BTT_LOG_S("\nDESCRIPTOR: \n");
			printf_characteristic(cb.p_data.descr_id, 0);
			BTT_LOG_S("Unformatted value: \n\t");

			for (i = 0; i < cb.p_data.value.len; i++)
				BTT_LOG_S("%.2X", cb.p_data.value.value[i]);

			BTT_LOG_S("\nValue type: %.4X\n", cb.p_data.value_type);
			BTT_LOG_S("Status: %.2X\n", cb.p_data.status);
		}

		break;
	}
	case BTT_GATT_CLIENT_CB_WRITE_CHARACTERISTIC:
	{
		struct btt_gatt_client_cb_write_characteristic cb;

		if (!RECV(&cb, app_socket)) {
			BTT_LOG_S("Error: incorrect size of received structure.\n");
			return;
		}

		BTT_LOG_S("\nGATTC: Write characteristic.\n");
		BTT_LOG_S("Status: %s\n", (!cb.status) ? "OK" : "ERROR");
		BTT_LOG_S("Connection Id: %d.\n", cb.conn_id);

		if (!cb.status) {
			BTT_LOG_S("SERVICE: \n");
			printf_service(cb.p_data.srvc_id);
			BTT_LOG_S("\nCHARACTERISTIC: \n");
			printf_characteristic(cb.p_data.char_id, 0);
			BTT_LOG_S("\n");
		}

		break;
	}
	case BTT_GATT_CLIENT_CB_EXECUTE_WRITE:
	{
		struct btt_gatt_client_cb_execute_write cb;

		if (!RECV(&cb, app_socket)) {
			BTT_LOG_S("Error: incorrect size of received structure.\n");
			return;
		}

		BTT_LOG_S("\nGATTC: Execute write.\n");
		BTT_LOG_S("Status: %s\n", (!cb.status) ? "OK" : "ERROR");
		BTT_LOG_S("Connection Id: %d.\n\n", cb.conn_id);

		break;
	}
	case BTT_GATT_CLIENT_CB_WRITE_DESCRIPTOR:
	{
		struct btt_gatt_client_cb_write_descriptor cb;

		if (!RECV(&cb, app_socket)) {
			BTT_LOG_S("Error: incorrect size of received structure.\n");
			return;
		}

		BTT_LOG_S("\nGATTC: Write descriptor.\n");
		BTT_LOG_S("Status: %s\n", (!cb.status) ? "OK" : "ERROR");
		BTT_LOG_S("Connection Id: %d.\n", cb.conn_id);

		if (!cb.status) {
			BTT_LOG_S("SERVICE: \n");
			printf_service(cb.p_data.srvc_id);
			BTT_LOG_S("\nCHARACTERISTIC: \n");
			printf_characteristic(cb.p_data.char_id, 0);
			BTT_LOG_S("\nDESCRIPTOR: \n");
			printf_characteristic(cb.p_data.descr_id, 0);
		}

		break;
	}
	case BTT_GATT_CLIENT_CB_REGISTER_FOR_NOTIFICATION:
	{
		struct btt_gatt_client_cb_reg_for_notification cb;

		if (!RECV(&cb, app_socket)) {
			BTT_LOG_S("Error: incorrect size of received structure.\n");
			return;
		}

		BTT_LOG_S("\nGATTC: Register for notification.\n");
		BTT_LOG_S("Status: %s\n", (!cb.status) ? "OK" : "ERROR");
		BTT_LOG_S("Connection Id: %d.\n", cb.conn_id);
		BTT_LOG_S("Registered: %s\n", (!cb.registered) ? "TRUE" : "FALSE");

		if (!cb.status) {
			BTT_LOG_S("SERVICE: \n");
			printf_service(cb.srvc_id);
			BTT_LOG_S("\nCHARACTERISTIC: \n");
			printf_characteristic(cb.char_id, 0);
			BTT_LOG_S("\n");
		}

		break;
	}
	case BTT_GATT_CLIENT_CB_NOTIFY:
	{
		struct btt_gatt_client_cb_notify cb;

		if (!RECV(&cb, app_socket)) {
			BTT_LOG_S("Error: incorrect size of received structure.\n");
			return;
		}

		BTT_LOG_S("\nGATTC: Notify.\n");
		BTT_LOG_S("Connection Id: %d.\n", cb.conn_id);
		BTT_LOG_S("\nAddress: ");
		print_bdaddr(cb.p_data.bda.address);
		BTT_LOG_S("\nSERVICE: \n");
		printf_service(cb.p_data.srvc_id);
		BTT_LOG_S("\nCHARACTERISTIC: \n");
		printf_characteristic(cb.p_data.char_id, 0);
		BTT_LOG_S("Value: \n\t");

		for (i = 0; i < cb.p_data.len; i++)
			BTT_LOG_S("%.2X", cb.p_data.value[i]);

		BTT_LOG_S("\nNotify: %s\n", (cb.p_data.is_notify) ? "TRUE" : "FALSE");
		break;
	}
	default:
		buffer = malloc(btt_cb->length);

		if (buffer) {
			recv(app_socket, buffer, btt_cb->length, 0);
			free(buffer);
		}

		break;
	}
}

static void printf_service(btgatt_srvc_id_t srv)
{
	BTT_LOG_S("Service is %s.\n", (srv.is_primary ?
			"primary" : "secondary"));
	BTT_LOG_S("Instance Id: %d.\n", srv.id.inst_id);
	printf_UUID_128(srv.id.uuid.uu, TRUE, FALSE);
}

static void printf_characteristic(btgatt_gatt_id_t cha, int char_prop)
{
	BTT_LOG_S("Instance Id: %d.\n", cha.inst_id);
	printf_UUID_128(cha.uuid.uu, TRUE, FALSE);

	if (char_prop != 0)
		BTT_LOG_S("\nBit set: \n");

	if (char_prop & GATT_CHAR_PROP_BIT_BROADCAST)
		BTT_LOG_S("\tBroadcast\n");

	if (char_prop & GATT_CHAR_PROP_BIT_READ)
		BTT_LOG_S("\tRead\n");

	if (char_prop & GATT_CHAR_PROP_BIT_WRITE_NR)
		BTT_LOG_S("\tWrite without Response\n");

	if (char_prop & GATT_CHAR_PROP_BIT_WRITE)
		BTT_LOG_S("\tWrite\n");

	if (char_prop & GATT_CHAR_PROP_BIT_NOTIFY)
		BTT_LOG_S("\tNotify\n");

	if (char_prop & GATT_CHAR_PROP_BIT_INDICATE)
		BTT_LOG_S("\tIndicate\n");

	if (char_prop & GATT_CHAR_PROP_BIT_AUTH)
		BTT_LOG_S("\tSigned Write\n");

	if (char_prop & GATT_CHAR_PROP_BIT_EXT_PROP)
		BTT_LOG_S("\tExtended properties\n");

}

/* 4 hex-number as argument, like FFFF */
static void run_gatt_client_register_client(int argc, char **argv)
{
	char input[256];
	struct btt_gatt_client_register_client req;

	sscanf(argv[1], "%s", input);

	if (!sscanf_UUID(input, req.UUID.uu, FALSE, FALSE)) {
		BTT_LOG_S("Error: Incorrect UUID\n");
		return;
	}

	process_request(BTT_GATT_CLIENT_REQ_REGISTER_CLIENT, &req,
			DEFAULT_TIME_SEC);
}

static void run_gatt_client_un_register_client(int argc, char **argv)
{
	struct btt_gatt_client_unregister_client req;

	sscanf(argv[1], "%d", &req.client_if);
	process_request(BTT_GATT_CLIENT_REQ_UNREGISTER_CLIENT, &req,
			DEFAULT_TIME_SEC);
}

static void run_gatt_client_connect(int argc, char **argv)
{
	struct btt_gatt_client_connect req;

	sscanf(argv[1], "%d", &req.client_if);

	if(!sscanf_bdaddr(argv[2], req.addr.address)) {
		BTT_LOG_S("Error: Incorrect address\n");
		return;
	}

	sscanf(argv[3], "%d", &req.is_direct);

	process_request(BTT_GATT_CLIENT_REQ_CONNECT, &req, DEFAULT_TIME_SEC);
}

static void run_gatt_client_disconnect(int argc, char **argv)
{
	struct btt_gatt_client_disconnect req;

	sscanf(argv[1], "%d", &req.client_if);

	if(!sscanf_bdaddr(argv[2], req.addr.address)) {
		BTT_LOG_S("Error: Incorrect address\n");
		return;
	}

	sscanf(argv[3], "%d", &req.conn_id);

	process_request(BTT_GATT_CLIENT_REQ_DISCONNECT, &req, DEFAULT_TIME_SEC);
}

static void run_gatt_client_read_remote_rssi(int argc, char **argv)
{
	struct btt_gatt_client_read_remote_rssi req;

	if(!sscanf_bdaddr(argv[1], req.addr.address)) {
		BTT_LOG_S("Error: Incorrect address\n");
		return;
	}

	sscanf(argv[2], "%d", &req.client_if);
	process_request(BTT_GATT_CLIENT_REQ_READ_REMOTE_RSSI, &req,
			DEFAULT_TIME_SEC);
}

static void run_gatt_client_listen(int argc, char **argv)
{
	struct btt_gatt_client_listen req;

	sscanf(argv[1], "%d", &req.client_if);
	sscanf(argv[2], "%d", &req.start);
	process_request(BTT_GATT_CLIENT_REQ_LISTEN, &req, DEFAULT_TIME_SEC);
}

static void run_gatt_client_set_adv_data_basic(int argc, char **argv)
{
	struct btt_gatt_client_set_adv_data req;

	sscanf(argv[1], "%d", &req.server_if);
	sscanf(argv[2], "%d", &req.set_scan_rsp);
	sscanf(argv[3], "%d", &req.include_name);
	sscanf(argv[4], "%d", &req.include_txpower);
	sscanf(argv[5], "%d", &req.min_interval);
	sscanf(argv[6], "%d", &req.max_interval);
	sscanf(argv[7], "%d", &req.appearance);

	req.service_data_len = 0;
	req.manufacturer_len = 0;
	req.service_uuid_len = 0;
	process_request(BTT_GATT_CLIENT_REQ_SET_ADV_DATA, &req, DEFAULT_TIME_SEC);
}

/* default settings of advertisement data taken:
 * - include name
 * - include txpower
 * - exclude appearance info
 * - exclude info about interval
 * - set scan response */
static void run_gatt_client_set_adv_data(int argc, char **argv)
{
	struct btt_gatt_client_set_adv_data req;
	unsigned char hex[256];
	int len = 0;

	sscanf(argv[1], "%d", &req.server_if);

	if ((len = string_to_hex(argv[2], hex)) < 0)
		return;

	memcpy(req.manufacturer_data, hex, len);
	req.manufacturer_len = (uint16_t) len;

	if ((len = string_to_hex(argv[3], hex)) < 0)
		return;

	memcpy(req.service_data, hex, len);
	req.service_data_len = (uint16_t) len;

	if ((len = string_to_hex(argv[4], hex)) < 0)
		return;

	memcpy(req.service_uuid, hex, len);
	req.service_uuid_len = (uint16_t) len;

	req.include_name = 1;
	req.include_txpower = 1;
	req.appearance = 0;
	req.min_interval = 0;
	req.max_interval = 0;
	req.set_scan_rsp = 1;

	process_request(BTT_GATT_CLIENT_REQ_SET_ADV_DATA, &req, DEFAULT_TIME_SEC);
}

static void run_gatt_client_get_device_type(int argc, char **argv)
{
	struct btt_gatt_client_get_device_type req;

	if (!sscanf_bdaddr(argv[1], req.addr.address)) {
		BTT_LOG_S("Error: Incorrect address\n");
		return;
	}

	process_request(BTT_GATT_CLIENT_REQ_GET_DEVICE_TYPE, &req, DEFAULT_TIME_SEC);
}

static void run_gatt_client_refresh(int argc, char **argv)
{
	struct btt_gatt_client_refresh req;

	sscanf(argv[1], "%d", &req.client_if);

	if (!sscanf_bdaddr(argv[2], req.addr.address)) {
		BTT_LOG_S("Error: Incorrect address\n");
		return;
	}

	process_request(BTT_GATT_CLIENT_REQ_REFRESH, &req, DEFAULT_TIME_SEC);
}

static bool process_UUID_sscanf(char *src, uint8_t *dest)
{
	if (strlen(src) == 4) {
		if (!sscanf_UUID(src, dest, TRUE, FALSE)) {
			BTT_LOG_S("Error: Incorrect UUID\n");
			return FALSE;
		}
	} else if (strlen(src) == 36) {
		if (!sscanf_UUID_128(src, dest, TRUE, FALSE)) {
			BTT_LOG_S("Error: Incorrect UUID\n");
			return FALSE;
		}
	} else {
		BTT_LOG_S("Error: Incorrect UUID\n");
		return FALSE;
	}

	return TRUE;
}

static void run_gatt_client_search_service(int argc, char **argv)
{
	struct btt_gatt_client_search_service req;
	char input[256];

	sscanf(argv[1], "%d", &req.conn_id);

	if (argc == 3) {
		sscanf(argv[2], "%s", input);
		req.is_filter = 1;

		if (!process_UUID_sscanf(input, req.filter_uuid.uu))
			return;
	} else {
		req.is_filter = 0;
	}

	process_request(BTT_GATT_CLIENT_REQ_SEARCH_SERVICE, &req, LONG_TIME_SEC);
}

static void run_gatt_client_get_included_service(int argc, char **argv)
{
	struct btt_gatt_client_get_included_service req;
	char input[256];

	if (argc == 5) {
		req.is_start = 0;
	} else if (argc == 8) {
		req.is_start = 1;
	} else {
		BTT_LOG_S("Error: Incorrect number of arguments\n");
		return;
	}

	sscanf(argv[1], "%d", &req.conn_id);
	sscanf(argv[2], "%s", input);

	if (!process_UUID_sscanf(input, req.srvc_id.id.uuid.uu))
		return;

	sscanf(argv[3], "%"SCNd8"", &req.srvc_id.is_primary);
	sscanf(argv[4], "%"SCNd8"", &req.srvc_id.id.inst_id);

	if (req.is_start) {
		sscanf(argv[5], "%s", input);

		if (!process_UUID_sscanf(input, req.start_incl_srvc_id.id.uuid.uu))
			return;

		sscanf(argv[6], "%"SCNd8, &req.start_incl_srvc_id.is_primary);
		sscanf(argv[7], "%"SCNd8, &req.start_incl_srvc_id.id.inst_id);
	}

	process_request(BTT_GATT_CLIENT_REQ_GET_INCLUDED_SERVICE, &req,
			DEFAULT_TIME_SEC);
}

static void run_gatt_client_get_characteristic(int argc, char **argv)
{
	struct btt_gatt_client_get_characteristic req;
	char input[256];

	if (argc == 5) {
		req.is_start = 0;
	} else if (argc == 7) {
		req.is_start = 1;
	} else {
		BTT_LOG_S("Error: Incorrect number of arguments\n");
		return;
	}

	sscanf(argv[1], "%d", &req.conn_id);
	sscanf(argv[2], "%s", input);

	if (!process_UUID_sscanf(input, req.srvc_id.id.uuid.uu))
		return;

	sscanf(argv[3], "%"SCNd8"", &req.srvc_id.is_primary);
	sscanf(argv[4], "%"SCNd8"", &req.srvc_id.id.inst_id);

	if (req.is_start) {
		sscanf(argv[5], "%s", input);

		if (!process_UUID_sscanf(input, req.start_char_id.uuid.uu))
			return;

		sscanf(argv[6], "%"SCNd8"", &req.start_char_id.inst_id);
	}

	process_request(BTT_GATT_CLIENT_REQ_GET_CHARACTERISTIC, &req,
			DEFAULT_TIME_SEC);
}

static void run_gatt_client_get_descriptor(int argc, char **argv)
{
	struct btt_gatt_client_get_descriptor req;
	char input[256];

	if (argc == 7) {
		req.is_start = 0;
	} else if (argc == 9) {
		req.is_start = 1;
	} else {
		BTT_LOG_S("Error: Incorrect number of arguments\n");
		return;
	}

	sscanf(argv[1], "%d", &req.conn_id);
	sscanf(argv[2], "%s", input);

	if (!process_UUID_sscanf(input, req.srvc_id.id.uuid.uu))
		return;

	sscanf(argv[3], "%"SCNd8"", &req.srvc_id.is_primary);
	sscanf(argv[4], "%"SCNd8"", &req.srvc_id.id.inst_id);

	sscanf(argv[5], "%s", input);

	if (!process_UUID_sscanf(input, req.char_id.uuid.uu))
		return;

	sscanf(argv[6], "%"SCNd8"", &req.char_id.inst_id);

	if (req.is_start) {
		sscanf(argv[7], "%s", input);

		if (!process_UUID_sscanf(input, req.start_descr_id.uuid.uu))
			return;

		sscanf(argv[8], "%"SCNd8"", &req.start_descr_id.inst_id);
	}

	process_request(BTT_GATT_CLIENT_REQ_GET_DESCRIPTOR, &req,
			DEFAULT_TIME_SEC);
}

static void run_gatt_client_read_characteristic(int argc, char **argv)
{
	struct btt_gatt_client_read_characteristic req;
	char input[256];

	sscanf(argv[1], "%d", &req.conn_id);
	sscanf(argv[2], "%s", input);

	if (!process_UUID_sscanf(input, req.srvc_id.id.uuid.uu))
		return;

	sscanf(argv[3], "%"SCNd8"", &req.srvc_id.is_primary);
	sscanf(argv[4], "%"SCNd8"", &req.srvc_id.id.inst_id);

	sscanf(argv[5], "%s", input);

	if (!process_UUID_sscanf(input, req.char_id.uuid.uu))
		return;

	sscanf(argv[6], "%"SCNd8"", &req.char_id.inst_id);
	/* Types of auth_req:
	 * 0 - NONE
	 * 1 - ENCRIPTION
	 * 2 - AUTHENTICATION (MITM) */
	sscanf(argv[7], "%d", &req.auth_req);

	process_request(BTT_GATT_CLIENT_REQ_READ_CHARACTERISTIC, &req,
			LONG_TIME_SEC);
}

static void run_gatt_client_read_descriptor(int argc, char **argv)
{
	struct btt_gatt_client_read_descriptor req;
	char input[256];

	sscanf(argv[1], "%d", &req.conn_id);
	sscanf(argv[2], "%s", input);

	if (!process_UUID_sscanf(input, req.srvc_id.id.uuid.uu))
		return;

	sscanf(argv[3], "%"SCNd8"", &req.srvc_id.is_primary);
	sscanf(argv[4], "%"SCNd8"", &req.srvc_id.id.inst_id);

	sscanf(argv[5], "%s", input);

	if (!process_UUID_sscanf(input, req.char_id.uuid.uu))
		return;

	sscanf(argv[6], "%"SCNd8"", &req.char_id.inst_id);

	sscanf(argv[7], "%s", input);

	if (!process_UUID_sscanf(input, req.descr_id.uuid.uu))
		return;

	sscanf(argv[8], "%"SCNd8"", &req.descr_id.inst_id);
	sscanf(argv[9], "%d", &req.auth_req);

	process_request(BTT_GATT_CLIENT_REQ_READ_DESCRIPTOR, &req,
			DEFAULT_TIME_SEC);
}

static void run_gatt_client_write_characteristic(int argc, char **argv)
{
	struct btt_gatt_client_write_characteristic req;
	char input[256];

	sscanf(argv[1], "%d", &req.conn_id);
	sscanf(argv[2], "%s", input);

	if (!process_UUID_sscanf(input, req.srvc_id.id.uuid.uu))
		return;

	sscanf(argv[3], "%"SCNd8"", &req.srvc_id.is_primary);
	sscanf(argv[4], "%"SCNd8"", &req.srvc_id.id.inst_id);
	sscanf(argv[5], "%s", input);

	if (!process_UUID_sscanf(input, req.char_id.uuid.uu))
		return;

	sscanf(argv[6], "%"SCNd8"", &req.char_id.inst_id);
	sscanf(argv[7], "%d", &req.write_type);
	/* Types of auth_req:
	 * 0 - NONE
	 * 1 - ENCRIPTION
	 * 2 - AUTHENTICATION (MITM) */
	sscanf(argv[8], "%d", &req.auth_req);
	sscanf(argv[9], "%s", input);
	req.len = string_to_hex(input, (uint8_t *) &req.p_value);

	if (req.len < 0) {
		BTT_LOG_S("Error: Incorrect hex value.\n");
		return;
	}

	process_request(BTT_GATT_CLIENT_REQ_WRITE_CHARACTERISTIC, &req,
			DEFAULT_TIME_SEC);
}

static void run_gatt_client_execute_write(int argc, char **argv)
{
	struct btt_gatt_client_execute_write req;
	char input[256];

	sscanf(argv[1], "%d", &req.conn_id);
	sscanf(argv[2], "%d", &req.execute);

	process_request(BTT_GATT_CLIENT_REQ_EXECUTE_WRITE, &req,
			DEFAULT_TIME_SEC);
}

static void run_gatt_client_write_descriptor(int argc, char **argv)
{
	struct btt_gatt_client_write_descriptor req;
	char input[256];

	sscanf(argv[1], "%d", &req.conn_id);
	sscanf(argv[2], "%s", input);

	if (!process_UUID_sscanf(input, req.srvc_id.id.uuid.uu))
		return;

	sscanf(argv[3], "%"SCNd8"", &req.srvc_id.is_primary);
	sscanf(argv[4], "%"SCNd8"", &req.srvc_id.id.inst_id);
	sscanf(argv[5], "%s", input);

	if (!process_UUID_sscanf(input, req.char_id.uuid.uu))
		return;

	sscanf(argv[6], "%"SCNd8"", &req.char_id.inst_id);
	sscanf(argv[7], "%s", input);

	if (!process_UUID_sscanf(input, req.descr_id.uuid.uu))
		return;

	sscanf(argv[8], "%"SCNd8"", &req.descr_id.inst_id);
	sscanf(argv[9], "%d", &req.write_type);
	/* Types of auth_req:
	 * 0 - NONE
	 * 1 - ENCRIPTION
	 * 2 - AUTHENTICATION (MITM) */
	sscanf(argv[10], "%d", &req.auth_req);
	sscanf(argv[11], "%s", input);
	req.len = string_to_hex(input, (uint8_t *) &req.p_value);

	if (req.len < 0) {
		BTT_LOG_S("Error: Incorrect hex value.\n");
		return;
	}

	process_request(BTT_GATT_CLIENT_REQ_WRITE_DESCRIPTOR, &req,
			DEFAULT_TIME_SEC);
}

static void run_gatt_client_reg_for_notification(int argc, char **argv)
{
	struct btt_gatt_client_reg_for_notification req;
	char input[256];

	sscanf(argv[1], "%d", &req.client_if);

	if(!sscanf_bdaddr(argv[2], req.addr.address)) {
		BTT_LOG_S("Error: Incorrect address\n");
		return;
	}

	sscanf(argv[3], "%s", input);

	if (!process_UUID_sscanf(input, req.srvc_id.id.uuid.uu))
		return;

	sscanf(argv[4], "%"SCNd8"", &req.srvc_id.is_primary);
	sscanf(argv[5], "%"SCNd8"", &req.srvc_id.id.inst_id);
	sscanf(argv[6], "%s", input);

	if (!process_UUID_sscanf(input, req.char_id.uuid.uu))
		return;

	sscanf(argv[7], "%"SCNd8"", &req.char_id.inst_id);

	process_request(BTT_GATT_CLIENT_REQ_REGISTER_FOR_NOTIFICATION, &req,
			DEFAULT_TIME_SEC);
}

static void run_gatt_client_dereg_for_notification(int argc, char **argv)
{
	struct btt_gatt_client_dereg_for_notification req;
	char input[256];

	sscanf(argv[1], "%d", &req.client_if);

	if(!sscanf_bdaddr(argv[2], req.addr.address)) {
		BTT_LOG_S("Error: Incorrect address\n");
		return;
	}

	sscanf(argv[3], "%s", input);

	if (!process_UUID_sscanf(input, req.srvc_id.id.uuid.uu))
		return;

	sscanf(argv[4], "%"SCNd8"", &req.srvc_id.is_primary);
	sscanf(argv[5], "%"SCNd8"", &req.srvc_id.id.inst_id);
	sscanf(argv[6], "%s", input);

	if (!process_UUID_sscanf(input, req.char_id.uuid.uu))
		return;

	sscanf(argv[7], "%"SCNd8"", &req.char_id.inst_id);

	process_request(BTT_GATT_CLIENT_REQ_DEREGISTER_FOR_NOTIFICATION, &req,
			DEFAULT_TIME_SEC);
}

static void run_gatt_client_test_command(int argc, char **argv)
{
	struct btt_gatt_client_test_command req;
	char input[256];

	sscanf(argv[1], "%d", &req.command);

	if(!sscanf_bdaddr(argv[2], req.bda1.address)) {
		BTT_LOG_S("Error: Incorrect address\n");
		return;
	}

	sscanf(argv[3], "%s", input);

	if (!process_UUID_sscanf(input, req.uuid1.uu))
		return;

	if (argc > 4) {
		sscanf(argv[4], "%"SCNd16"", &req.u1);

		if (argc > 5) {
			sscanf(argv[5], "%"SCNd16"", &req.u2);

			if (argc > 6) {
				sscanf(argv[6], "%"SCNd16"", &req.u3);

				if (argc > 7) {
					sscanf(argv[7], "%"SCNd16"", &req.u4);

					if (argc > 8)
						sscanf(argv[8], "%"SCNd16"", &req.u5);
				}
			}
		}
	}

	process_request(BTT_GATT_CLIENT_REQ_TEST_COMMAND, &req,
			DEFAULT_TIME_SEC);
}
