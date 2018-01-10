
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
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "cJSON/cJSON.h"

#include "json.h"

struct mbus_json * mbus_json_parse (const char *string)
{
	return (struct mbus_json *) mbus_cJSON_Parse(string);
}

struct mbus_json * mbus_json_parse_end (const char *string, const char **end)
{
	return (struct mbus_json *) mbus_cJSON_ParseWithOpts(string, end, 0);
}

struct mbus_json * mbus_json_parse_file (const char *path)
{
	int rc;
	FILE *fp;
	struct stat stbuf;
	char *buffer;
	struct mbus_json *mbus_json;
	fp = NULL;
	buffer = NULL;
	mbus_json = NULL;
	if (path == NULL) {
		goto bail;
	}
	rc = stat(path, &stbuf);
	if (rc != 0) {
		goto bail;
	}
	buffer = malloc(stbuf.st_size + 1);
	if (buffer == NULL) {
		goto bail;
	}
	fp = fopen(path, "r");
	if (fp == NULL) {
		goto bail;
	}
	rc = fread(buffer, 1, stbuf.st_size, fp);
	if (rc != stbuf.st_size) {
		goto bail;
	}
	buffer[stbuf.st_size] = '\0';
	mbus_json = (struct mbus_json *) mbus_cJSON_Parse(buffer);
	if (mbus_json == NULL) {
		goto bail;
	}
	fclose(fp);
	free(buffer);
	return mbus_json;
bail:	if (fp != NULL) {
		fclose(fp);
	}
	if (buffer != NULL) {
		free(buffer);
	}
	if (mbus_json != NULL) {
		mbus_json_delete(mbus_json);
	}
	return NULL;
}

struct mbus_json * mbus_json_create_object (void)
{
	return (struct mbus_json *) mbus_cJSON_CreateObject();
}

struct mbus_json * mbus_json_create_array (void)
{
	return (struct mbus_json *) mbus_cJSON_CreateArray();
}

int mbus_json_add_item_to_array (struct mbus_json *array, const struct mbus_json *item)
{
	mbus_cJSON_AddItemToArray((mbus_cJSON *) array, (mbus_cJSON *) item);
	return 0;
}

struct mbus_json * mbus_json_create_string (const char *string)
{
	return (struct mbus_json *) mbus_cJSON_CreateString(string);
}

struct mbus_json * mbus_json_create_number (double number)
{
	return (struct mbus_json *) mbus_cJSON_CreateNumber(number);
}

struct mbus_json * mbus_json_create_null (void)
{
	return (struct mbus_json *) mbus_cJSON_CreateNull();
}

void mbus_json_delete (struct mbus_json *json)
{
	mbus_cJSON_Delete((mbus_cJSON *) json);
}

int mbus_json_compare (const struct mbus_json *a, const struct mbus_json *b)
{
	const struct mbus_json *ac;
	const struct mbus_json *bc;
	if (mbus_json_get_type(a) != mbus_json_get_type(b)) {
		return -1;
	}
	switch (mbus_json_get_type(a)) {
		case mbus_json_type_number:
			if (mbus_json_get_value_number(a) != mbus_json_get_value_number(b)) {
				return -1;
			}
			break;
		case mbus_json_type_string:
			if (strcmp(mbus_json_get_value_string(a), mbus_json_get_value_string(b)) != 0) {
				return -1;
			}
			break;
		default:
			break;
	}
	ac = mbus_json_get_child(a);
	bc = mbus_json_get_child(b);
	while (ac != NULL && bc != NULL) {
		if (mbus_json_compare(ac, bc) != 0) {
			return -1;
		}
		ac = mbus_json_get_next(ac);
		bc = mbus_json_get_next(bc);
	}
	if (ac == NULL && bc != NULL) {
		return -1;
	}
	if (ac != NULL && bc == NULL) {
		return -1;
	}
	return 0;
}

struct mbus_json * mbus_json_get_child (const struct mbus_json *json)
{
	return (struct mbus_json *) (((mbus_cJSON *) json)->child);
}

struct mbus_json * mbus_json_get_next (const struct mbus_json *json)
{
	return (struct mbus_json *) (((mbus_cJSON *) json)->next);
}

enum mbus_json_type mbus_json_get_type (const struct mbus_json *json)
{
	if (json == NULL) {
		return mbus_json_type_unknown;
	}
	if (((mbus_cJSON *) json)->type & mbus_cJSON_False)	return mbus_json_type_false;
	if (((mbus_cJSON *) json)->type & mbus_cJSON_True)	return mbus_json_type_true;
	if (((mbus_cJSON *) json)->type & mbus_cJSON_NULL)	return mbus_json_type_null;
	if (((mbus_cJSON *) json)->type & mbus_cJSON_Number)	return mbus_json_type_number;
	if (((mbus_cJSON *) json)->type & mbus_cJSON_String)	return mbus_json_type_string;
	if (((mbus_cJSON *) json)->type & mbus_cJSON_Array)	return mbus_json_type_array;
	if (((mbus_cJSON *) json)->type & mbus_cJSON_Object)	return mbus_json_type_object;
	return mbus_json_type_unknown;
}

const char * mbus_json_get_name (const struct mbus_json *json)
{
	return ((mbus_cJSON *) json)->string;
}

const char * mbus_json_get_value_string (const struct mbus_json *json)
{
	if (json == NULL) {
		return NULL;
	}
	if (mbus_json_get_type(json) != mbus_json_type_string) {
		return NULL;
	}
	return ((mbus_cJSON *) json)->valuestring;
}

int mbus_json_get_value_int (const struct mbus_json *json)
{
	if (json == NULL) {
		return 0;
	}
	if (mbus_json_get_type(json) != mbus_json_type_number) {
		return 0;
	}
	return ((mbus_cJSON *) json)->valueint;
}

double mbus_json_get_value_number (const struct mbus_json *json)
{
	if (json == NULL) {
		return 0;
	}
	if (mbus_json_get_type(json) != mbus_json_type_number) {
		return 0;
	}
	return ((mbus_cJSON *) json)->valuedouble;
}

int mbus_json_set_value_number (const struct mbus_json *json, double value)
{
	if (json == NULL) {
		return -1;
	}
	if (mbus_json_get_type(json) != mbus_json_type_number) {
		return -1;
	}
	((mbus_cJSON *) json)->valueint = value;
	((mbus_cJSON *) json)->valuedouble = value;
	return 0;
}

int mbus_json_get_value_bool (const struct mbus_json *json)
{
	if (json == NULL) {
		return 0;
	}
	if (mbus_json_get_type(json) == mbus_json_type_true) {
		return 1;
	}
	if (mbus_json_get_type(json) == mbus_json_type_false) {
		return 0;
	}
	return 0;
}

int mbus_json_get_array_size (const struct mbus_json *json)
{
	if (json == NULL) {
		return -1;
	}
	if (mbus_json_get_type(json) != mbus_json_type_array) {
		return -1;
	}
	return mbus_cJSON_GetArraySize((mbus_cJSON *) json);
}

struct mbus_json * mbus_json_get_array_item (const struct mbus_json *json, int at)
{
	if (json == NULL) {
		return NULL;
	}
	if (mbus_json_get_type(json) != mbus_json_type_array) {
		return NULL;
	}
	return (struct mbus_json *) mbus_cJSON_GetArrayItem((mbus_cJSON *) json, at);
}

int mbus_json_add_item_to_object (struct mbus_json *json, const char *name, struct mbus_json *item)
{
	if (json == NULL) {
		return -1;
	}
	if (name == NULL) {
		return -1;
	}
	if (item == NULL) {
		return -1;
	}
	mbus_cJSON_AddItemToObject((mbus_cJSON *) json, name, (mbus_cJSON *) item);
	return 0;
}

int mbus_json_add_item_to_object_cs (struct mbus_json *json, const char *name, struct mbus_json *item)
{
	if (json == NULL) {
		return -1;
	}
	if (name == NULL) {
		return -1;
	}
	if (item == NULL) {
		return -1;
	}
	mbus_cJSON_AddItemToObjectCS((mbus_cJSON *) json, name, (mbus_cJSON *) item);
	return 0;
}

int mbus_json_delete_item_from_object (struct mbus_json *json, const char *name)
{
	if (json == NULL) {
		return -1;
	}
	if (name == NULL) {
		return -1;
	}
	mbus_cJSON_DeleteItemFromObject((mbus_cJSON *) json, name);
	return 0;
}

int mbus_json_add_null_to_array (struct mbus_json *json)
{
	return mbus_json_add_item_to_array(json, mbus_json_create_null());
}

int mbus_json_add_bool_to_object_cs (struct mbus_json *json, const char *name, int on)
{
	mbus_cJSON_AddBoolToObjectCS((mbus_cJSON *) json, name, on);
	return 0;
}

int mbus_json_add_number_to_array (struct mbus_json *json, double number)
{
	mbus_json_add_item_to_array(json, mbus_json_create_number(number));
	return 0;
}

int mbus_json_add_number_to_object_cs (struct mbus_json *json, const char *name, double number)
{
	mbus_cJSON_AddNumberToObjectCS((mbus_cJSON *) json, name, number);
	return 0;
}

int mbus_json_add_string_to_array (struct mbus_json *json, const char *string)
{
	if (string == NULL) {
		return -1;
	}
	return mbus_json_add_item_to_array(json, mbus_json_create_string(string));
}

int mbus_json_add_string_to_object (struct mbus_json *json, const char *name, const char *string)
{
	if (name == NULL) {
		return -1;
	}
	if (string == NULL) {
		return -1;
	}
	return mbus_json_add_item_to_object(json, name, mbus_json_create_string(string));
}

int mbus_json_add_string_to_object_cs (struct mbus_json *json, const char *name, const char *string)
{
	if (name == NULL) {
		return -1;
	}
	if (string == NULL) {
		return -1;
	}
	return mbus_json_add_item_to_object_cs(json, name, mbus_json_create_string(string));
}

int mbus_json_get_int_value (const struct mbus_json *json, const char *name, int value)
{
	struct mbus_cJSON *mbus_cJSON;
	mbus_cJSON = (struct mbus_cJSON *) mbus_json_get_object(json, name);
	if (mbus_cJSON == NULL) {
		return value;
	}
	if (!(mbus_cJSON->type & mbus_cJSON_Number)) {
		return value;
	}
	return mbus_cJSON->valueint;
}

int mbus_json_get_bool_value (const struct mbus_json *json, const char *name, int value)
{
	struct mbus_json *object;
	object = (struct mbus_json *) mbus_json_get_object(json, name);
	if (object == NULL) {
		return value;
	}
	if (mbus_json_get_type(object) == mbus_json_type_true) {
		return 1;
	}
	if (mbus_json_get_type(object) == mbus_json_type_false) {
		return 0;
	}
	return value;
}

const char * mbus_json_get_string_value (const struct mbus_json *json, const char *name, const char *value)
{
	struct mbus_cJSON *mbus_cJSON;
	mbus_cJSON = (struct mbus_cJSON *) mbus_json_get_object(json, name);
	if (mbus_cJSON == NULL) {
		return value;
	}
	if (!(mbus_cJSON->type & mbus_cJSON_String)) {
		return value;
	}
	return mbus_cJSON->valuestring;
}

double mbus_json_get_number_value (const struct mbus_json *json, const char *name, double value)
{
	struct mbus_cJSON *mbus_cJSON;
	mbus_cJSON = (struct mbus_cJSON *) mbus_json_get_object(json, name);
	if (mbus_cJSON == NULL) {
		return value;
	}
	if (!(mbus_cJSON->type & mbus_cJSON_Number)) {
		return value;
	}
	return mbus_cJSON->valuedouble;
}

int mbus_json_set_number_value (struct mbus_json *json, const char *name, double number)
{
	struct mbus_json *object;
	object = mbus_json_get_object(json, name);
	if (object == NULL) {
		return -1;
	}
	mbus_json_set_value_number(object, number);
	return 0;
}

struct mbus_json * mbus_json_get_object (const struct mbus_json *root, const char *path)
{
	char *str;
	char *ptr;
	char *tmp;
	mbus_cJSON *object;
	mbus_cJSON *child;
	str = NULL;
	if (root == NULL) {
		goto bail;
	}
	if (path == NULL) {
		goto bail;
	}
	str = strdup(path);
	if (str == NULL) {
		goto bail;
	}
	object = (mbus_cJSON *) root;
	child = NULL;
	ptr = str;
	while (ptr && *ptr && *ptr == '/') {
		ptr++;
	}
	while (ptr && *ptr) {
		tmp = strchr(ptr, '/');
		if (tmp != NULL) {
			*tmp = '\0';
		}
		if (strlen(ptr) != 0) {
			child = mbus_cJSON_GetObjectItem(object, ptr);
			if (child == NULL) {
				goto bail;
			}
		}
		object = child;
		if (tmp == NULL) {
			break;
		}
		ptr = tmp + 1;
	}
	free(str);
	return (struct mbus_json *) object;
bail:	if (str != NULL) {
		free(str);
	}
	return NULL;
}

struct mbus_json * mbus_json_duplicate (const struct mbus_json *json, int recursive)
{
	return (struct mbus_json *) mbus_cJSON_Duplicate((mbus_cJSON *) json, recursive);
}

char * mbus_json_print (const struct mbus_json *json)
{
	return mbus_cJSON_Print((mbus_cJSON *) json);
}

char * mbus_json_print_unformatted (const struct mbus_json *json)
{
	return mbus_cJSON_PrintUnformatted((mbus_cJSON *) json);
}
