/**
 *
 * Phantom OS
 *
 * Copyright (C) 2005-2008 Dmitry Zavalishin, dz@dz.ru
 *
 * Kernel ready: yes
 *
 *
**/

#ifndef PVM_OBJECT_H
#define PVM_OBJECT_H

#include <phantom_types.h>
#include <phantom_libc.h>
#include <limits.h>


// This structure must be first in any object
// for garbage collector to work ok

#define PVM_OBJECT_START_MARKER 0x7FAA7F55


struct object_PVM_ALLOC_Header
{
    unsigned int		object_start_marker;
    int32_t      		refCount; // for fast dealloc of locally-owned objects. If grows to INT_MAX it will be fixed at that value and ignored further. Such objects will be GC'ed in usual way
    unsigned char		alloc_flags;
    unsigned char		gc_flags;
    unsigned int		exact_size; // full object size including this header
};


struct pvm_object
{
    struct pvm_object_storage	* data;
    struct pvm_object_storage	* interface; // method list is here
};

typedef struct pvm_object pvm_object_t;

struct pvm_object_storage
{
    struct object_PVM_ALLOC_Header	_ah;

    struct pvm_object  		        _class;
    u_int32_t				_flags; // TODO int! Need more flags
    unsigned int			_da_size; // in bytes!

    unsigned char                       da[];
};

typedef struct pvm_object_storage pvm_object_storage_t;



/**
 *
 * Create object. Fixed (in class) or dynamic size.
 *
 * das is data area size in bytes.
 *
**/

struct pvm_object	pvm_create_object(struct pvm_object type);
//struct pvm_object     	pvm_object_create_fixed( struct pvm_object object_class );

// THIS CAN'T BE USED FOR INTERNAL ONES! TODO: Remove or fix to use with internals
struct pvm_object     	pvm_object_create_dynamic( struct pvm_object object_class, int das );


struct data_area_4_thread;
/**
 * Lookup class. TODO reimplement! Can block!
 */

struct pvm_object pvm_exec_lookup_class( struct data_area_4_thread *thread, struct pvm_object name);

/**
 *
 * Is null*
 *
**/

#define pvm_is_null( o ) (o.data == 0 || (o.data == pvm_create_null_object().data))

/**
 *
 * Class is
 *
**/

int pvm_object_class_is( struct pvm_object object, struct pvm_object tclass );

/**
 *
 * Access object fields.
 * TODO: Check for not internal on read.
 *
**/

struct pvm_object       pvm_get_field( struct pvm_object_storage *, unsigned int no );
struct pvm_object       pvm_get_ofield( struct pvm_object, unsigned int no );
void       		pvm_set_field( struct pvm_object_storage *, unsigned int no, struct pvm_object value );
void       		pvm_set_ofield( struct pvm_object, unsigned int no, struct pvm_object value );

// Need it here? It will be called by usual set filed ones...
struct pvm_object  	pvm_get_array_ofield(struct pvm_object_storage *o, unsigned int slot  );
void 			pvm_set_array_ofield(struct pvm_object_storage *o, unsigned int slot, struct pvm_object value );

void 			pvm_append_array(struct pvm_object_storage *array, struct pvm_object value_to_append );
int                     get_array_size(struct pvm_object_storage *array);

// Debug

void pvm_object_print( struct pvm_object );
void dumpo( int addr );

/**
 *
 * Fast access to some basic classes, see pvm_root.c.
 *
**/

struct pvm_object     pvm_get_null_class();
struct pvm_object     pvm_get_class_class();
struct pvm_object     pvm_get_interface_class();
struct pvm_object     pvm_get_code_class();
struct pvm_object     pvm_get_int_class();
struct pvm_object     pvm_get_string_class();
struct pvm_object     pvm_get_array_class();
struct pvm_object     pvm_get_page_class();
struct pvm_object     pvm_get_thread_class();
struct pvm_object     pvm_get_call_frame_class();
struct pvm_object     pvm_get_istack_class();
struct pvm_object     pvm_get_ostack_class();
struct pvm_object     pvm_get_estack_class();
struct pvm_object     pvm_get_boot_class();
struct pvm_object     pvm_get_binary_class();
struct pvm_object     pvm_get_bitmap_class();


struct pvm_object     pvm_create_null_object();
struct pvm_object     pvm_create_class_object(struct pvm_object name, struct pvm_object iface, int da_size);
struct pvm_object     pvm_create_interface_object( int n_methods, struct pvm_object parent_class );
//struct pvm_object     pvm_create_interface_object();
struct pvm_object     pvm_create_code_object(int size, void *code);
struct pvm_object     pvm_create_int_object(int value);
struct pvm_object     pvm_create_string_object(const char *value);
struct pvm_object     pvm_create_string_object_binary(const char *value, int length);
struct pvm_object     pvm_create_string_object_binary_cat(
	const char *value1, int n_bytes1,
        const char *value2, int n_bytes2 );

//struct pvm_object     pvm_create_array_object();
struct pvm_object     pvm_create_page_object( int n_slots, struct pvm_object *init, int init_slots );
struct pvm_object     pvm_create_thread_object( struct pvm_object start_call_frame );
struct pvm_object     pvm_create_call_frame_object();
struct pvm_object     pvm_create_istack_object();
struct pvm_object     pvm_create_ostack_object();
struct pvm_object     pvm_create_estack_object();
struct pvm_object     pvm_create_binary_object(int size, void *init);



//static __inline__ struct pvm_object     pvm_get_null_object() { return pvm_create_null_object(); }
#define pvm_get_null_object pvm_create_null_object

/**
 *
 * This is the most general object creation function. Serves bytecode 'new'.
 * Must call above creators on special cases.
 *
**/


struct pvm_object     pvm_create_object(struct pvm_object type);


/**
 *
 * Create a shallow copy of object.
 *
**/

struct pvm_object     pvm_copy_object(struct pvm_object orig);




void pvm_check_is_thread( struct pvm_object new_thread );



#endif // PVM_OBJECT_H
