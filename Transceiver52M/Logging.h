#ifndef _LOGGING_H_
#define _LOGGING_H_

#include <osmocom/core/logging.h>

enum {
	DTRX,
	DDEV,
	DDSP,
};

extern const struct log_info trx_log_info;

int trx_log_init(const char *category_mask);

#endif /* _LOGGING_H_ */
