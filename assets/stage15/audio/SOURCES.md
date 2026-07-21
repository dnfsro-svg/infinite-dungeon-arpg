# Stage 15 audio mix sources

All ten assets are redistributed under Creative Commons CC0. Long streams retain the original OGG bytes; the six UI releases are deterministic PCM s16le/44.1 kHz/mono conversions of selected files from Kenney's official UI Audio package.

- Global Resonance, Eponasoft: https://opengameart.org/content/global-resonance
- Trance Dungeon, MintoDog: https://opengameart.org/content/trance-dungeon
- Loopable Dungeon Ambience, JaggedStone: https://opengameart.org/content/loopable-dungeon-ambience
- Ancient caverns, congusbongus: https://opengameart.org/content/ancient-caverns-horror-ambient-loop
- Kenney UI Audio: https://kenney.nl/assets/ui-audio
- Kenney official ZIP: https://kenney.nl/media/pages/assets/ui-audio/490d233f68-1677590494/kenney_ui-audio.zip

| Published file | Original file | Author | Original SHA-256 | Release SHA-256 |
| --- | --- | --- | --- | --- |
| `music-explore.ogg` | `global_resonance.ogg` | Eponasoft | `B5095B1517E0563141E4158554B8730E30E82FCB2000C1081BD448DD4B722A24` | `B5095B1517E0563141E4158554B8730E30E82FCB2000C1081BD448DD4B722A24` |
| `music-combat.ogg` | `trance_dungeon_bpm130_0.ogg` | MintoDog | `783FA3DB1B466DD44B4F2097601DD05DF9FE39FDB505AB3B073B73681250FA93` | `783FA3DB1B466DD44B4F2097601DD05DF9FE39FDB505AB3B073B73681250FA93` |
| `ambience-room.ogg` | `dungeon_ambient_1_0.ogg` | JaggedStone | `DF491823E4877371C34DBDA4E9321CD83A4A14FA7573CEE0EBCA1AE423B70E6E` | `DF491823E4877371C34DBDA4E9321CD83A4A14FA7573CEE0EBCA1AE423B70E6E` |
| `ambience-abyss.ogg` | `caverns_0.ogg` | congusbongus | `5C841EF1A7BACD2A038801ECEE0741E200D1CF1CD08F36A6A89CCE95BCFB6A8A` | `5C841EF1A7BACD2A038801ECEE0741E200D1CF1CD08F36A6A89CCE95BCFB6A8A` |
| `ui-navigate.wav` | `Audio/rollover2.ogg` | Kenney | `D0BC0FCBA496D73DF1DA8C145E5330DF75F87AB5A4F33A9B71461B006B5C338E` | `671496A390CDF1052A59B6204EB10DB0CFC3B01EC5F8F6FB0B3B31FAEDC9FED5` |
| `ui-confirm.wav` | `Audio/click3.ogg` | Kenney | `E3E7FC7CCD9C5CFDF77BFFA05AEE10D4973812076C76DC889BA2B16EA434127F` | `985993A23D40387D02EBB7CBBED7B4B09869788A8E282E545E3037B8084BC695` |
| `ui-cancel.wav` | `Audio/click5.ogg` | Kenney | `52D41E0BB012731BF2A391D53FFB52265EF21589C7701E66B391D5E36A65B293` | `9F31B9AEE0551E50932717A76B10F5536BF912DB161CB80D2075C0E25FB9C857` |
| `ui-open.wav` | `Audio/switch1.ogg` | Kenney | `EFDD1D1E2904FB2D81259CD96BB80101CACEAE94BE02BAA4B5310714A6708B19` | `6965E9AFF1ED62E3B40749DAD1E9871A29BF775BB9DC6A9AFF6B34AD0AB0A6CA` |
| `ui-close.wav` | `Audio/switch2.ogg` | Kenney | `AA8AD6E4745E87C84C24A0335E0E0F8629CCBBC474CCFD544FE07D7F3A1B2280` | `BF655C861B6B6E4ECED0057FBD2C9BD6625C844606FF91B5569D54D0A8073186` |
| `ui-reward.wav` | `Audio/switch33.ogg` | Kenney | `977ECEAD4B27CF9801AED433BFA4BDFCF49444C15990D27D2E472401A1FB2742` | `36E3E28F1365F80001925F699654131F476269D39D79709B82385266650AEB56` |

Preparation command for each UI source:

```text
ffmpeg -v error -y -i <source.ogg> -ac 1 -ar 44100 -c:a pcm_s16le <release.wav>
```

The source ZIP SHA-256 is `946FC23A63D535D693EB31B2EABB80C8C28D6351E2186B344CEB71B2CB1D5EB6`.
