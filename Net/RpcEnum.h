#pragma once
#include <iostream>

// maybe do an automated-generation process here
// server开头指的是发送到server的rpc
// client开头指的是发送到client的rpc
enum RpcEnum : std::uint16_t
{
	rpc_connect,
	rpc_debug,

	rpc_server_tick,
	rpc_server_error_respond,
	rpc_server_register,
	rpc_server_log_in,
	rpc_server_set_name,
	rpc_server_set_language,
	rpc_server_send_text,
	rpc_server_print_user,
	rpc_server_print_room,
	rpc_server_goto_room,
	rpc_server_leave_room,
	rpc_server_get_my_rooms,
	rpc_server_create_room,
	rpc_server_refresh_user_info,

	rpc_client_tick,
	rpc_client_error_respond,
	//rpc_client_register,
	rpc_client_log_in,
	rpc_client_set_name,
	rpc_client_set_language,
	rpc_client_send_text,
	rpc_client_print_user,
	rpc_client_print_room,
	rpc_client_goto_room,
	rpc_client_leave_room,
	rpc_client_get_my_rooms,
	rpc_client_create_room,
	rpc_client_refresh_user_info,

	// poker room rpc
	rpc_server_get_poker_table_info,
	rpc_client_get_poker_table_info,
	rpc_server_sit_down,
	rpc_client_sit_down,
	rpc_server_poker_action,
	rpc_client_poker_hand_result,
	rpc_server_poker_buyin,
	rpc_client_poker_buyin,
	rpc_server_poker_standup,
	rpc_client_poker_standup,
	rpc_server_poker_set_blinds,
	rpc_client_poker_set_blinds,

	INVALID,
};