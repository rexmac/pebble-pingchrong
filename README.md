# pebble-pingchrong

**NOTE:** Requires Pebble Firmware v2.0 or greater.

_The Pebble Firmware v2.0  is still in BETA and has not been officialy released. Use AT YOUR OWN RISK. Please [see this post](http://www.reddit.com/r/pebble/comments/1ttwv2/should_i_update_my_pebble_to_20/) for more information._

Watchface app for the Pebble Smartwatch that displays a game of ping-pong
between two AI opponents. Each opponent's score is displayed at the top of the
screen. Although they play 24/7, the AI players are quite good. So good, in
fact, that the player on the right only misses 24 times a day. The player on
the left is not quite as good and misses the ball 60 times an hour. As a result,
the current score of their game is also the current time of day! Coincidence?

### Configuration

The app is configurable via the Pebble smartphone app. Options include:
  - 12-hour time mode - Displays time using 12H format rather than 24H format.
  - Inverted colors - black text on white screen rather than white text on black screen

### Screenshots

Example watchfaces in various states:

[![Screenshot showing normal, "first run" state (time is 21:06)](https://s3.amazonaws.com/pebble.rexmac.com/pingchrong/screenshot1.png)](https://s3.amazonaws.com/pebble.rexmac.com/pingchrong/screenshot1.png)
[![Screenshot showing inverted colors (time is 17:38)](https://s3.amazonaws.com/pebble.rexmac.com/pingchrong/screenshot1.png)](https://s3.amazonaws.com/pebble.rexmac.com/pingchrong/screenshot2.png)

### [Download](http://github.com/rexmac/pebble-pingchrong/releases)

## Install

To install this watchface onto your Pebble device using your phone's browser, go to the [downloads page](http://github.com/rexmac/pebble-pingchrong/releases) and click the green `pebble-pingchrong.pbw` button for the latest release. Your phone should download and automatically install the file into your phone's Pebble app (or it may prompt you for which app to use to open the file, in which case you should select the Pebble app). You can then use the Pebble app on your phone to install the watchface to your Pebble watch.

## Build

To build this watchface from source. Follow these steps:

1. Clone this repository in an appropriate directory. For example:

    `$ git clone https://github.com/rexmac/pebble-pingchrong.git`

2. Build the project:

    `$ pebble build`

3. Install the compiled project to your Pebble:

    `$ pebble install --phone [your_phone_ip_here]`

For more information on building and installing Pebble apps from source, please see the official [Getting Started](https://developer.getpebble.com/2/getting-started/) guide.

## Bugs, Suggestions, Comments

Please use the [Github issue system](https://github.com/rexmac/pebble-pingchrong/issues) to report bugs, request new features, or ask questions.

## Credits

Based on and inspired by:

* MONOCHRON clock by [Adafruit](http://www.adafruit.com/products/204) - [github repo](https://github.com/adafruit/monochron)

Icons:

* Menu icon is a modified version of [this icon](https://www.iconfinder.com/icons/175735/ping_pong_icon) by [Visual Pharm](http://icons8.com/)

## License

Copyright (c) 2013, Rex McConnell. All rights reserved.

Redistribution and use in source and binary forms, with or without modification,
are permitted provided that the following conditions are met:

* Redistributions of source code must retain the above copyright notice, this
  list of conditions and the following disclaimer.

* Redistributions in binary form must reproduce the above copyright notice, this
  list of conditions and the following disclaimer in the documentation and/or
  other materials provided with the distribution.

* Neither the name of the {organization} nor the names of its
  contributors may be used to endorse or promote products derived from
  this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR
ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

