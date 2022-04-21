// Communication with HX711 ADC
//
// Copyright (C) 2021 Konstantin Vogel <konstantin.vogel@gmx.net>
//
// This file may be distributed under the terms of the GNU GPLv3 license.

#include "autoconf.h" // CONFIG_*
#include "board/misc.h" // timer_from_us
#include "command.h" // shutdown DECL_COMMAND
#include "compiler.h" // ARRAY_SIZE
#include "generic/armcm_timer.h" // udelay
#include "sched.h" // sched_shutdown DECL_TASK
#include "basecmd.h" // oid_alloc
#include "board/gpio.h" // struct gpio_adc
#include "board/irq.h" // irq_disable

struct hx711 {
    uint32_t oid;
    int32_t sample;
    uint32_t sample_idx;
    uint32_t gain;
    uint32_t SAMPLE_INTERVAL;
    uint32_t COMM_DELAY;
    struct timer timer;
    struct gpio_in dout;
    struct gpio_out sck;
};

static uint_fast8_t hx711_event(struct timer *timer)
{
    struct hx711 *h = container_of(timer, struct hx711, timer);
    uint32_t out = 0; // set clock low

    // if first sample index and dout pin high, wait
    if (h->sample_idx == 0 && gpio_in_read(h->dout)) {
        // no limit on acknowledgement of dout low
        h->timer.waketime += 2*h->COMM_DELAY;
    }
    // if index odd read value and wait
    else if (h->sample_idx % 2) {
        if (h->sample_idx < 48){
            uint8_t read_bit = gpio_in_read(h->dout);
            h->sample = (h->sample << 1) | read_bit;
        }
        h->sample_idx++;
        h->timer.waketime += h->COMM_DELAY;
    }
    else {
        // if index even, set clock high
        out = 1;
        h->sample_idx++;
        h->timer.waketime += h->COMM_DELAY;
    }
    if (h->sample_idx >= 48 + 2*h->gain){
        // set clock low for slow mcus, 60us high and hx711 goes to sleep
        gpio_out_write(h->sck, out);
        uint32_t next_begin_time = h->timer.waketime + h->SAMPLE_INTERVAL;
        sendf("hx711_in_state oid=%c next_clock=%u value=%i", h->oid,
            next_begin_time, h->sample >> 8);
        out = 0;
        h->sample_idx = 0;
        h->sample = 0;
        h->timer.waketime = next_begin_time;
    }
    gpio_out_write(h->sck, out);
    return SF_RESCHEDULE;
}


void command_config_hx711(uint32_t *args)
{
    struct hx711 *h = oid_alloc(args[0], command_config_hx711, sizeof(*h));
    h->dout = gpio_in_setup(args[1], 1); // enable pullup
    h->sck = gpio_out_setup(args[2], 0); // initialize as low
    h->gain = args[3];
    h->SAMPLE_INTERVAL = args[4]*(CONFIG_CLOCK_FREQ/10);
    h->COMM_DELAY = args[5]*(10*(CONFIG_CLOCK_FREQ/1000000));
    h->sample_idx = 0;
    h->sample = 0;
    h->oid = args[0];
}
DECL_COMMAND(command_config_hx711,
    "config_hx711 oid=%c dout_pin=%u sck_pin=%u gain=%u"
    " sample_interval=%u comm_delay=%u");


void command_query_hx711(uint32_t *args)
{
    struct hx711 *h = oid_lookup(args[0], command_config_hx711);
    sched_del_timer(&h->timer);
    h->timer.func = hx711_event;
    h->timer.waketime = timer_read_time() + CONFIG_CLOCK_FREQ/80;//args[1];
    sched_add_timer(&h->timer);
}
DECL_COMMAND(command_query_hx711,
             "query_hx711 oid=%c clock=%u sample_ticks=%u sample_count=%c"
             " rest_ticks=%u min_value=%u max_value=%u range_check_count=%c");
