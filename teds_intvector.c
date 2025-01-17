/*
  +----------------------------------------------------------------------+
  | teds extension for PHP                                               |
  | See COPYING file for further copyright information                   |
  +----------------------------------------------------------------------+
  | Author: Tyson Andre <tandre@php.net>                                 |
  +----------------------------------------------------------------------+
*/

/* This is based on spl_fixedarray.c but has lower overhead (when size is known) and is more efficient to push and remove elements from the end of the list */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "php.h"
#include "php_ini.h"
#include "ext/standard/info.h"
#include "zend_exceptions.h"

#include "php_teds.h"
#include "teds.h"
#include "teds_intvector_arginfo.h"
#include "teds_intvector.h"
#include "teds_interfaces.h"
#include "teds_exceptions.h"
// #include "ext/spl/spl_functions.h"
#include "ext/spl/spl_exceptions.h"
#include "ext/spl/spl_iterators.h"
#include "ext/json/php_json.h"
#include "teds_util.h"
#include "teds_serialize_util.h"

#include <stdbool.h>

static zend_always_inline zend_long teds_intvector_convert_zval_value_to_long(const zval *offset) {
	switch (Z_TYPE_P(offset)) {
		case IS_LONG:
			return Z_LVAL_P(offset);
		case IS_REFERENCE:
			offset = Z_REFVAL_P(offset);
			if (Z_TYPE_P(offset) == IS_LONG) {
				return Z_LVAL_P(offset);
			}
			break;
	}

	zend_type_error("Illegal Teds\\IntVector value type %s", zend_zval_type_name(offset));
	return 0;
}

#define TEDS_INTVECTOR_VALUE_TO_LONG_OR_THROW(value, value_zv) do { \
	if (UNEXPECTED(Z_TYPE_P((value_zv)) != IS_LONG)) { \
		value = teds_intvector_convert_zval_value_to_long((value_zv)); \
		if (UNEXPECTED(EG(exception))) { \
			return; \
		} \
	} else { \
		value = Z_LVAL_P((value_zv)); \
	} \
} while(0)


/* Though rare, it is possible to have 64-bit zend_longs and a 32-bit size_t. */
#define MAX_ZVAL_COUNT ((SIZE_MAX / sizeof(zval)) - 1)
#define MAX_VALID_OFFSET ((size_t)(MAX_ZVAL_COUNT > ZEND_LONG_MAX ? ZEND_LONG_MAX : MAX_ZVAL_COUNT))

zend_object_handlers teds_handler_IntVector;
zend_class_entry *teds_ce_IntVector;

/* This is a placeholder value to distinguish between empty and uninitialized IntVector instances.
 * Compilers require at least one element. Make this constant - reads/writes should be impossible. */
static const int8_t empty_entry_list[1];

#define _TEDS_INTVECTOR_TYPE_BOOL_BITSET   0
#define TEDS_INTVECTOR_TYPE_UNINITIALIZED  0
#define TEDS_INTVECTOR_TYPE_INT8           1
#define TEDS_INTVECTOR_TYPE_INT16          2
#define TEDS_INTVECTOR_TYPE_INT32          3
#if SIZEOF_ZEND_LONG > 4
#define TEDS_INTVECTOR_TYPE_INT64          4
#define TEDS_INTVECTOR_TYPE_COUNT          TEDS_INTVECTOR_TYPE_INT64 + 1
#else
#define TEDS_INTVECTOR_TYPE_COUNT          TEDS_INTVECTOR_TYPE_INT32 + 1
#endif

static const uint8_t teds_lmv_memory_per_element[TEDS_INTVECTOR_TYPE_COUNT] = {
	0,
	sizeof(int8_t),  /* TEDS_INTVECTOR_TYPE_INT8 */
	sizeof(int16_t), /* TEDS_INTVECTOR_TYPE_INT16 */
	sizeof(int32_t), /* TEDS_INTVECTOR_TYPE_INT32 */
#ifdef TEDS_INTVECTOR_TYPE_INT64
	sizeof(int64_t), /* TEDS_INTVECTOR_TYPE_INT64 */
#endif

};
static const uint8_t teds_lmv_shift_for_element[TEDS_INTVECTOR_TYPE_COUNT] = {
	0,
	0, /* TEDS_INTVECTOR_TYPE_INT8 */
	1, /* TEDS_INTVECTOR_TYPE_INT16 */
	2, /* TEDS_INTVECTOR_TYPE_INT32 */
#ifdef TEDS_INTVECTOR_TYPE_INT64
	3, /* TEDS_INTVECTOR_TYPE_INT64 */
#endif
};

typedef struct _teds_intvector_entries {
	/* This is deliberately a size_t instead of an uint32_t.
	 * This is memory efficient enough that it's more likely to be used in practice for more than 4 billion values,
	 * and garbage collection isn't a problem. */
	size_t size;
	size_t capacity;
	union {
		uint8_t     *entries_uint8;
		int8_t      *entries_int8;
		int16_t     *entries_int16;
		int32_t     *entries_int32;
#ifdef TEDS_INTVECTOR_TYPE_INT64
		int64_t     *entries_int64;
#endif
		void        *entries_raw;
	};
	int8_t type_tag;
} teds_intvector_entries;

typedef struct _teds_intvector {
	teds_intvector_entries		array;
	zend_object				std;
} teds_intvector;

/* Used by InternalIterator returned by IntVector->getIterator() */
typedef struct _teds_intvector_it {
	zend_object_iterator intern;
	size_t               current;
	/* Temporary memory location to store the most recent get_current_value() result */
	zval                 tmp;
} teds_intvector_it;

static void teds_intvector_entries_raise_capacity(teds_intvector_entries *intern, const size_t new_capacity);
static zend_always_inline void teds_intvector_entries_push(teds_intvector_entries *array, const zend_long value, const bool check_capacity);
static zend_always_inline void teds_intvector_entries_update_type_tag(teds_intvector_entries *array, const zend_long val);
static zend_always_inline void teds_intvector_entries_copy_offset(const teds_intvector_entries *array, const size_t offset, zval *dst, bool remove);
static zend_always_inline zval *teds_intvector_entries_read_offset(const teds_intvector_entries *intern, const size_t offset, zval *tmp);
static void teds_intvector_entries_init_from_array_values(teds_intvector_entries *array, zend_array *raw_data);
static zend_array *teds_intvector_entries_to_refcounted_array(const teds_intvector_entries *array);

#ifdef TEDS_INTVECTOR_TYPE_INT64
#define TEDS_INTVECTOR__GENERATE_INT_CASES() \
	TEDS_INTVECTOR__INT_CODEGEN(TEDS_INTVECTOR_TYPE_INT8, int8_t,   entries_int8) \
	TEDS_INTVECTOR__INT_CODEGEN(TEDS_INTVECTOR_TYPE_INT16, int16_t, entries_int16) \
	TEDS_INTVECTOR__INT_CODEGEN(TEDS_INTVECTOR_TYPE_INT32, int32_t, entries_int32) \
	TEDS_INTVECTOR__INT_CODEGEN(TEDS_INTVECTOR_TYPE_INT64, int64_t, entries_int64)
#else
#define TEDS_INTVECTOR__GENERATE_INT_CASES() \
	TEDS_INTVECTOR__INT_CODEGEN(TEDS_INTVECTOR_TYPE_INT8, int8_t,   entries_int8) \
	TEDS_INTVECTOR__INT_CODEGEN(TEDS_INTVECTOR_TYPE_INT16, int16_t, entries_int16) \
	TEDS_INTVECTOR__INT_CODEGEN(TEDS_INTVECTOR_TYPE_INT32, int32_t, entries_int32)
#endif

/*
 * If a size this large is encountered, assume the allocation will likely fail or
 * future changes to the capacity will overflow.
 */
static ZEND_COLD void teds_error_noreturn_max_intvector_capacity()
{
	zend_error_noreturn(E_ERROR, "exceeded max valid Teds\\IntVector capacity");
}

static zend_always_inline teds_intvector *teds_intvector_from_object(zend_object *obj)
{
	return (teds_intvector*)((char*)(obj) - XtOffsetOf(teds_intvector, std));
}

#define Z_INTVECTOR_P(zv)  (teds_intvector_from_object(Z_OBJ_P((zv))))
#define Z_INTVECTOR_ENTRIES_P(zv)  (&(Z_INTVECTOR_P((zv))->array))

static zend_always_inline bool teds_intvector_entries_empty_capacity(const teds_intvector_entries *array)
{
	if (array->capacity > 0) {
		ZEND_ASSERT(array->entries_int8 != (void *)empty_entry_list);
		return false;
	}
	// This intvector may have reserved capacity.
	return true;
}

static zend_always_inline bool teds_intvector_entries_uninitialized(const teds_intvector_entries *array)
{
	if (array->entries_raw == NULL) {
		ZEND_ASSERT(array->size == 0);
		ZEND_ASSERT(array->capacity == 0);
		ZEND_ASSERT(array->type_tag == TEDS_INTVECTOR_TYPE_UNINITIALIZED);
		return true;
	}
	ZEND_ASSERT((array->entries_raw == (void *)empty_entry_list && array->capacity == 0) || array->capacity > 0);
	return false;
}

static uint8_t teds_intvector_entries_compute_memory_per_element(const teds_intvector_entries *array) {
	ZEND_ASSERT(array->type_tag != TEDS_INTVECTOR_TYPE_UNINITIALIZED);
	ZEND_ASSERT(array->type_tag < TEDS_INTVECTOR_TYPE_COUNT);
	return teds_lmv_memory_per_element[array->type_tag];
}

static void teds_intvector_entries_raise_capacity(teds_intvector_entries *array, const size_t new_capacity) {
	ZEND_ASSERT(new_capacity > array->capacity);
	const uint8_t memory_per_element = teds_intvector_entries_compute_memory_per_element(array);
	if (array->capacity == 0) {
		array->entries_raw = safe_emalloc(new_capacity, memory_per_element, 0);
	} else {
		array->entries_raw = safe_erealloc(array->entries_raw, new_capacity, memory_per_element, 0);
	}
	array->capacity = new_capacity;
	ZEND_ASSERT(array->entries_raw != NULL);
}

static inline void teds_intvector_entries_shrink_capacity(teds_intvector_entries *array, size_t size, size_t capacity, void *old_entries_raw) {
	ZEND_ASSERT(size <= capacity);
	ZEND_ASSERT(size == array->size);
	ZEND_ASSERT(capacity > 0);
	ZEND_ASSERT(capacity < array->capacity);
	ZEND_ASSERT(old_entries_raw == array->entries_raw);
	const uint8_t memory_per_element = teds_intvector_entries_compute_memory_per_element(array);
	array->capacity = capacity;
	array->entries_raw = erealloc2(old_entries_raw, capacity * memory_per_element, size * memory_per_element);
	ZEND_ASSERT(array->entries_raw != NULL);
}

/* Initializes the range [from, to) to null. Does not dtor existing entries. */
/* TODO: Delete if this isn't used in the final version
static void teds_intvector_entries_init_elems(teds_intvector_entries *array, zend_long from, zend_long to)
{
	ZEND_ASSERT(from <= to);
	zval *begin = &array->entries[from];
	zval *end = &array->entries[to];

	while (begin != end) {
		ZVAL_NULL(begin++);
	}
}
*/

static zend_always_inline void teds_intvector_entries_set_empty_list(teds_intvector_entries *array) {
	array->size = 0;
	array->capacity = 0;
	array->type_tag = TEDS_INTVECTOR_TYPE_UNINITIALIZED;
	array->entries_raw = (void *)empty_entry_list;
}

static void teds_intvector_entries_init_from_traversable(teds_intvector_entries *array, zend_object *obj)
{
	zend_class_entry *ce = obj->ce;
	zend_object_iterator *iter;
	teds_intvector_entries_set_empty_list(array);
	zval tmp_obj;
	ZVAL_OBJ(&tmp_obj, obj);
	iter = ce->get_iterator(ce, &tmp_obj, 0);

	if (UNEXPECTED(EG(exception))) {
		return;
	}

	const zend_object_iterator_funcs *funcs = iter->funcs;

	if (funcs->rewind) {
		funcs->rewind(iter);
		if (UNEXPECTED(EG(exception))) {
			goto cleanup_iter;
		}
	}

	/* Reindex keys from 0. */
	while (funcs->valid(iter) == SUCCESS) {
		if (EG(exception)) {
			break;
		}
		zval *value_zv = funcs->get_current_data(iter);
		if (UNEXPECTED(EG(exception))) {
			break;
		}

		zend_long value;
		if (UNEXPECTED(Z_TYPE_P(value_zv) != IS_LONG)) {
			value = teds_intvector_convert_zval_value_to_long(value_zv);
			if (UNEXPECTED(EG(exception))) {
				/* Free iterator */
				break;
			}
		} else {
			value = Z_LVAL_P(value_zv);
		}
		teds_intvector_entries_push(array, value, true);

		iter->index++;
		funcs->move_forward(iter);
		if (UNEXPECTED(EG(exception))) {
			break;
		}
	}

cleanup_iter:
	if (iter) {
		zend_iterator_dtor(iter);
	}
}

static void teds_intvector_entries_copy_ctor(teds_intvector_entries *to, const teds_intvector_entries *from)
{
	zend_long size = from->size;
	if (!size) {
		teds_intvector_entries_set_empty_list(to);
		return;
	}

	to->size = 0; /* reset size in case emalloc() fails */
	to->capacity = 0;
	to->type_tag = from->type_tag;
	const uint8_t bytes_per_element = teds_intvector_entries_compute_memory_per_element(from);
	to->entries_int8 = safe_emalloc(size, bytes_per_element, 0);
	to->size = size;
	to->capacity = size;

	memcpy(to->entries_int8, from->entries_int8, bytes_per_element * size);
}

static HashTable* teds_intvector_get_gc(zend_object *obj, zval **table, int *n)
{
	/* Zend/zend_gc.c does not initialize table or n. So we need to set n to 0 at minimum. */
	*n = 0;
	(void) table;
	(void) obj;
	/* Nothing needs to be garbage collected */
	return NULL;
}

static HashTable* teds_intvector_get_properties(zend_object *obj)
{
	(void)obj;
	/* Thankfully, anything using Z_OBJPROP_P for infinite recursion detection (var_export) won't need to worry about infinite recursion, all fields are integers and there are no properties. */
	return (HashTable*)&zend_empty_array;
}

static HashTable* teds_intvector_get_properties_for(zend_object *obj, zend_prop_purpose purpose)
{
	(void)purpose;
	teds_intvector_entries *array = &teds_intvector_from_object(obj)->array;
	if (!array->size) {
		/* Similar to ext/ffi/ffi.c zend_fake_get_properties */
		return (HashTable*)&zend_empty_array;
	}
	/* var_export uses get_properties_for for infinite recursion detection rather than get_properties(Z_OBJPROP).
	 * or checking for recursion on the object itself (php_var_dump).
	 * However, IntVector can only contain integers, making infinite recursion impossible, so it's safe to return new arrays. */
	return teds_intvector_entries_to_refcounted_array(array);
}

static void teds_intvector_entries_clear(teds_intvector_entries *array)
{
	if (!teds_intvector_entries_empty_capacity(array)) {
		efree(array->entries_raw);
	}
}

static void teds_intvector_free_storage(zend_object *object)
{
	teds_intvector *intern = teds_intvector_from_object(object);
	teds_intvector_entries_clear(&intern->array);
	zend_object_std_dtor(&intern->std);
}

static zend_object *teds_intvector_new_ex(zend_class_entry *class_type, zend_object *orig, bool clone_orig)
{
	teds_intvector *intern;

	intern = zend_object_alloc(sizeof(teds_intvector), class_type);
	/* This is a final class */
	ZEND_ASSERT(class_type == teds_ce_IntVector);

	zend_object_std_init(&intern->std, class_type);
	object_properties_init(&intern->std, class_type);
	intern->std.handlers = &teds_handler_IntVector;

	if (orig && clone_orig) {
		teds_intvector *other = teds_intvector_from_object(orig);
		teds_intvector_entries_copy_ctor(&intern->array, &other->array);
	} else {
		intern->array.entries_raw = NULL;
		intern->array.type_tag = TEDS_INTVECTOR_TYPE_UNINITIALIZED;
	}

	return &intern->std;
}

static zend_object *teds_intvector_new(zend_class_entry *class_type)
{
	return teds_intvector_new_ex(class_type, NULL, 0);
}


static zend_object *teds_intvector_clone(zend_object *old_object)
{
	zend_object *new_object = teds_intvector_new_ex(old_object->ce, old_object, 1);

	teds_assert_object_has_empty_member_list(new_object);

	return new_object;
}

static int teds_intvector_count_elements(zend_object *object, zend_long *count)
{
	const teds_intvector *intern = teds_intvector_from_object(object);
	*count = intern->array.size;
	return SUCCESS;
}

/* Get number of entries in this intvector */
PHP_METHOD(Teds_IntVector, count)
{
	zval *object = ZEND_THIS;

	ZEND_PARSE_PARAMETERS_NONE();

	const teds_intvector *intern = Z_INTVECTOR_P(object);
	RETURN_LONG(intern->array.size);
}

/* Get number of entries in this intvector */
PHP_METHOD(Teds_IntVector, isEmpty)
{
	zval *object = ZEND_THIS;

	ZEND_PARSE_PARAMETERS_NONE();

	const teds_intvector *intern = Z_INTVECTOR_P(object);
	RETURN_BOOL(intern->array.size == 0);
}

/* Get capacity of this intvector */
PHP_METHOD(Teds_IntVector, capacity)
{
	zval *object = ZEND_THIS;

	ZEND_PARSE_PARAMETERS_NONE();

	const teds_intvector *intern = Z_INTVECTOR_P(object);
	RETURN_LONG(intern->array.capacity);
}

/* Create this from an optional iterable */
PHP_METHOD(Teds_IntVector, __construct)
{
	zval *object = ZEND_THIS;
	zval* iterable = NULL;

	ZEND_PARSE_PARAMETERS_START(0, 1)
		Z_PARAM_OPTIONAL
		Z_PARAM_ITERABLE(iterable)
	ZEND_PARSE_PARAMETERS_END();

	teds_intvector_entries *array = Z_INTVECTOR_ENTRIES_P(object);

	if (UNEXPECTED(!teds_intvector_entries_uninitialized(array))) {
		zend_throw_exception(spl_ce_RuntimeException, "Called Teds\\IntVector::__construct twice", 0);
		/* called __construct() twice, bail out */
		RETURN_THROWS();
	}
	if (!iterable) {
		teds_intvector_entries_set_empty_list(array);
		return;
	}

	switch (Z_TYPE_P(iterable)) {
		case IS_ARRAY:
			teds_intvector_entries_init_from_array_values(array, Z_ARRVAL_P(iterable));
			return;
		case IS_OBJECT:
			teds_intvector_entries_init_from_traversable(array, Z_OBJ_P(iterable));
			return;
		EMPTY_SWITCH_DEFAULT_CASE();
	}
}

/* Clear this */
PHP_METHOD(Teds_IntVector, clear)
{
	ZEND_PARSE_PARAMETERS_NONE();

	teds_intvector_entries *array = Z_INTVECTOR_ENTRIES_P(ZEND_THIS);

	if (!teds_intvector_entries_empty_capacity(array)) {
		efree(array->entries_raw);
	}
	teds_intvector_entries_set_empty_list(array);
	TEDS_RETURN_VOID();
}

PHP_METHOD(Teds_IntVector, getIterator)
{
	ZEND_PARSE_PARAMETERS_NONE();

	zend_create_internal_iterator_zval(return_value, ZEND_THIS);
}

static void teds_intvector_it_dtor(zend_object_iterator *iter)
{
	zval_ptr_dtor(&iter->data);
}

static void teds_intvector_it_rewind(zend_object_iterator *iter)
{
	((teds_intvector_it*)iter)->current = 0;
}

static int teds_intvector_it_valid(zend_object_iterator *iter)
{
	teds_intvector_it     *iterator = (teds_intvector_it*)iter;
	teds_intvector *object   = Z_INTVECTOR_P(&iter->data);

	if (iterator->current < object->array.size) {
		return SUCCESS;
	}

	return FAILURE;
}


static zval *teds_intvector_it_get_current_data(zend_object_iterator *iter)
{
	teds_intvector_it *iterator = (teds_intvector_it*)iter;
	teds_intvector_entries *array   = Z_INTVECTOR_ENTRIES_P(&iter->data);
	if (UNEXPECTED(iterator->current >= array->size)) {
		zend_throw_exception(spl_ce_OutOfBoundsException, "Index out of range", 0);
		return &EG(uninitialized_zval);
	}
	return teds_intvector_entries_read_offset(array, iterator->current, &iterator->tmp);
}

static void teds_intvector_it_get_current_key(zend_object_iterator *iter, zval *key)
{
	teds_intvector_it     *iterator = (teds_intvector_it*)iter;
	teds_intvector *object   = Z_INTVECTOR_P(&iter->data);

	size_t offset = iterator->current;
	if (offset >= object->array.size) {
		ZVAL_NULL(key);
	} else {
		ZVAL_LONG(key, offset);
	}
}

static void teds_intvector_it_move_forward(zend_object_iterator *iter)
{
	((teds_intvector_it*)iter)->current++;
}

/* iterator handler table */
static const zend_object_iterator_funcs teds_intvector_it_funcs = {
	teds_intvector_it_dtor,
	teds_intvector_it_valid,
	teds_intvector_it_get_current_data,
	teds_intvector_it_get_current_key,
	teds_intvector_it_move_forward,
	teds_intvector_it_rewind,
	NULL,
	NULL, /* get_gc */
};


zend_object_iterator *teds_intvector_get_iterator(zend_class_entry *ce, zval *object, int by_ref)
{
	// This is final
	ZEND_ASSERT(ce == teds_ce_IntVector);
	teds_intvector_it *iterator;

	if (UNEXPECTED(by_ref)) {
		zend_throw_error(NULL, "An iterator cannot be used with foreach by reference");
		return NULL;
	}

	iterator = emalloc(sizeof(teds_intvector_it));

	zend_iterator_init((zend_object_iterator*)iterator);

	ZVAL_OBJ_COPY(&iterator->intern.data, Z_OBJ_P(object));
	iterator->intern.funcs = &teds_intvector_it_funcs;

	return &iterator->intern;
}

PHP_METHOD(Teds_IntVector, __unserialize)
{
	HashTable *raw_data;
	if (zend_parse_parameters(ZEND_NUM_ARGS(), "h", &raw_data) == FAILURE) {
		RETURN_THROWS();
	}
	teds_intvector_entries *array = Z_INTVECTOR_ENTRIES_P(ZEND_THIS);
	if (UNEXPECTED(!teds_intvector_entries_uninitialized(array))) {
		zend_throw_exception(spl_ce_RuntimeException, "Already unserialized", 0);
		RETURN_THROWS();
	}
	if (zend_hash_num_elements(raw_data) == 0) {
		teds_intvector_entries_set_empty_list(array);
		return;
	}

	if (UNEXPECTED(zend_hash_num_elements(raw_data) != 2)) {
		zend_throw_exception(spl_ce_RuntimeException, "IntVector unexpected __unserialize data: expected exactly 2 values", 0);
		RETURN_THROWS();
	}
	const zval *type_tag_zval = zend_hash_index_find(raw_data, 0);
	if (UNEXPECTED(type_tag_zval == NULL || Z_TYPE_P(type_tag_zval) != IS_LONG)) {
		zend_throw_exception(spl_ce_RuntimeException, "IntVector unserialize got invalid type tag, expected int", 0);
		RETURN_THROWS();
	}
	const zend_ulong type_tag = Z_LVAL_P(type_tag_zval);
	if (UNEXPECTED(type_tag >= TEDS_INTVECTOR_TYPE_COUNT)) {
		zend_throw_exception_ex(spl_ce_RuntimeException, 0, "IntVector unserialize got unsupported type tag %d", (int)type_tag);
		RETURN_THROWS();
	}
	const zval *raw_zval = zend_hash_index_find(raw_data, 1);
	if (UNEXPECTED(raw_zval == NULL)) {
		zend_throw_exception(spl_ce_RuntimeException, "IntVector missing data to unserialize", 0);
		RETURN_THROWS();
	}
	array->type_tag = type_tag;
	if (Z_TYPE_P(raw_zval) != IS_STRING) {
		zend_throw_exception(spl_ce_RuntimeException, "IntVector expected string for binary data", 0);
		RETURN_THROWS();
	}
	const size_t str_byte_length = Z_STRLEN_P(raw_zval);
	const char *strval = Z_STRVAL_P(raw_zval);
	switch (type_tag) {
		case TEDS_INTVECTOR_TYPE_INT32 + 1: /* 32-bit check */
#ifndef TEDS_INTVECTOR_TYPE_INT64
			zend_throw_exception(spl_ce_RuntimeException, "IntVector unserialize binary not implemented for serialized int64 on 32-bit builds", 0);
			RETURN_THROWS();
#endif
		case TEDS_INTVECTOR_TYPE_INT32:
		case TEDS_INTVECTOR_TYPE_INT16:
		case TEDS_INTVECTOR_TYPE_INT8: {
			uint8_t shift = teds_lmv_shift_for_element[type_tag];
			const size_t num_elements = str_byte_length >> shift;
			if (UNEXPECTED((num_elements << shift) != str_byte_length)) {
				zend_throw_exception_ex(spl_ce_RuntimeException, 0, "IntVector Unexpected binary length for type tag, expected multiple of 8 * 2**%d, got %d bytes", (int)shift, (int)str_byte_length);
				RETURN_THROWS();
			}
			char *const values = emalloc(str_byte_length);
			array->entries_int8 = (int8_t *)values;
			array->size = num_elements;
			array->capacity = num_elements;
#if WORDS_BIGENDIAN
			/* TODO: Can probably optimize this with C restrict keyword to indicate memory doesn't overlap? */
			if (type_tag != TEDS_INTVECTOR_TYPE_INT8) {
				if (type_tag == TEDS_INTVECTOR_TYPE_INT64) {
					uint64_t *dst = (uint64_t *)values;
					const uint64_t *src = (const uint64_t *)strval;
					const uint64_t *const end = src + num_elements;
					for (;src < end; src++, dst++) {
						/* This compiles down to a bswap assembly instruction in optimized builds */
						*dst = bswap_64(*src);
					}
				} else if (type_tag == TEDS_INTVECTOR_TYPE_INT32) {
					uint32_t *dst = (uint32_t *)values;
					const uint32_t *src = (const uint32_t *)strval;
					const uint32_t *const end = src + num_elements;
					for (;src < end; src++, dst++) {
						/* This compiles down to a bswap assembly instruction in optimized builds */
						*dst = bswap_32(*src);
					}
				} else if (type_tag == TEDS_INTVECTOR_TYPE_INT16) {
					uint16_t *dst = (uint16_t *)values;
					const uint16_t *src = (const uint16_t *)strval;
					const uint16_t *const end = src + num_elements;
					for (;src < end; src++, dst++) {
						/* This compiles down to a bswap assembly instruction in optimized builds */
						*dst = teds_bswap_16(*src);
					}
				} else {
					ZEND_UNREACHABLE();
				}
				return;
			}
#endif
			/* Little-endian and int8_t are just a memcpy */
			memcpy(values, strval, str_byte_length);
			break;
		}
		default:
			ZEND_UNREACHABLE();
	}
}

static zend_always_inline zend_string *teds_create_string_from_entries_int8(const char *raw, const size_t len) {
	return zend_string_init(raw, len, 0);
}

static zend_always_inline zend_string *teds_create_string_from_entries_int16(const char *raw, const size_t len) {
#if WORDS_BIGENDIAN
	zend_string *const result = zend_string_alloc(len * sizeof(int16_t), 0);
	uint16_t *dst = (uint16_t *)ZSTR_VAL(result);
	const uint16_t *src = (const uint16_t *)raw;
	const uint16_t *const end = src + len;
	for (;src < end; src++, dst++) {
		const uint16_t v = *src;
		*dst = teds_bswap_16(*src);
	}
	*(char *)dst = '\0';
	return result;
#else
	return zend_string_init(raw, len * sizeof(int16_t), 0);
#endif
}

static zend_always_inline zend_string *teds_create_string_from_entries_int32(const char *raw, const size_t len) {
#if WORDS_BIGENDIAN
	zend_string *const result = zend_string_alloc(len * sizeof(int32_t), 0);
	uint32_t *dst = (uint32_t *)ZSTR_VAL(result);
	const uint32_t *src = (const uint32_t *)raw;
	const uint32_t *const end = src + len;
	for (;src < end; src++, dst++) {
		/* This compiles down to a bswap assembly instruction in optimized builds */
		*dst = bswap_32(*src);
	}
	*(char *)dst = '\0';
	return result;
#else
	return zend_string_init(raw, len * sizeof(int32_t), 0);
#endif
}

static zend_always_inline zend_string *teds_create_string_from_entries_int64(const char *raw, const size_t len) {
#if WORDS_BIGENDIAN
	zend_string *const result = zend_string_alloc(len * sizeof(int64_t), 0);
	uint64_t *dst = (uint64_t *)ZSTR_VAL(result);
	const uint64_t *src = (const uint64_t *)raw;
	const uint64_t *const end = src + len;
	for (;src < end; src++, dst++) {
		/* This compiles down to a bswap assembly instruction in optimized builds */
		*dst = bswap_64(*src);
	}
	*(char *)dst = '\0';
	return result;
#else
	return zend_string_init(raw, len * sizeof(int64_t), 0);
#endif
}

PHP_METHOD(Teds_IntVector, __serialize)
{
	ZEND_PARSE_PARAMETERS_NONE();

	const teds_intvector_entries *const array = Z_INTVECTOR_ENTRIES_P(ZEND_THIS);
	const size_t len = array->size;
	if (len == 0) {
		RETURN_EMPTY_ARRAY();
	}
	zval tmp;
	uint8_t type_tag = array->type_tag;
	switch (type_tag) {
#define TEDS_INTVECTOR__INT_CODEGEN(TEDS_INTVECTOR_TYPE_X, intx_t, entries_intx) \
		case TEDS_INTVECTOR_TYPE_X: \
			ZVAL_STR(&tmp, teds_create_string_from_##entries_intx((const char *)array->entries_int8, len)); \
			break;
TEDS_INTVECTOR__GENERATE_INT_CASES()
#undef TEDS_INTVECTOR__INT_CODEGEN

		default:
			ZEND_UNREACHABLE();
	}
	RETURN_ARR(teds_create_serialize_pair(type_tag, &tmp));
}

static void teds_intvector_entries_init_from_array_values(teds_intvector_entries *array, zend_array *raw_data)
{
	size_t num_entries = zend_hash_num_elements(raw_data);
	teds_intvector_entries_set_empty_list(array);
	if (num_entries == 0) {
		return;
	}
	/* TODO: Can probably precompute capacity to avoid reallocations */
	zval *val_zv;
	ZEND_HASH_FOREACH_VAL(raw_data, val_zv) {
		zend_long val;
		TEDS_INTVECTOR_VALUE_TO_LONG_OR_THROW(val, val_zv);
		teds_intvector_entries_push(array, val, 1);
	} ZEND_HASH_FOREACH_END();
}

PHP_METHOD(Teds_IntVector, __set_state)
{
	zend_array *array_ht;

	ZEND_PARSE_PARAMETERS_START(1, 1)
		Z_PARAM_ARRAY_HT(array_ht)
	ZEND_PARSE_PARAMETERS_END();
	zend_object *object = teds_intvector_new(teds_ce_IntVector);
	teds_intvector *intern = teds_intvector_from_object(object);
	teds_intvector_entries_init_from_array_values(&intern->array, array_ht);

	RETURN_OBJ(object);
}

static zend_array *teds_intvector_entries_to_refcounted_array(const teds_intvector_entries *array) {
	size_t len = array->size;
	zend_array *values = teds_new_array_check_overflow(len);
	/* Initialize return array */
	zend_hash_real_init_packed(values);

	/* Go through values and add values to the return array */
	ZEND_HASH_FILL_PACKED(values) {
		switch (array->type_tag) {
#define TEDS_INTVECTOR__INT_CODEGEN(TEDS_INTVECTOR_TYPE_X, intx_t, entries_intx) \
			case TEDS_INTVECTOR_TYPE_X: { \
				const intx_t *const entries = array->entries_intx; \
				for (size_t i = 0; i < len; i++) { \
					ZEND_HASH_FILL_SET_LONG(entries[i]); \
					ZEND_HASH_FILL_NEXT(); \
				} \
				break; \
			}
TEDS_INTVECTOR__GENERATE_INT_CASES()
#undef TEDS_INTVECTOR__INT_CODEGEN
			default:
				ZEND_UNREACHABLE();
		}
	} ZEND_HASH_FILL_END();
	return values;
}

PHP_METHOD(Teds_IntVector, toArray)
{
	ZEND_PARSE_PARAMETERS_NONE();
	const teds_intvector_entries *array = Z_INTVECTOR_ENTRIES_P(ZEND_THIS);
	size_t len = array->size;
	if (!len) {
		RETURN_EMPTY_ARRAY();
	}
	RETURN_ARR(teds_intvector_entries_to_refcounted_array(array));
}

static zend_always_inline void teds_intvector_convert_zval_value_to_long_at_offset(zval *return_value, const zval *zval_this, zend_long offset)
{
	teds_intvector_entries *array = Z_INTVECTOR_ENTRIES_P(zval_this);
	size_t len = array->size;
	if (UNEXPECTED((zend_ulong) offset >= len)) {
		zend_throw_exception(spl_ce_OutOfBoundsException, "Index out of range", 0);
		RETURN_THROWS();
	}
	teds_intvector_entries_copy_offset(array, offset, return_value, false);
}

PHP_METHOD(Teds_IntVector, get)
{
	zend_long offset;
	ZEND_PARSE_PARAMETERS_START(1, 1)
		Z_PARAM_LONG(offset)
	ZEND_PARSE_PARAMETERS_END();

	teds_intvector_convert_zval_value_to_long_at_offset(return_value, ZEND_THIS, offset);
}

PHP_METHOD(Teds_IntVector, offsetGet)
{
	zval *offset_zv;
	ZEND_PARSE_PARAMETERS_START(1, 1)
		Z_PARAM_ZVAL(offset_zv)
	ZEND_PARSE_PARAMETERS_END();

	zend_long offset;
	CONVERT_OFFSET_TO_LONG_OR_THROW(offset, offset_zv);

	teds_intvector_convert_zval_value_to_long_at_offset(return_value, ZEND_THIS, offset);
}

PHP_METHOD(Teds_IntVector, offsetExists)
{
	zval *offset_zv;
	ZEND_PARSE_PARAMETERS_START(1, 1)
		Z_PARAM_ZVAL(offset_zv)
	ZEND_PARSE_PARAMETERS_END();

	zend_long offset;
	CONVERT_OFFSET_TO_LONG_OR_THROW(offset, offset_zv);

	RETURN_BOOL((zend_ulong) offset < Z_INTVECTOR_ENTRIES_P(ZEND_THIS)->size);
}

PHP_METHOD(Teds_IntVector, containsKey)
{
	zval *offset_zv;
	ZEND_PARSE_PARAMETERS_START(1, 1)
		Z_PARAM_ZVAL(offset_zv)
	ZEND_PARSE_PARAMETERS_END();

	if (Z_TYPE_P(offset_zv) != IS_LONG) {
		RETURN_FALSE;
	}
	zend_long offset = Z_LVAL_P(offset_zv);

	RETURN_BOOL((zend_ulong) offset < Z_INTVECTOR_ENTRIES_P(ZEND_THIS)->size);
}

PHP_METHOD(Teds_IntVector, indexOf)
{
	zend_long value;
	ZEND_PARSE_PARAMETERS_START(1, 1)
		Z_PARAM_LONG(value)
	ZEND_PARSE_PARAMETERS_END();

	const teds_intvector_entries *array = Z_INTVECTOR_ENTRIES_P(ZEND_THIS);
	const size_t len = array->size;
	if (len == 0) {
		RETURN_NULL();
	}
	switch (array->type_tag) {
		case TEDS_INTVECTOR_TYPE_INT8: {
			const int8_t v = (int8_t) value;
			if (v != value) { RETURN_NULL(); }
			const uint8_t *start = array->entries_uint8;
			const uint8_t *offset = memchr(start, (uint8_t)v, len);
			if (offset) {
				RETURN_LONG(offset - start);
			}
			break;
		}
		case TEDS_INTVECTOR_TYPE_INT16: {
			const int16_t v = (int16_t) value;
			if (v != value) { RETURN_NULL(); }
			const int16_t *start = array->entries_int16;
			const int16_t *it = start;
			for (const int16_t *end = it + len; it < end; it++) {
				if (v == *it) {
					RETURN_LONG(it - start);
				}
			}
			break;
		}
		case TEDS_INTVECTOR_TYPE_INT32: {
			const int32_t v = (int32_t) value;
#if SIZEOF_ZEND_LONG > 4
			if (v != value) { RETURN_NULL(); }
#endif
			const int32_t *start = array->entries_int32;
			const int32_t *it = start;
			for (const int32_t *end = it + len; it < end; it++) {
				if (v == *it) {
					RETURN_LONG(it - start);
				}
			}
			break;
		}
#ifdef TEDS_INTVECTOR_TYPE_INT64
		case TEDS_INTVECTOR_TYPE_INT64: {
			const int64_t *start = array->entries_int64;
			const int64_t *it = start;
			for (const int64_t *end = it + len; it < end; it++) {
				if (value == *it) {
					RETURN_LONG(it - start);
				}
			}
			break;
		}
#endif
		default:
			ZEND_UNREACHABLE();
	}
	RETURN_NULL();
}

PHP_METHOD(Teds_IntVector, contains)
{
	zval *value;
	ZEND_PARSE_PARAMETERS_START(1, 1)
		Z_PARAM_ZVAL(value)
	ZEND_PARSE_PARAMETERS_END();

	const teds_intvector_entries *array = Z_INTVECTOR_ENTRIES_P(ZEND_THIS);
	const size_t len = array->size;
	if (len == 0) {
		RETURN_FALSE;
	}
	switch (array->type_tag) {
		case TEDS_INTVECTOR_TYPE_INT8: {
			if (Z_TYPE_P(value) != IS_LONG) {
				RETURN_FALSE;
			}
			const int8_t v = (int8_t) Z_LVAL_P(value);
			if (v != Z_LVAL_P(value)) { RETURN_FALSE; }
			const int8_t *start = array->entries_int8;
			RETURN_BOOL(memchr((uint8_t *)start, (uint8_t)v, len));
		}
		case TEDS_INTVECTOR_TYPE_INT16: {
			if (Z_TYPE_P(value) != IS_LONG) {
				RETURN_FALSE;
			}
			const int16_t v = (int16_t) Z_LVAL_P(value);
			if (v != Z_LVAL_P(value)) { RETURN_FALSE; }
			const int16_t *start = array->entries_int16;
			const int16_t *it = start;
			for (const int16_t *end = it + len; it < end; it++) {
				if (v == *it) {
					RETURN_TRUE;
				}
			}
			break;
		}
		case TEDS_INTVECTOR_TYPE_INT32: {
			if (Z_TYPE_P(value) != IS_LONG) {
				RETURN_FALSE;
			}
			const int32_t v = (int32_t) Z_LVAL_P(value);
#if SIZEOF_ZEND_LONG > 4
			if (v != Z_LVAL_P(value)) { RETURN_FALSE; }
#endif
			const int32_t *start = array->entries_int32;
			const int32_t *it = start;
			for (const int32_t *end = it + len; it < end; it++) {
				if (v == *it) {
					RETURN_TRUE;
				}
			}
			break;
		}
#ifdef TEDS_INTVECTOR_TYPE_INT64
		case TEDS_INTVECTOR_TYPE_INT64: {
			if (Z_TYPE_P(value) != IS_LONG) {
				RETURN_FALSE;
			}
			const zend_long v = Z_LVAL_P(value);
			const int64_t *it = array->entries_int64;
			for (const int64_t *end = it + len; it < end; it++) {
				if (v == *it) {
					RETURN_TRUE;
				}
			}
			break;
		}
#endif
		default:
			ZEND_UNREACHABLE();
	}
	RETURN_FALSE;
}

static zend_always_inline void teds_intvector_entries_set_value_at_offset(teds_intvector_entries *const array, const zend_long offset, const zend_long value, const bool check_type_tag) {
	if (check_type_tag) {
		teds_intvector_entries_update_type_tag(array, value);
	}
	const size_t len = array->size;
	if (UNEXPECTED((zend_ulong) offset >= len)) {
		zend_throw_exception(spl_ce_OutOfBoundsException, "Index out of range", 0);
		return;
	}
	switch (array->type_tag) {
#define TEDS_INTVECTOR__INT_CODEGEN(TEDS_INTVECTOR_TYPE_X, intx_t, entries_intx) \
		case TEDS_INTVECTOR_TYPE_X: \
			array->entries_intx[offset] = value; \
			break;
TEDS_INTVECTOR__GENERATE_INT_CASES()
#undef TEDS_INTVECTOR__INT_CODEGEN

		default:
			ZEND_UNREACHABLE();
	}
}

PHP_METHOD(Teds_IntVector, set)
{
	zend_long offset;
	zval *value_zv;
	ZEND_PARSE_PARAMETERS_START(2, 2)
		Z_PARAM_LONG(offset)
		Z_PARAM_ZVAL(value_zv)
	ZEND_PARSE_PARAMETERS_END();

	zend_long value;
	TEDS_INTVECTOR_VALUE_TO_LONG_OR_THROW(value, value_zv);

	teds_intvector_entries_set_value_at_offset(Z_INTVECTOR_ENTRIES_P(ZEND_THIS), offset, value, true);
	TEDS_RETURN_VOID();
}

PHP_METHOD(Teds_IntVector, setInt)
{
	zend_long offset, value;
	ZEND_PARSE_PARAMETERS_START(2, 2)
		Z_PARAM_LONG(offset)
		Z_PARAM_LONG(value)
	ZEND_PARSE_PARAMETERS_END();

	teds_intvector_entries_set_value_at_offset(Z_INTVECTOR_ENTRIES_P(ZEND_THIS), offset, value, true);
	TEDS_RETURN_VOID();
}


PHP_METHOD(Teds_IntVector, offsetSet)
{
	zval                  *offset_zv, *value_zv;

	ZEND_PARSE_PARAMETERS_START(2, 2)
		Z_PARAM_ZVAL(offset_zv)
		Z_PARAM_ZVAL(value_zv)
	ZEND_PARSE_PARAMETERS_END();

	zend_long value;
	zend_long offset;
	CONVERT_OFFSET_TO_LONG_OR_THROW(offset, offset_zv);
	TEDS_INTVECTOR_VALUE_TO_LONG_OR_THROW(value, value_zv);

	teds_intvector_entries_set_value_at_offset(Z_INTVECTOR_ENTRIES_P(ZEND_THIS), offset, value, true);
	TEDS_RETURN_VOID();
}

static zend_always_inline void teds_intvector_entries_copy_offset(const teds_intvector_entries *array, const zend_ulong offset, zval *dst, const bool remove)
{
	ZEND_ASSERT(remove ? array->size < array->capacity : array->size <= array->capacity);
	ZEND_ASSERT(remove ? offset <= array->size : offset < array->size);
	ZEND_ASSERT(dst != NULL);

	switch (array->type_tag) {
#define TEDS_INTVECTOR__INT_CODEGEN(TEDS_INTVECTOR_TYPE_X, intx_t, entries_intx) \
		case TEDS_INTVECTOR_TYPE_X: ZVAL_LONG(dst, array->entries_intx[offset]); return;
TEDS_INTVECTOR__GENERATE_INT_CASES();
#undef TEDS_INTVECTOR__INT_CODEGEN
		default:
			ZEND_UNREACHABLE();
	}
}

static zend_always_inline zend_long teds_intvector_entries_read_offset_as_long(const teds_intvector_entries *const array, const size_t offset)
{
	ZEND_ASSERT(array->size <= array->capacity);
	ZEND_ASSERT(offset < array->size);

	switch (array->type_tag) {
#define TEDS_INTVECTOR__INT_CODEGEN(TEDS_INTVECTOR_TYPE_X, intx_t, entries_intx) \
		case TEDS_INTVECTOR_TYPE_X: return array->entries_intx[offset];
TEDS_INTVECTOR__GENERATE_INT_CASES();
#undef TEDS_INTVECTOR__INT_CODEGEN
		default:
			ZEND_UNREACHABLE();
	}
}

static zend_always_inline zval *teds_intvector_entries_read_offset(const teds_intvector_entries *const array, const size_t offset, zval *const tmp)
{
	ZVAL_LONG(tmp, teds_intvector_entries_read_offset_as_long(array, offset));
	return tmp;
}

static zend_always_inline void teds_intvector_entries_update_type_tag(teds_intvector_entries *array, const zend_long val)
{

#define TEDS_RETURN_IF_LVAL_FITS_IN_TYPE(intx_t) do {  \
		if (EXPECTED(val == (intx_t) val)) { return; } \
	} while (0)

	switch (array->type_tag) {
		case TEDS_INTVECTOR_TYPE_UNINITIALIZED: {
			ZEND_ASSERT(teds_intvector_entries_empty_capacity(array));
			if (val != (int8_t)val) {
#ifdef TEDS_INTVECTOR_TYPE_INT64
				if (val != (int32_t)val) {
					array->type_tag = TEDS_INTVECTOR_TYPE_INT64;
					return;
				}
#endif
				if (val != (int16_t)val) {
					array->type_tag = TEDS_INTVECTOR_TYPE_INT32;
					return;
				}
				array->type_tag = TEDS_INTVECTOR_TYPE_INT16;
				return;
			}
			array->type_tag = TEDS_INTVECTOR_TYPE_INT8;
			return;
		}
		case TEDS_INTVECTOR_TYPE_INT8:
			TEDS_RETURN_IF_LVAL_FITS_IN_TYPE(int8_t);

#ifdef TEDS_INTVECTOR_TYPE_INT64
			if (val != (int32_t) val) {
				TEDS_PROMOTE_INT_TYPE_AND_RETURN(int8_t, entries_int8, int64_t, entries_int64, TEDS_INTVECTOR_TYPE_INT64);
				return;
			}
#endif
			if (val != (int16_t) val) {
				TEDS_PROMOTE_INT_TYPE_AND_RETURN(int8_t, entries_int8, int32_t, entries_int32, TEDS_INTVECTOR_TYPE_INT32);
				return;
			}
			TEDS_PROMOTE_INT_TYPE_AND_RETURN(int8_t, entries_int8, int16_t, entries_int16, TEDS_INTVECTOR_TYPE_INT16);
			return;
		case TEDS_INTVECTOR_TYPE_INT16:
			TEDS_RETURN_IF_LVAL_FITS_IN_TYPE(int16_t);

#ifdef TEDS_INTVECTOR_TYPE_INT64
			if (val != (int32_t) val) {
				TEDS_PROMOTE_INT_TYPE_AND_RETURN(int16_t, entries_int16, int64_t, entries_int64, TEDS_INTVECTOR_TYPE_INT64);
			}
#endif
			TEDS_PROMOTE_INT_TYPE_AND_RETURN(int16_t, entries_int16, int32_t, entries_int32, TEDS_INTVECTOR_TYPE_INT32);
			return;
		case TEDS_INTVECTOR_TYPE_INT32:

#ifdef TEDS_INTVECTOR_TYPE_INT64
			if (UNEXPECTED(val != (int32_t) val)) {
				TEDS_PROMOTE_INT_TYPE_AND_RETURN(int32_t, entries_int32, int64_t, entries_int64, TEDS_INTVECTOR_TYPE_INT64);
			}
#endif
			return;
#ifdef TEDS_INTVECTOR_TYPE_INT64
		case TEDS_INTVECTOR_TYPE_INT64:
			return;
#endif

#undef TEDS_PROMOTE_INT_TYPE_AND_RETURN
		default:
			ZEND_UNREACHABLE();
	}
}

static zend_always_inline void teds_intvector_entries_push(teds_intvector_entries *array, const zend_long value, const bool check_capacity)
{
	const size_t old_size = array->size;
	teds_intvector_entries_update_type_tag(array, value);
	/* update_type_tag may raise the capacity */
	const size_t old_capacity = array->capacity;

	if (check_capacity) {
		if (old_size >= old_capacity) {
			ZEND_ASSERT(old_size == old_capacity);
			teds_intvector_entries_raise_capacity(array, old_size > 2 ? old_size * 2 : 4);
		}
	} else {
		ZEND_ASSERT(old_size < old_capacity);
	}
	switch (array->type_tag) {
#define TEDS_INTVECTOR__INT_CODEGEN(TEDS_INTVECTOR_TYPE_X, intx_t, entries_intx) \
		case TEDS_INTVECTOR_TYPE_X: \
			array->entries_intx[old_size] = value; \
			break;
TEDS_INTVECTOR__GENERATE_INT_CASES()
#undef TEDS_INTVECTOR__INT_CODEGEN
		default:
			ZEND_UNREACHABLE();
	}
	array->size++;
}

/* Based on array_push */
PHP_METHOD(Teds_IntVector, push)
{
	const zval *args;
	uint32_t argc;

	ZEND_PARSE_PARAMETERS_START(0, -1)
		Z_PARAM_VARIADIC('+', args, argc)
	ZEND_PARSE_PARAMETERS_END();

	if (UNEXPECTED(argc == 0)) {
		return;
	}
	teds_intvector_entries *array = Z_INTVECTOR_ENTRIES_P(ZEND_THIS);
	const size_t old_size = array->size;
	const size_t new_size = old_size + argc;
	/* The compiler will type check but eliminate dead code on platforms where size_t is 32 bits (4 bytes) */
	if (SIZEOF_SIZE_T < 8 && UNEXPECTED(new_size > MAX_VALID_OFFSET + 1 || new_size < old_size)) {
		teds_error_noreturn_max_intvector_capacity();
		ZEND_UNREACHABLE();
	}

	for (uint32_t i = 0; i < argc; i++) {
		zend_long new_value;
		TEDS_INTVECTOR_VALUE_TO_LONG_OR_THROW(new_value, (&args[i]));
		teds_intvector_entries_push(array, new_value, true);
	}
	TEDS_RETURN_VOID();
}

PHP_METHOD(Teds_IntVector, unshift)
{
	const zval *args;
	uint32_t argc;

	ZEND_PARSE_PARAMETERS_START(0, -1)
		Z_PARAM_VARIADIC('+', args, argc)
	ZEND_PARSE_PARAMETERS_END();

	if (UNEXPECTED(argc == 0)) {
		return;
	}
	teds_intvector_entries *array = Z_INTVECTOR_ENTRIES_P(ZEND_THIS);
	zend_long *const longs = safe_emalloc(argc, sizeof(zend_long), 0);

	for (uint32_t i = 0; i < argc; i++) {
		const zval *const arg = &args[i];
		zend_long v;
		if (UNEXPECTED(Z_TYPE_P(arg) != IS_LONG)) {
			v = teds_intvector_convert_zval_value_to_long(arg);
			if (UNEXPECTED(EG(exception))) {
				efree(longs);
				return;
			}
		} else {
			v = Z_LVAL_P(arg);
		}
		longs[i] = v;
		teds_intvector_entries_update_type_tag(array, v);
	}
	const size_t old_size = array->size;
	const size_t new_size = old_size + argc;
	/* The compiler will type check but eliminate dead code on platforms where size_t is 32 bits (4 bytes) */
	if (SIZEOF_SIZE_T < 8 && UNEXPECTED(new_size > MAX_VALID_OFFSET + 1 || new_size < old_size)) {
		teds_error_noreturn_max_intvector_capacity();
		ZEND_UNREACHABLE();
	}
	if (new_size > array->capacity) {
		teds_intvector_entries_raise_capacity(array, new_size > 2 ? new_size * 2 : 4);
	}
	const uint8_t bytes_per_element = teds_intvector_entries_compute_memory_per_element(array);
	uint8_t *const entries_uint8 = array->entries_uint8;
	memmove(entries_uint8 + argc * bytes_per_element, entries_uint8, bytes_per_element * old_size);
	array->size = new_size;
	for (uint32_t i = 0; i < argc; i++) {
		teds_intvector_entries_set_value_at_offset(array, argc - i - 1, longs[i], false);
	}

	efree(longs);

	TEDS_RETURN_VOID();
}

/* Based on array_push */
PHP_METHOD(Teds_IntVector, pushInts)
{
	const zval *args;
	uint32_t argc;

	ZEND_PARSE_PARAMETERS_START(0, -1)
		Z_PARAM_VARIADIC('+', args, argc)
	ZEND_PARSE_PARAMETERS_END();

	if (UNEXPECTED(argc == 0)) {
		return;
	}
	teds_intvector_entries *array = Z_INTVECTOR_ENTRIES_P(ZEND_THIS);
	const size_t old_size = array->size;
	const size_t new_size = old_size + argc;
	/* The compiler will type check but eliminate dead code on platforms where size_t is 32 bits (4 bytes) */
	if (SIZEOF_SIZE_T < 8 && UNEXPECTED(new_size > MAX_VALID_OFFSET + 1 || new_size < old_size)) {
		teds_error_noreturn_max_intvector_capacity();
		ZEND_UNREACHABLE();
	}

	for (uint32_t i = 0; i < argc; i++) {
		zend_long new_value;
		TEDS_INTVECTOR_VALUE_TO_LONG_OR_THROW(new_value, (&args[i]));
		teds_intvector_entries_push(array, new_value, true);
	}
	TEDS_RETURN_VOID();
}

PHP_METHOD(Teds_IntVector, pop)
{
	ZEND_PARSE_PARAMETERS_NONE();

	teds_intvector_entries *array = Z_INTVECTOR_ENTRIES_P(ZEND_THIS);
	const size_t old_size = array->size;
	if (old_size == 0) {
		zend_throw_exception(spl_ce_UnderflowException, "Cannot pop from empty Teds\\IntVector", 0);
		RETURN_THROWS();
	}
	const size_t old_capacity = array->capacity;
	array->size--;
	teds_intvector_entries_copy_offset(array, array->size, return_value, true);
	if (old_size * 4 < old_capacity) {
		/* Shrink the storage if only a quarter of the capacity is used  */
		const size_t size = old_size - 1;
		const size_t capacity = size > 2 ? size * 2 : 4;
		if (capacity < old_capacity) {
			teds_intvector_entries_shrink_capacity(array, size, capacity, array->entries_raw);
		}
	}
}

PHP_METHOD(Teds_IntVector, last)
{
	ZEND_PARSE_PARAMETERS_NONE();

	const teds_intvector_entries *array = Z_INTVECTOR_ENTRIES_P(ZEND_THIS);
	const size_t old_size = array->size;
	if (old_size == 0) {
		zend_throw_exception(spl_ce_UnderflowException, "Cannot read last value of empty Teds\\IntVector", 0);
		RETURN_THROWS();
	}
	teds_intvector_entries_copy_offset(array, old_size - 1, return_value, false);
}

PHP_METHOD(Teds_IntVector, first)
{
	ZEND_PARSE_PARAMETERS_NONE();

	const teds_intvector_entries *array = Z_INTVECTOR_ENTRIES_P(ZEND_THIS);
	const size_t old_size = array->size;
	if (old_size == 0) {
		zend_throw_exception(spl_ce_UnderflowException, "Cannot read first value of empty Teds\\IntVector", 0);
		RETURN_THROWS();
	}
	teds_intvector_entries_copy_offset(array, 0, return_value, false);
}

PHP_METHOD(Teds_IntVector, shift)
{
	ZEND_PARSE_PARAMETERS_NONE();

	teds_intvector_entries *array = Z_INTVECTOR_ENTRIES_P(ZEND_THIS);
	const size_t old_size = array->size;
	if (old_size == 0) {
		zend_throw_exception(spl_ce_UnderflowException, "Cannot pop from empty Teds\\IntVector", 0);
		RETURN_THROWS();
	}
	const size_t old_capacity = array->capacity;
	const uint8_t bytes_per_element = teds_intvector_entries_compute_memory_per_element(array);
	uint8_t *const entries_uint8 = array->entries_uint8;
	teds_intvector_entries_copy_offset(array, 0, return_value, true);
	array->size--;
	memmove(entries_uint8, entries_uint8 + bytes_per_element, bytes_per_element + (old_size - 1));
	if (old_size * 4 < old_capacity) {
		/* Shrink the storage if only a quarter of the capacity is used  */
		const size_t size = old_size - 1;
		const size_t capacity = size > 2 ? size * 2 : 4;
		if (capacity < old_capacity) {
			teds_intvector_entries_shrink_capacity(array, size, capacity, array->entries_raw);
		}
	}
}

ZEND_COLD PHP_METHOD(Teds_IntVector, offsetUnset)
{
	zval                  *offset_zv;

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "z", &offset_zv) == FAILURE) {
		RETURN_THROWS();
	}
	TEDS_THROW_UNSUPPORTEDOPERATIONEXCEPTION("Teds\\IntVector does not support offsetUnset - elements must be removed by resizing");
}

static void teds_intvector_write_dimension(zend_object *object, zval *offset_zv, zval *value_zv)
{
	teds_intvector_entries *array = &teds_intvector_from_object(object)->array;

	zend_long v;
	TEDS_INTVECTOR_VALUE_TO_LONG_OR_THROW(v, value_zv);

	if (!offset_zv) {
		teds_intvector_entries_push(array, v, true);
		return;
	}

	zend_long offset;
	CONVERT_OFFSET_TO_LONG_OR_THROW(offset, offset_zv);

	if (UNEXPECTED(!teds_offset_within_size_t(offset, array->size))) {
		zend_throw_exception(spl_ce_OutOfBoundsException, "Index invalid or out of range", 0);
		return;
	}
	teds_intvector_entries_set_value_at_offset(array, offset, v, true);
}

static zval *teds_intvector_read_dimension(zend_object *object, zval *offset_zv, int type, zval *rv)
{
	if (UNEXPECTED(!offset_zv || Z_ISUNDEF_P(offset_zv))) {
handle_missing_key:
		if (type != BP_VAR_IS) {
			zend_throw_exception(spl_ce_OutOfBoundsException, "Index out of range", 0);
			return NULL;
		}
		return &EG(uninitialized_zval);
	}

	zend_long offset;
	CONVERT_OFFSET_TO_LONG_OR_THROW_RETURN_NULLPTR(offset, offset_zv);

	const teds_intvector_entries *array = &teds_intvector_from_object(object)->array;

	if (UNEXPECTED(!teds_offset_within_size_t(offset, array->size))) {
		goto handle_missing_key;
	} else {
		return teds_intvector_entries_read_offset(array, (zend_ulong) offset, rv);
	}
}

static int teds_intvector_has_dimension(zend_object *object, zval *offset_zv, int check_empty)
{
	zend_long offset;
	if (UNEXPECTED(Z_TYPE_P(offset_zv) != IS_LONG)) {
		offset = teds_get_offset(offset_zv);
		if (UNEXPECTED(EG(exception))) {
			return 0;
		}
	} else {
		offset = Z_LVAL_P(offset_zv);
	}

	const teds_intvector_entries *array = &teds_intvector_from_object(object)->array;

	if (UNEXPECTED(!teds_offset_within_size_t(offset, array->size))) {
		return 0;
	}

	/* TODO can optimize */
	if (check_empty) {
		return teds_intvector_entries_read_offset_as_long(array, offset) != 0;
	}
	return true; /* int !== null */
}

PHP_MINIT_FUNCTION(teds_intvector)
{
	TEDS_MINIT_IGNORE_UNUSED();
	teds_ce_IntVector = register_class_Teds_IntVector(zend_ce_aggregate, teds_ce_Sequence, php_json_serializable_ce);
	teds_ce_IntVector->create_object = teds_intvector_new;

	memcpy(&teds_handler_IntVector, &std_object_handlers, sizeof(zend_object_handlers));

	teds_handler_IntVector.offset          = XtOffsetOf(teds_intvector, std);
	teds_handler_IntVector.clone_obj       = teds_intvector_clone;
	teds_handler_IntVector.count_elements  = teds_intvector_count_elements;
	teds_handler_IntVector.get_properties  = teds_intvector_get_properties;
	teds_handler_IntVector.get_properties_for = teds_intvector_get_properties_for;
	teds_handler_IntVector.get_gc          = teds_intvector_get_gc;
	teds_handler_IntVector.free_obj        = teds_intvector_free_storage;

	teds_handler_IntVector.read_dimension  = teds_intvector_read_dimension;
	teds_handler_IntVector.write_dimension = teds_intvector_write_dimension;
	teds_handler_IntVector.has_dimension   = teds_intvector_has_dimension;

	teds_ce_IntVector->ce_flags |= ZEND_ACC_FINAL | ZEND_ACC_NO_DYNAMIC_PROPERTIES;
	teds_ce_IntVector->get_iterator = teds_intvector_get_iterator;

	return SUCCESS;
}
