
#include <osmocom/core/logging.h>
#include <osmocom/core/application.h>
#include <osmocom/core/utils.h>

#include "Logging.h"

static struct log_info_cat trx_log_info_cat[] = {
	[DTRX] = {
		.name = "DTRX",
		.description = "Transceiver L1",
		.color = "\033[1;35m",
		.enabled = 1, .loglevel = LOGL_INFO,
	},
	[DDEV] = {
		.name = "DDEV",
		.description = "Device Driver Interface",
		.color = "\033[1;36m",
		.enabled = 1, .loglevel = LOGL_INFO,
	},
	[DDSP] = {
		.name = "DDSP",
		.description = "Transceiver L1 DSP",
		.color = "\033[1;31m",
		.enabled = 1, .loglevel = LOGL_INFO,
	},
};

const struct log_info trx_log_info = {
	.cat = trx_log_info_cat,
	.num_cat = ARRAY_SIZE(trx_log_info_cat),
};

int trx_log_init(const char *category_mask)
{
	osmo_init_logging(&trx_log_info);

	if (category_mask)
		log_parse_category_mask(osmo_stderr_target, category_mask);

	return 0;
}
