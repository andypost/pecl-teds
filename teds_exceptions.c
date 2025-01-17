/*
  +----------------------------------------------------------------------+
  | teds extension for PHP                                               |
  | See COPYING file for further copyright information                   |
  +----------------------------------------------------------------------+
  | Author: Tyson Andre <tandre@php.net>                                 |
  +----------------------------------------------------------------------+
*/
#include "Zend/zend_interfaces.h"
#include "php.h"
#include "teds_exceptions.h"
#include "teds_exceptions_arginfo.h"
#include "Zend/zend_exceptions.h"
//#include "Zend/zend_portability.h"
#include "Zend/zend_types.h"
#include "ext/spl/spl_exceptions.h"

zend_class_entry *teds_ce_UnsupportedOperationException;

void teds_throw_unsupportedoperationexception(const char *message) {
	zend_throw_exception(teds_ce_UnsupportedOperationException, message, 0);
}

void teds_register_exceptions(void)
{
	teds_ce_UnsupportedOperationException = register_class_Teds_UnsupportedOperationException(spl_ce_RuntimeException);
}
