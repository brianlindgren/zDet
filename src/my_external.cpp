extern "C" {
    #include "m_pd.h"
}

static t_class *my_external_class;

typedef struct _my_external {
    t_object  x_obj;
} t_my_external;

void my_external_bang(t_my_external *x) {
    (void)x; // Suppress unused parameter warning
    post("Hello, Pure Data!");
}

void *my_external_new(void) {
    t_my_external *x = (t_my_external *)pd_new(my_external_class);
    return (void *)x;
}

extern "C" void my_external_setup(void) {
    my_external_class = class_new(gensym("my_external"),
                                  (t_newmethod)my_external_new,
                                  0,
                                  sizeof(t_my_external),
                                  CLASS_DEFAULT,
                                  A_NULL);
    class_addbang(my_external_class, my_external_bang);
}
