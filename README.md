# Wave Link Sync

This plugin adds a new audio filter named **"Wave Link Sync"** that synchronizes with Wave Link.

The optimal use-case is to add, say, the Voice Chat channel / device from Wave Link as a separate
Audio Output Capture inside OBS to be able to control the Stream Mix with it as a sidechain / ducking source.

It will apply volume adjustments on the source based on the selected Wave Link channel and mixer volume.

This also means the specific channel shouldn't be included in the Stream Mix inside Wave Link,
otherwise it will be audible twice.

*Video explanation will follow*

It currently is only available for Windows since I don't have access to macOS.

## Quick Start / How to build

Please refer to the OBS plugin [Quick Start Guide](https://github.com/obsproject/obs-plugintemplate/wiki/Quick-Start-Guide).
