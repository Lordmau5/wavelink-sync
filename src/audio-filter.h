#pragma once

#include <obs-module.h>
#include <string>

typedef struct {
	obs_source_t *context;

	size_t channels;

	std::string channel;
	int volume_mixer_type;

	bool follow_channel_mute;
	int channel_mixer_mute_type;

	bool apply_mixer_volume;
	int apply_mixer_volume_type;

	bool follow_mixer_mute;
	int follow_mixer_mute_type;
} filter_t;