
/*
  +------------------------------------------------------------------------+
  | Phalcon Framework                                                      |
  +------------------------------------------------------------------------+
  | Copyright (c) 2011-2014 Phalcon Team (http://www.phalconphp.com)       |
  +------------------------------------------------------------------------+
  | This source file is subject to the New BSD License that is bundled     |
  | with this package in the file docs/LICENSE.txt.                        |
  |                                                                        |
  | If you did not receive a copy of the license and are unable to         |
  | obtain it through the world-wide-web, please send an email             |
  | to license@phalconphp.com so we can send you a copy immediately.       |
  +------------------------------------------------------------------------+
  | Authors: Andres Gutierrez <andres@phalconphp.com>                      |
  |          Eduar Carvajal <eduar@phalconphp.com>                         |
  +------------------------------------------------------------------------+
*/

#include "mvc/view/engine.h"
#include "mvc/view/engineinterface.h"
#include "mvc/view/exception.h"
#include "di/injectable.h"

#include <Zend/zend_closures.h>

#include "kernel/main.h"
#include "kernel/memory.h"
#include "kernel/object.h"
#include "kernel/fcall.h"
#include "kernel/array.h"
#include "kernel/concat.h"
#include "kernel/operators.h"
#include "kernel/exception.h"

#include "interned-strings.h"

/**
 * Phalcon\Mvc\View\Engine
 *
 * All the template engine adapters must inherit this class. This provides
 * basic interfacing between the engine and the Phalcon\Mvc\View component.
 */
zend_class_entry *phalcon_mvc_view_engine_ce;

PHP_METHOD(Phalcon_Mvc_View_Engine, __construct);
PHP_METHOD(Phalcon_Mvc_View_Engine, getContent);
PHP_METHOD(Phalcon_Mvc_View_Engine, partial);
PHP_METHOD(Phalcon_Mvc_View_Engine, getView);
PHP_METHOD(Phalcon_Mvc_View_Engine, addMethod);
PHP_METHOD(Phalcon_Mvc_View_Engine, __call);

ZEND_BEGIN_ARG_INFO_EX(arginfo_phalcon_mvc_view_engine___construct, 0, 0, 1)
	ZEND_ARG_INFO(0, view)
	ZEND_ARG_INFO(0, dependencyInjector)
ZEND_END_ARG_INFO()

static const zend_function_entry phalcon_mvc_view_engine_method_entry[] = {
	PHP_ME(Phalcon_Mvc_View_Engine, __construct, arginfo_phalcon_mvc_view_engine___construct, ZEND_ACC_PUBLIC|ZEND_ACC_CTOR)
	PHP_ME(Phalcon_Mvc_View_Engine, getContent, NULL, ZEND_ACC_PUBLIC)
	PHP_ME(Phalcon_Mvc_View_Engine, partial, arginfo_phalcon_mvc_view_engineinterface_partial, ZEND_ACC_PUBLIC)
	PHP_ME(Phalcon_Mvc_View_Engine, getView, NULL, ZEND_ACC_PUBLIC)
	PHP_ME(Phalcon_Mvc_View_Engine, addMethod, arginfo_phalcon_mvc_view_engineinterface_addmethod, ZEND_ACC_PUBLIC)
	PHP_ME(Phalcon_Mvc_View_Engine, __call, arginfo_phalcon_mvc_view_engineinterface___call, ZEND_ACC_PUBLIC)
	PHP_FE_END
};

/**
 * Phalcon\Mvc\View\Engine initializer
 */
PHALCON_INIT_CLASS(Phalcon_Mvc_View_Engine){

	PHALCON_REGISTER_CLASS_EX(Phalcon\\Mvc\\View, Engine, mvc_view_engine, phalcon_di_injectable_ce, phalcon_mvc_view_engine_method_entry, ZEND_ACC_EXPLICIT_ABSTRACT_CLASS);

	zend_declare_property_null(phalcon_mvc_view_engine_ce, SL("_view"), ZEND_ACC_PROTECTED);
	zend_declare_property_null(phalcon_mvc_view_engine_ce, SL("_methods"), ZEND_ACC_PROTECTED);

	zend_class_implements(phalcon_mvc_view_engine_ce, 1, phalcon_mvc_view_engineinterface_ce);

	return SUCCESS;
}

/**
 * Phalcon\Mvc\View\Engine constructor
 *
 * @param Phalcon\Mvc\ViewInterface $view
 * @param Phalcon\DiInterface $dependencyInjector
 */
PHP_METHOD(Phalcon_Mvc_View_Engine, __construct){

	zval *view, *dependency_injector = NULL;

	phalcon_fetch_params(0, 1, 1, &view, &dependency_injector);

	if (dependency_injector && Z_TYPE_P(dependency_injector) != IS_NULL) {
		PHALCON_CALL_METHODW(NULL, getThis(), "setdi", dependency_injector);
	}

	phalcon_update_property_zval(getThis(), SL("_view"), view);
}

/**
 * Returns cached ouput on another view stage
 *
 * @return array
 */
PHP_METHOD(Phalcon_Mvc_View_Engine, getContent){

	zval view = {};
	phalcon_read_property(&view, getThis(), SL("_view"), PH_NOISY);
	PHALCON_RETURN_CALL_METHODW(&view, "getcontent");
}

/**
 * Renders a partial inside another view
 *
 * @param string $partialPath
 * @param array $params
 * @return string
 */
PHP_METHOD(Phalcon_Mvc_View_Engine, partial){

	zval *partial_path, *params = NULL, view = {};

	phalcon_fetch_params(0, 1, 1, &partial_path, &params);

	if (!params) {
		params = &PHALCON_GLOBAL(z_null);
	}

	phalcon_read_property(&view, getThis(), SL("_view"), PH_NOISY);
	PHALCON_RETURN_CALL_METHODW(&view, "partial", partial_path, params);
}

/**
 * Returns the view component related to the adapter
 *
 * @return Phalcon\Mvc\ViewInterface
 */
PHP_METHOD(Phalcon_Mvc_View_Engine, getView){


	RETURN_MEMBER(getThis(), "_view");
}

/**
 * Adds a user-defined method
 *
 * @param string $name
 * @param closure $methodCallable
 * @return Phalcon\Mvc\View\Engine
 */
PHP_METHOD(Phalcon_Mvc_View_Engine, addMethod){

	zval *name, *method_callable, class_name = {}, method = {};

	phalcon_fetch_params(0, 2, 0, &name, &method_callable);
	PHALCON_ENSURE_IS_STRING(name);

	if (Z_TYPE_P(method_callable) != IS_OBJECT || !instanceof_function(Z_OBJCE_P(method_callable), zend_ce_closure)) {
		PHALCON_THROW_EXCEPTION_STRW(phalcon_mvc_view_exception_ce, "Method must be an closure object");
		return;
	}

	phalcon_get_class(&class_name, getThis(), 0);

	PHALCON_CALL_CE_STATICW(&method, zend_ce_closure, "bind", method_callable, getThis(), &class_name);

	phalcon_update_property_array(getThis(), SL("_methods"), name, &method);
	RETURN_THISW();
}

/**
 * Handles method calls when a method is not implemented
 *
 * @param string $method
 * @param array $arguments
 * @return mixed
 */
PHP_METHOD(Phalcon_Mvc_View_Engine, __call){

	zval *method, *args = NULL, method_name = {}, arguments = {}, methods = {}, func = {}, exception_message = {}, service_name = {}, service = {}, callback = {};

	phalcon_fetch_params(0, 1, 1, &method, &arguments);

	PHALCON_CPY_WRT(&method_name, method);

	if (!args) {
		array_init(&arguments);
	} else {
		PHALCON_CPY_WRT(&arguments, args);
	}

	phalcon_read_property(&methods, getThis(), SL("_methods"), PH_NOISY);
	if (phalcon_array_isset_fetch(&func, &methods, &method_name, 0)) {
			PHALCON_CALL_USER_FUNC_ARRAYW(return_value, &func, &arguments);
			return;
	}

	if (phalcon_compare_strict_string(&method_name, SL("get")) 
		|| phalcon_compare_strict_string(&method_name, SL("getPost"))
		|| phalcon_compare_strict_string(&method_name, SL("getPut"))
		|| phalcon_compare_strict_string(&method_name, SL("getQuery"))
		|| phalcon_compare_strict_string(&method_name, SL("getServer"))) {
		ZVAL_STRING(&service_name, ISV(request));
	} else if (phalcon_compare_strict_string(&method_name, SL("getSession"))) {
		ZVAL_STRING(&method_name, "get");
		ZVAL_STRING(&service_name, ISV(session));
	} else if (phalcon_compare_strict_string(&method_name, SL("getParam"))) {
		ZVAL_STRING(&service_name, ISV(dispatcher));
	}

	PHALCON_CALL_METHODW(&service, getThis(), "getresolveservice", &service_name);

	if (Z_TYPE(service) != IS_OBJECT) {
		PHALCON_CONCAT_SVS(&exception_message, "The injected service '", &service_name, "' is not valid");
		PHALCON_THROW_EXCEPTION_ZVALW(phalcon_mvc_view_exception_ce, &exception_message);
		return;
	}

	if (phalcon_method_exists(&service, &method_name) == FAILURE) {
		PHALCON_CONCAT_SVS(&exception_message, "The method \"", &method_name, "\" doesn't exist on view");
		PHALCON_THROW_EXCEPTION_ZVALW(phalcon_mvc_view_exception_ce, &exception_message);
		return;
	}

	array_init(&callback);
	phalcon_array_append(&callback, &service, PH_COPY);
	phalcon_array_append(&callback, &method_name, PH_COPY);

	PHALCON_CALL_USER_FUNC_ARRAYW(return_value, &callback, &arguments);
}
