#pragma once
typedef int gpio_num_t;
enum gpio_int_type_t { GPIO_INTR_DISABLE, GPIO_INTR_POSEDGE, GPIO_INTR_NEGEDGE, GPIO_INTR_ANYEDGE,
                       GPIO_INTR_LOW_LEVEL, GPIO_INTR_HIGH_LEVEL };
inline int gpio_wakeup_enable(gpio_num_t, gpio_int_type_t) { return 0; }
inline int gpio_wakeup_disable(gpio_num_t) { return 0; }
inline int gpio_set_intr_type(gpio_num_t, gpio_int_type_t) { return 0; }
