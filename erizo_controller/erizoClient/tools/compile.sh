#!/usr/bin/env bash

set -e

google-closure-compiler-js ../lib/socket.io.js ../src/Events.js ../src/webrtc-stacks/FcStack.js ../src/webrtc-stacks/BowserStack.js ../src/webrtc-stacks/FirefoxStack.js ../src/webrtc-stacks/ChromeStableStack.js ../src/webrtc-stacks/ChromeCanaryStack.js ../src/Connection.js ../src/Stream.js ../src/Room.js ../src/utils/L.Logger.js ../src/utils/L.Base64.js ../src/utils/L.Resizer.js ../src/views/View.js ../src/views/VideoPlayer.js ../src/views/AudioPlayer.js ../src/views/Bar.js ../src/views/Speaker.js > ../dist/erizo.js