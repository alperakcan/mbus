
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
struct mbus_json * mbus_json_parse_end (const char *string, const char **end);
struct mbus_json * mbus_json_parse_file (const char *path);
struct mbus_json * mbus_json_create_object (void);
struct mbus_json * mbus_json_create_array (void);
struct mbus_json * mbus_json_create_string (const char *string);
struct mbus_json * mbus_json_create_number (double number);
struct mbus_json * mbus_json_create_null (void);
void mbus_json_delete (struct mbus_json *json);

int mbus_json_compare (const struct mbus_json *a, const struct mbus_json *b);
struct mbus_json * mbus_json_get_child (const struct mbus_json *json);
struct mbus_json * mbus_json_get_next (const struct mbus_json *json);
enum mbus_json_type mbus_json_get_type (const struct mbus_json *json);

const char * mbus_json_get_name (const struct mbus_json *json);
const char * mbus_json_get_value_string (const struct mbus_json *json);
int mbus_json_get_value_int (const struct mbus_json *json);
double mbus_json_get_value_number (const struct mbus_json *json);
int mbus_json_set_value_number (const struct mbus_json *json, double value);
int mbus_json_get_value_bool (const struct mbus_json *json);

int mbus_json_get_array_size (const struct mbus_json *json);
struct mbus_json * mbus_json_get_array_item (const struct mbus_json *json, int at);

int mbus_json_add_item_to_array (struct mbus_json *array, const struct mbus_json *item);

int mbus_json_add_item_to_object (struct mbus_json *json, const char *name, struct mbus_json *item);
int mbus_json_add_item_to_object_cs (struct mbus_json *json, const char *name, struct mbus_json *item);
int mbus_json_delete_item_from_object (struct mbus_json *json, const char *name);

int mbus_json_add_null_to_array (struct mbus_json *json);

int mbus_json_add_bool_to_object_cs (struct mbus_json *json, const char *name, int on);

int mbus_json_add_number_to_array (struct mbus_json *json, double number);
int mbus_json_add_number_to_object_cs (struct mbus_json *json, const char *name, double number);

int mbus_json_add_string_to_array (struct mbus_json *json, const char *string);
int mbus_json_add_string_to_object (struct mbus_json *json, const char *name, const char *string);
int mbus_json_add_string_to_object_cs (struct mbus_json *json, const char *name, const char *string);

int mbus_json_get_int_value (const struct mbus_json *json, const char *name, int value);
double mbus_json_get_number_value (const struct mbus_json *json, const char *name, double value);
const char * mbus_json_get_string_value (const struct mbus_json *json, const char *name, const char *value);
int mbus_json_get_bool_value (const struct mbus_json *json, const char *name, int value);

int mbus_json_set_number_value (struct mbus_json *json, const char *name, double number);

struct mbus_json * mbus_json_get_object (const struct mbus_json *json, const char *name);
struct mbus_json * mbus_json_duplicate (const struct mbus_json *json, int recursive);

char * mbus_json_print (const struct mbus_json *json);
char * mbus_json_print_unformatted (const struct mbus_json *json);
