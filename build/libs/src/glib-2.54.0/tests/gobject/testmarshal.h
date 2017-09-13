/* This file is generated, all changes will be lost */
#ifndef __TEST_MARSHAL_MARSHAL_H__
#define __TEST_MARSHAL_MARSHAL_H__

#include <glib-object.h>

G_BEGIN_DECLS

/* BOOLEAN:INT (./testmarshal.list:2) */
extern
void test_marshal_BOOLEAN__INT (GClosure     *closure,
                                GValue       *return_value,
                                guint         n_param_values,
                                const GValue *param_values,
                                gpointer      invocation_hint,
                                gpointer      marshal_data);

/* STRING:INT (./testmarshal.list:3) */
extern
void test_marshal_STRING__INT (GClosure     *closure,
                               GValue       *return_value,
                               guint         n_param_values,
                               const GValue *param_values,
                               gpointer      invocation_hint,
                               gpointer      marshal_data);

/* VARIANT:POINTER (./testmarshal.list:4) */
extern
void test_marshal_VARIANT__POINTER (GClosure     *closure,
                                    GValue       *return_value,
                                    guint         n_param_values,
                                    const GValue *param_values,
                                    gpointer      invocation_hint,
                                    gpointer      marshal_data);


G_END_DECLS

#endif /* __TEST_MARSHAL_MARSHAL_H__ */
