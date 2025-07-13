#pragma once

#include <obs-module.h>
#include <string>

typedef struct {
	obs_source_t *context;

	size_t channels;

	std::string channel;
	int mixer_type;
} filter_t;