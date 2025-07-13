#pragma once

#include <audio-filter.h>

#include <obs-module.h>
#include <plugin-support.h>

#include <websocket.hpp>

typedef struct {
	float percent;
	float db;
} volume_map_t;

static const volume_map_t volume_curve[] = {{0, -100.0f}, {1, -39.3f},  {5, -38.0f},  {15, -34.3f}, {30, -28.2f},
					    {45, -22.5f}, {60, -16.3f}, {75, -10.2f}, {90, -4.2f},  {100, 0.0f}};
#define VOLUME_CURVE_SIZE (sizeof(volume_curve) / sizeof(volume_map_t))

const char *filter_get_name(void *)
{
	return obs_module_text("WaveLinkSync.FilterName");
}

obs_properties_t *filter_get_properties(void *)
{
	obs_log(LOG_DEBUG, "+filter_get_properties(...)");
	obs_properties_t *props = obs_properties_create();

	obs_property_t *channel_list = obs_properties_add_list(props, "channel",
							       obs_module_text("WaveLinkSync.ChannelSelection"),
							       OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_STRING);
	obs_property_list_add_string(channel_list, "Game", "Wave Link Game");
	obs_property_list_add_string(channel_list, "Music", "Wave Link Music");
	obs_property_list_add_string(channel_list, "Browser", "Wave Link Browser");
	obs_property_list_add_string(channel_list, "Voice Chat", "Wave Link Voice Chat");
	obs_property_list_add_string(channel_list, "System", "Wave Link System");
	obs_property_list_add_string(channel_list, "SFX", "Wave Link SFX");
	obs_property_list_add_string(channel_list, "Aux 1", "Wave Link Aux 1");
	obs_property_list_add_string(channel_list, "Aux 2", "Wave Link Aux 2");

	obs_property_t *mixer_list = obs_properties_add_list(props, "mixer_type",
							     obs_module_text("WaveLinkSync.MixerSelection"),
							     OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_INT);
	obs_property_list_add_int(mixer_list, "Monitor Mix", 1);
	obs_property_list_add_int(mixer_list, "Stream Mix", 2);

	obs_log(LOG_DEBUG, "-filter_get_properties(...)");
	return props;
}

void filter_get_defaults(obs_data_t *defaults)
{
	obs_log(LOG_DEBUG, "+filter_get_defaults(...)");

	obs_data_set_default_string(defaults, "channel", "Wave Link Music");
	obs_data_set_default_int(defaults, "mixer_type", 1);

	obs_log(LOG_DEBUG, "-filter_get_defaults(...)");
}

void filter_update(void *data, obs_data_t *settings)
{
	obs_log(LOG_DEBUG, "+filter_update");

	auto filter = (filter_t *)data;
	filter->channels = audio_output_get_channels(obs_get_audio());

	auto channel = obs_data_get_string(settings, "channel");
	auto mixer_type = (int)obs_data_get_int(settings, "mixer_type");

	filter->channel = std::string(channel);
	filter->mixer_type = mixer_type;

	obs_log(LOG_DEBUG, "-filter_update");
}

void *filter_create(obs_data_t *settings, obs_source_t *obs_source)
{
	obs_log(LOG_DEBUG, "+filter_create");

	auto filter = (filter_t *)bzalloc(sizeof(filter_t));
	filter->context = obs_source;
	filter_update(filter, settings);

	obs_log(LOG_DEBUG, "-filter_create(...)");

	return filter;
}

void filter_destroy(void *data)
{
	obs_log(LOG_DEBUG, "+filter_destroy");

	auto filter = (filter_t *)data;
	bfree(filter);

	obs_log(LOG_DEBUG, "-filter_destroy");
}

float interpolate_volume_db(float percent)
{
	if (percent <= volume_curve[0].percent)
		return volume_curve[0].db;
	if (percent >= volume_curve[VOLUME_CURVE_SIZE - 1].percent)
		return volume_curve[VOLUME_CURVE_SIZE - 1].db;

	for (size_t i = 0; i < VOLUME_CURVE_SIZE - 1; ++i) {
		const volume_map_t *a = &volume_curve[i];
		const volume_map_t *b = &volume_curve[i + 1];

		if (percent >= a->percent && percent <= b->percent) {
			float t = (percent - a->percent) / (b->percent - a->percent);
			return a->db + t * (b->db - a->db); // Linear interpolation
		}
	}

	return -100.0f; // Fallback
}

float getCombinedDb(filter_t *filter)
{
	float volume = (float)WebSocketHandler::getVolumeForFilter(filter);
	float mixer_volume = (float)WebSocketHandler::getMixerVolumeForFilter(filter);

	float volume_db = interpolate_volume_db(volume);
	float mixer_volume_db = interpolate_volume_db(mixer_volume);

	return obs_db_to_mul(volume_db) * obs_db_to_mul(mixer_volume_db);
}

obs_audio_data *filter_handle_audio(void *data, obs_audio_data *audio)
{
	auto filter = (filter_t *)data;
	const size_t channels = filter->channels;
	float **adata = (float **)audio->data;
	float gain = getCombinedDb(filter);

	for (size_t c = 0; c < channels; c++) {
		if (audio->data[c]) {
			for (size_t i = 0; i < audio->frames; i++) {
				adata[c][i] *= gain;
			}
		}
	}

	return audio;
}

obs_source_info create_audio_filter_info()
{
	obs_source_info audio_filter_info = {};
	audio_filter_info.id = "wavelink_sync_filter";
	audio_filter_info.type = OBS_SOURCE_TYPE_FILTER;
	audio_filter_info.output_flags = OBS_SOURCE_AUDIO;

	audio_filter_info.get_name = filter_get_name;
	audio_filter_info.get_properties = filter_get_properties;
	audio_filter_info.get_defaults = filter_get_defaults;

	audio_filter_info.create = filter_create;
	audio_filter_info.update = filter_update;
	audio_filter_info.destroy = filter_destroy;

	audio_filter_info.filter_audio = filter_handle_audio;

	return audio_filter_info;
}