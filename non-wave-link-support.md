# Non Wave Link Support
In theory this plugin isn't requiring Wave Link and its websocket server.

It would work with any websocket server that runs on the local port :1824 and adheres
to the same messages as Wave Link, or rather the ones implemented by this plugin.

So let's document that!

## Basic information
The websocket server needs to accept and send out JSON data.

It needs to run on the local port `:1824` (so `localhost:1824` or `127.0.0.1:1824`)

## Receiving methods
These are methods that should be received by the websocket server.

### getInputConfigs
*Gets the configuration for the separate input devices (in Wave Link this would be the Game, Music, Browser, ... inputs)*

Request:
```json
{
	"method": "getInputConfigs",
	"id": <response_id>
}
```

Response:
```json
{
	"id": <response_id>,
	"result": [
		{
			"identifier": "audio_device_id_0",
			"name": "My Cool Audio Device",
			"localMixer": [
				false,
				50
			],
			"streamMixer": [
				true,
				70
			]
		},
		{
			// ...
		}
	]
}
```

`id` has to be what the websocket client requested in the prior message.

`result` value has to be an array of objects corresponding to the respective input.

`identifier` has to be a unique string for the input. This should also not change throughout the session.

`name` can realistically be anything and can also be updated during the session (more on that later)

The first entry in the `localMixer` and `streamMixer` array is the muted state as a boolean.

The second entry is the volume between 0 and 100.

### getOutputConfig
*Gets the configuration for the separate output devices (in Wave Link this would be the Monitor Mix and Stream Mix outputs)*

Request:
```json
{
	"method": "getOutputConfig",
	"id": <response_id>
}
```

Response:
```json
{
	"id": <response_id>,
	"result": {
		"localMixer": [
			false,
			50
		],
		"streamMixer": [
			true,
			70
		]
	}
}
```

`id` has to be what the websocket client requested in the prior message.

`result` value has to be an object that has 2 arrays, `localMixer` and `streamMixer`. This is the information for the actual main output mixes (Monitor Mix and Stream Mix in Wave Link respectively)

The first entry in the `localMixer` and `streamMixer` array is the muted state as a boolean.

The second entry is the volume between 0 and 100.

## Update methods
These are methods that should be sent by the websocket server to all clients when an update is necessary (such as an input changing volume)

### inputNameChanged
*Is sent when an input has its name changed.*

Example:
```json
{
	"method": "inputNameChanged",
	"params": {
		"identifier": "audio_device_id_0",
		"value": "My Cool Audio Device (Renamed)"
	}
}
```

### inputMuteChanged
*Is sent when an input has its mute state changed.*

Example:
```json
{
	"method": "inputMuteChanged",
	"params": {
		"identifier": "audio_device_id_0",
		"value": true
	}
}
```

### inputVolumeChanged
*Is sent when an input has its volume changed.*

Example:
```json
{
	"method": "inputVolumeChanged",
	"params": {
		"identifier": "audio_device_id_0",
		"value": 75
	}
}
```

### outputMuteChanged
*Is sent when an output has its mute state changed.*

Example:
```json
{
	"method": "outputMuteChanged",
	"params": {
		"mixerID": "com.elgato.mix.local",
		"value": true
	}
}
```

### outputVolumeChanged
*Is sent when an output has its volume changed.*

Example:
```json
{
	"method": "outputVolumeChanged",
	"params": {
		"mixerID": "com.elgato.mix.stream",
		"value": 75
	}
}
```