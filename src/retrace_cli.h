/*
 * Copyright (c) 2017, [Ribose Inc](https://www.ribose.com).
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef SRC_RETRACE_CLI_H_
#define SRC_RETRACE_CLI_H_

#define CLI_MAX_CMD_NAME_LEN 20

//#define cli_err(fmt, ...) cli_printf("[ERROR] %s:%u" #fmt, __FILE__, __LINE__, __VA_ARGS__)

/**
 * @brief Command information. Used during command registration
 *
 * @see cli_register_command_blk
 */
typedef struct
{
	/* name of command, as it will appear on cli */
	char name[CLI_MAX_CMD_NAME_LEN];

	/* command function, will be called when command is selected */
	void (*func)(void);
} cli_cmd_t;

/**
 * @brief Initialize cli
 *
 * Opens pseudo terminal for cli and return its file path,
 * so a terminal emulator can be connected.
 * Must be called before any other calls to cli functions can be made.
 * Not threadsafe.
 *
 * @param[out] pts_path PTS file path
 * @param[in] path_len size of pts_path in bytes
 *
 * @return 0 in case of success.
 *
 */
int cli_init(char *pts_path, size_t path_len);


/**
 * @brief Main cli loop
 *
 * Implements command dispatching loop. Never returns.
 * Not threadsafe - only one instance is intended to be run.
 *
 */
void cli_run(void);

/**
 * @brief Register command block
 *
 * Registers a command block which will be presented on cli
 * during cli_run(). Each command is assigned with an ID.
 * User selects a command by entering its ID.
 * Tested with minicom.
 * Not threadsafe.
 *
 * @param[in] cmd_blk Array of command blocks,
 * end of block must be marked by empty entry.
 *
 * @return 0 in case of success, -1 in case of error
 */
int cli_register_command_blk(const cli_cmd_t *cmd_blk);

/**
 * @brief Pseudo terminal version of printf
 *
 * Intended to be used only with raw terminal mode.
 * Tested with minicom.
 * Threadsafe.
 *
 * @param format as of printf
 * @param ... as of printf
 * @return as of printf
 */
int cli_printf(const char *format, ...);

/**
 * @brief Pseudo terminal version of scanf
 *
 * Intended to be used only with raw terminal mode.
 * Tested only with single integer input.
 * End of input is indicated by '\r' character. Tested with minicom.
 * Threadsafe.
 *
 * @param format as of scanf
 * @param ... as of scanf
 * @return as of scanf
 */
int cli_scanf(const char *format, ...);

#endif /* SRC_RETRACE_CLI_H_ */
