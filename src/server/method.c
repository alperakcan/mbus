
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

#include "mbus/debug.h"
#include "mbus/tailq.h"
#include "mbus/json.h"
#include "mbus/method.h"

#include "method.h"

struct private {
	struct method method;
	struct {
		struct mbus_json *json;
		char *string;
	} request;
	struct {
		struct mbus_json *json;
		char *string;
	} result;
	struct client *source;
};

const char * mbus_server_method_get_request_type (struct method *method)
{
	struct private *private;
	if (method == NULL) {
		return NULL;
	}
	private = (struct private *) method;
	return mbus_json_get_string_value(private->request.json, MBUS_METHOD_TAG_TYPE, NULL);
}

const char * mbus_server_method_get_request_destination (struct method *method)
{
	struct private *private;
	if (method == NULL) {
		return NULL;
	}
	private = (struct private *) method;
	return mbus_json_get_string_value(private->request.json, MBUS_METHOD_TAG_DESTINATION, NULL);
}

const char * mbus_server_method_get_request_identifier (struct method *method)
{
	struct private *private;
	if (method == NULL) {
		return NULL;
	}
	private = (struct private *) method;
	return mbus_json_get_string_value(private->request.json, MBUS_METHOD_TAG_IDENTIFIER, NULL);
}

int mbus_server_method_get_request_sequence (struct method *method)
{
	struct private *private;
	if (method == NULL) {
		return -1;
	}
	private = (struct private *) method;
	return mbus_json_get_int_value(private->request.json, MBUS_METHOD_TAG_SEQUENCE, -1);
}

struct mbus_json * mbus_server_method_get_request_payload (struct method *method)
{
	struct private *private;
	if (method == NULL) {
		return NULL;
	}
	private = (struct private *) method;
	return mbus_json_get_object(private->request.json, MBUS_METHOD_TAG_PAYLOAD);
}

char * mbus_server_method_get_request_string (struct method *method)
{
	struct private *private;
	if (method == NULL) {
		return NULL;
	}
	private = (struct private *) method;
	if (private->request.string != NULL) {
		free(private->request.string);
	}
	private->request.string = mbus_json_print_unformatted(private->request.json);
	return private->request.string;
}

int mbus_server_method_set_result_code (struct method *method, int code)
{
	struct private *private;
	if (method == NULL) {
		return -1;
	}
	private = (struct private *) method;
	mbus_json_add_number_to_object_cs(private->result.json, MBUS_METHOD_TAG_STATUS, code);
	return 0;
}

int mbus_server_method_set_result_payload (struct method *method, struct mbus_json *payload)
{
	struct private *private;
	if (method == NULL) {
		return -1;
	}
	private = (struct private *) method;
	mbus_json_delete_item_from_object(private->result.json, MBUS_METHOD_TAG_PAYLOAD);
	mbus_json_add_item_to_object_cs(private->result.json, MBUS_METHOD_TAG_PAYLOAD, payload);
	return 0;
}

char * mbus_server_method_get_result_string (struct method *method)
{
	struct private *private;
	if (method == NULL) {
		return NULL;
	}
	private = (struct private *) method;
	if (private->result.string != NULL) {
		free(private->result.string);
	}
	private->result.string = mbus_json_print_unformatted(private->result.json);
	return private->result.string;
}

struct client * mbus_server_method_get_source (struct method *method)
{
	struct private *private;
	if (method == NULL) {
		return NULL;
	}
	private = (struct private *) method;
	return private->source;
}

void mbus_server_method_destroy (struct method *method)
{
	struct private *private;
	if (method == NULL) {
		return;
	}
	private = (struct private *) method;
	if (private->request.json != NULL) {
		mbus_json_delete(private->request.json);
	}
	if (private->request.string != NULL) {
		free(private->request.string);
	}
	if (private->result.json != NULL) {
		mbus_json_delete(private->result.json);
	}
	if (private->result.string != NULL) {
		free(private->result.string);
	}
	if (private->source != NULL) {
		private->source = NULL;
	}
	free(private);
}

struct method * mbus_server_method_create_request (struct client *source, const char *string)
{
	struct private *private;
	private = NULL;
	if (string == NULL) {
		mbus_errorf("string is null");
		goto bail;
	}
	private = malloc(sizeof(struct private));
	if (private == NULL) {
		mbus_errorf("can not allocate memory");
		goto bail;
	}
	memset(private, 0, sizeof(struct private));
	private->request.json = mbus_json_parse(string);
	if (private->request.json == NULL) {
		mbus_errorf("can not parse method");
		goto bail;
	}
	if (mbus_json_get_string_value(private->request.json, MBUS_METHOD_TAG_TYPE, NULL) == NULL) {
		mbus_errorf("invalid method type: '%s'", string);
		goto bail;
	}
	if (mbus_json_get_string_value(private->request.json, MBUS_METHOD_TAG_DESTINATION, NULL) == NULL) {
		mbus_errorf("invalid method destination: '%s'", string);
		goto bail;
	}
	if (mbus_json_get_string_value(private->request.json, MBUS_METHOD_TAG_IDENTIFIER, NULL) == NULL) {
		mbus_errorf("invalid method identifier: '%s'", string);
		goto bail;
	}
	if (mbus_json_get_int_value(private->request.json, MBUS_METHOD_TAG_SEQUENCE, -1) == -1) {
		mbus_errorf("invalid method sequence: '%s'", string);
		goto bail;
	}
	if (mbus_json_get_object(private->request.json, MBUS_METHOD_TAG_PAYLOAD) == NULL) {
		mbus_errorf("invalid method payload: '%s'", string);
		goto bail;
	}
	private->result.json = mbus_json_create_object();
	if (private->result.json == NULL) {
		mbus_errorf("can not create object");
		goto bail;
	}
	mbus_json_add_string_to_object_cs(private->result.json, MBUS_METHOD_TAG_TYPE, MBUS_METHOD_TYPE_RESULT);
	mbus_json_add_number_to_object_cs(private->result.json, MBUS_METHOD_TAG_SEQUENCE, mbus_server_method_get_request_sequence(&private->method));
	mbus_json_add_item_to_object_cs(private->result.json, MBUS_METHOD_TAG_PAYLOAD, mbus_json_create_object());
	private->source = source;
	return &private->method;
bail:	if (private != NULL) {
		mbus_server_method_destroy(&private->method);
	}
	return NULL;
}

struct method * mbus_server_method_create_response (const char *type, const char *source, const char *identifier, int sequence, const struct mbus_json *payload)
{
	struct private *private;
	struct mbus_json *data;
	private = NULL;
	if (type == NULL) {
		mbus_errorf("type is null");
		goto bail;
	}
	if (source == NULL) {
		mbus_errorf("source is null");
		goto bail;
	}
	if (identifier == NULL) {
		mbus_errorf("identifier is null");
		goto bail;
	}
	if (sequence < 0) {
		mbus_errorf("sequence is invalid");
		goto bail;
	}
	if (payload == NULL) {
		data = mbus_json_create_object();
	} else {
		data = mbus_json_duplicate(payload, 1);
	}
	if (data == NULL) {
		mbus_errorf("can not create payload");
		goto bail;
	}
	private = malloc(sizeof(struct private));
	if (private == NULL) {
		mbus_errorf("can not allocate memory");
		goto bail;
	}
	memset(private, 0, sizeof(struct private));
	private->request.json = mbus_json_create_object();
	if (private->request.json == NULL) {
		mbus_errorf("can not create method object");
		goto bail;
	}
	mbus_json_add_string_to_object_cs(private->request.json, MBUS_METHOD_TAG_TYPE, type);
	mbus_json_add_string_to_object_cs(private->request.json, MBUS_METHOD_TAG_SOURCE, source);
	mbus_json_add_string_to_object_cs(private->request.json, MBUS_METHOD_TAG_IDENTIFIER, identifier);
	mbus_json_add_number_to_object_cs(private->request.json, MBUS_METHOD_TAG_SEQUENCE, sequence);
	mbus_json_add_item_to_object_cs(private->request.json, MBUS_METHOD_TAG_PAYLOAD, data);
	return &private->method;
bail:	if (private != NULL) {
		mbus_server_method_destroy(&private->method);
	}
	return NULL;
}
