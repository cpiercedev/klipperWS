# Support for HX711 ADC chip
#
# Copyright (C) 2021  Konstantin Vogel <konstantin.vogel@gmx.net>
#
# This file may be distributed under the terms of the GNU GPLv3 license.

import logging


class MCU_hx711:

    def __init__(self, main, config):
        self._main = main
        self.reactor = main.printer.get_reactor()
        self.mcu = main.mcu
        self.report_time = 0
        self._last_value = 0.
        self._last_time = 0
        self._callback = None
        self._oid = self.mcu.create_oid()
        self.mcu.register_config_callback(self._build_config)
        try:
            query_adc = main.printer.lookup_object('query_adc')
        except:
            query_adc = main.printer.load_object(config, 'query_adc')
        query_adc.register_adc(main.name, self)
    def _build_config(self):
        self.mcu._serial.register_response(self._handle_adc_state,
            "hx711_in_state", self._oid)
        self.mcu.add_config_cmd("config_hx711 oid=%d dout_pin=%s sck_pin=%s "
            " gain=%d sample_interval=%d comm_delay=%d sps=%d" % (self._oid,
            self._main.dout_pin, self._main.sck_pin, self._main.gain,
            self._main.sample_interval, self._main.comm_delay, self._main.sps) )
        self.mcu.add_config_cmd("query_hx711 oid=%d clock=0 sample_ticks=0 "
            " sample_count=0 rest_ticks=0 min_value=0 max_value=0 "
            " range_check_count=0" % ( self._oid) )
    def setup_adc_callback(self, report_time, callback):
        if self._callback is not None:
            logging.exception("Hx711: ADC callback already configured")
        if report_time is not None:
            self.report_time = report_time
        self._callback = callback
    def setup_minmax(self, sample_time, sample_count, minval, maxval,
        range_check_count):
        pass
    def get_last_value(self):
        return self._last_value, self._last_time
    def read_single_value(self):
        # wait until conversion is ready and the timer callback has been called
        self._last_time = self.reactor.pause(self._last_time +
            self.report_time + 0.0001)
        return self._last_value
    def _handle_adc_state(self, params):
        self._last_value = params['value']
        self._last_time = params['#sent_time']
        if self._callback:
            self._callback(self.mcu.estimated_print_time(self._last_time),
                self._last_value)
        logging.info("hx711 value is %d \t%s" % ( self._last_value,
        bin(self._last_value) ) )



class PrinterHx711:

    def __init__(self, config):
        self.printer = config.get_printer()
        self.name = config.get_name().split()[1]
        ppins = self.printer.lookup_object('pins')
        dout_pin_params = ppins.lookup_pin(config.get('dout_pin'))
        sck_pin_params = ppins.lookup_pin(config.get('sck_pin'))
        self.dout_pin = dout_pin_params['pin']
        self.sck_pin = sck_pin_params['pin']
        self.mcu = dout_pin_params['chip']
        self.gain = config.getchoice('gain', {32: 2, 64: 3, 128: 1}, default=64)
        self.sps = config.getchoice('board_freq',{10: 10, 80: 80}, default=10)
        self.sample_interval = config.getfloat('sample_interval', default=1)
        self.comm_delay = config.getint('comm_delay', default=1)
        self.config = config
        ppins.register_chip(self.name, self)

    def setup_pin(self, pin_type, pin_params):
        return MCU_hx711(self, self.config)

def load_config_prefix(config):
    return PrinterHx711(config)
