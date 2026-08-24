/* SPDX-License-Identifier: BSD-3-Clause */
/*
 * otlp-c — umbrella public header. Include this to get everything
 * the typical caller needs. Power users can include the specific
 * sub-headers (span.h, exporter.h, etc.) individually.
 *
 * Public API stability: stable within a major version. The 0.x
 * line may break between minor versions; document changes in
 * CHANGELOG.
 */
#ifndef OTLP_C_H
#define OTLP_C_H

#include "version.h"
#include "status.h"
#include "allocator.h"
#include "value.h"
#include "span.h"
#include "tracer.h"
#include "sampler.h"
#include "exporter.h"
#include "metric.h"
#include "log.h"
#include "w3c.h"
#include "context.h"
#include "slab.h"
#include "visibility.h"

/* otlp_version() is declared in version.h (included above). */

#endif
