# Bell DSP Configuration

## Configuration properties

The configuration should be a json object, containing a single array, under key `transforms`.

A transform, is a single element in the DSP chain. It takes in input audio, and applies a given effect on it. A type of applied transform is defined by it's `type` parameter.

#### Example

this example config will apply a 2dB gain on channel 0.

```json
{
  "transforms": [
    {
      "type": "gain",
      "gain": 2,
      "channel": 0
    }
  ]
}
```

### Transform properties - all transforms

All transforms take in following properties:

| Name      | Optional?                 | Description                                                                                                                                                                         |
| :-------- | :------------------------ | :---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `type`    | false                     | Type of transform. Either `mixer`, `biquad`, `biquad_combo`, `compressor` or `gain`                                                                                                 |
| `channel` | false, except for `mixer` | Channel the transform will be applied to. As default, channel `0` is left, and channel `1` is right. Can be also provided as `channels` properties, in that case it takes an array. |

### Transform properties - `mixer`

Mixer transform is used for two things:
 - extending the amount of channels by copying the channel data into mapped destination
 - downmixing a higher amount of channels into a lower amount. This mixes the channels together.

In addition to generic parameter, `mixer` type takes following parameters

| Name              | Optional? | Description                                            |
| :---------------- | :-------- | :----------------------------------------------------- |
| `mapped_channels` | false     | An array containing channel mappings. Described below. |


#### `mapped_channels` array item


| Name          | Optional? | Description                                                            |
| :------------ | :-------- | :--------------------------------------------------------------------- |
| `source`      | false     | An array containing input channel / channels to put in the destination |
| `destination` | false     | Index of the destination channel                                       |


#### Example: **downmix channel 0 and 1 into mono**

```json
{
  "type": "mixer",
  "mapped_channels": [
    {
      "source": [0, 1],
      "destination": 0
    }
  ]
},
```

#### Example: **Copy a mono channel 0 into channel 0 and 1**

```json
{
  "type": "mixer",
  "mapped_channels": [
    {
      "source": [0],
      "destination": 0
    },
    {
      "source": [0],
      "destination": 1
    }
  ]
},
```

### Transform properties - `biquad`

Used for applying a biquad filter of any kind and configuration into a channel.

**Doesn't support being applied to two channels at once**

| Name          | Optional? | Description                                                                                                                                            |
| :------------ | :-------- | :----------------------------------------------------------------------------------------------------------------------------------------------------- |
| `biquad_type` | false     | One of `free`, `highpass`, `highpass_fo` `lowpass`, `lowpass_fo`, `highshelf`, `highshelf_fo`, `peaking`, `notch`, `bandpass`, `allpass`, `allpass_fo` |
| `frequency`   | false     | Biquad frequency, in (Hz)                                                                                                                              |
| `q`           | true      | Q Factor value, only used for `highpass`, `lowpass`, `peaking`, `highshelf`, `lowshelf`, `notch`, `bandpass`, `allpass`                                |
| `gain`        | true      | Gain value (dB), only used for `peaking`, `highshelf`, `lowshelf`, `notch`                                                                             |
| `bandwidth`   | true      | Bandwidth value, can be used instead of q for `peaking`, `notch`, `bandpass`, `allpass`                                                                |
| `slope`       | true      | Slope value, can be used instead of q for `lowshelf`, `highshelf`                                                                                      |
| `a1`          | true      | Only for `free` type. Allows for setting the coefficients manually                                                                                     |
| `a2`          | true      | Only for `free` type. Allows for setting the coefficients manually                                                                                     |
| `b0`          | true      | Only for `free` type. Allows for setting the coefficients manually                                                                                     |
| `b1`          | true      | Only for `free` type. Allows for setting the coefficients manually                                                                                     |
| `b2`          | true      | Only for `free` type. Allows for setting the coefficients manually                                                                                     |


#### Example: **Peaking filter**
```json
{
  "type": "biquad",
  "channel": 0,
  "biquad_type": "peaking",
  "frequency": 550,
  "q": 0.707,
  "gain": -2
}
```

### Transform properties - `biquad_combo`

Creates a chain of multiple biquad filters, making them into a higher order filter. Currently supports Linkwitz-Riley and Butterworth. Used mostly for crossovers.

**Doesn't support being applied to two channels at once**


| Name         | Optional? | Description                                                                                     |
| :----------- | :-------- | :---------------------------------------------------------------------------------------------- |
| `combo_type` | false     | Type of the biquad combination. Can be `lr_lowpass`, `lr_highpass`, `bw_lowpass`, `bw_highpass` |
| `order`      | false     | n-th order of the filter                                                                        |
| `frequency`  | false     | Corner frequency provided to the filter                                                         |

#### Example: **Linkwitz-Riley lowpass**
```json
{
  "type": "biquad_comboo",
  "channel": 0,
  "combo_type": "lr_lowpass",
  "frequency": 160
}
```




### Transform properties - `compressor`

Applies compression to input audio.

| Name          | Optional? | Description                                                                              |
| :------------ | :-------- | :--------------------------------------------------------------------------------------- |
| `attack`      | false     | Attack time, in (ms)                                                                     |
| `release`     | false     | Release time, in (ms)                                                                    |
| `threshold`   | false     | Compression threshold, in (dB)                                                           |
| `factor`      | true      | Compression factor, defaults to `4`. Aka, after compression, `4dB` get scaled into `1dB` |
| `makeup_gain` | true      | Gain to apply after the compressor (dB). Works same way as a gain filter afterwards.     |

The compressor filter supports monitoring multiple channels at once, and compressing both the same way. Handled by the `channels` parameter.


### Transform properties - `gain`


Applies gain to the channel / channels.

| Name   | Optional? | Description            |
| :----- | :-------- | :--------------------- |
| `gain` | false     | Gain to apply, in (dB) |


## Setting value dependent on volume


The DSP configuration allows to set any numeric value to be speaker volume level dependent. This allows for different settings, depending on the volume.

This is configured, by replacing a numeric value with an array. The array should contain all the possible values for a given setting, and the DSP will automatically select which value to use at a given volume level.

For example: 

```json
{
  "type": "gain",
  "gain": [0.0, 2.0, 3.0, 3.0],
  "channel": 0
}
```

Will result in the gain being `0.0` at 0% to 25% volume, `2.0` at 25% to 50%, and `3.0` at 50% to 100%.