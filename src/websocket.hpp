#pragma once

#include <obs-module.h>
#include <plugin-support.h>
#include <nlohmann/json.hpp>

#include <ixwebsocket/IXNetSystem.h>
#include <ixwebsocket/IXWebSocket.h>
#include <ixwebsocket/IXUserAgent.h>
#include <iostream>

#include <audio-filter.h>

enum MixerType { INVALID, LOCAL, STREAM };

struct Mixer {
	bool muted;
	int volume;
};

struct Channel {
	std::string identifier;
	std::string name;

	std::unordered_map<MixerType, bool> muted;
	std::unordered_map<MixerType, int> volume;
};

class WebSocketHandler {
private:
	static inline ix::WebSocket webSocket;
	static inline std::unordered_map<MixerType, Mixer *> mixers;
	static inline std::unordered_map<std::string, Channel *> channels;

	static inline int input_configs_id = 469;
	static inline int output_config_id = 470;

public:
	static void initialize()
	{
		srand((unsigned int)time(NULL));
		input_configs_id = rand() % 420 + 69;
		output_config_id = rand() % 420 + 69;

		ix::initNetSystem();

		std::string url("ws://localhost:1824");
		webSocket.setUrl(url);

		obs_log(LOG_INFO, "Attempting to connect to WebSocket...");

		webSocket.setOnMessageCallback([](const ix::WebSocketMessagePtr &msg) {
			if (msg->type == ix::WebSocketMessageType::Message) {
				handleWebsocketMessage(msg->str);
			} else if (msg->type == ix::WebSocketMessageType::Open) {
				obs_log(LOG_INFO, "WebSocket connection established.");

				sendGetInputConfigsMessage();
				sendGetOutputConfigMessage();
			} else if (msg->type == ix::WebSocketMessageType::Error) {
				// Server probably isn't up, fail silently
				if (msg->errorInfo.http_status == 0)
					return;

				obs_log(LOG_ERROR, "WebSocket connection error: %d, %s", msg->errorInfo.http_status,
					msg->errorInfo.reason.c_str());
			}
		});

		webSocket.start();
	}

	static void refreshInputsAndOutputs()
	{
		obs_log(LOG_INFO, "Refreshing inputs and outputs");

		sendGetInputConfigsMessage();
		sendGetOutputConfigMessage();
	}

	static void sendGetInputConfigsMessage()
	{
		auto json = nlohmann::json();
		json["jsonrpc"] = "2.0";
		json["method"] = "getInputConfigs";
		json["id"] = input_configs_id;

		webSocket.send(json.dump());
	}

	static void sendGetOutputConfigMessage()
	{
		auto json = nlohmann::json();
		json["jsonrpc"] = "2.0";
		json["method"] = "getOutputConfig";
		json["id"] = output_config_id;

		webSocket.send(json.dump());
	}

	static Channel *getChannel(std::string identifier)
	{
		if (!channels.count(identifier))
			return nullptr;

		return channels[identifier];
	}

	static Mixer *getOutput(MixerType mixer_type)
	{
		if (!mixers.count(mixer_type)) {
			mixers[mixer_type] = new Mixer();
		}

		return mixers[mixer_type];
	}

	static std::vector<Channel *> getChannels()
	{
		std::vector<Channel *> channel_values;

		for (auto it = channels.begin(); it != channels.end(); ++it) {
			channel_values.push_back(it->second);
		}

		return channel_values;
	}

	static void handleInputConfigs(nlohmann::json json)
	{
		obs_log(LOG_DEBUG, "input configs");

		if (!json.contains("result"))
			return;

		channels.clear();

		for (auto &json_input : json["result"]) {
			if (!json_input.contains("identifier") || !json_input.contains("name") ||
			    !json_input.contains("localMixer") || !json_input.contains("streamMixer"))
				continue;

			std::string identifier = json_input["identifier"];

			auto channel = channels[identifier] = new Channel{identifier = identifier};
			channel->name = json_input["name"];

			channel->muted[MixerType::LOCAL] = json_input["localMixer"][0];
			channel->volume[MixerType::LOCAL] = json_input["localMixer"][1];

			channel->muted[MixerType::STREAM] = json_input["streamMixer"][0];
			channel->volume[MixerType::STREAM] = json_input["streamMixer"][1];

			obs_log(LOG_DEBUG, "input, %s, %s, %d, %d, %d, %d - volumes size: %d",
				channel->identifier.c_str(), channel->name.c_str(), channel->muted[MixerType::LOCAL],
				channel->volume[MixerType::LOCAL], channel->muted[MixerType::STREAM],
				channel->volume[MixerType::STREAM], channels.size());
		}
	}

	static void handleOutputConfig(nlohmann::json json)
	{
		obs_log(LOG_DEBUG, "output config");

		if (!json.contains("result"))
			return;

		auto result = json["result"];

		if (!result.contains("localMixer") || !result.contains("streamMixer"))
			return;

		Mixer *localOutput = getOutput(MixerType::LOCAL);
		Mixer *streamOutput = getOutput(MixerType::STREAM);

		localOutput->muted = result["localMixer"][0];
		localOutput->volume = result["localMixer"][1];

		streamOutput->muted = result["streamMixer"][0];
		streamOutput->volume = result["streamMixer"][1];

		obs_log(LOG_DEBUG, "outputs, %d, %d, %d, %d", localOutput->muted, localOutput->volume,
			streamOutput->muted, streamOutput->volume);
	}

	static MixerType getMixerFromParams(nlohmann::json params)
	{
		MixerType mixerType = MixerType::INVALID;

		if (!params.contains("mixerID"))
			return mixerType;

		std::string mixerID = params["mixerID"];
		if (mixerID == "com.elgato.mix.microphoneFX")
			return mixerType;

		if (mixerID == "com.elgato.mix.local") {
			mixerType = MixerType::LOCAL;
		} else if (mixerID == "com.elgato.mix.stream") {
			mixerType = MixerType::STREAM;
		}

		return mixerType;
	}

	static void handleOutputVolumeChanged(nlohmann::json params)
	{
		obs_log(LOG_DEBUG, "- outputVolumeChanged");

		if (!params.contains("value"))
			return;

		MixerType mixerType = getMixerFromParams(params);
		if (mixerType == MixerType::INVALID)
			return;

		int volume = params["value"];

		Mixer *output = getOutput(mixerType);

		output->volume = volume;

		obs_log(LOG_DEBUG, "Output %d, Volume %d", mixerType, volume);
	}

	static void handleOutputMuteChanged(nlohmann::json params)
	{
		obs_log(LOG_DEBUG, "- outputMuteChanged");

		if (!params.contains("value"))
			return;

		bool muted = params["value"];

		MixerType mixerType = getMixerFromParams(params);
		if (mixerType == MixerType::INVALID)
			return;

		Mixer *output = getOutput(mixerType);

		output->muted = muted;

		obs_log(LOG_DEBUG, "Output %d, %s", mixerType, muted ? "Muted" : "Unmuted");
	}

	static void handleInputVolumeChanged(nlohmann::json params)
	{
		obs_log(LOG_DEBUG, "- inputVolumeChanged");

		if (!params.contains("identifier") || !params.contains("value"))
			return;

		std::string identifier = params["identifier"];

		int volume = params["value"];

		MixerType mixerType = getMixerFromParams(params);
		if (mixerType == MixerType::INVALID)
			return;

		updateFilterVolume(identifier, mixerType, volume);

		obs_log(LOG_DEBUG, "%s, %d, Volume: %d", identifier.c_str(), mixerType, volume);
	}

	static void handleInputMuteChanged(nlohmann::json params)
	{
		obs_log(LOG_DEBUG, "- inputMuteChanged");

		if (!params.contains("identifier") || !params.contains("value"))
			return;

		std::string identifier = params["identifier"];

		bool muted = params["value"];

		MixerType mixerType = getMixerFromParams(params);
		if (mixerType == MixerType::INVALID)
			return;

		updateFilterMuted(identifier, mixerType, muted);

		obs_log(LOG_DEBUG, "%s, %d, %s", identifier.c_str(), mixerType, muted ? "Muted" : "Unmuted");
	}

	static void handleInputNameChanged(nlohmann::json params)
	{
		obs_log(LOG_DEBUG, "- inputNameChanged");

		if (!params.contains("identifier") || !params.contains("value"))
			return;

		std::string identifier = params["identifier"];

		std::string name = params["value"];

		Channel *channel = getChannel(identifier);
		if (!channel)
			return;

		channel->name = name;

		obs_log(LOG_DEBUG, "%s, %s", identifier.c_str(), name.c_str());
	}

	static void handleWebsocketMessage(std::string text)
	{
		auto json = nlohmann::json::parse(text);

		if (json.contains("id") && json.contains("result")) {
			if (json["id"] == input_configs_id) {
				handleInputConfigs(json);
			} else if (json["id"] == output_config_id) {
				handleOutputConfig(json);
			}
			return;
		}

		if (!json.contains("method"))
			return;

		auto method = json["method"];
		if (method == "inputsChanged") {
			sendGetInputConfigsMessage();
		}

		if (!json.contains("params"))
			return;

		auto params = json["params"];

		if (method == "outputVolumeChanged") {
			handleOutputVolumeChanged(params);
		} else if (method == "outputMuteChanged") {
			handleOutputMuteChanged(params);
		} else if (method == "inputVolumeChanged") {
			handleInputVolumeChanged(params);
		} else if (method == "inputMuteChanged") {
			handleInputMuteChanged(params);
		} else if (method == "inputNameChanged") {
			handleInputNameChanged(params);
		}
	}

	static void updateFilterVolume(std::string identifier, MixerType mixer_type, int volume)
	{
		Channel *channel = getChannel(identifier);
		if (!channel)
			return;

		channel->volume[mixer_type] = volume;
	}

	static void updateFilterMuted(std::string identifier, MixerType mixer_type, bool muted)
	{
		Channel *channel = getChannel(identifier);
		if (!channel)
			return;

		channel->muted[mixer_type] = muted;
	}

	static int getMixerVolumeForFilter(filter_t *filter)
	{
		MixerType mixer_type = static_cast<MixerType>(filter->mixer_type);
		Mixer *output = getOutput(mixer_type);

		if (!filter->apply_mixer_volume) {
			return 100;
		}

		if (filter->follow_mixer_mute && output->muted) {
			return 0;
		}

		return output->volume;
	}

	static int getChannelVolumeForFilter(filter_t *filter)
	{
		if (filter->channel == "None")
			return 100;

		Channel *channel = getChannel(filter->channel);
		if (!channel)
			return 100;

		MixerType mixer_type = static_cast<MixerType>(filter->mixer_type);

		if (filter->follow_channel_mute && channel->muted[mixer_type]) {
			return 0;
		}

		return channel->volume[mixer_type];
	}
};