
#include <stdlib.h>

#include "cJSON/cJSON.h"

#include "json.h"

struct mbus_json * mbus_json_parse (const char *string)
{
	return (struct mbus_json *) mbus_cJSON_Parse(string);
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

enum mbus_json_type mbus_json_get_type (struct mbus_json *json)
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

const char * mbus_json_get_value_string (struct mbus_json *json)
{
	return ((mbus_cJSON *) json)->valuestring;
}

int mbus_json_get_array_size (struct mbus_json *json)
{
	return mbus_cJSON_GetArraySize((mbus_cJSON *) json);
}

struct mbus_json * mbus_json_get_array_item (struct mbus_json *json, int at)
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

const char * mbus_json_get_string_value (struct mbus_json *json, const char *name)
{
	return mbus_cJSON_GetStringValue((mbus_cJSON *) json, name);
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

struct mbus_json * mbus_json_get_object_item (struct mbus_json *json, const char *name)
{
	return (struct mbus_json *) mbus_cJSON_GetObjectItem((mbus_cJSON *) json, name);
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
