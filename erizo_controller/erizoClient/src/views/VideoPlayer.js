/* global window, document, webkitURL, L*/

import View from './View';
import Bar from './Bar';
import Logger from '../utils/Logger';

/*
 * VideoPlayer represents a Licode video component that shows either a local or a remote video.
 * Ex.: var player = VideoPlayer({id: id, stream: stream, elementID: elementID});
 * A VideoPlayer is also a View component.
 */
const VideoPlayer = (spec) => {
  const that = View({});

  // Variables

  // VideoPlayer ID
  that.id = spec.id;

  // Stream that the VideoPlayer will play
  that.stream = spec.stream.stream;

  // DOM element in which the VideoPlayer will be appended
  that.elementID = spec.elementID;

  // Private functions
  const onmouseover = () => {
    that.bar.display();
  };

  const onmouseout = () => {
    that.bar.hide();
  };

  const applyRatio = (ratio, width, height, reverse) => {
    const condition = !reverse ? width * (1 / ratio) < height : width * (1 / ratio) > height;
    if (condition) {
      that.video.style.width = `${width}px`;
      that.video.style.height = `${(1 / ratio) * width}px`;

      that.video.style.top = `${-((((1 / ratio) * width) / 2) - (height / 2))}px`;
      that.video.style.left = '0px';
    } else {
      that.video.style.height = `${height}px`;
      that.video.style.width = `${ratio * height}px`;

      that.video.style.left = `${-(((ratio * height) / 2) - (width / 2))}px`;
      that.video.style.top = '0px';
    }
  };

  // Public functions

  // It will stop the VideoPlayer and remove it from the HTML
  that.destroy = () => {
    that.video.pause();
    delete that.Loggerr;
    that.parentNode.removeChild(that.div);
  };

  that.Logger = () => {
    const width = that.container.offsetWidth;
    const height = that.container.offsetHeight;

    if (spec.stream.screen || spec.options.crop === false) {
      applyRatio(16 / 9, width, height, false);
    } else if (width !== that.containerWidth || height !== that.containerHeight) {
      applyRatio(4 / 3, width, height, true);
    }

    that.containerWidth = width;
    that.containerHeight = height;
  };

  /* window.location.href.replace(/[?&]+([^=&]+)=([^&]*)/gi, function (m, key, value) {
      document.getElementById(key).value = unescape(value);
  });*/

  Logger.debug(`Creating URL from stream ${that.stream}`);
  const myURL = window.URL || webkitURL;
  that.streamUrl = myURL.createObjectURL(that.stream);

  // Container
  that.div = document.createElement('div');
  that.div.setAttribute('id', `player_${that.id}`);
  that.div.setAttribute('class', 'licode_player');
  that.div.setAttribute('style', 'width: 100%; height: 100%; position: relative; ' +
                                   'background-color: black; overflow: hidden;');

  // Loader icon
  if (spec.options.loader !== false) {
    that.loader = document.createElement('img');
    that.loader.setAttribute('style', 'width: 16px; height: 16px; position: absolute; ' +
                                        'top: 50%; left: 50%; margin-top: -8px; margin-left: -8px');
    that.loader.setAttribute('id', `back_${that.id}`);
    that.loader.setAttribute('class', 'licode_loader');
    that.loader.setAttribute('src', `${that.url}/assets/loader.gif`);
  }

  // Video tag
  that.video = document.createElement('video');
  that.video.setAttribute('id', `stream${that.id}`);
  that.video.setAttribute('class', 'licode_stream');
  that.video.setAttribute('style', 'width: 100%; height: 100%; position: absolute');
  that.video.setAttribute('autoplay', 'autoplay');

  if (spec.stream.local) { that.video.volume = 0; }

  if (that.elementID !== undefined) {
    // Check for a passed DOM node.
    if (typeof that.elementID === 'object' &&
          typeof that.elementID.appendChild === 'function') {
      that.container = that.elementID;
    } else {
      that.container = document.getElementById(that.elementID);
    }
  } else {
    that.container = document.body;
  }
  that.container.appendChild(that.div);

  that.parentNode = that.div.parentNode;

  if (that.loader) {
    that.div.appendChild(that.loader);
  }
  that.div.appendChild(that.video);

  that.containerWidth = 0;
  that.containerHeight = 0;

  if (spec.options.resizer !== false) {
    that.resizer = L.ResizeSensor(that.container, that.resize);

    that.Logger();
  }

  // Bottom Bar
  if (spec.options.bar !== false) {
    that.bar = Bar({ elementID: `player_${that.id}`,
      id: that.id,
      stream: spec.stream,
      media: that.video,
      options: spec.options });

    that.div.onmouseover = onmouseover;
    that.div.onmouseout = onmouseout;
  } else {
    // Expose a consistent object to manipulate the media.
    that.media = that.video;
  }

  that.video.src = that.streamUrl;

  return that;
};

export default VideoPlayer;
