# Schillinger Max Package

© Manolo Müller

Hochschule der Künste Bern - 2020

## About

The Schillinger Max Package is an ongoing effort to implement rhythmical concepts developed by Joseph Schillinger in the first half of the 20th century. The initial development was done as part of my bachelor thesis, BA Sound Arts, Hochschule der Künste Bern, 2020. The thesis, along with additional example patches, mapping the progress of implementation can be downloaded here (all in German):

<http://bit.ly/schillingerpackage>

## Compatibility

This package depends on multichannel capabilities and thus is compatible only with Max 8 onwards.

As of right now, there are no Windows binaries, as I don't have a Windows developement environment. If you would want to compile the externals for Windows, feel free to do so, and message me so that they can be added to the official release of the package.

## Installation

To install this package, simply move this whole folder to your `Packages` folder, usually found at `~/Documents/Max 8`.
The externals may either be compiled by yourself or you can download the latest release here:
<https://github.com/rybenmensch/schillinger/releases>
To remove this package, just delete it from the `Packages` folder.

## Compilation

To compile the externals yourself, download the MaxSDK from <https://github.com/Cycling74/max-sdk>. Move the extracted folder to your `Max 8/Packages` directory and rename it to `max-sdk`. The necessary folder (`c47support/`) and files (`Info.plist`, `build.rb`, `maxmspsdk.xcconfig`) are linked into this folder (SDK). You can open each project in XCode and build it or run `./build.rb` to build them in one run.

## Contact

If you find any bugs, have suggestions for improvement or any questions, feel free to contact me at manolo.mueller@gmail.com.

## Licence

This software package is licensed under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.

See the LICENCE.md file for the full version of this licence.
