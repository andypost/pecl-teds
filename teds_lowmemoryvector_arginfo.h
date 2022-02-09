/* This is a generated file, edit the .stub.php file instead.
 * Stub hash: 1f93ee2bb9efeadcda35b594d1855accb1ea95a9 */

ZEND_BEGIN_ARG_INFO_EX(arginfo_class_Teds_LowMemoryVector___construct, 0, 0, 0)
	ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, iterator, IS_ITERABLE, 0, "[]")
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_OBJ_INFO_EX(arginfo_class_Teds_LowMemoryVector_getIterator, 0, 0, InternalIterator, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Teds_LowMemoryVector_count, 0, 0, IS_LONG, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Teds_LowMemoryVector_isEmpty, 0, 0, _IS_BOOL, 0)
ZEND_END_ARG_INFO()

#define arginfo_class_Teds_LowMemoryVector_capacity arginfo_class_Teds_LowMemoryVector_count

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Teds_LowMemoryVector___serialize, 0, 0, IS_ARRAY, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Teds_LowMemoryVector___unserialize, 0, 1, IS_VOID, 0)
	ZEND_ARG_TYPE_INFO(0, data, IS_ARRAY, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_OBJ_INFO_EX(arginfo_class_Teds_LowMemoryVector___set_state, 0, 1, Teds\\LowMemoryVector, 0)
	ZEND_ARG_TYPE_INFO(0, array, IS_ARRAY, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Teds_LowMemoryVector_push, 0, 0, IS_VOID, 0)
	ZEND_ARG_VARIADIC_TYPE_INFO(0, values, IS_MIXED, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Teds_LowMemoryVector_pop, 0, 0, IS_MIXED, 0)
ZEND_END_ARG_INFO()

#define arginfo_class_Teds_LowMemoryVector_toArray arginfo_class_Teds_LowMemoryVector___serialize

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Teds_LowMemoryVector_get, 0, 1, IS_MIXED, 0)
	ZEND_ARG_TYPE_INFO(0, offset, IS_LONG, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Teds_LowMemoryVector_set, 0, 2, IS_VOID, 0)
	ZEND_ARG_TYPE_INFO(0, offset, IS_LONG, 0)
	ZEND_ARG_TYPE_INFO(0, value, IS_MIXED, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Teds_LowMemoryVector_offsetGet, 0, 1, IS_MIXED, 0)
	ZEND_ARG_TYPE_INFO(0, offset, IS_MIXED, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Teds_LowMemoryVector_offsetExists, 0, 1, _IS_BOOL, 0)
	ZEND_ARG_TYPE_INFO(0, offset, IS_MIXED, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Teds_LowMemoryVector_offsetSet, 0, 2, IS_VOID, 0)
	ZEND_ARG_TYPE_INFO(0, offset, IS_MIXED, 0)
	ZEND_ARG_TYPE_INFO(0, value, IS_MIXED, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Teds_LowMemoryVector_offsetUnset, 0, 1, IS_VOID, 0)
	ZEND_ARG_TYPE_INFO(0, offset, IS_MIXED, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Teds_LowMemoryVector_indexOf, 0, 1, IS_LONG, 1)
	ZEND_ARG_TYPE_INFO(0, value, IS_MIXED, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Teds_LowMemoryVector_contains, 0, 1, _IS_BOOL, 0)
	ZEND_ARG_TYPE_INFO(0, value, IS_MIXED, 0)
ZEND_END_ARG_INFO()

#define arginfo_class_Teds_LowMemoryVector_jsonSerialize arginfo_class_Teds_LowMemoryVector___serialize


ZEND_METHOD(Teds_LowMemoryVector, __construct);
ZEND_METHOD(Teds_LowMemoryVector, getIterator);
ZEND_METHOD(Teds_LowMemoryVector, count);
ZEND_METHOD(Teds_LowMemoryVector, isEmpty);
ZEND_METHOD(Teds_LowMemoryVector, capacity);
ZEND_METHOD(Teds_LowMemoryVector, __serialize);
ZEND_METHOD(Teds_LowMemoryVector, __unserialize);
ZEND_METHOD(Teds_LowMemoryVector, __set_state);
ZEND_METHOD(Teds_LowMemoryVector, push);
ZEND_METHOD(Teds_LowMemoryVector, pop);
ZEND_METHOD(Teds_LowMemoryVector, toArray);
ZEND_METHOD(Teds_LowMemoryVector, get);
ZEND_METHOD(Teds_LowMemoryVector, set);
ZEND_METHOD(Teds_LowMemoryVector, offsetGet);
ZEND_METHOD(Teds_LowMemoryVector, offsetExists);
ZEND_METHOD(Teds_LowMemoryVector, offsetSet);
ZEND_METHOD(Teds_LowMemoryVector, offsetUnset);
ZEND_METHOD(Teds_LowMemoryVector, indexOf);
ZEND_METHOD(Teds_LowMemoryVector, contains);


static const zend_function_entry class_Teds_LowMemoryVector_methods[] = {
	ZEND_ME(Teds_LowMemoryVector, __construct, arginfo_class_Teds_LowMemoryVector___construct, ZEND_ACC_PUBLIC)
	ZEND_ME(Teds_LowMemoryVector, getIterator, arginfo_class_Teds_LowMemoryVector_getIterator, ZEND_ACC_PUBLIC)
	ZEND_ME(Teds_LowMemoryVector, count, arginfo_class_Teds_LowMemoryVector_count, ZEND_ACC_PUBLIC)
	ZEND_ME(Teds_LowMemoryVector, isEmpty, arginfo_class_Teds_LowMemoryVector_isEmpty, ZEND_ACC_PUBLIC)
	ZEND_ME(Teds_LowMemoryVector, capacity, arginfo_class_Teds_LowMemoryVector_capacity, ZEND_ACC_PUBLIC)
	ZEND_ME(Teds_LowMemoryVector, __serialize, arginfo_class_Teds_LowMemoryVector___serialize, ZEND_ACC_PUBLIC)
	ZEND_ME(Teds_LowMemoryVector, __unserialize, arginfo_class_Teds_LowMemoryVector___unserialize, ZEND_ACC_PUBLIC)
	ZEND_ME(Teds_LowMemoryVector, __set_state, arginfo_class_Teds_LowMemoryVector___set_state, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	ZEND_ME(Teds_LowMemoryVector, push, arginfo_class_Teds_LowMemoryVector_push, ZEND_ACC_PUBLIC)
	ZEND_ME(Teds_LowMemoryVector, pop, arginfo_class_Teds_LowMemoryVector_pop, ZEND_ACC_PUBLIC)
	ZEND_ME(Teds_LowMemoryVector, toArray, arginfo_class_Teds_LowMemoryVector_toArray, ZEND_ACC_PUBLIC)
	ZEND_ME(Teds_LowMemoryVector, get, arginfo_class_Teds_LowMemoryVector_get, ZEND_ACC_PUBLIC)
	ZEND_ME(Teds_LowMemoryVector, set, arginfo_class_Teds_LowMemoryVector_set, ZEND_ACC_PUBLIC)
	ZEND_ME(Teds_LowMemoryVector, offsetGet, arginfo_class_Teds_LowMemoryVector_offsetGet, ZEND_ACC_PUBLIC)
	ZEND_ME(Teds_LowMemoryVector, offsetExists, arginfo_class_Teds_LowMemoryVector_offsetExists, ZEND_ACC_PUBLIC)
	ZEND_ME(Teds_LowMemoryVector, offsetSet, arginfo_class_Teds_LowMemoryVector_offsetSet, ZEND_ACC_PUBLIC)
	ZEND_ME(Teds_LowMemoryVector, offsetUnset, arginfo_class_Teds_LowMemoryVector_offsetUnset, ZEND_ACC_PUBLIC)
	ZEND_ME(Teds_LowMemoryVector, indexOf, arginfo_class_Teds_LowMemoryVector_indexOf, ZEND_ACC_PUBLIC)
	ZEND_ME(Teds_LowMemoryVector, contains, arginfo_class_Teds_LowMemoryVector_contains, ZEND_ACC_PUBLIC)
	ZEND_MALIAS(Teds_LowMemoryVector, jsonSerialize, toArray, arginfo_class_Teds_LowMemoryVector_jsonSerialize, ZEND_ACC_PUBLIC)
	ZEND_FE_END
};

static zend_class_entry *register_class_Teds_LowMemoryVector(zend_class_entry *class_entry_IteratorAggregate, zend_class_entry *class_entry_Countable, zend_class_entry *class_entry_JsonSerializable, zend_class_entry *class_entry_ArrayAccess)
{
	zend_class_entry ce, *class_entry;

	INIT_NS_CLASS_ENTRY(ce, "Teds", "LowMemoryVector", class_Teds_LowMemoryVector_methods);
	class_entry = zend_register_internal_class_ex(&ce, NULL);
	class_entry->ce_flags |= ZEND_ACC_FINAL;
	zend_class_implements(class_entry, 4, class_entry_IteratorAggregate, class_entry_Countable, class_entry_JsonSerializable, class_entry_ArrayAccess);

	return class_entry;
}