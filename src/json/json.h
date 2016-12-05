
enum mbus_json_type {
	mbus_json_type_unknown,
	mbus_json_type_false,
	mbus_json_type_true,
	mbus_json_type_null,
	mbus_json_type_number,
	mbus_json_type_string,
	mbus_json_type_array,
	mbus_json_type_object
};

struct mbus_json;

struct mbus_json * mbus_json_parse (const char *string);
struct mbus_json * mbus_json_create_object (void);
struct mbus_json * mbus_json_create_array (void);
struct mbus_json * mbus_json_create_string (const char *string);
void mbus_json_delete (struct mbus_json *json);

struct mbus_json * mbus_json_get_child (struct mbus_json *json);
struct mbus_json * mbus_json_get_next (struct mbus_json *json);
enum mbus_json_type mbus_json_get_type (struct mbus_json *json);
const char * mbus_json_get_name (struct mbus_json *json);
const char * mbus_json_get_value_string (struct mbus_json *json);

int mbus_json_get_array_size (struct mbus_json *json);
struct mbus_json * mbus_json_get_array_item (struct mbus_json *json, int at);

int mbus_json_add_item_to_array (struct mbus_json *array, struct mbus_json *item);

int mbus_json_add_item_to_object_cs (struct mbus_json *array, const char *name, struct mbus_json *item);
int mbus_json_delete_item_from_object (struct mbus_json *array, const char *name);

int mbus_json_add_number_to_object_cs (struct mbus_json *json, const char *name, double number);
int mbus_json_add_string_to_object_cs (struct mbus_json *json, const char *name, const char *string);

int mbus_json_get_int_value (struct mbus_json *json, const char *name);
const char * mbus_json_get_string_value (struct mbus_json *json, const char *name);
double mbus_json_get_number_value (struct mbus_json *json, const char *name);

struct mbus_json * mbus_json_get_object_item (struct mbus_json *json, const char *name);
struct mbus_json * mbus_json_duplicate (struct mbus_json *json, int recursive);

char * mbus_json_print (struct mbus_json *json);
char * mbus_json_print_unformatted (struct mbus_json *json);
