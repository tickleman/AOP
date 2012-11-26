/*
/+----------------------------------------------------------------------+
 | AOP                                                                  |
 +----------------------------------------------------------------------+
 | Copyright (c) 2012 Julien Salleyron, Gérald Croës                    |
 +----------------------------------------------------------------------+
 | This source file is subject to version 3.01 of the PHP license,      |
 | that is bundled with this package in the file LICENSE, and is        |
 | available through the world-wide-web at the following url:           |
 | http://www.php.net/license/3_01.txt.                                 |
 | If you did not receive a copy of the PHP license and are unable to   |
 | obtain it through the world-wide-web, please send a note to          |
 | license@php.net so we can mail you a copy immediately.               |
 +----------------------------------------------------------------------+
 | Author: Julien Salleyron <julien.salleyron@gmail.com>                |
 +----------------------------------------------------------------------+
*/
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "php.h"
#include "ext/standard/php_string.h"
#include "ext/pcre/php_pcre.h"
#include "aop.h"
#include "Zend/zend_operators.h"

ZEND_DECLARE_MODULE_GLOBALS(aop)

static void php_aop_init_globals(zend_aop_globals *aop_globals)
{
}

ZEND_BEGIN_ARG_INFO_EX(arginfo_aop_add, 0, 0, 2) 
    ZEND_ARG_INFO(0,pointcut)
    ZEND_ARG_INFO(0,advice)
ZEND_END_ARG_INFO()

static zend_function_entry aop_functions[] =
{
    PHP_FE(aop_add_around, arginfo_aop_add)
    PHP_FE(aop_add_before,  arginfo_aop_add)
    PHP_FE(aop_add_after, arginfo_aop_add)
    PHP_FE(aop_add_after_returning, arginfo_aop_add)
    PHP_FE(aop_add_after_throwing, arginfo_aop_add)
    {NULL, NULL, NULL}
};

zend_module_entry aop_module_entry =
{
#if ZEND_MODULE_API_NO >= 20010901
    STANDARD_MODULE_HEADER,
#endif
    PHP_AOP_EXTNAME,
    aop_functions,
    PHP_MINIT(aop),
    NULL,
    PHP_RINIT(aop),
    PHP_RSHUTDOWN(aop),
    NULL,
#if ZEND_MODULE_API_NO >= 20010901
    PHP_AOP_VERSION,
#endif
    STANDARD_MODULE_PROPERTIES
};

#ifdef COMPILE_DL_AOP
ZEND_GET_MODULE(aop)
#endif

static zend_class_entry* aop_class_entry;
static zend_class_entry* aop_const_class_entry;

zend_object_handlers AopJoinpoint_object_handlers;

void aop_free_storage(void *object TSRMLS_DC)
{
    AopJoinpoint_object *obj = (AopJoinpoint_object *)object;
//    zend_hash_destroy(obj->std.properties);
//    FREE_HASHTABLE(obj->std.properties);
    efree(obj);
}

zend_object_value aop_create_handler(zend_class_entry *type TSRMLS_DC)
{
    zend_object_value retval;

    AopJoinpoint_object *obj = (AopJoinpoint_object *)emalloc(sizeof(AopJoinpoint_object));
    memset(obj, 0, sizeof(AopJoinpoint_object));
    obj->std.ce = type;

//    ALLOC_HASHTABLE(obj->std.properties);
//    zend_hash_init(obj->std.properties, 0, NULL, ZVAL_PTR_DTOR, 0);

    retval.handle = zend_objects_store_put(obj, NULL,
                                           aop_free_storage, NULL TSRMLS_CC);
    retval.handlers = &AopJoinpoint_object_handlers;

    return retval;
}

ZEND_BEGIN_ARG_INFO_EX(arginfo_aop_args_returnbyref, 0, ZEND_RETURN_REFERENCE, -1)
ZEND_END_ARG_INFO()

static const zend_function_entry aop_methods[] = {
    PHP_ME(AopJoinpoint, getArguments, NULL, 0)
    PHP_ME(AopJoinpoint, getPropertyName, NULL, 0)
    PHP_ME(AopJoinpoint, setArguments, NULL, 0)
    PHP_ME(AopJoinpoint, getKindOfAdvice, NULL, 0)
    PHP_ME(AopJoinpoint, getReturnedValue, arginfo_aop_args_returnbyref, 0)
    PHP_ME(AopJoinpoint, getAssignedValue, arginfo_aop_args_returnbyref, 0)
    PHP_ME(AopJoinpoint, setReturnedValue, NULL, 0)
    PHP_ME(AopJoinpoint, setAssignedValue, NULL, 0)
    PHP_ME(AopJoinpoint, getPointcut, NULL, 0)
    PHP_ME(AopJoinpoint, getObject, NULL, 0)
    PHP_ME(AopJoinpoint, getClassName, NULL, 0)
    PHP_ME(AopJoinpoint, getMethodName, NULL, 0)
    PHP_ME(AopJoinpoint, getFunctionName, NULL, 0)
    PHP_ME(AopJoinpoint, getException, NULL, 0)
    PHP_ME(AopJoinpoint, process, NULL, 0)
    {NULL, NULL, NULL}
};

PHP_RSHUTDOWN_FUNCTION(aop)
{
    efree(aop_g(cache_write_properties));
    efree(aop_g(cache_read_properties));
    return SUCCESS;
}
PHP_RINIT_FUNCTION(aop)
{
    aop_g(count_pcs) = 0;
    aop_g(recurse_num)=0;
    aop_g(lock_function)=0;
    aop_g(count_write_property) = 0;
    aop_g(lock_write_property) = 0;
    aop_g(count_read_property) = 0;
    aop_g(lock_read_property) = 0;
    aop_g(count_aopJoinpoint_cache) = 0;
    aop_g(cache_func_size) = 1024;
    aop_g(cache_func) = ecalloc(1024, sizeof(HashTable *));
    aop_g(cache_write_size) = 1024;
    aop_g(cache_write_properties) = ecalloc(1024, sizeof(handled_ht *));
    aop_g(cache_read_size) = 1024;
    aop_g(cache_read_properties) = ecalloc(1024, sizeof(handled_ht *));
    ALLOC_HASHTABLE(aop_g(aop_functions));
    zend_hash_init(aop_g(aop_functions), 16, NULL, NULL,0);
    return SUCCESS;
}

static zval *get_aopJoinpoint () {
    int i;
    zval *aop_object;
	TSRMLS_FETCH();
    for (i = 0; i < aop_g(count_aopJoinpoint_cache); i++) {
        zval *aop_object = aop_g(aopJoinpoint_cache)[i];
        if (Z_REFCOUNT_P(aop_object) == 1) {
            AopJoinpoint_object *obj = (AopJoinpoint_object *)zend_object_store_get_object(aop_object TSRMLS_CC);
            obj->value = NULL;
            #if ZEND_MODULE_API_NO >= 20100525
            obj->key = NULL;
            #endif
            obj->member = NULL;
            obj->type = 0;
            obj->object = NULL;
            Z_ADDREF_P(aop_object);
            return aop_object;
        }
    }
    aop_g(count_aopJoinpoint_cache)++;
    if (aop_g(count_aopJoinpoint_cache) == 1) {
        aop_g(aopJoinpoint_cache) = emalloc(sizeof(pointcut *));
    } else {
        aop_g(aopJoinpoint_cache) = erealloc(aop_g(aopJoinpoint_cache), aop_g(count_aopJoinpoint_cache)*sizeof(pointcut *));
    }
    MAKE_STD_ZVAL(aop_object);
    Z_TYPE_P(aop_object) = IS_OBJECT;
    (aop_object)->value.obj = aop_create_handler(aop_class_entry TSRMLS_CC);
    aop_g(aopJoinpoint_cache)[aop_g(count_aopJoinpoint_cache)-1] = aop_object;
    Z_ADDREF_P(aop_object);
    return aop_object;
}

ZEND_DLEXPORT zval **zend_std_get_property_ptr_ptr_overload(zval *object, zval *member AOP_KEY_D TSRMLS_DC) {
    zend_execute_data *ex = EG(current_execute_data);
    //Test if ++
    if (ex->opline->opcode != ZEND_PRE_INC_OBJ && ex->opline->opcode != ZEND_POST_INC_OBJ && ex->opline->opcode != ZEND_PRE_DEC_OBJ && ex->opline->opcode != ZEND_POST_DEC_OBJ) {
        return zend_std_get_property_ptr_ptr(object, member AOP_KEY_C TSRMLS_CC);
    } else {
        // Call original to not have a notice
        zend_std_get_property_ptr_ptr(object, member AOP_KEY_C TSRMLS_CC);
        return NULL;
    }
}

ZEND_DLEXPORT zval * zend_std_read_property_overload(zval *object, zval *member, int type AOP_KEY_D TSRMLS_DC) {
    zval *to_return;
    if (aop_g(count_read_property)>0) {
        if (aop_g(lock_read_property)>25) {
            zend_error(E_ERROR, "Too many level of nested advices. Are there any recursive call ?");
        }
        aop_g(lock_read_property)++;
        to_return = test_read_pointcut_and_execute(0, object, member, type, EG(scope) AOP_KEY_C);
        aop_g(lock_read_property)--;
        return to_return;
    } else {
        return zend_std_read_property(object,member, type AOP_KEY_C TSRMLS_CC);
    }
}

static int get_pointcuts_read_properties(zval *object, zval *member, pointcut ***pointcuts AOP_KEY_D) {
    zval *tmp_member;
    int i, count = 0;
    pointcut *current_pc;
    zend_class_entry *ce;
	TSRMLS_FETCH();

    ce = Z_OBJCE_P(object);
    for (i = 0; i < aop_g(count_read_property); i++) {
        current_pc = aop_g(property_pointcuts_read)[i];
        if (current_pc->method[0] != '*') {
            if (!strcmp_with_joker_case(current_pc->method, Z_STRVAL_P(member), 1)) {
                continue;
            }
        }
        //Scope
        if (current_pc->static_state != 2 || current_pc->scope != 0) {
            if (!test_property_scope(current_pc, ce, member AOP_KEY_C)) {
                continue;
            }
        }

        if (pointcut_match_zend_class_entry(current_pc, ce)) {
            if (count==0) {
                (*pointcuts) = emalloc(sizeof(pointcut *));
            } else {
                (*pointcuts) = erealloc((*pointcuts), sizeof(pointcut *)*(count+1));
            }
            (*pointcuts)[count] = current_pc;
            count++;
        }
    }
    return count;

}

static HashTable *make_matching_ht (zend_execute_data *ex) {
    TSRMLS_FETCH();
    zend_function *curr_func;
    HashTable *ht = NULL;
    HashPosition pos;
    pointcut **temp;
    if (ex) {
        curr_func = ex->function_state.function;
    }
    zend_hash_internal_pointer_reset_ex(aop_g(aop_functions), &pos);
    while (zend_hash_get_current_data_ex(aop_g(aop_functions), (void **)&temp, &pos) == SUCCESS) {
        if (pointcut_match_zend_function(*temp, curr_func, ex)) {
            if (ht==NULL) {
                ALLOC_HASHTABLE(ht);
                zend_hash_init(ht, 16, NULL, NULL,0);
            }
            zend_hash_next_index_insert (ht, temp, sizeof(pointcut **), NULL);
        }
        zend_hash_move_forward_ex (aop_g(aop_functions), &pos);
    }
    return ht;

}

static HashTable *get_matching_ht (zval *object, zend_execute_data *ex) {
    TSRMLS_FETCH();
    zend_function *curr_func;
    char *func_name; 
    pointcut_cache *cache = NULL;
    int i;
    zend_object_handle handle;
    if (object==NULL) {
        return make_matching_ht(ex);
    }
    handle = Z_OBJ_HANDLE_P(object);
    if (ex) {
        curr_func = ex->function_state.function;
    }
    func_name = estrdup(curr_func->common.function_name);
    if (handle>=aop_g(cache_func_size)) {
        aop_g(cache_func) = erealloc(aop_g(cache_func), sizeof (HashTable *)*handle+1);
        for (i = aop_g(cache_func_size); i <= handle; i++) {
            aop_g(cache_func)[i] = NULL;
        }
        aop_g(cache_func_size) = handle+1;
    }
    if (aop_g(cache_func)[handle] == NULL) {
        ALLOC_HASHTABLE(aop_g(cache_func)[handle]);
        zend_hash_init(aop_g(cache_func)[handle], 16, NULL, NULL,0);
    } else {
        zend_hash_find(aop_g(cache_func)[handle],func_name, strlen(func_name), (void **)&cache);
    }
    if (cache == NULL || cache->declare_count < aop_g(count_pcs) || cache->ce != Z_OBJCE_P(object)) {
        if (cache != NULL) {
            zend_hash_del(aop_g(cache_func)[handle],func_name, strlen(func_name));
        }
        cache = emalloc (sizeof (pointcut_cache));
        cache->ht = (HashTable *)make_matching_ht (ex);
        if (cache->ht==NULL) {
            cache->count = 0;
        } else {
            cache->count = cache->ht->nApplyCount;
        }
        cache->declare_count = aop_g(count_pcs);
        cache->ce = Z_OBJCE_P(object);
        zend_hash_add(aop_g(cache_func)[handle], func_name, strlen(func_name), cache, sizeof(pointcut_cache), NULL);
    }
    return cache->ht;
}

static void _test_func_pointcut_and_execute(HashPosition pos, HashTable *ht, zend_execute_data *ex, zval *object, zend_class_entry *scope, zend_class_entry *called_scope, int args_overloaded, zval *args, zval **to_return_ptr_ptr) {
    zval *aop_object, *exception;
    TSRMLS_FETCH();
    AopJoinpoint_object *obj;
    pointcut *current_pc;
    pointcut **temp;
    if (ht==NULL) {
        ht = get_matching_ht (object, ex); 
        if (ht==NULL) {
            aop_g(recurse_num)--;
            execute_context (ex, object, scope, called_scope,args_overloaded, args, to_return_ptr_ptr);
            aop_g(recurse_num)++;
            return;
        }
        zend_hash_internal_pointer_reset_ex(ht, &pos);
    } else {
        zend_hash_move_forward_ex (ht, &pos);
    }
    
    if (zend_hash_get_current_data_ex(aop_g(aop_functions), (void **)&temp, &pos) != SUCCESS) {
        aop_g(recurse_num)--;
        execute_context (ex, object, scope, called_scope,args_overloaded, args, to_return_ptr_ptr);
        aop_g(recurse_num)++;
        return;
    }

    current_pc = *temp;

    aop_object = get_aopJoinpoint();
    obj = (AopJoinpoint_object *) zend_object_store_get_object(aop_object TSRMLS_CC);
    obj->current_pointcut = current_pc;
    //obj->current_pointcut_index = current_pointcut_index; 
    obj->pos = pos;
    obj->advice = ht;
    obj->kind_of_advice = current_pc->kind_of_advice;
    obj->object = object;
    obj->to_return_ptr_ptr = to_return_ptr_ptr;
    obj->value = (*to_return_ptr_ptr);
    obj->ex = ex;
    obj->object = object;
    obj->scope = scope;
    obj->called_scope = called_scope;
    obj->args = args;
    obj->args_overloaded = args_overloaded;
    obj->exception = NULL;
    if (current_pc->kind_of_advice & AOP_KIND_BEFORE) {
        if (!EG(exception)) {
            execute_pointcut(current_pc, aop_object);
        }
    }
    if (current_pc->kind_of_advice & AOP_KIND_AROUND) {
        if (!EG(exception)) {
            execute_pointcut(current_pc, aop_object);
            if (obj->value != NULL) {
                (*to_return_ptr_ptr) = obj->value;
            }
        }
    } else {
        _test_func_pointcut_and_execute(pos, ht, ex, object, scope, called_scope, obj->args_overloaded, obj->args, to_return_ptr_ptr);
    }
    if (current_pc->kind_of_advice & AOP_KIND_AFTER) {
        if (current_pc->kind_of_advice & AOP_KIND_CATCH && EG(exception)) {
            exception = EG(exception); 
            obj->exception = exception;
            EG(exception)=NULL;
            execute_pointcut(current_pc, aop_object);
            EG(exception) = exception;
            if (obj->value != NULL) {
                (*to_return_ptr_ptr) = obj->value;
            }
        } else if (current_pc->kind_of_advice & AOP_KIND_RETURN && !EG(exception)) {
            execute_pointcut(current_pc, aop_object);
            if (obj->value != NULL) {
                (*to_return_ptr_ptr) = obj->value;
            }
        }
    }
    Z_DELREF_P(aop_object);
    return;
}
    
    
static void test_func_pointcut_and_execute(int current_pointcut_index, zend_execute_data *ex, zval *object, zend_class_entry *scope, zend_class_entry *called_scope, int args_overloaded, zval *args, zval **to_return_ptr_ptr) {

    zend_function *curr_func = NULL;
    zval *aop_object, *exception;
    AopJoinpoint_object *obj;
    pointcut *current_pc;
    pointcut **temp;
    HashTable *test;
    TSRMLS_FETCH();
    if (current_pointcut_index == aop_g(count_pcs)) {
        aop_g(recurse_num)--;
        execute_context (ex, object, scope, called_scope,args_overloaded, args, to_return_ptr_ptr);
        aop_g(recurse_num)++;
        return;
    }

    if (zend_hash_index_find(aop_g(aop_functions), current_pointcut_index, (void **)&temp)!=SUCCESS) {
        php_printf("ERROR");
    }
    current_pc = *temp;
    //current_pc = aop_g(pcs)[current_pointcut_index];
    

    if (ex) {
        curr_func = ex->function_state.function;
    }
    if (!pointcut_match_zend_function(current_pc, curr_func, ex)) {
        test_func_pointcut_and_execute(current_pointcut_index+1, ex, object, scope, called_scope, args_overloaded, args, to_return_ptr_ptr);
        return;
    }

    aop_object = get_aopJoinpoint();
    obj = (AopJoinpoint_object *) zend_object_store_get_object(aop_object TSRMLS_CC);
    obj->current_pointcut = current_pc;
    obj->current_pointcut_index = current_pointcut_index; 
    obj->kind_of_advice = current_pc->kind_of_advice;
    obj->object = object;
    obj->to_return_ptr_ptr = to_return_ptr_ptr;
    obj->value = (*to_return_ptr_ptr);
    obj->ex = ex;
    obj->object = object;
    obj->scope = scope;
    obj->called_scope = called_scope;
    obj->args = args;
    obj->args_overloaded = args_overloaded;
    obj->exception = NULL;
    if (current_pc->kind_of_advice & AOP_KIND_BEFORE) {
        if (!EG(exception)) {
            execute_pointcut(current_pc, aop_object);
        }
    }
    if (current_pc->kind_of_advice & AOP_KIND_AROUND) {
        if (!EG(exception)) {
            execute_pointcut(current_pc, aop_object);
            if (obj->value != NULL) {
                (*to_return_ptr_ptr) = obj->value;
            }
        }
    } else {
        test_func_pointcut_and_execute(current_pointcut_index+1, ex, object, scope, called_scope, obj->args_overloaded, obj->args, to_return_ptr_ptr);
    }
    if (current_pc->kind_of_advice & AOP_KIND_AFTER) {
        if (current_pc->kind_of_advice & AOP_KIND_CATCH && EG(exception)) {
            exception = EG(exception); 
            obj->exception = exception;
            EG(exception)=NULL;
            execute_pointcut(current_pc, aop_object);
            EG(exception) = exception;
            if (obj->value != NULL) {
                (*to_return_ptr_ptr) = obj->value;
            }
        } else if (current_pc->kind_of_advice & AOP_KIND_RETURN && !EG(exception)) {
            execute_pointcut(current_pc, aop_object);
            if (obj->value != NULL) {
                (*to_return_ptr_ptr) = obj->value;
            }
        }
    }
    Z_DELREF_P(aop_object);
    return;
}
static zval *test_read_pointcut_and_execute(int current_pointcut_index, zval *object, zval *member, int type, zend_class_entry *current_scope AOP_KEY_D) {
    zval *temp, *to_return, *tmp_member;
    zend_class_entry *scope;

    zend_object_handle handle = Z_OBJ_HANDLE_P(object);
    pointcut *current_pc;
    zend_class_entry *ce;
    AopJoinpoint_object *obj;
    zval *aop_object;
    int i;
    pointcut_cache *cache = NULL;
    TSRMLS_FETCH();
    if (Z_TYPE_P(member) != IS_STRING ) {
        ALLOC_ZVAL(tmp_member);
        *tmp_member = *member;
        INIT_PZVAL(tmp_member);
        zval_copy_ctor(tmp_member);
        convert_to_string(tmp_member);
        member = tmp_member;
#if ZEND_MODULE_API_NO >= 20100525
        key = NULL;
#endif
    }

    if (handle>=aop_g(cache_read_size)) {
        aop_g(cache_read_properties) = erealloc(aop_g(cache_read_properties), sizeof (handled_ht *)*handle+1);
        for (i = aop_g(cache_read_size); i <= handle; i++) {
            aop_g(cache_read_properties)[i] = NULL;
        }
        aop_g(cache_read_size) = handle+1;
    }
    if (aop_g(cache_read_properties)[handle] == NULL) {
        aop_g(cache_read_properties)[handle] = emalloc(sizeof(handled_ht *));
        ALLOC_HASHTABLE(aop_g(cache_read_properties)[handle]->ht);
        zend_hash_init(aop_g(cache_read_properties)[handle]->ht, 16, NULL, NULL,0);
    } else {
        zend_hash_find(aop_g(cache_read_properties)[handle]->ht, Z_STRVAL_P(member), Z_STRLEN_P(member), (void **)&cache);
    }
    if (cache == NULL || cache->declare_count < aop_g(count_read_property) || cache->ce != Z_OBJCE_P(object)) {
        if (cache != NULL) {
            zend_hash_del(aop_g(cache_read_properties)[handle]->ht, Z_STRVAL_P(member), Z_STRLEN_P(member));
        }
        cache = emalloc (sizeof (pointcut_cache));
        cache->count = get_pointcuts_read_properties(object, member, &cache->pointcuts_cache AOP_KEY_C);
        cache->declare_count = aop_g(count_read_property);
        cache->ce = Z_OBJCE_P(object);
        zend_hash_add(aop_g(cache_read_properties)[handle]->ht, Z_STRVAL_P(member), Z_STRLEN_P(member), cache, sizeof(pointcut_cache), NULL);
    } else {
    }

    if (current_pointcut_index == cache->count) {
        scope = EG(scope);
        temp = EG(This);
        EG(scope) = current_scope;
        EG(This) = object;
        to_return = zend_std_read_property(object, member, type AOP_KEY_C TSRMLS_CC);
        EG(This) = temp;
        EG(scope) = scope;
        return to_return;
    }
    current_pc = cache->pointcuts_cache[current_pointcut_index];

    aop_object = get_aopJoinpoint();
    obj = (AopJoinpoint_object *) zend_object_store_get_object(aop_object TSRMLS_CC);
    obj->current_pointcut = current_pc;
    obj->current_pointcut_index = current_pointcut_index; 
    obj->kind_of_advice = (current_pc->kind_of_advice&AOP_KIND_WRITE) ? (current_pc->kind_of_advice - AOP_KIND_WRITE) : current_pc->kind_of_advice;
    obj->object = object;
    obj->member = member;
    obj->type = type;
    obj->scope = current_scope;
#if ZEND_MODULE_API_NO >= 20100525
    obj->key = key;
#endif

    if (current_pc->kind_of_advice & AOP_KIND_BEFORE) {
        execute_pointcut (current_pc, aop_object);
    }
    if (current_pc->kind_of_advice & AOP_KIND_AROUND) {
        execute_pointcut (current_pc, aop_object);
        to_return = obj->value;
    } else {
        to_return = test_read_pointcut_and_execute(current_pointcut_index+1, object, member, type, current_scope AOP_KEY_C);
    }
    if (current_pc->kind_of_advice & AOP_KIND_AFTER) {
        execute_pointcut (current_pc, aop_object);
        if (obj->value != NULL) {
            to_return = obj->value;
        }
    }
    Z_DELREF_P(aop_object);
    return to_return;
}
static int test_property_scope (pointcut *current_pc, zend_class_entry *ce, zval *member AOP_KEY_D) {
    zend_property_info *property_info = NULL;
    ulong h;
#if ZEND_MODULE_API_NO < 20100525
    h = zend_get_hash_value(Z_STRVAL_P(member), Z_STRLEN_P(member) + 1);
#else
    h = key ? key->hash_value : zend_get_hash_value(Z_STRVAL_P(member), Z_STRLEN_P(member) + 1);
#endif
    if (zend_hash_quick_find(&ce->properties_info, Z_STRVAL_P(member), Z_STRLEN_P(member)+1, h, (void **) &property_info)==SUCCESS) {
        if (property_info) {
            if (current_pc->static_state != 2) {
                if (current_pc->static_state) {
                    if (!(property_info->flags & ZEND_ACC_STATIC)) {
                        return 0;
                    }
                } else {
                    if ((property_info->flags & ZEND_ACC_STATIC)) {
                        return 0;
                    }
                }
            }       
            if (current_pc->scope != 0 && !(current_pc->scope & (property_info->flags & ZEND_ACC_PPP_MASK))) {
                return 0;
            }
        } else {
            return 0;
        }
    } else {
        if (current_pc->scope != 0 && !(current_pc->scope & ZEND_ACC_PUBLIC)) {
            return 0;
        }
        if (current_pc->static_state == 1) {
            return 0;
        }
    }
    return 1;
}

static int get_pointcuts_write_properties(zval *object, zval *member, pointcut ***pointcuts AOP_KEY_D) {
    int i, count = 0;
    pointcut *current_pc;
    zend_class_entry *ce;
    TSRMLS_FETCH();

    ce = Z_OBJCE_P(object);

    for (i = 0; i < aop_g(count_write_property); i++) {
        current_pc = aop_g(property_pointcuts_write)[i];
        if (current_pc->method[0] != '*') {
            if (!strcmp_with_joker_case(current_pc->method, Z_STRVAL_P(member), 1)) {
                continue;
            }
        }
        //Scope
        if (current_pc->static_state != 2 || current_pc->scope != 0) {
            if (!test_property_scope(current_pc, ce, member AOP_KEY_C)) {
                continue;
            }
        }

        if (pointcut_match_zend_class_entry(current_pc, ce)) {
            if (count == 0) {
                (*pointcuts) = emalloc(sizeof(pointcut *));
            } else {
                (*pointcuts) = erealloc((*pointcuts), sizeof(pointcut *)*(count+1));
            }
            (*pointcuts)[count] = current_pc;
            count++;
        }
    }
    return count;

}

static void test_write_pointcut_and_execute(int current_pointcut_index, zval *object, zval *member, zval *value, zend_class_entry *current_scope AOP_KEY_D) {
    zval *temp, *tmp_member;
    zend_class_entry *scope;
    zend_object_handle handle = Z_OBJ_HANDLE_P(object);
    pointcut *current_pc;
    zend_class_entry *ce;
    AopJoinpoint_object *obj;
    zval *aop_object;
    int i;
    pointcut_cache *cache = NULL;
    TSRMLS_FETCH();

    if (Z_TYPE_P(member) != IS_STRING ) {
        ALLOC_ZVAL(tmp_member);
        *tmp_member = *member;
        INIT_PZVAL(tmp_member);
        zval_copy_ctor(tmp_member);
        convert_to_string(tmp_member);
        member = tmp_member;
#if ZEND_MODULE_API_NO >= 20100525
        key = NULL;
#endif
    }

    if (handle >= aop_g(cache_write_size)) {
        aop_g(cache_write_properties) = erealloc(aop_g(cache_write_properties), sizeof(handled_ht *)*handle+1);
        for (i = aop_g(cache_write_size); i <= handle; i++) {
            aop_g(cache_write_properties)[i] = NULL;
        }
        aop_g(cache_write_size) = handle+1;
    }

    if (aop_g(cache_write_properties)[handle] == NULL) {
        aop_g(cache_write_properties)[handle] = emalloc(sizeof(handled_ht *));
        ALLOC_HASHTABLE(aop_g(cache_write_properties)[handle]->ht);
        zend_hash_init(aop_g(cache_write_properties)[handle]->ht, 16, NULL, NULL, 0);
    } else {
        zend_hash_find(aop_g(cache_write_properties)[handle]->ht, Z_STRVAL_P(member), Z_STRLEN_P(member), (void **)&cache);
    }

    if (cache == NULL || cache->declare_count<aop_g(count_write_property) || cache->ce != Z_OBJCE_P(object)) {
        if (cache != NULL) {
            zend_hash_del(aop_g(cache_write_properties)[handle]->ht, Z_STRVAL_P(member), Z_STRLEN_P(member));
        }
        cache = emalloc (sizeof (pointcut_cache));
        cache->count = get_pointcuts_write_properties(object, member, &cache->pointcuts_cache AOP_KEY_C);
        cache->declare_count = aop_g(count_write_property);
        cache->ce = Z_OBJCE_P(object);
        zend_hash_add(aop_g(cache_write_properties)[handle]->ht, Z_STRVAL_P(member), Z_STRLEN_P(member), cache, sizeof(pointcut_cache), NULL);
    } 

    if (current_pointcut_index == cache->count) {
        scope = EG(scope);
        temp = EG(This);
        EG(scope) = current_scope;
        EG(This) = object;
        zend_std_write_property(object,member,value AOP_KEY_C TSRMLS_CC);
        EG(This) = temp;
        EG(scope) = scope;    
        return;
    }

    current_pc = cache->pointcuts_cache[current_pointcut_index];
    aop_object = get_aopJoinpoint();
    obj = (AopJoinpoint_object *)zend_object_store_get_object(aop_object TSRMLS_CC);
    obj->current_pointcut = current_pc;
    obj->current_pointcut_index = current_pointcut_index;
    obj->kind_of_advice = (current_pc->kind_of_advice&AOP_KIND_READ) ? (current_pc->kind_of_advice - AOP_KIND_READ) : current_pc->kind_of_advice;
    obj->object = object;
    obj->member = member;
    obj->value = value;
    obj->scope = current_scope;
#if ZEND_MODULE_API_NO >= 20100525
    obj->key = key;
#endif
    if (current_pc->kind_of_advice & AOP_KIND_BEFORE) {
        execute_pointcut (current_pc, aop_object);
    }
    if (current_pc->kind_of_advice & AOP_KIND_AROUND) {
        execute_pointcut (current_pc, aop_object);
    } else {
        value = obj->value;
        test_write_pointcut_and_execute(current_pointcut_index+1, object, member, value, current_scope AOP_KEY_C);
    }
    if (current_pc->kind_of_advice & AOP_KIND_AFTER) {
        execute_pointcut (current_pc, aop_object);
    }
    Z_DELREF_P(aop_object);
}

static void execute_pointcut (pointcut *pointcut_to_execute, zval *arg) {
    zval *args[1], *zret_ptr;
    TSRMLS_FETCH();
    args[0] = (zval *)&(arg);
    zret_ptr = NULL;


    pointcut_to_execute->fci.param_count = 1;
    pointcut_to_execute->fci.size = sizeof(pointcut_to_execute->fci);
    pointcut_to_execute->fci.retval_ptr_ptr = &zret_ptr;
    pointcut_to_execute->fci.params = (zval ***)args;
    if (zend_call_function(&(pointcut_to_execute->fci), &(pointcut_to_execute->fcic) TSRMLS_CC) == FAILURE) {
        zend_error(E_ERROR, "Problem in AOP Callback");
    }
    if (zret_ptr != NULL && Z_TYPE_P(zret_ptr) != IS_NULL) {
        AopJoinpoint_object *obj = (AopJoinpoint_object *)zend_object_store_get_object(arg TSRMLS_CC);
        obj->value = zret_ptr;
    }
}

ZEND_DLEXPORT void zend_std_write_property_overload(zval *object, zval *member, zval *value AOP_KEY_D TSRMLS_DC) {
    if (aop_g(count_write_property) > 0) {
        if (aop_g(lock_write_property) > 25) {
            zend_error(E_ERROR, "Too many level of nested advices. Are there any recursive call ?");
        }
        aop_g(lock_write_property)++;
        test_write_pointcut_and_execute(0, object, member, value, EG(scope) AOP_KEY_C);
        aop_g(lock_write_property)--;
    } else {
        zend_std_write_property(object, member, value AOP_KEY_C TSRMLS_CC);
    }
}

static int resource_pointcut;

PHP_MINIT_FUNCTION(aop)
{
    zend_class_entry ce;
    ZEND_INIT_MODULE_GLOBALS(aop, php_aop_init_globals, NULL);
    INIT_CLASS_ENTRY(ce, "AopJoinpoint", aop_methods);
    aop_class_entry = zend_register_internal_class(&ce TSRMLS_CC);
    aop_class_entry->create_object = aop_create_handler;
    memcpy(&AopJoinpoint_object_handlers, zend_get_std_object_handlers(), sizeof(zend_object_handlers));
    AopJoinpoint_object_handlers.clone_obj = NULL;

    REGISTER_LONG_CONSTANT("AOP_KIND_BEFORE", AOP_KIND_BEFORE, CONST_CS | CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("AOP_KIND_AFTER", AOP_KIND_AFTER, CONST_CS | CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("AOP_KIND_AROUND", AOP_KIND_AROUND, CONST_CS | CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("AOP_KIND_PROPERTY", AOP_KIND_PROPERTY, CONST_CS | CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("AOP_KIND_FUNCTION", AOP_KIND_FUNCTION, CONST_CS | CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("AOP_KIND_METHOD", AOP_KIND_METHOD, CONST_CS | CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("AOP_KIND_READ", AOP_KIND_READ, CONST_CS | CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("AOP_KIND_WRITE", AOP_KIND_WRITE, CONST_CS | CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("AOP_KIND_AROUND_WRITE_PROPERTY", AOP_KIND_AROUND_WRITE_PROPERTY, CONST_CS | CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("AOP_KIND_AROUND_READ_PROPERTY", AOP_KIND_AROUND_READ_PROPERTY, CONST_CS | CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("AOP_KIND_BEFORE_WRITE_PROPERTY", AOP_KIND_BEFORE_WRITE_PROPERTY, CONST_CS | CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("AOP_KIND_BEFORE_READ_PROPERTY", AOP_KIND_BEFORE_READ_PROPERTY, CONST_CS | CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("AOP_KIND_AFTER_WRITE_PROPERTY", AOP_KIND_AFTER_WRITE_PROPERTY, CONST_CS | CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("AOP_KIND_AFTER_READ_PROPERTY", AOP_KIND_AFTER_READ_PROPERTY, CONST_CS | CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("AOP_KIND_BEFORE_METHOD", AOP_KIND_BEFORE_METHOD, CONST_CS | CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("AOP_KIND_AFTER_METHOD", AOP_KIND_AFTER_METHOD, CONST_CS | CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("AOP_KIND_AROUND_METHOD", AOP_KIND_AROUND_METHOD, CONST_CS | CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("AOP_KIND_BEFORE_FUNCTION", AOP_KIND_BEFORE_FUNCTION, CONST_CS | CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("AOP_KIND_AFTER_FUNCTION", AOP_KIND_AFTER_FUNCTION, CONST_CS | CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("AOP_KIND_AROUND_FUNCTION", AOP_KIND_AROUND_FUNCTION, CONST_CS | CONST_PERSISTENT);

#if ZEND_MODULE_API_NO < 20100525
    zend_std_write_property = std_object_handlers.write_property;
#endif
    std_object_handlers.write_property = zend_std_write_property_overload;
    zend_std_read_property = std_object_handlers.read_property;
    std_object_handlers.read_property = zend_std_read_property_overload;

    zend_std_get_property_ptr_ptr = std_object_handlers.get_property_ptr_ptr;
    std_object_handlers.get_property_ptr_ptr = zend_std_get_property_ptr_ptr_overload;

    _zend_execute = zend_execute;
    zend_execute  = aop_execute;
    _zend_execute_internal = zend_execute_internal;
    zend_execute_internal  = aop_execute_internal;

    //Resources
    resource_pointcut = zend_register_list_destructors_ex(NULL, NULL, PHP_POINTCUT_RES_NAME, module_number);


    return SUCCESS;
}

PHP_METHOD(AopJoinpoint, getPropertyName){
    AopJoinpoint_object *obj = (AopJoinpoint_object *)zend_object_store_get_object(getThis() TSRMLS_CC);
    if (!(obj->current_pointcut->kind_of_advice & AOP_KIND_PROPERTY)) {
        zend_error(E_ERROR, "getPropertyName is only available when the JoinPoint is a property operation (read or write)"); 
    }
    if (obj->member != NULL) {
        RETURN_ZVAL(obj->member, 1, 0);
        return; 
    }
    RETURN_NULL();
}
PHP_METHOD(AopJoinpoint, getArguments){
    AopJoinpoint_object *obj = (AopJoinpoint_object *)zend_object_store_get_object(getThis() TSRMLS_CC);
    if (obj->current_pointcut->kind_of_advice & AOP_KIND_PROPERTY) {
        zend_error(E_ERROR, "getArguments is only available when the JoinPoint is a function or method call"); 
    }
    if (obj->args == NULL) {
        obj->args = get_current_args(obj->ex TSRMLS_CC);
    }
    if (obj->args != NULL) {
        RETURN_ZVAL(obj->args, 1, 0);
    }   
    RETURN_NULL();
}

PHP_METHOD(AopJoinpoint, setArguments){
    AopJoinpoint_object *obj = (AopJoinpoint_object *)zend_object_store_get_object(getThis() TSRMLS_CC);
    zval *params;
    if (obj->current_pointcut->kind_of_advice & AOP_KIND_PROPERTY) {
        zend_error(E_ERROR, "setArguments is only available when the JoinPoint is a function or ia method call"); 
    }
    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "a", &params) == FAILURE) {
        zend_error(E_ERROR, "setArguments expects an array as its first argument");
        return;
    }
    obj->args = params;
    obj->args_overloaded = 1;
    Z_ADDREF_P(params);
    RETURN_NULL();
}

PHP_METHOD(AopJoinpoint, getKindOfAdvice){
    AopJoinpoint_object *obj = (AopJoinpoint_object *)zend_object_store_get_object(getThis() TSRMLS_CC);
    RETURN_LONG(obj->kind_of_advice);
}

PHP_METHOD(AopJoinpoint, getPointcut){
    AopJoinpoint_object *obj = (AopJoinpoint_object *)zend_object_store_get_object(getThis() TSRMLS_CC);
    RETURN_STRING(obj->current_pointcut->selector, 1);

}

PHP_METHOD(AopJoinpoint, getReturnedValue){
    AopJoinpoint_object *obj = (AopJoinpoint_object *)zend_object_store_get_object(getThis() TSRMLS_CC);
    if (obj->current_pointcut->kind_of_advice & AOP_KIND_PROPERTY) {
        zend_error(E_ERROR, "getReturnedValue is not available when the JoinPoint is a property operation (read or write)"); 
    }
    if (obj->current_pointcut->kind_of_advice & AOP_KIND_BEFORE) {
        zend_error(E_ERROR, "getReturnedValue is not available when the advice was added with aop_add_before");
    }
    if ((*obj->to_return_ptr_ptr) != NULL) {
        zval_ptr_dtor (return_value_ptr);
        (*return_value_ptr) = (*obj->to_return_ptr_ptr);
        Z_ADDREF_P(*obj->to_return_ptr_ptr);
    }
    //    RETURN_NULL();
}

PHP_METHOD(AopJoinpoint, getAssignedValue){
    AopJoinpoint_object *obj = (AopJoinpoint_object *)zend_object_store_get_object(getThis() TSRMLS_CC);
    if (!(obj->kind_of_advice & AOP_KIND_WRITE)) {
        zend_error(E_ERROR, "getAssignedValue is only available when the JoinPoint is a property write operation"); 
    }
    if (obj->value != NULL) {
        zval_ptr_dtor (return_value_ptr);
        *return_value_ptr = obj->value;
        Z_ADDREF_P(obj->value);
    } else {
        RETURN_NULL();
    } 
}

PHP_METHOD(AopJoinpoint, setAssignedValue){
    zval *ret;
    AopJoinpoint_object *obj = (AopJoinpoint_object *)zend_object_store_get_object(getThis() TSRMLS_CC);
    if (obj->kind_of_advice & AOP_KIND_READ) {
        zend_error(E_ERROR, "setAssignedValue is not available when the JoinPoint is a property read operation"); 
    }
    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "z", &ret) == FAILURE) {
        zend_error(E_ERROR, "Error");
        return;
    } 
    obj->value = ret;
    Z_ADDREF_P(ret);
    RETURN_NULL();
}

PHP_METHOD(AopJoinpoint, setReturnedValue){
    zval *ret;
    AopJoinpoint_object *obj = (AopJoinpoint_object *)zend_object_store_get_object(getThis() TSRMLS_CC);
    if (obj->kind_of_advice & AOP_KIND_WRITE) {
        zend_error(E_ERROR, "setReturnedValue is not available when the JoinPoint is a property write operation"); 
    }
    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "z", &ret) == FAILURE) {
        zend_error(E_ERROR, "Error");
        return;
    }

    obj->value = ret;
    Z_ADDREF_P(ret);
    RETURN_NULL();
}

PHP_METHOD(AopJoinpoint, getObject) {
    AopJoinpoint_object *obj = (AopJoinpoint_object *)zend_object_store_get_object(getThis() TSRMLS_CC);
    if (obj->object != NULL) {
        RETURN_ZVAL(obj->object, 1, 0);
    }
    RETURN_NULL();

}

PHP_METHOD(AopJoinpoint, getClassName){
    AopJoinpoint_object *obj = (AopJoinpoint_object *)zend_object_store_get_object(getThis() TSRMLS_CC);
    if (obj->current_pointcut->kind_of_advice & AOP_KIND_PROPERTY) {
        if (obj->object != NULL) {
            zend_class_entry *ce = Z_OBJCE_P(obj->object);
            RETURN_STRING(ce->name, 1);
        }
    } else {
        zend_class_entry *ce;
        zend_execute_data *data = obj->ex;
        zend_function *curr_func;
        if (data == NULL) {
            RETURN_NULL();
        }
        curr_func = data->function_state.function;
        ce = curr_func->common.scope;
        if (ce != NULL) {
            RETURN_STRING(ce->name, 1);
        }
    }
    RETURN_NULL();
}

PHP_METHOD(AopJoinpoint, getFunctionName){
    AopJoinpoint_object *obj = (AopJoinpoint_object *)zend_object_store_get_object(getThis() TSRMLS_CC);
    zend_execute_data *data = obj->ex;
    zend_function *curr_func;
    if (obj->current_pointcut->kind_of_advice & AOP_KIND_PROPERTY || obj->current_pointcut->kind_of_advice & AOP_KIND_METHOD) {
        zend_error(E_ERROR, "getMethodName is only available when the JoinPoint is a function call"); 
    }
    if (data == NULL) {
        RETURN_NULL();
    }
    curr_func = data->function_state.function;
    RETURN_STRING(curr_func->common.function_name, 1);
}

PHP_METHOD(AopJoinpoint, getException){
    AopJoinpoint_object *obj = (AopJoinpoint_object *)zend_object_store_get_object(getThis() TSRMLS_CC);
    if (!(obj->current_pointcut->kind_of_advice & AOP_KIND_CATCH)){
        zend_error(E_ERROR, "getException is only available when the advice was added with aop_add_after or aop_add_after_throwing"); 
    }
    if (obj->exception != NULL) {
        RETURN_ZVAL(obj->exception, 1, 0);
    }
    RETURN_NULL();
}



PHP_METHOD(AopJoinpoint, getMethodName){
    AopJoinpoint_object *obj = (AopJoinpoint_object *)zend_object_store_get_object(getThis() TSRMLS_CC);
    zend_execute_data *data = obj->ex;
    zend_function *curr_func;
    if (obj->current_pointcut->kind_of_advice & AOP_KIND_PROPERTY || obj->current_pointcut->kind_of_advice & AOP_KIND_FUNCTION) {
        zend_error(E_ERROR, "getMethodName is only available when the JoinPoint is a method call"); 
    }
    if (data == NULL) {
        RETURN_NULL();
    }
    curr_func = data->function_state.function;
    RETURN_STRING(curr_func->common.function_name, 1);
}

PHP_METHOD(AopJoinpoint, process){
    AopJoinpoint_object *obj = (AopJoinpoint_object *)zend_object_store_get_object(getThis() TSRMLS_CC);
    if (!obj || !obj->current_pointcut || !obj->current_pointcut->kind_of_advice) {
        zend_error(E_ERROR, "Error");
    }
    if (!(obj->current_pointcut->kind_of_advice & AOP_KIND_AROUND)) {
        zend_error(E_ERROR, "process is only available when the advice was added with aop_add_around"); 
    }
    if (obj->current_pointcut->kind_of_advice & AOP_KIND_PROPERTY) {
        if (obj->kind_of_advice & AOP_KIND_WRITE) {
#if ZEND_MODULE_API_NO >= 20100525
            //            zend_literal *key = obj->key;
            // NULL for no segfault (use by zend_get_property_info_quick)
            zend_literal *key = NULL;
#endif
            test_write_pointcut_and_execute(obj->current_pointcut_index+1, obj->object, obj->member, obj->value, obj->scope AOP_KEY_C);
        } else {
#if ZEND_MODULE_API_NO >= 20100525
            //            zend_literal *key = obj->key;
            // NULL for no segfault (use by zend_get_property_info_quick)
            zend_literal *key = NULL;
#endif
            obj->value = test_read_pointcut_and_execute(obj->current_pointcut_index+1, obj->object, obj->member, obj->type, obj->scope AOP_KEY_C);
        }
    } else {
        _test_func_pointcut_and_execute(obj->pos, obj->advice, obj->ex, obj->object, obj->scope, obj->called_scope, obj->args_overloaded, obj->args, obj->to_return_ptr_ptr);
        obj->value = (*obj->to_return_ptr_ptr);
        if (!EG(exception)) {
            if ((*obj->to_return_ptr_ptr) != NULL) {
                RETURN_ZVAL((*obj->to_return_ptr_ptr), 1, 0);
            } else {
                RETURN_NULL();
            }
        } else {
            RETURN_NULL();
        }
    }
}

static pointcut *add_pointcut_property (zend_fcall_info fci, zend_fcall_info_cache fcic, char *selector, int selector_len, int type TSRMLS_DC) {
    int count;
    type = type|AOP_KIND_PROPERTY;

    if (selector_len > 4 && !strncmp("read", selector, 4)) {
        return aop_add_read(estrndup(selector+5, selector_len-5), fci, fcic, type);
    } else if (selector_len > 5 && !strncmp("write", selector, 5)) {
        return aop_add_write(estrndup(selector+6, selector_len-6), fci, fcic, type);
    } else {
        //We create the pointcut with the read
        pointcut *pc = aop_add_read(selector, fci, fcic, type);

        //And we had it on the write
        pc->kind_of_advice |= AOP_KIND_WRITE;
        aop_g(count_write_property)++;
        if (aop_g(count_write_property) == 1) {
            aop_g(property_pointcuts_write) = emalloc(sizeof(pointcut *));
        } else {
            aop_g(property_pointcuts_write) = erealloc(aop_g(property_pointcuts_write), aop_g(count_write_property)*sizeof(pointcut *));
        }
        aop_g(property_pointcuts_write)[aop_g(count_write_property) - 1] = pc; 
        return pc;
    }
}

static void add_pointcut (zend_fcall_info fci, zend_fcall_info_cache fcic, char *selector, int selector_len, int type , zval **return_value_ptr TSRMLS_DC) {
    pointcut *pc = NULL;
    if (selector_len < 2) {
        zend_error(E_ERROR, "The given pointcut is invalid. You must specify a function call, a method call or a property operation"); 
    }
    if (selector_len > 2 && (selector[selector_len-2] != '(' || selector[selector_len-1] != ')')) {
        pc = add_pointcut_property(fci, fcic, selector, selector_len, type TSRMLS_CC);
    } else {
        int count;
        aop_g(count_pcs)++;
        count = aop_g(count_pcs)-1;
        pc = emalloc(sizeof(pointcut));
        pc->selector = estrdup(selector);
        pc->fci = fci;
        pc->fcic = fcic;
        pc->kind_of_advice = type;
        parse_pointcut(&pc);

        zend_hash_index_update(aop_g(aop_functions), count, &pc, sizeof(pointcut **),NULL);



    }
    if (pc) {
        //ZEND_REGISTER_RESOURCE((*return_value_ptr), pc, resource_pointcut);
    }
}

void make_regexp_on_pointcut (pointcut **pc) { 
    pcre_extra *pcre_extra = NULL;
    int preg_options = 0, i;
    int *replace_count, *new_length;
    char *regexp;
    char tempregexp[500];
    TSRMLS_FETCH();

    (*pc)->method_jok = (strchr((*pc)->method, '*') != NULL);
    replace_count = emalloc (sizeof(int));
    new_length = emalloc (sizeof(int));
    regexp = estrdup((*pc)->method);
    regexp = php_str_to_str_ex(regexp, strlen(regexp), "**\\", 3, "[.#}", 4, new_length, 0, replace_count);
    regexp = php_str_to_str_ex(regexp, strlen(regexp), "**", 2, "[.#]", 4, new_length, 0, replace_count);
    regexp = php_str_to_str_ex(regexp, strlen(regexp), "\\", 1, "\\\\", 2, new_length, 0, replace_count);
    regexp = php_str_to_str_ex(regexp, strlen(regexp), "*", 1, "[^\\\\]*", 6, new_length, 0, replace_count);
    regexp = php_str_to_str_ex(regexp, strlen(regexp), "[.#]", 4, ".*", 2, new_length, 0, replace_count);
    regexp = php_str_to_str_ex(regexp, strlen(regexp), "[.#}", 4, "(.*\\\\)?", 7, new_length, 0, replace_count);
    sprintf((char *)tempregexp, "/^%s$/i", regexp);
    (*pc)->regexp_method = estrdup(tempregexp);
    (*pc)->re_method = pcre_get_compiled_regex(estrdup(tempregexp), &pcre_extra, &preg_options TSRMLS_CC);
    if (!(*pc)->re_method) {
        php_error_docref(NULL TSRMLS_CC, E_WARNING, "Invalid expression");
    }
    if ((*pc)->class_name != NULL) {
        regexp = estrdup((*pc)->class_name);
        regexp = php_str_to_str_ex(regexp, strlen(regexp), "**\\", 3, "[.#}", 4, new_length, 0, replace_count);
        regexp = php_str_to_str_ex(regexp, strlen(regexp), "**", 2, "[.#]", 4, new_length, 0, replace_count);
        regexp = php_str_to_str_ex(regexp, strlen(regexp), "\\", 1, "\\\\", 2, new_length, 0, replace_count);
        regexp = php_str_to_str_ex(regexp, strlen(regexp), "*", 1, "[^\\\\]*", 6, new_length, 0, replace_count);
        regexp = php_str_to_str_ex(regexp, strlen(regexp), "[.#]", 4, ".*", 2, new_length, 0, replace_count);
        regexp = php_str_to_str_ex(regexp, strlen(regexp), "[.#}", 4, "(.*\\\\)?", 7, new_length, 0, replace_count);
        sprintf(tempregexp, "/^%s$/i", regexp);
        (*pc)->regexp_class = estrdup(tempregexp);
        (*pc)->re_class = pcre_get_compiled_regex(estrdup(tempregexp), &pcre_extra, &preg_options TSRMLS_CC);
        if (!(*pc)->re_class) {
            php_error_docref(NULL TSRMLS_CC, E_WARNING, "Invalid expression");
        }
    }
}

static void parse_pointcut (pointcut **pc) {
    char *strval;
    char *space;
    char *temp;
    (*pc)->method = NULL;
    (*pc)->class_name = NULL;
    (*pc)->scope = 0;
    (*pc)->static_state = 2;
    (*pc)->method_jok = 0;
    (*pc)->class_jok = 0;
    strval = estrndup ((*pc)->selector, strlen((*pc)->selector)-2);
    php_strtolower(strval, strlen(strval));
    (*pc)->scope = get_scope(strval);
    (*pc)->static_state = is_static(strval);
    space = strrchr(strval,' ');
    if (space != NULL) {
        strval = space+1;
    }
    temp = strstr(strval, "::");
    if (temp == NULL) {
        temp = strstr(strval, "->");
    }
    if (temp == NULL) {
        (*pc)->method = strval;
    } else {
        (*pc)->method = estrdup(temp+2);
        temp[0] = '\0';
        (*pc)->class_name = strval;
    }
    if ((*pc)->class_name != NULL) {
        (*pc)->kind_of_advice = (*pc)->kind_of_advice|AOP_KIND_METHOD;
        (*pc)->class_jok = (strchr((*pc)->class_name, '*') != NULL);
    } else {
        (*pc)->kind_of_advice = (*pc)->kind_of_advice|AOP_KIND_FUNCTION;
    }
    make_regexp_on_pointcut(pc);
}

static pointcut *aop_add_read (char *selector, zend_fcall_info fci, zend_fcall_info_cache fcic, int type) {
    int nb_char;
    int count;
    char * temp;
    char *strval;
    char *space;
    pointcut *pc;
    TSRMLS_FETCH();

    type = type|AOP_KIND_READ;
    aop_g(count_read_property)++;
    count=aop_g(count_read_property) - 1;
    if (aop_g(count_read_property) == 1) {
        aop_g(property_pointcuts_read) = emalloc(sizeof(pointcut *));
    } else {
        aop_g(property_pointcuts_read) = erealloc(aop_g(property_pointcuts_read),aop_g(count_read_property)*sizeof(pointcut *));
    }
    pc = emalloc(sizeof(pointcut));

    pc->selector = estrdup(selector);
    nb_char = 3;
    space = strrchr(selector,' ');
    if (space != NULL) {
        selector = space+1;
    }
    temp = strstr(selector, "::$");
    if (temp == NULL) {
        temp = strstr(selector, "->$");
    }
    if (temp == NULL) {
        temp = strstr(selector, "::");
        nb_char = 2;
    }
    if (temp == NULL) {
        temp = strstr(selector, "->");
    }
    if (temp == NULL) {
        zend_error(E_ERROR, "You must specify a class and a property name");
    }

    pc->method = estrdup(temp+nb_char);
    pc->class_name = estrndup(selector, temp-selector);
    pc->kind_of_advice = type;
    pc->class_jok = (strchr(pc->class_name, '*') != NULL);
    pc->fci = fci;
    pc->fcic = fcic;
    strval = estrdup(pc->selector);
    php_strtolower(strval, strlen(strval));
    pc->scope = get_scope(strval);
    pc->static_state = is_static(strval);

    make_regexp_on_pointcut(&pc);
    aop_g(property_pointcuts_read)[count]=pc;
    return pc;
}

static pointcut *aop_add_write (char *selector, zend_fcall_info fci, zend_fcall_info_cache fcic, int type) {
    int nb_char;
    int count;
    char * temp;
    char *strval;
    char *space;
    pointcut *pc;
    TSRMLS_FETCH();

    type = type|AOP_KIND_WRITE;
    aop_g(count_write_property)++;
    count = aop_g(count_write_property) - 1;
    if (aop_g(count_write_property) == 1) {
        aop_g(property_pointcuts_write) = emalloc(sizeof(pointcut *));
    } else {
        aop_g(property_pointcuts_write) = erealloc(aop_g(property_pointcuts_write), aop_g(count_write_property)*sizeof(pointcut *));
    }
    pc = emalloc(sizeof(pointcut));
    pc->selector = estrdup (selector);
    space = strrchr(selector,' ');
    if (space != NULL) {
        selector = space+1;
    }
    nb_char=3;
    temp = strstr(selector, "::$");
    if (temp == NULL) {
        temp = strstr(selector, "->$");
    }
    if (temp == NULL) {
        temp = strstr(selector, "::");
        nb_char = 2;
    }
    if (temp == NULL) {
        temp = strstr(selector, "->");
    }
    if (temp == NULL) {
        zend_error(E_ERROR, "You must specify a class and a property name");
    }

    pc->method = estrdup(temp+nb_char);
    pc->class_name = estrndup(selector, temp-selector);
    pc->kind_of_advice = type;
    pc->class_jok = (strchr(pc->class_name, '*') != NULL);
    pc->fci = fci;
    pc->fcic = fcic;
    strval = estrdup(pc->selector);
    php_strtolower(strval, strlen(strval));
    pc->scope = get_scope(strval);
    pc->static_state = is_static(strval);
    make_regexp_on_pointcut(&pc);
    aop_g(property_pointcuts_write)[count] = pc;
    return pc;
}

PHP_FUNCTION(aop_add_around)
{
    zend_fcall_info fci;
    zend_fcall_info_cache fcic= { 0, NULL, NULL, NULL, NULL };
    char *selector;
    int selector_len;
    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "sf", &selector, &selector_len, &fci, &fcic) == FAILURE) {
        zend_error(E_ERROR, "aop_add_around() expects a string for the pointcut as a first argument and a callback as a second argument");
        return;
    }
    if (fci.function_name) {
        Z_ADDREF_P(fci.function_name);
    }
    if (fci.object_ptr) {
        Z_ADDREF_P(fci.object_ptr);
    }
    add_pointcut(fci, fcic, selector, selector_len, AOP_KIND_AROUND, return_value_ptr TSRMLS_CC);
}

PHP_FUNCTION(aop_add_before)
{
    zend_fcall_info fci;
    zend_fcall_info_cache fcic = { 0, NULL, NULL, NULL, NULL };
    char *selector;
    int selector_len;
    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "sf", &selector, &selector_len, &fci, &fcic) == FAILURE) {
        zend_error(E_ERROR, "aop_add_before() expects a string for the pointcut as a first argument and a callback as a second argument");
        return;
    }
    if (fci.function_name) {
        Z_ADDREF_P(fci.function_name);
    }
    if (fci.object_ptr) {
        Z_ADDREF_P(fci.object_ptr);
    }
    add_pointcut(fci, fcic, selector, selector_len, AOP_KIND_BEFORE, return_value_ptr TSRMLS_CC);
}

PHP_FUNCTION(aop_add_after_throwing)
{
    zend_fcall_info fci;
    zend_fcall_info_cache fcic= { 0, NULL, NULL, NULL, NULL };
    char *selector;
    int selector_len;
    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "sf", &selector, &selector_len, &fci, &fcic) == FAILURE) {
        zend_error(E_ERROR, "aop_add_after() expects a string for the pointcut as a first argument and a callback as a second argument");
        return;
    }
    if (fci.function_name) {
        Z_ADDREF_P(fci.function_name);
    }
    if (fci.object_ptr) {
        Z_ADDREF_P(fci.object_ptr);
    }

    add_pointcut(fci, fcic, selector, selector_len, AOP_KIND_AFTER|AOP_KIND_CATCH, return_value_ptr TSRMLS_CC);

}

PHP_FUNCTION(aop_add_after_returning)
{
    zend_fcall_info fci;
    zend_fcall_info_cache fcic= { 0, NULL, NULL, NULL, NULL };
    char *selector;
    int selector_len;
    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "sf", &selector, &selector_len, &fci, &fcic) == FAILURE) {
        zend_error(E_ERROR, "aop_add_after() expects a string for the pointcut as a first argument and a callback as a second argument");
        return;
    }
    if (fci.function_name) {
        Z_ADDREF_P(fci.function_name);
    }
    if (fci.object_ptr) {
        Z_ADDREF_P(fci.object_ptr);
    }

    add_pointcut(fci, fcic, selector, selector_len, AOP_KIND_AFTER|AOP_KIND_RETURN, return_value_ptr TSRMLS_CC);

}

PHP_FUNCTION(aop_add_after)
{
    zend_fcall_info fci;
    zend_fcall_info_cache fcic= { 0, NULL, NULL, NULL, NULL };
    char *selector;
    int selector_len;
    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "sf", &selector, &selector_len, &fci, &fcic) == FAILURE) {
        zend_error(E_ERROR, "aop_add_after() expects a string for the pointcut as a first argument and a callback as a second argument");
        return;
    }
    if (fci.function_name) {
        Z_ADDREF_P(fci.function_name);
    }
    if (fci.object_ptr) {
        Z_ADDREF_P(fci.object_ptr);
    }
    add_pointcut(fci, fcic, selector, selector_len, AOP_KIND_AFTER|AOP_KIND_CATCH|AOP_KIND_RETURN, return_value_ptr TSRMLS_CC);
}

ZEND_DLEXPORT void aop_execute (zend_op_array *ops TSRMLS_DC) {
    zend_execute_data *data;
    zend_function *curr_func = NULL;
    int must_return = (EG(return_value_ptr_ptr)!=NULL);
    data = EG(current_execute_data);

    if (data) {
        curr_func = data->function_state.function;
    }
    if (ops->type==ZEND_EVAL_CODE || curr_func == NULL || curr_func->common.function_name == NULL || aop_g(recurse_num)>25 || aop_g(lock_function) || EG(exception)) {
        _zend_execute(ops TSRMLS_CC);
        return;
    }
    if (!EG(return_value_ptr_ptr)) {
        EG(return_value_ptr_ptr) = emalloc(sizeof(zval *));
        *(EG(return_value_ptr_ptr)) = NULL;
    }
    aop_g(recurse_num)++;
    _test_func_pointcut_and_execute(NULL, NULL, EG(current_execute_data), EG(This), EG(scope),EG(called_scope), 0, NULL, EG(return_value_ptr_ptr));
    aop_g(recurse_num)--;
    if (!must_return 
#if ZEND_MODULE_API_NO >= 20100525
            && !(EG(opline_ptr) && ((zend_op *)EG(opline_ptr))->result_type & EXT_TYPE_UNUSED)
#endif
       ) {
        if (*EG(return_value_ptr_ptr)) {
            zval_ptr_dtor(EG(return_value_ptr_ptr));
            efree(EG(return_value_ptr_ptr));
        } else {
            efree(EG(return_value_ptr_ptr));
        }
    } else if (!must_return) {
        efree(EG(return_value_ptr_ptr));
    } else {
        if (!*EG(return_value_ptr_ptr)) {
            MAKE_STD_ZVAL(*EG(return_value_ptr_ptr));
            Z_TYPE_P(*EG(return_value_ptr_ptr)) = IS_NULL;

        }
    }

}

#if ZEND_MODULE_API_NO < 20121113
void aop_execute_internal (zend_execute_data *current_execute_data, int return_value_used TSRMLS_DC) {
#else
void aop_execute_internal (zend_execute_data *current_execute_data, struct _zend_fcall_info *fci, int return_value_used TSRMLS_DC) {
#endif
    zend_execute_data *data;
    zend_function *curr_func = NULL;

    zval ** to_return_ptr_ptr;

    data = EG(current_execute_data);


    if (data) {
        curr_func = data->function_state.function;
    }
    if (curr_func == NULL || curr_func->common.function_name == NULL || aop_g(recurse_num)>25 || aop_g(lock_function) || EG(exception)) {
        if (_zend_execute_internal) {
#if ZEND_MODULE_API_NO < 20121113
            _zend_execute_internal(current_execute_data, return_value_used TSRMLS_CC);
#else
            _zend_execute_internal(current_execute_data, fci, return_value_used TSRMLS_CC);
#endif
        } else {
#if ZEND_MODULE_API_NO < 20121113
            execute_internal(current_execute_data, return_value_used TSRMLS_CC);
#else
            execute_internal(current_execute_data, fci, return_value_used TSRMLS_CC);
#endif
        }
        return;
    }   

#if ZEND_MODULE_API_NO >= 20100525
    to_return_ptr_ptr = &(*(temp_variable *)((char *) current_execute_data->Ts + current_execute_data->opline->result.var)).var.ptr; 
#else
    to_return_ptr_ptr = &(*(temp_variable *)((char *) current_execute_data->Ts + current_execute_data->opline->result.u.var)).var.ptr;
#endif
    aop_g(recurse_num)++;
    _test_func_pointcut_and_execute(NULL,NULL, EG(current_execute_data), current_execute_data->object, EG(scope), EG(called_scope), 0, NULL, to_return_ptr_ptr);
    aop_g(recurse_num)--;
    // SegFault
    /*
       if (!return_value_used && !(EG(opline_ptr) && ((zend_op *)EG(opline_ptr))->result_type & EXT_TYPE_UNUSED)) {
       zval_ptr_dtor(to_return_ptr_ptr);
       }
    //*/
}

static void execute_context (zend_execute_data *ex, zval *object, zend_class_entry *calling_scope, zend_class_entry *called_scope, int args_overloaded, zval *args, zval **to_return_ptr_ptr) {
    zval **return_value_ptr;
    zval ***params;
    zend_uint i;
    zval **original_return_value;
    HashTable *calling_symbol_table;
    zend_op_array *original_op_array;
    zend_op **original_opline_ptr;
    zend_class_entry *current_scope;
    zend_class_entry *current_called_scope;
    //    zend_class_entry *calling_scope = NULL;
    zval *current_this;
    zend_execute_data *original_execute_data;
    zend_execute_data execute_data;
    zval *original_object;
    HashPosition pos;
    zval ** temp = NULL;
    int arg_count = 0;
    TSRMLS_FETCH();

    if (!EG(active)) {
        //TODO ERROR
        return ;
    }

    if (EG(exception)) {
        //TODO ERROR
        return ;
    }
    execute_data = *ex;

    //EX(function_state).function = fci_cache->function_handler;
    original_object = EX(object);
    EX(object) = object;
    if (object && Z_TYPE_P(object) == IS_OBJECT &&
            (!EG(objects_store).object_buckets || !EG(objects_store).object_buckets[Z_OBJ_HANDLE_P(object)].valid)) {
        //TODO ERROR
        php_printf("ERRRORR");
        return ;
    }
    original_execute_data = EG(current_execute_data);
    EG(current_execute_data) = ex;

    if (args_overloaded) {
        if (args && Z_TYPE_P(args) == IS_ARRAY) {
            args_overloaded = 1;
            arg_count=0;
            zend_hash_internal_pointer_reset_ex(Z_ARRVAL_P(args), &pos);
            while (zend_hash_get_current_data_ex(Z_ARRVAL_P(args), (void **)&temp, &pos) == SUCCESS) {
                arg_count++;
                if (arg_count == 1) {
                    params = emalloc(sizeof(zval **));
                } else {
                    params = erealloc(params, arg_count*sizeof(zval **));
                }
                params[arg_count-1] = temp;
                zend_hash_move_forward_ex(Z_ARRVAL_P(args), &pos);
            }
        }

        if (arg_count > 0) {
            //Copy from zend_call_function
            ZEND_VM_STACK_GROW_IF_NEEDED((int) arg_count + 1);
            for (i=0; i < arg_count; i++) {
                zval *param;
                if (ARG_SHOULD_BE_SENT_BY_REF(EX(function_state).function, i + 1)) {
                    if (!PZVAL_IS_REF(*params[i]) && Z_REFCOUNT_PP(params[i]) > 1) {
                        zval *new_zval;

                        if (!ARG_MAY_BE_SENT_BY_REF(EX(function_state).function, i + 1)) {
                            if (i || UNEXPECTED(ZEND_VM_STACK_ELEMETS(EG(argument_stack)) == (EG(argument_stack)->top))) {
                                zend_vm_stack_push_nocheck((void *) (zend_uintptr_t)i TSRMLS_CC);
                                zend_vm_stack_clear_multiple(TSRMLS_C);
                            }

                            zend_error(E_WARNING, "Parameter %d to %s%s%s() expected to be a reference, value given",
                                    i+1,
                                    EX(function_state).function->common.scope ? EX(function_state).function->common.scope->name : "",
                                    EX(function_state).function->common.scope ? "::" : "",
                                    EX(function_state).function->common.function_name
                                    );
                            return;
                        }

                        ALLOC_ZVAL(new_zval);
                        *new_zval = **params[i];
                        zval_copy_ctor(new_zval);
                        Z_SET_REFCOUNT_P(new_zval, 1);
                        Z_DELREF_PP(params[i]);
                        *params[i] = new_zval;
                    }
                    Z_ADDREF_PP(params[i]);
                    Z_SET_ISREF_PP(params[i]);
                    param = *params[i];
                } else if (PZVAL_IS_REF(*params[i]) && (EX(function_state).function->common.fn_flags & ZEND_ACC_CALL_VIA_HANDLER) == 0 ) {
                    ALLOC_ZVAL(param);
                    *param = **(params[i]);
                    INIT_PZVAL(param);
                    zval_copy_ctor(param);
                } else if (*params[i] != &EG(uninitialized_zval)) {
                    Z_ADDREF_PP(params[i]);
                    param = *params[i];
                } else {
                    ALLOC_ZVAL(param);
                    *param = **(params[i]);
                    INIT_PZVAL(param);
                }
                zend_vm_stack_push_nocheck(param TSRMLS_CC);
            }
            EG(current_execute_data)->function_state.arguments = zend_vm_stack_top(TSRMLS_C);
            zend_vm_stack_push_nocheck((void*)(zend_uintptr_t)arg_count TSRMLS_CC);
        }
    } else {
        arg_count = (int)(zend_uintptr_t) *EX(function_state).arguments;
    }

    current_scope = EG(scope);
    EG(scope) = calling_scope;
    current_this = EG(This);
    current_called_scope = EG(called_scope);
    if (called_scope) {
        EG(called_scope) = called_scope;
    } else if (EX(function_state).function->type != ZEND_INTERNAL_FUNCTION) {
        EG(called_scope) = NULL;
    }

    if (object) {
        if ((EX(function_state).function->common.fn_flags & ZEND_ACC_STATIC)) {
            EG(This) = NULL;
        } else {
            EG(This) = object;

            if (!PZVAL_IS_REF(EG(This))) {
                Z_ADDREF_P(EG(This)); 
            } else {
                zval *this_ptr;
                ALLOC_ZVAL(this_ptr);
                *this_ptr = *EG(This);
                INIT_PZVAL(this_ptr);
                zval_copy_ctor(this_ptr);
                EG(This) = this_ptr;
            }
        }
    } else {
        EG(This) = NULL;
    }

    //    EX(prev_execute_data) = EG(current_execute_data);
    if (EX(function_state).function->type == ZEND_USER_FUNCTION) {
        calling_symbol_table = EG(active_symbol_table);
        EG(scope) = EX(function_state).function->common.scope;

        original_return_value = EG(return_value_ptr_ptr);
        original_op_array = EG(active_op_array);
        EG(return_value_ptr_ptr) = to_return_ptr_ptr;
        EG(active_op_array) = (zend_op_array *) EX(function_state).function;
        original_opline_ptr = EG(opline_ptr);
        _zend_execute(EG(active_op_array) TSRMLS_CC);

        if (EG(symtable_cache_ptr)>=EG(symtable_cache_limit)) {
            zend_hash_destroy(EG(active_symbol_table));
            FREE_HASHTABLE(EG(active_symbol_table));
        } else {
            /* clean before putting into the cache, since clean
               could call dtors, which could use cached hash */
            if (EG(active_symbol_table)) {
                zend_hash_clean(EG(active_symbol_table));
                *(++EG(symtable_cache_ptr)) = EG(active_symbol_table);
            }
        }
        EG(active_op_array) = original_op_array;
        EG(return_value_ptr_ptr)=original_return_value;
        EG(opline_ptr) = original_opline_ptr;
        EG(active_symbol_table) = calling_symbol_table;
    } else if (EX(function_state).function->type == ZEND_INTERNAL_FUNCTION) {
        int call_via_handler = (EX(function_state).function->common.fn_flags & ZEND_ACC_CALL_VIA_HANDLER) != 0;
        if ((*to_return_ptr_ptr)==NULL) {
            ALLOC_INIT_ZVAL(*to_return_ptr_ptr);
        }
        if (EX(function_state).function->common.scope) {
            EG(scope) = EX(function_state).function->common.scope;
        }
        ((zend_internal_function *) EX(function_state).function)->handler(arg_count, *to_return_ptr_ptr, to_return_ptr_ptr, object, 1 TSRMLS_CC);
        /*  We shouldn't fix bad extensions here,
            because it can break proper ones (Bug #34045)
            if (!EX(function_state).function->common.return_reference)
            {
            INIT_PZVAL(*fci->retval_ptr_ptr);
            }*/

    } else { /* ZEND_OVERLOADED_FUNCTION */
        if ((*to_return_ptr_ptr)==NULL) {
            ALLOC_INIT_ZVAL(*to_return_ptr_ptr);
        }
        if (object) {
            Z_OBJ_HT_P(object)->call_method(EX(function_state).function->common.function_name, arg_count, *to_return_ptr_ptr, to_return_ptr_ptr, object, 1 TSRMLS_CC);
        } else {
            zend_error(E_ERROR, "Cannot call overloaded function for non-object");
        }

        if (EX(function_state).function->type == ZEND_OVERLOADED_FUNCTION_TEMPORARY) {
            efree((char*)EX(function_state).function->common.function_name);
        }
        efree(EX(function_state).function);

    }


    EG(current_execute_data) =  original_execute_data;
    if (args_overloaded) {
        zend_vm_stack_clear_multiple(TSRMLS_C);
    }

    if (EG(This)) {
        zval_ptr_dtor(&EG(This));
    }
    EG(called_scope) = current_called_scope;
    EG(scope) = current_scope;
    EG(This) = current_this;
    //    EG(current_execute_data) = EX(prev_execute_data);
    EX(object) = original_object;
}

static int strcmp_with_joker_case(char *str_with_jok, char *str, int case_sensitive) {
    int joker = 0;
    if (str_with_jok[0] == '*') {
        if (str_with_jok[1] == '\0') {
            return 1;
        }
    }
    if (str_with_jok[0] == '*') {
        if (case_sensitive) {
            return !strcmp(str_with_jok+1, str+(strlen(str)-(strlen(str_with_jok)-1)));
        } else {
            return !strcasecmp(str_with_jok+1, str+(strlen(str)-(strlen(str_with_jok)-1)));
        }
    }
    if (str_with_jok[strlen(str_with_jok)-1] == '*') {
        if (case_sensitive) {
            return !strncmp(str_with_jok, str, strlen(str_with_jok)-1);
        } else {
            return !strncasecmp(str_with_jok, str, strlen(str_with_jok)-1);
        }
    }
    if (case_sensitive) {
        return !strcmp(str_with_jok, str);
    } else {
        return !strcasecmp(str_with_jok, str);
    }
}

static int strcmp_with_joker(char *str_with_jok, char *str) {
    return strcmp_with_joker_case (str_with_jok, str, 0);
}

static int is_static (char *str) {
    int i = 0;
    int last = 0;
    char *space;
    char *p_space;
    char *temp;
    space = strchr(str, ' ');
    p_space = str;
    while (space != NULL) {
        if (!strncmp(p_space, "static", space-p_space)) {
            return 1;
        }
        if (!strncmp(p_space, "!static", space-p_space)) {
            return 0;
        }
        temp = space + 1;
        space = strchr(p_space, ' ');
        p_space = temp;
    }
    return 2;
}

static int explode_scope_by_pipe (char *partial) {
    int i = 0;
    int last = 0;
    int toReturn = 0;
    while (i< (int) strlen(partial)) {
        if (partial[i] == '|') {
            if (!strcmp(estrndup(partial+last, i-last), "public")) {
                toReturn = toReturn | ZEND_ACC_PUBLIC;
            }
            if (!strcmp(estrndup(partial+last, i-last), "private")) {
                toReturn = toReturn | ZEND_ACC_PRIVATE;
            }
            if (!strcmp(estrndup(partial+last, i-last), "protected")) {
                toReturn = toReturn | ZEND_ACC_PROTECTED;
            }
            if (!strcmp(estrndup(partial+last, i-last), "!public")) {
                toReturn = toReturn | ZEND_ACC_PROTECTED | ZEND_ACC_PRIVATE;
            }
            if (!strcmp(estrndup(partial+last, i-last), "!private")) {
                toReturn = toReturn | ZEND_ACC_PROTECTED | ZEND_ACC_PUBLIC;
            }
            if (!strcmp(estrndup(partial+last, i-last), "!protected")) {
                toReturn = toReturn | ZEND_ACC_PUBLIC | ZEND_ACC_PRIVATE;
            }
            last = i+1; 
        }
        i++;
    }
    if (!strcmp(estrdup(partial+last), "public")) {
        toReturn = toReturn | ZEND_ACC_PUBLIC;
    }
    if (!strcmp(estrdup(partial+last), "private")) {
        toReturn = toReturn | ZEND_ACC_PRIVATE;
    }
    if (!strcmp(estrdup(partial+last), "protected")) {
        toReturn = toReturn | ZEND_ACC_PROTECTED;
    }
    if (!strcmp(estrdup(partial+last), "!public")) {
        toReturn = toReturn | ZEND_ACC_PROTECTED | ZEND_ACC_PRIVATE;
    }
    if (!strcmp(estrdup(partial+last), "!private")) {
        toReturn = toReturn | ZEND_ACC_PROTECTED | ZEND_ACC_PUBLIC;
    }
    if (!strcmp(estrdup(partial+last), "!protected")) {
        toReturn = toReturn | ZEND_ACC_PUBLIC | ZEND_ACC_PRIVATE;
    }
    return toReturn;
}

static int get_scope (char *str) {
    int i = 0;
    int toReturn = 0;
    char *partial = NULL;
    int last = 0;
    int temp_return;
    while (i < (int) strlen(str)) {
        if (str[i] == ' ') {
            partial = estrndup(str,i);
            temp_return = explode_scope_by_pipe(partial);
            if (temp_return != 0) {
                toReturn |= explode_scope_by_pipe(partial);
            }
            last = i;
        }
        i++;
    }
    return toReturn;
}

static int pointcut_match_zend_class_entry (pointcut *pc, zend_class_entry *ce) {
    int i, matches;

    matches = pcre_exec(pc->re_class, NULL, ce->name, strlen(ce->name), 0, 0, NULL, 0);
    if (matches >= 0) {
        return 1;
    }
    for (i = 0; i < (int) ce->num_interfaces; i++) {
        matches = pcre_exec(pc->re_class, NULL, ce->interfaces[i]->name, strlen(ce->interfaces[i]->name), 0, 0, NULL, 0);
        if (matches >= 0) {
            return 1;
        }
    }
#if ZEND_MODULE_API_NO >= 20100525
    for (i = 0; i < (int) ce->num_traits; i++) {
        matches = pcre_exec(pc->re_class, NULL, ce->traits[i]->name, strlen(ce->traits[i]->name), 0, 0, NULL, 0);
        if (matches>=0) {
            return 1;
        }
    }
#endif
    ce = ce->parent;
    while (ce != NULL) {
        matches = pcre_exec(pc->re_class, NULL, ce->name, strlen(ce->name), 0, 0, NULL, 0);
        if (matches >= 0) {
            return 1;
        }
        ce = ce->parent;
    }
    return 0;
}

static int pointcut_match_zend_function (pointcut *pc, zend_function *curr_func, zend_execute_data *data) {
    zend_class_entry *ce = NULL;
    TSRMLS_FETCH();

    if (pc->static_state != 2) {
        if (pc->static_state) {
            if (!(curr_func->common.fn_flags & ZEND_ACC_STATIC)) {
                return 0;
            }
        } else {
            if ((curr_func->common.fn_flags & ZEND_ACC_STATIC)) {
                return 0;
            }
        }
    }
    if (pc->scope != 0 && !(pc->scope & (curr_func->common.fn_flags & ZEND_ACC_PPP_MASK))) {
        return 0;
    }
    if (pc->class_name == NULL && pc->method[0] == '*' && pc->method[1]=='\0') {
        return 1;
    }
    if (pc->class_name == NULL && curr_func->common.scope != NULL) {    
        return 0;
    }
    if (pc->method_jok) {
        int matches = pcre_exec(pc->re_method, NULL, curr_func->common.function_name, strlen(curr_func->common.function_name), 0, 0, NULL, 0);

        if (matches < 0) {
            return 0;
        }
    } else {
        if (strcasecmp(pc->method, curr_func->common.function_name)) {
            return 0;
        }
    }
    if (data != NULL) {
        if (curr_func->common.fn_flags & ZEND_ACC_STATIC) {
            ce = curr_func->common.scope;
        } else if (data->object != NULL && Z_OBJCE(*data->object) != NULL) {
            ce = Z_OBJCE(*data->object);
        }
    }
    if (ce != NULL) {
        return pointcut_match_zend_class_entry(pc, ce);
    }
    if (pc->class_name == NULL) {
        return 1;
    }
    return 0;
}

static zval *get_current_args (zend_execute_data *ex TSRMLS_DC) {
    void **p;
    int arg_count;
    int i;
    zval *return_value;
    MAKE_STD_ZVAL(return_value);
    array_init(return_value);
    if (!ex || !ex->function_state.arguments) {
        zend_error(E_WARNING, "Problem in AOP getArgs");
        return 0;
    }

    p = ex->function_state.arguments;
    arg_count = (int)(zend_uintptr_t) *p;

    for (i=0; i < arg_count; i++) {
        zval *element;
        element = *((zval **) (p-(arg_count-i)));
        zend_hash_next_index_insert(return_value->value.ht, &element, sizeof(zval *), NULL);
    }
    return return_value;
}
