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

#include "common.h"
#include "str.h"

#include "plugin.h"

// retrace plugin data
typedef struct rtr_plugin
{
        int exist;

        char name[RTR_MAX_PLUGIN_NAME_LEN];
        char so_path[RTR_MAX_PATH];

        void *handle;
        void *data;
} rtr_plugin_t;

// retrace plugin context
typedef struct rtr_plugin_context
{
        rtr_plugin_t plugins[RTR_PLUGIN_TYPE_MAX];
        int plugins_count;
} rtr_plugin_context_t;

static rtr_plugin_context_t g_plugin_ctx;
static int g_plugin_init = 0;

// initialize plugin context
static int rtr_plugin_ctx_init()
{
        rtr_config config = NULL;

        int plugin_type;
        char *plugin_name = NULL, *plugin_path = NULL;

        // check plugin has been inited already
        if (g_plugin_init)
                return 0;

        fprintf(stderr, "initialize rtr plugin\n");

        // initialize plugin context structure
        memset(&g_plugin_ctx, 0, sizeof(g_plugin_ctx));

        while (rtr_get_config_multiple(&config, "plugin",
                        ARGUMENT_TYPE_INT,
                        ARGUMENT_TYPE_STRING,
                        ARGUMENT_TYPE_STRING,
                        ARGUMENT_TYPE_STRING,
                        ARGUMENT_TYPE_END,
                        &plugin_type,
                        &plugin_name,
                        &plugin_path))
        {
                // validate plugin type
                if (plugin_type < RTR_PLUGIN_TYPE_SOCK || plugin_type >= RTR_PLUGIN_TYPE_MAX)
                {
                        trace_printf(1, "Unknown plugin type '%d'\n", plugin_type);

                        free(plugin_name);
                        free(plugin_path);

                        continue;
                }

                trace_printf(1, "Find plugin with type:%d, name:%s, path:%s\n", plugin_type, plugin_name, plugin_path);

                rtr_plugin_t *plugin = &g_plugin_ctx.plugins[plugin_type];

                // check plugin is already exist
                if (plugin->exist)
                {
                        trace_printf(1, "The plugin with type %d has already exist.\n", plugin_type);

                        free(plugin_name);
                        free(plugin_path);

                        continue;
                }

                // load plugin library
                plugin->handle = dlopen(plugin_path, RTLD_LOCAL | RTLD_LAZY);
                if (!plugin->handle)
                {
                        trace_printf(1, "Could not load %s plugin from '%s'\n", plugin_name, plugin_path);
                }
                else
                {
                        // register plugin
                        int (*register_func)(void **plugin_data) = dlsym(plugin->handle, "rtr_plugin_register");
                        if (register_func && register_func(&plugin->data) == 0)
                        {
                                real_strcpy = RETRACE_GET_REAL(strcpy);

                                real_strcpy(plugin->name, plugin_name);
                                real_strcpy(plugin->so_path, plugin_path);

                                plugin->exist = 1;
                        }
                }

                free(plugin_name);
                free(plugin_path);
        }

        // free config
        if (config)
                rtr_confing_close(config);

        // set init flag
        g_plugin_init = 1;

        return 0;
}

// get plugin for type
void *rtr_plugin_get(int plugin_type)
{
        // check plugin context was init
        if (!g_plugin_init)
                rtr_plugin_ctx_init();

        // get plugin data by type
        rtr_plugin_t *plugin = &g_plugin_ctx.plugins[plugin_type];
        if (!plugin->exist)
                return NULL;

        return plugin->data;
}
