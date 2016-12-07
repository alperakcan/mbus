
#include <stdlib.h>

#include "cJSON/cJSON.h"

#include "json.h"

struct mbus_json * mbus_json_parse (const char *string)
{
	return (struct mbus_json *) cJSON_Parse(string);
}

struct mbus_json * mbus_json_create_object (void)
{
	return (struct mbus_json *) cJSON_CreateObject();
}

struct mbus_json * mbus_json_create_array (void)
{
	return (struct mbus_json *) cJSON_CreateArray();
}

int mbus_json_add_item_to_array (struct mbus_json *array, const struct mbus_json *item)
{
	cJSON_AddItemToArray((cJSON *) array, (cJSON *) item);
	return 0;
}

struct mbus_json * mbus_json_create_string (const char *string)
{
	return (struct mbus_json *) cJSON_CreateString(string);
}

void mbus_json_delete (struct mbus_json *json)
{
	cJSON_Delete((cJSON *) json);
}

struct mbus_json * mbus_json_get_child (struct mbus_json *json)
{
	return (struct mbus_json *) (((cJSON *) json)->child);
}

struct mbus_json * mbus_json_get_next (struct mbus_json *json)
{
	return (struct mbus_json *) (((cJSON *) json)->next);
}

enum mbus_json_type mbus_json_get_type (struct mbus_json *json)
{
	switch (((cJSON *) json)->type) {
		case cJSON_False:	return mbus_json_type_false;
		case cJSON_True:	return mbus_json_type_true;
		case cJSON_NULL:	return mbus_json_type_null;
		case cJSON_Number:	return mbus_json_type_number;
		case cJSON_String:	return mbus_json_type_string;
		case cJSON_Array:	return mbus_json_type_array;
		case cJSON_Object:	return mbus_json_type_object;
	}
	return mbus_json_type_unknown;
}

const char * mbus_json_get_name (struct mbus_json *json)
{
	return ((cJSON *) json)->string;
}

const char * mbus_json_get_value_string (struct mbus_json *json)
{
	return ((cJSON *) json)->valuestring;
}

int mbus_json_get_array_size (struct mbus_json *json)
{
	return cJSON_GetArraySize((cJSON *) json);
}

struct mbus_json * mbus_json_get_array_item (struct mbus_json *json, int at)
{
	return (struct mbus_json *) cJSON_GetArrayItem((cJSON *) json, at);
}

int mbus_json_add_item_to_object_cs (struct mbus_json *json, const char *name, struct mbus_json *item)
{
	cJSON_AddItemToObjectCS((cJSON *) json, name, (cJSON *) item);
	return 0;
}

int mbus_json_delete_item_from_object (struct mbus_json *json, const char *name)
{
	cJSON_DeleteItemFromObject((cJSON *) json, name);
	return 0;
}

int mbus_json_add_number_to_object_cs (struct mbus_json *json, const char *name, double number)
{
	cJSON_AddNumberToObjectCS((cJSON *) json, name, number);
	return 0;
}

int mbus_json_add_string_to_object_cs (struct mbus_json *json, const char *name, const char *string)
{
	cJSON_AddStringToObjectCS((cJSON *) json, name, string);
	return 0;
}

int mbus_json_get_int_value (struct mbus_json *json, const char *name)
{
	return cJSON_GetIntValue((cJSON *) json, name);
}

const char * mbus_json_get_string_value (struct mbus_json *json, const char *name)
{
	return cJSON_GetStringValue((cJSON *) json, name);
}

double mbus_json_get_number_value (struct mbus_json *json, const char *name)
{
	return cJSON_GetNumberValue((cJSON *) json, name);
}

int mbus_json_set_number_value (struct mbus_json *json, const char *name, double number)
{
	cJSON *object;
	object = cJSON_GetObjectItem((cJSON *) json, name);
	if (object == NULL) {
		return -1;
	}
	cJSON_SetNumberValue(object, number);
	return 0;
}

struct mbus_json * mbus_json_get_object_item (struct mbus_json *json, const char *name)
{
	return (struct mbus_json *) cJSON_GetObjectItem((cJSON *) json, name);
}

struct mbus_json * mbus_json_duplicate (const struct mbus_json *json, int recursive)
{
	return (struct mbus_json *) cJSON_Duplicate((cJSON *) json, recursive);
}

char * mbus_json_print (struct mbus_json *json)
{
	return cJSON_Print((cJSON *) json);
}

char * mbus_json_print_unformatted (struct mbus_json *json)
{
	return cJSON_PrintUnformatted((cJSON *) json);
}
