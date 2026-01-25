#pragma once

#define IS_CPP_SERVER

// how many chips does a new account start with
#define USER_ACCOUNT_START_CHIP 1000000

// sql command to create a user accounts
#define CREATE_USER_ACCOUNT_SQL_CMD "INSERT INTO `wkr_server_schema`.`user` (`_name`, `pswd`, `lang`) VALUES ('{0}', '{1}', '{2}');"
#define CREATE_USER_ASSET_SQL_CMD "INSERT INTO `wkr_server_schema`.`user_asset` (`chip`, `_id`) VALUES ('{0}', LAST_INSERT_ID);"

// toggle for sql debug
#define ENABLE_SQL_DEBUG