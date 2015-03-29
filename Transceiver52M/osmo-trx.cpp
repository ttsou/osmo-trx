/*
 * Copyright (C) 2013 Thomas Tsou <tom@tsou.cc>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "Transceiver.h"
#include "radioDevice.h"
#include "osmo-trx-options.h"

#include <time.h>
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>

#include <GSMCommon.h>
#include <Logger.h>

struct trx_config {
	std::string log_level;
	std::string addr;
	std::string dev_args;
	unsigned port;
	unsigned sps;
	unsigned chans;
	bool extref;
	bool filler;
	bool diversity;
	double offset;
};

volatile bool gshutdown = false; // FIXME: OpenBTS leftovers

/* Setup configuration values
 *     Don't query the existence of the Log.Level because it's a
 *     mandatory value. That is, if it doesn't exist, the configuration
 *     table will crash or will have already crashed. Everything else we
 *     can survive without and use default values if the database entries
 *     are empty.
 */
bool trx_setup_config(struct trx_config *config, gengetopt_args_info *args_info)
{
	config->log_level = args_info->logging_arg;
	config->port = args_info->port_arg;
	config->addr = args_info->address_arg;
	config->extref = args_info->extref_given;
	config->diversity = args_info->diversity_given;
	config->filler = args_info->filler_given;
	config->sps = args_info->samples_arg;
	config->chans = args_info->channels_arg;
	config->offset = args_info->offset_arg;
	if (args_info->uhd_given)
	    config->dev_args = args_info->uhd_arg;

	/* Diversity only supported on 2 channels */
	if (config->diversity)
		config->chans = 2;

	std::ostringstream ost("");
	ost << "Config Settings" << std::endl;
	ost << "   Log Level............... " << config->log_level << std::endl;
	ost << "   Device args............. " << config->dev_args << std::endl;
	ost << "   TRX Base Port........... " << config->port << std::endl;
	ost << "   TRX Address............. " << config->addr << std::endl;
	ost << "   Channels................ " << config->chans << std::endl;
	ost << "   Samples-per-Symbol...... " << config->sps << std::endl;
	ost << "   External Reference...... " << (config->extref ? "Enabled" : "Disabled") << std::endl;
	ost << "   C0 Filler Table......... " << (config->filler ? "Enabled" : "Disabled") << std::endl;
	ost << "   Diversity............... " << (config->diversity ? "Enabled" : "Disabled") << std::endl;
	ost << "   Tuning offset........... " << config->offset << std::endl;
	std::cout << ost << std::endl;

	return true;
}

/* Create radio interface
 *     The interface consists of sample rate changes, frequency shifts,
 *     channel multiplexing, and other conversions. The transceiver core
 *     accepts input vectors sampled at multiples of the GSM symbol rate.
 *     The radio interface connects the main transceiver with the device
 *     object, which may be operating some other rate.
 */
RadioInterface *makeRadioInterface(struct trx_config *config,
                                   RadioDevice *usrp, int type)
{
	RadioInterface *radio = NULL;

	switch (type) {
	case RadioDevice::NORMAL:
		radio = new RadioInterface(usrp, config->sps, config->chans);
		break;
	case RadioDevice::RESAMP_64M:
	case RadioDevice::RESAMP_100M:
		radio = new RadioInterfaceResamp(usrp,
						 config->sps, config->chans);
		break;
	case RadioDevice::DIVERSITY:
		radio = new RadioInterfaceDiversity(usrp,
						    config->sps, config->chans);
		break;
	default:
		LOG(ALERT) << "Unsupported radio interface configuration";
		return NULL;
	}

	if (!radio->init(type)) {
		LOG(ALERT) << "Failed to initialize radio interface";
		return NULL;
	}

	return radio;
}

/* Create transceiver core
 *     The multi-threaded modem core operates at multiples of the GSM rate of
 *     270.8333 ksps and consists of GSM specific modulation, demodulation,
 *     and decoding schemes. Also included are the socket interfaces for
 *     connecting to the upper layer stack.
 */
Transceiver *makeTransceiver(struct trx_config *config, RadioInterface *radio)
{
	Transceiver *trx;
	VectorFIFO *fifo;

	trx = new Transceiver(config->port, config->addr.c_str(), config->sps,
			      config->chans, GSM::Time(3,0), radio);
	if (!trx->init(config->filler)) {
		LOG(ALERT) << "Failed to initialize transceiver";
		delete trx;
		return NULL;
	}

	for (size_t i = 0; i < config->chans; i++) {
		fifo = radio->receiveFIFO(i);
		if (fifo && trx->receiveFIFO(fifo, i))
			continue;

		LOG(ALERT) << "Could not attach FIFO to channel " << i;
		delete trx;
		return NULL;
	}

	return trx;
}

static void sig_handler(int signo)
{
	fprintf(stdout, "Received shutdown signal");
	gshutdown = true;
}

static void setup_signal_handlers()
{
	if (signal(SIGINT, sig_handler) == SIG_ERR) {
		fprintf(stderr, "Failed to install SIGINT signal handler\n");
		exit(EXIT_FAILURE);
	}
	if (signal(SIGTERM, sig_handler) == SIG_ERR) {
		fprintf(stderr, "Couldn't install SIGTERM signal handler\n");
		exit( EXIT_FAILURE);
	}
}


int main(int argc, char *argv[])
{
	gengetopt_args_info args_info;
	if (cmdline_parser (argc, argv, &args_info) != 0)
	    exit(1) ;

	int type, chans;
	RadioDevice *usrp;
	RadioInterface *radio = NULL;
	Transceiver *trx = NULL;
	struct trx_config config;

	setup_signal_handlers();
	trx_setup_config(&config, &args_info);
	gLogInit("transceiver", config.log_level.c_str(), LOG_LOCAL7);

	srandom(time(NULL));

	/* Create the low level device object */
	usrp = RadioDevice::make(config.sps, config.chans,
				 config.diversity, config.offset);
	type = usrp->open(config.dev_args, config.extref);
	if (type < 0) {
		LOG(ALERT) << "Failed to create radio device" << std::endl;
		goto shutdown;
	}

	/* Setup the appropriate device interface */
	radio = makeRadioInterface(&config, usrp, type);
	if (!radio)
		goto shutdown;

	/* Create the transceiver core */
	trx = makeTransceiver(&config, radio);
	if (!trx)
		goto shutdown;

	chans = trx->numChans();
	std::cout << "-- Transceiver active with "
		  << chans << " channel(s)" << std::endl;

	while (!gshutdown)
		sleep(1);

shutdown:
	std::cout << "Shutting down transceiver..." << std::endl;

	delete trx;
	delete radio;
	delete usrp;

	return 0;
}
