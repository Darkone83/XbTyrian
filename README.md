# XbTryian

**XbTryian** is an original Xbox / RXDK conversion of **OpenTyrian**, converted by **darkone83**.

OpenTyrian is an open-source port of the DOS vertical scrolling shooter **Tyrian**. This project keeps the OpenTyrian foundation intact while adding the Xbox/RXDK platform work needed to run on original Xbox hardware.

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

XbTryian requires the **Tyrian 2.1 data files**. These are not included as source code in this project.

OpenTyrian documents that the Tyrian 2.1 data files were released as freeware. Place the required data files beside the XBE or in the configured data directory used by this Xbox build.

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
