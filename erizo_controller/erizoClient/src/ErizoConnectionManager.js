/* global */
import ChromeStableStack from './webrtc-stacks/ChromeStableStack';
import FirefoxStack from './webrtc-stacks/FirefoxStack';
import FcStack from './webrtc-stacks/FcStack';
import Logger from './utils/Logger';
import ErizoMap from './utils/ErizoMap';
import ConnectionHelpers from './utils/ConnectionHelpers';


let ErizoSessionId = 103;

class ErizoConnection {
  constructor(specInput, erizoId = undefined) {
    Logger.debug('Building a new Connection');
    this.stack = {};

    this.erizoId = erizoId;
    this.streamsMap = ErizoMap(); // key:streamId, value: stream

    const spec = specInput;
    ErizoSessionId += 1;
    spec.sessionId = ErizoSessionId;
    this.sessionId = ErizoSessionId;

    // Check which WebRTC Stack is installed.
    this.browser = ConnectionHelpers.getBrowser();
    if (this.browser === 'fake') {
      Logger.warning('Publish/subscribe video/audio streams not supported in erizofc yet');
      this.stack = FcStack(spec);
    } else if (this.browser === 'mozilla') {
      Logger.debug('Firefox Stack');
      this.stack = FirefoxStack(spec);
    } else if (this.browser === 'safari') {
      Logger.debug('Safari using Chrome Stable Stack');
      this.stack = ChromeStableStack(spec);
    } else if (this.browser === 'chrome-stable' || this.browser === 'electron') {
      Logger.debug('Chrome Stable Stack');
      this.stack = ChromeStableStack(spec);
    } else {
      Logger.error('No stack available for this browser');
      throw new Error('WebRTC stack not available');
    }
    if (!this.stack.updateSpec) {
      this.stack.updateSpec = (newSpec, callback = () => {}) => {
        Logger.error('Update Configuration not implemented in this browser');
        callback('unimplemented');
      };
    }
    if (!this.stack.setSignallingCallback) {
      this.stack.setSignallingCallback = () => {
        Logger.error('setSignallingCallback is not implemented in this stack');
      };
    }

    // PeerConnection Events
    if (this.stack.peerConnection) {
      this.peerConnection = this.stack.peerConnection; // For backwards compatibility
      this.stack.peerConnection.onaddstream = (stream) => {
        if (this.onaddstream) {
          this.onaddstream(stream);
        }
      };

      this.stack.peerConnection.onremovestream = (stream) => {
        if (this.onremovestream) {
          this.onremovestream(stream);
        }
      };

      this.stack.peerConnection.oniceconnectionstatechange = (ev) => {
        if (this.oniceconnectionstatechange) {
          this.oniceconnectionstatechange(ev.target.iceConnectionState);
        }
      };
    }
  }

  close() {
    Logger.debug('Closing ErizoConnection');
    this.streamsMap.clear();
    this.stack.close();
  }

  createOffer(isSubscribe) {
    this.stack.createOffer(isSubscribe);
  }

  addStream(stream) {
    Logger.debug(`message: Adding stream to Connection, streamId: ${stream.getID()}`);
    this.streamsMap.add(stream.getID(), stream);
    if (stream.local) {
      this.stack.addStream(stream.stream);
    }
  }

  removeStream(stream) {
    const streamId = stream.getID();
    if (!this.streamsMap.has(streamId)) {
      Logger.warning(`message: Cannot remove stream not in map, streamId: ${streamId}`);
      return;
    }
    this.streamsMap.remove(streamId);
  }

  processSignalingMessage(msg) {
    this.stack.processSignalingMessage(msg);
  }

  sendSignalingMessage(msg) {
    this.stack.sendSignalingMessage(msg);
  }

  enableSimulcast(sdpInput) {
    this.stack.enableSimulcast(sdpInput);
  }

  updateSpec(configInput, callback) {
    this.stack.updateSpec(configInput, callback);
  }
}

class ErizoConnectionManager {
  constructor() {
    this.ErizoConnectionsMap = new Map(); // key: erizoId, value: {connectionId: connection}
  }

  getOrBuildErizoConnection(specInput, erizoId = undefined) {
    Logger.debug(`message: getOrBuildErizoConnection, erizoId: ${erizoId}`);
    let connection = {};

    if (erizoId === undefined) {
      // we have no erizoJS id - p2p
      return new ErizoConnection(specInput);
    }
    connection = new ErizoConnection(specInput, erizoId);
    if (this.ErizoConnectionsMap.has(erizoId)) {
      this.ErizoConnectionsMap.get(erizoId)[connection.sessionId] = connection;
    } else {
      const connectionEntry = {};
      connectionEntry[connection.sessionId] = connection;
      this.ErizoConnectionsMap.set(erizoId, connectionEntry);
    }
    return connection;
  }

  closeConnection(connection) {
    Logger.debug(`Removing connection ${connection.sessionId}
       with erizoId ${connection.erizoId}`);
    connection.close();
    if (this.ErizoConnectionsMap.get(connection.erizoId) !== undefined) {
      delete this.ErizoConnectionsMap.get(connection.erizoId)[connection.sessionId];
    }
  }
}

export default ErizoConnectionManager;
