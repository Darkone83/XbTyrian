# XbTyrian

<div align=center>

<img src="https://github.com/Darkone83/XbTyrian/blob/main/img/XbTyrain.png" width=300> <img src="https://github.com/Darkone83/XbTryian/blob/main/img/Darkone83.png" width=400>

</div>

**XbTyrian** is an original Xbox / RXDK conversion of **OpenTyrian**, converted by **darkone83**.

OpenTyrian is an open-source port of the DOS vertical scrolling shooter **Tyrian**. This project keeps the OpenTyrian foundation intact while adding the Xbox/RXDK platform work needed to run on original Xbox hardware.

<div align=center>

<img src="https://github.com/Darkone83/XbTryian/blob/main/img/menu.jpg" width=360> <img src="https://github.com/Darkone83/XbTryian/blob/main/img/game.jpg" width=360>

</div>

Original upstream project:

<https://github.com/opentyrian/opentyrian/>

## Status

This is an Xbox/RXDK homebrew port. Current port work includes:

- Original Xbox / RXDK startup flow
- Direct3D video presentation path
- Controller-to-keyboard input compatibility layer
- DirectSound audio streaming timing fixes
- Xbox-safe compatibility fixes where needed
- Custom startup splash support
- Dashboard quit behavior

## Required Game Data

XbTryian requires the **Tyrian 2.1 data files**. The OpenTyrian project documents that the Tyrian 2.1 data files were released as freeware.

Freeware Tyrian 2.1 data download:

<https://camanis.net/tyrian/tyrian21.zip>

Place the required data files beside the XBE or in the configured data directory used by this Xbox build. Do not use Tyrian 1.x, Tyrian 2.0, or Tyrian 2000 data files for this build unless the source has been specifically modified for them.

### Required Data File List

```text
cubetxt1.dat
cubetxt2.dat
cubetxt3.dat
cubetxt4.dat
cubetxt5.dat

demo.1
demo.2
demo.3
demo.4
demo.5

levels1.dat
levels2.dat
levels3.dat
levels4.dat
levels5.dat

music.mus
palette.dat
tyrend.anm
tyrian.cdt
tyrian.hdt
tyrian.pic
tyrian.shp
tyrian.snd
voices.snd

tyrian1.lvl
tyrian2.lvl
tyrian3.lvl
tyrian4.lvl
tyrian5.lvl

estsc.shp

newsh0.shp
newsh1.shp
newsh2.shp
newsh3.shp
newsh4.shp
newsh5.shp
newsh6.shp
newsh7.shp
newsh8.shp
newsh9.shp
newsha.shp
newshb.shp
newshc.shp
newshd.shp
newshe.shp
newshf.shp
newshg.shp
newshh.shp
newshi.shp
newshj.shp
newshk.shp
newshl.shp
newshm.shp
newshn.shp
newsho.shp
newshp.shp
newshq.shp
newshr.shp
newshs.shp
newsht.shp
newshu.shp
newshv.shp
newsh#.shp
newsh@.shp
newsh^.shp
newsh~.shp

shapesa.dat
shapesb.dat
shapesc.dat
shapesd.dat
shapese.dat
shapesf.dat
shapesg.dat
shapesh.dat
shapesi.dat
shapesj.dat
shapesk.dat
shapesl.dat
shapesm.dat
shapesn.dat
shapeso.dat
shapesp.dat
shapesq.dat
shapesr.dat
shapess.dat
shapest.dat
shapesu.dat
shapesv.dat
```

### Optional / Runtime-Generated Files

These files are not required for a clean install, but may appear after running the game:

```text
opentyrian.cfg
tyrian.cfg
tyrian.sav
demorec.*
```

Optional Christmas-mode files, if you intentionally want them:

```text
tyrianc.shp
voicesc.snd
```

## Controls

### Xbox Controller

| Button | Action |
|---|---|
| D-Pad / Left Stick | Move ship / menu navigation |
| A / Right Trigger | Fire primary weapons |
| B / Start | Confirm / menu select / rear weapon mode |
| Left Trigger | Rear weapon mode |
| X | Fire left sidekick |
| Y | Fire right sidekick |
| Back | Escape / back / in-game menu |
| White | Pause |
| Black | Help |

### Keyboard-Compatible Mapping

The Xbox controller compatibility layer maps controller input into OpenTyrian's keyboard-style input system.

| Key | Action |
|---|---|
| Arrow Keys | Move ship |
| Space | Fire primary weapons |
| Enter | Toggle rear weapon mode / confirm |
| Left Ctrl | Fire left sidekick |
| Left Alt | Fire right sidekick |
| Escape | Back / menu / quit prompt |
| P | Pause |
| F1 | Help |

## Credits

### XbTryian

- Xbox/RXDK conversion: **darkone83**

### OpenTyrian

- Original upstream project: **OpenTyrian Development Team**
- Project link: <https://github.com/opentyrian/opentyrian/>

### Original Game

- **Tyrian** by Eclipse Productions / World Tree Games / Epic MegaGames

This project exists because of the work done by the OpenTyrian project. Please preserve the OpenTyrian credits, copyright notices, and license files in any redistributed source or binary release.

## License

OpenTyrian is licensed under the **GNU General Public License version 2.0**.

Because XbTryian is based on OpenTyrian, redistributed builds should follow the GPL-2.0 requirements:

- Keep original copyright and license notices intact.
- Clearly state that this is a modified Xbox/RXDK port.
- Include the GPL-2.0 license text, normally as `COPYING` or `LICENSE`.
- Provide the corresponding source code for the modified build, or provide a valid written offer for the source when distributing binaries.

This README is not a replacement for the full license text. Include the original OpenTyrian `COPYING` file with releases.

## Notes for Releases

Recommended release package structure:

```text
XbTryian/
  default.xbe
  README.txt
  README.md
  COPYING
  data files...
```

Do not include temporary build files, Visual Studio intermediates, generated patch files, or debug-only assets in release packages.

## Upstream Documentation

For original OpenTyrian documentation, source history, and license details, see:

<https://github.com/opentyrian/opentyrian/>
