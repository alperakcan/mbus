/*
  Copyright (c) 2009 Dave Gamble

  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files (the "Software"), to deal
  in the Software without restriction, including without limitation the rights
  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
  copies of the Software, and to permit persons to whom the Software is
  furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in
  all copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
  THE SOFTWARE.
*/

#ifndef cJSON__h
#define cJSON__h

#ifdef __cplusplus
extern "C"
{
#endif

#include <stddef.h>

/* cJSON Types: */
#define mbus_cJSON_False  (1 << 0)
#define mbus_cJSON_True   (1 << 1)
#define mbus_cJSON_NULL   (1 << 2)
#define mbus_cJSON_Number (1 << 3)
#define mbus_cJSON_String (1 << 4)
#define mbus_cJSON_Array  (1 << 5)
#define mbus_cJSON_Object (1 << 6)

#define mbus_cJSON_IsReference 256
#define mbus_cJSON_StringIsConst 512

/* The cJSON structure: */
typedef struct mbus_cJSON
{
    /* next/prev allow you to walk array/object chains. Alternatively, use GetArraySize/GetArrayItem/GetObjectItem */
    struct mbus_cJSON *next;
    struct mbus_cJSON *prev;
    /* An array or object item will have a child pointer pointing to a chain of the items in the array/object. */
    struct mbus_cJSON *child;

    /* The type of the item, as above. */
    int type;

    /* The item's string, if type==cJSON_String */
    char *valuestring;
    /* The item's number, if type==cJSON_Number */
    int valueint;
    /* The item's number, if type==cJSON_Number */
    double valuedouble;

    /* The item's name string, if this item is the child of, or is in the list of subitems of an object. */
    char *string;
} mbus_cJSON;

typedef struct mbus_cJSON_Hooks
{
      void *(*malloc_fn)(size_t sz);
      void (*free_fn)(void *ptr);
} mbus_cJSON_Hooks;

/* Supply malloc, realloc and free functions to cJSON */
extern void mbus_cJSON_InitHooks(mbus_cJSON_Hooks* hooks);


/* Supply a block of JSON, and this returns a cJSON object you can interrogate. Call cJSON_Delete when finished. */
extern mbus_cJSON *mbus_cJSON_Parse(const char *value);
/* Render a cJSON entity to text for transfer/storage. Free the char* when finished. */
extern char  *mbus_cJSON_Print(const mbus_cJSON *item);
/* Render a cJSON entity to text for transfer/storage without any formatting. Free the char* when finished. */
extern char  *mbus_cJSON_PrintUnformatted(const mbus_cJSON *item);
/* Render a cJSON entity to text using a buffered strategy. prebuffer is a guess at the final size. guessing well reduces reallocation. fmt=0 gives unformatted, =1 gives formatted */
extern char *mbus_cJSON_PrintBuffered(const mbus_cJSON *item, int prebuffer, int fmt);
/* Render a cJSON entity to text using a buffer already allocated in memory with length buf_len */
extern int mbus_cJSON_PrintPreallocated(mbus_cJSON *item, char *buf, const int len, const int fmt);
/* Delete a cJSON entity and all subentities. */
extern void   mbus_cJSON_Delete(mbus_cJSON *c);

/* Returns the number of items in an array (or object). */
extern int	  mbus_cJSON_GetArraySize(const mbus_cJSON *array);
/* Retrieve item number "item" from array "array". Returns NULL if unsuccessful. */
extern mbus_cJSON *mbus_cJSON_GetArrayItem(const mbus_cJSON *array, int item);
/* Get item "string" from object. Case insensitive. */
extern mbus_cJSON *mbus_cJSON_GetObjectItem(const mbus_cJSON *object, const char *string);
extern int mbus_cJSON_HasObjectItem(const mbus_cJSON *object, const char *string);
/* For analysing failed parses. This returns a pointer to the parse error. You'll probably need to look a few chars back to make sense of it. Defined when cJSON_Parse() returns 0. 0 when cJSON_Parse() succeeds. */
extern const char *mbus_cJSON_GetErrorPtr(void);

/* These calls create a cJSON item of the appropriate type. */
extern mbus_cJSON *mbus_cJSON_CreateNull(void);
extern mbus_cJSON *mbus_cJSON_CreateTrue(void);
extern mbus_cJSON *mbus_cJSON_CreateFalse(void);
extern mbus_cJSON *mbus_cJSON_CreateBool(int b);
extern mbus_cJSON *mbus_cJSON_CreateNumber(double num);
extern mbus_cJSON *mbus_cJSON_CreateString(const char *string);
extern mbus_cJSON *mbus_cJSON_CreateArray(void);
extern mbus_cJSON *mbus_cJSON_CreateObject(void);

/* These utilities create an Array of count items. */
extern mbus_cJSON *mbus_cJSON_CreateIntArray(const int *numbers, int count);
extern mbus_cJSON *mbus_cJSON_CreateFloatArray(const float *numbers, int count);
extern mbus_cJSON *mbus_cJSON_CreateDoubleArray(const double *numbers, int count);
extern mbus_cJSON *mbus_cJSON_CreateStringArray(const char **strings, int count);

/* Append item to the specified array/object. */
extern void mbus_cJSON_AddItemToArray(mbus_cJSON *array, mbus_cJSON *item);
extern void	mbus_cJSON_AddItemToObject(mbus_cJSON *object, const char *string, mbus_cJSON *item);
/* Use this when string is definitely const (i.e. a literal, or as good as), and will definitely survive the cJSON object.
 * WARNING: When this function was used, make sure to always check that (item->type & cJSON_StringIsConst) is zero before
 * writing to `item->string` */
extern void	mbus_cJSON_AddItemToObjectCS(mbus_cJSON *object, const char *string, mbus_cJSON *item);
/* Append reference to item to the specified array/object. Use this when you want to add an existing cJSON to a new cJSON, but don't want to corrupt your existing cJSON. */
extern void mbus_cJSON_AddItemReferenceToArray(mbus_cJSON *array, mbus_cJSON *item);
extern void	mbus_cJSON_AddItemReferenceToObject(mbus_cJSON *object, const char *string, mbus_cJSON *item);

/* Remove/Detatch items from Arrays/Objects. */
extern mbus_cJSON *mbus_cJSON_DetachItemFromArray(mbus_cJSON *array, int which);
extern void   mbus_cJSON_DeleteItemFromArray(mbus_cJSON *array, int which);
extern mbus_cJSON *mbus_cJSON_DetachItemFromObject(mbus_cJSON *object, const char *string);
extern void   mbus_cJSON_DeleteItemFromObject(mbus_cJSON *object, const char *string);

/* Update array items. */
extern void mbus_cJSON_InsertItemInArray(mbus_cJSON *array, int which, mbus_cJSON *newitem); /* Shifts pre-existing items to the right. */
extern void mbus_cJSON_ReplaceItemInArray(mbus_cJSON *array, int which, mbus_cJSON *newitem);
extern void mbus_cJSON_ReplaceItemInObject(mbus_cJSON *object,const char *string,mbus_cJSON *newitem);

/* Duplicate a cJSON item */
extern mbus_cJSON *mbus_cJSON_Duplicate(const mbus_cJSON *item, int recurse);
/* Duplicate will create a new, identical cJSON item to the one you pass, in new memory that will
need to be released. With recurse!=0, it will duplicate any children connected to the item.
The item->next and ->prev pointers are always zero on return from Duplicate. */

/* ParseWithOpts allows you to require (and check) that the JSON is null terminated, and to retrieve the pointer to the final byte parsed. */
/* If you supply a ptr in return_parse_end and parsing fails, then return_parse_end will contain a pointer to the error. If not, then cJSON_GetErrorPtr() does the job. */
extern mbus_cJSON *mbus_cJSON_ParseWithOpts(const char *value, const char **return_parse_end, int require_null_terminated);

extern void cJSON_Minify(char *json);

/* Macros for creating things quickly. */
#define mbus_cJSON_AddNullToObject(object,name) mbus_cJSON_AddItemToObject(object, name, mbus_cJSON_CreateNull())
#define mbus_cJSON_AddTrueToObject(object,name) mbus_cJSON_AddItemToObject(object, name, mbus_cJSON_CreateTrue())
#define mbus_cJSON_AddFalseToObject(object,name) mbus_cJSON_AddItemToObject(object, name, mbus_cJSON_CreateFalse())
#define mbus_cJSON_AddBoolToObject(object,name,b) mbus_cJSON_AddItemToObject(object, name, mbus_cJSON_CreateBool(b))
#define mbus_cJSON_AddNumberToObject(object,name,n) mbus_cJSON_AddItemToObject(object, name, mbus_cJSON_CreateNumber(n))
#define mbus_cJSON_AddStringToObject(object,name,s) mbus_cJSON_AddItemToObject(object, name, mbus_cJSON_CreateString(s))

#define mbus_cJSON_AddNumberToObjectCS(object,name,n)	mbus_cJSON_AddItemToObjectCS(object, name, mbus_cJSON_CreateNumber(n))
#define mbus_cJSON_AddStringToObjectCS(object,name,s)	mbus_cJSON_AddItemToObjectCS(object, name, mbus_cJSON_CreateString(s))

/* When assigning an integer value, it needs to be propagated to valuedouble too. */
#define mbus_cJSON_SetIntValue(object,val) ((object) ? (object)->valueint = (object)->valuedouble = (val) : (val))
#define mbus_cJSON_SetNumberValue(object,val) ((object) ? (object)->valueint = (object)->valuedouble = (val) : (val))

/* Macro for iterating over an array */
#define mbus_cJSON_ArrayForEach(pos, head) for(pos = (head)->child; pos != NULL; pos = pos->next)

extern const char * mbus_cJSON_GetStringValue (mbus_cJSON *object,const char *string);
extern int mbus_cJSON_GetIntValue (mbus_cJSON *object,const char *string);
extern int mbus_cJSON_GetNumberValue (mbus_cJSON *object,const char *string);

#ifdef __cplusplus
}
#endif

#endif
