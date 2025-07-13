#pragma once

#include <obs-module.h>
#include <plugin-support.h>
#include <nlohmann/json.hpp>
#include <thread>

#include <ixwebsocket/IXNetSystem.h>
#include <ixwebsocket/IXWebSocket.h>
#include <ixwebsocket/IXUserAgent.h>
#include <iostream>

#include <audio-filter.h>

enum MixerType { INVALID, LOCAL, STREAM };

struct Input {
	std::unordered_map<MixerType, bool> muted;
	std::unordered_map<MixerType, int> volume;
};

struct Output {
	bool muted;
	int volume;
};

class WebSocketHandler {
private:
	static inline ix::WebSocket webSocket;
	static inline std::unordered_map<MixerType, Output *> mixers;
	static inline std::unordered_map<std::string, Input *> channels;

	static inline int input_configs_id = 469;
	static inline int output_config_id = 470;

public:
	static void initialize()
	{
		srand((unsigned int)time(NULL));
		input_configs_id = rand() % 420 + 69;
		output_config_id = rand() % 420 + 69;

		obs_log(LOG_INFO, "Random numbers, %d, %d", input_configs_id, output_config_id);

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
				obs_log(LOG_INFO, "WebSocket connection error: %d, %s", msg->errorInfo.http_status,
					msg->errorInfo.reason.c_str());
			}
		});

		webSocket.start();
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

	static Input *getInput(std::string identifier)
	{
		if (!channels[identifier]) {
			channels[identifier] = new Input();
		}

		return channels[identifier];
	}

	static Output *getOutput(MixerType mixer_type)
	{
		if (!mixers[mixer_type]) {
			mixers[mixer_type] = new Output();
		}

		return mixers[mixer_type];
	}

	static void handleInputConfigs(nlohmann::json json)
	{
		obs_log(LOG_INFO, "input configs");

		for (auto &json_input : json["result"]) {
			std::string identifier = json_input["identifier"];
			obs_log(LOG_INFO, "identifier, %s", identifier.c_str());

			Input *input = getInput(identifier);

			input->muted[MixerType::LOCAL] = json_input["localMixer"][0];
			input->volume[MixerType::LOCAL] = json_input["localMixer"][1];

			input->muted[MixerType::STREAM] = json_input["streamMixer"][0];
			input->volume[MixerType::STREAM] = json_input["streamMixer"][1];

			obs_log(LOG_INFO, "input, %d, %d, %d, %d - volumes size: %d", input->muted[MixerType::LOCAL],
				input->volume[MixerType::LOCAL], input->muted[MixerType::STREAM],
				input->volume[MixerType::STREAM], channels.size());
		}
	}

	static void handleOutputConfig(nlohmann::json json)
	{
		obs_log(LOG_INFO, "output config");

		auto result = json["result"];

		Output *localOutput = getOutput(MixerType::LOCAL);
		Output *streamOutput = getOutput(MixerType::STREAM);

		localOutput->muted = result["localMixer"][0];
		localOutput->volume = result["localMixer"][1];

		streamOutput->muted = result["streamMixer"][0];
		streamOutput->volume = result["streamMixer"][1];

		obs_log(LOG_INFO, "outputs, %d, %d, %d, %d", localOutput->muted, localOutput->volume,
			streamOutput->muted, streamOutput->volume);
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

		if (!json.contains("params") || !json.contains("method"))
			return;

		auto method = json["method"];
		auto params = json["params"];

		if (!params.contains("value") || !params.contains("mixerID"))
			return;

		std::string mixerID = params["mixerID"];
		if (mixerID == "com.elgato.mix.microphoneFX")
			return;

		MixerType mixerType = MixerType::INVALID;
		if (mixerID == "com.elgato.mix.local") {
			mixerType = MixerType::LOCAL;
		} else if (mixerID == "com.elgato.mix.stream") {
			mixerType = MixerType::STREAM;
		}

		if (method == "outputVolumeChanged") {
			int volume = params["value"];

			Output *output = getOutput(mixerType);

			output->volume = volume;

			obs_log(LOG_INFO, "Output, Volume %d", mixerID.c_str(), volume);
		} else if (method == "outputMuteChanged") {
			bool muted = params["value"];

			Output *output = getOutput(mixerType);

			output->muted = muted;

			obs_log(LOG_INFO, "Output, %s", mixerID.c_str(), muted ? "Muted" : "Unmuted");
		} else if (method == "inputVolumeChanged") {
			if (!params.contains("identifier"))
				return;

			int volume = params["value"];

			std::string identifier = params["identifier"];

			updateFilterVolume(identifier, mixerType, volume);

			obs_log(LOG_INFO, "%s, %d, Volume: %d", identifier.c_str(), mixerType, volume);
		} else if (method == "inputMuteChanged") {
			if (!params.contains("identifier"))
				return;

			bool muted = params["value"];

			std::string identifier = params["identifier"];

			updateFilterMuted(identifier, mixerType, muted);

			obs_log(LOG_INFO, "%s, %d, %s", identifier.c_str(), mixerType, muted ? "Muted" : "Unmuted");
		}
	}

	static void updateFilterVolume(std::string identifier, MixerType mixer_type, int volume)
	{
		Input *input = getInput(identifier);

		input->volume[mixer_type] = volume;

		obs_log(LOG_INFO, "volumes size: %d", channels.size());
	}

	static void updateFilterMuted(std::string identifier, MixerType mixer_type, bool muted)
	{
		Input *input = getInput(identifier);

		input->muted[mixer_type] = muted;

		obs_log(LOG_INFO, "volumes size: %d", channels.size());
	}

	static int getMixerVolumeForFilter(filter_t *filter)
	{
		MixerType mixer_type = static_cast<MixerType>(filter->mixer_type);
		Output *output = getOutput(mixer_type);

		return output->muted ? 0 : output->volume;
	}

	static int getVolumeForFilter(filter_t *filter)
	{
		Input *input = getInput(filter->channel);
		MixerType mixer_type = static_cast<MixerType>(filter->mixer_type);

		return input->muted[mixer_type] ? 0 : input->volume[mixer_type];
	}
};