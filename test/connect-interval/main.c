/*
 * Copyright (c) 2014-2018, Alper Akcan <alper.akcan@gmail.com>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *    * Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *    * Redistributions in binary form must reproduce the above copyright
 *      notice, this list of conditions and the following disclaimer in the
 *      documentation and/or other materials provided with the distribution.
 *    * Neither the name of the copyright holder nor the
 *      names of its contributors may be used to endorse or promote products
 *      derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL <COPYRIGHT HOLDER> BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>

#include <mbus/client.h>

#define OPTION_HELP             'h'
static struct option longopts[] = {
        { "help",                       no_argument,            NULL,   OPTION_HELP },
        { NULL,                         0,                      NULL,   0 },
};

static void usage (const char *pname)
{
        fprintf(stdout, "%s arguments:\n", pname);
        fprintf(stdout, "  -h, --help               : this text\n");
        fprintf(stdout, "  --mbus-help              : mbus help text\n");
        mbus_client_usage();
}

static void mbus_client_callback_connect (struct mbus_client *client, void *context, enum mbus_client_connect_status status)
{
        (void) client;
        (void) context;
        fprintf(stderr, "connect status: %d, %s\n", status, mbus_client_connect_status_string(status));
}

int main (int argc, char *argv[])
{
        int rc;

        int c;
        int _argc;
        char **_argv;

        struct mbus_client *mbus_client;
        struct mbus_client_options mbus_client_options;

        mbus_client = NULL;

        _argc = 0;
        _argv = NULL;

        _argv = malloc(sizeof(char *) * argc);
        if (_argv == NULL) {
                fprintf(stderr, "can not allocate memory\n");
                goto bail;
        }
        for (_argc = 0; _argc < argc; _argc++) {
                _argv[_argc] = argv[_argc];
        }

        while ((c = getopt_long(_argc, _argv, ":h", longopts, NULL)) != -1) {
                switch (c) {
                        case OPTION_HELP:
                                usage(argv[0]);
                                goto bail;
                }
        }

        while (1) {
                fprintf(stderr, "--> loop\n");

                if (mbus_client == NULL) {
                        rc = mbus_client_options_default(&mbus_client_options);
                        if (rc != 0) {
                                fprintf(stderr, "can not get default options\n");
                                goto bail;
                        }
                        rc = mbus_client_options_from_argv(&mbus_client_options, argc, argv);
                        if (rc != 0) {
                                fprintf(stderr, "can not parse options\n");
                                goto bail;
                        }
                        mbus_client_options.callbacks.connect = mbus_client_callback_connect;
                        mbus_client_options.callbacks.context = NULL;
                        mbus_client = mbus_client_create(&mbus_client_options);
                        if (mbus_client == NULL) {
                                fprintf(stderr, "can not create mbus client\n");
                                usleep(1000000);
                                continue;
                        }
                        rc = mbus_client_connect(mbus_client);
                        if (rc != 0) {
                                fprintf(stderr, "MBUS:can not connect to server\n");
                                continue;
                        }
                }

                fprintf(stderr, "  state: %d, %s\n", mbus_client_get_state(mbus_client), mbus_client_state_string(mbus_client_get_state(mbus_client)));
                rc = mbus_client_run(mbus_client, 1000);
                if (rc < 0) {
                        fprintf(stderr, "client run failed: %d\n", rc);
                        mbus_client_destroy(mbus_client);
                        mbus_client = NULL;
                        usleep(5000000);
                        continue;
                }
                if (rc > 1) {
                        fprintf(stderr, "client run exited: %d\n", rc);
                        mbus_client_destroy(mbus_client);
                        mbus_client = NULL;
                        usleep(1000000);
                        continue;
                }
        }

        mbus_client_destroy(mbus_client);
        free(_argv);
        return 0;
bail:   if (mbus_client != NULL) {
                mbus_client_destroy(mbus_client);
        }
        if (_argv != NULL) {
                free(_argv);
        }
        return -1;
}
