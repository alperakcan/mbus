
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

void mbus_json_delete (struct mbus_json *json)
{
	mbus_cJSON_Delete((mbus_cJSON *) json);
}

struct mbus_json * mbus_json_get_child (struct mbus_json *json)
{
	return (struct mbus_json *) (((mbus_cJSON *) json)->child);
}

struct mbus_json * mbus_json_get_next (struct mbus_json *json)
{
	return (struct mbus_json *) (((mbus_cJSON *) json)->next);
}

enum mbus_json_type mbus_json_get_type (const struct mbus_json *json)
{
	switch (((mbus_cJSON *) json)->type) {
		case mbus_cJSON_False:	return mbus_json_type_false;
		case mbus_cJSON_True:	return mbus_json_type_true;
		case mbus_cJSON_NULL:	return mbus_json_type_null;
		case mbus_cJSON_Number:	return mbus_json_type_number;
		case mbus_cJSON_String:	return mbus_json_type_string;
		case mbus_cJSON_Array:	return mbus_json_type_array;
		case mbus_cJSON_Object:	return mbus_json_type_object;
	}
	return mbus_json_type_unknown;
}

const char * mbus_json_get_name (struct mbus_json *json)
{
	return ((mbus_cJSON *) json)->string;
}

const char * mbus_json_get_value_string (const struct mbus_json *json)
{
	return ((mbus_cJSON *) json)->valuestring;
}

int mbus_json_get_array_size (const struct mbus_json *json)
{
	return mbus_cJSON_GetArraySize((mbus_cJSON *) json);
}

struct mbus_json * mbus_json_get_array_item (const struct mbus_json *json, int at)
{
	return (struct mbus_json *) mbus_cJSON_GetArrayItem((mbus_cJSON *) json, at);
}

int mbus_json_add_item_to_object_cs (struct mbus_json *json, const char *name, struct mbus_json *item)
{
	mbus_cJSON_AddItemToObjectCS((mbus_cJSON *) json, name, (mbus_cJSON *) item);
	return 0;
}

int mbus_json_delete_item_from_object (struct mbus_json *json, const char *name)
{
	mbus_cJSON_DeleteItemFromObject((mbus_cJSON *) json, name);
	return 0;
}

int mbus_json_add_number_to_object_cs (struct mbus_json *json, const char *name, double number)
{
	mbus_cJSON_AddNumberToObjectCS((mbus_cJSON *) json, name, number);
	return 0;
}

int mbus_json_add_string_to_object_cs (struct mbus_json *json, const char *name, const char *string)
{
	mbus_cJSON_AddStringToObjectCS((mbus_cJSON *) json, name, string);
	return 0;
}

int mbus_json_get_int_value (struct mbus_json *json, const char *name)
{
	return mbus_cJSON_GetIntValue((mbus_cJSON *) json, name);
}

static struct mbus_json * mbus_json_object (const struct mbus_json *root, const char *path)
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

const char * mbus_json_get_string_value (const struct mbus_json *json, const char *name)
{
	struct mbus_cJSON *mbus_cJSON;
	mbus_cJSON = (struct mbus_cJSON *) mbus_json_object(json, name);
	if (mbus_cJSON == NULL) {
		return NULL;
	}
	if (mbus_cJSON->type != mbus_cJSON_String) {
		return NULL;
	}
	return mbus_cJSON->valuestring;
}

double mbus_json_get_number_value (struct mbus_json *json, const char *name)
{
	return mbus_cJSON_GetNumberValue((mbus_cJSON *) json, name);
}

int mbus_json_set_number_value (struct mbus_json *json, const char *name, double number)
{
	mbus_cJSON *object;
	object = mbus_cJSON_GetObjectItem((mbus_cJSON *) json, name);
	if (object == NULL) {
		return -1;
	}
	mbus_cJSON_SetNumberValue(object, number);
	return 0;
}

struct mbus_json * mbus_json_get_object (const struct mbus_json *json, const char *name)
{
	return mbus_json_object(json, name);
}

struct mbus_json * mbus_json_duplicate (const struct mbus_json *json, int recursive)
{
	return (struct mbus_json *) mbus_cJSON_Duplicate((mbus_cJSON *) json, recursive);
}

char * mbus_json_print (struct mbus_json *json)
{
	return mbus_cJSON_Print((mbus_cJSON *) json);
}

char * mbus_json_print_unformatted (struct mbus_json *json)
{
	return mbus_cJSON_PrintUnformatted((mbus_cJSON *) json);
}
