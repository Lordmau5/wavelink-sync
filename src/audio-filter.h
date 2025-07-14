#pragma once

#include <obs-module.h>
#include <string>

typedef struct {
	obs_source_t *context;

	size_t channels;

	std::string channel;
	int mixer_type;

	bool follow_channel_mute;
	bool apply_mixer_volume;
	bool follow_mixer_mute;
} filter_t;