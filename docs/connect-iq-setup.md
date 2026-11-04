# Setting up Connect IQ

## 1. Install the tools

- **Java:** a JDK, e.g. Eclipse Temurin 17. Compiler and simulator need it.
- **VS Code** with the extension *Monkey C* (`garmin.monkey-c`).

## 2. SDK Manager

The SDK Manager downloads the SDK and the device files. It needs a Garmin account and your acceptance of the license.

1. Open <https://developer.garmin.com/connect-iq/sdk/> and download the SDK Manager.
2. Unpack and start it, sign in with your Garmin account.
3. Tab **SDK**: download the latest version and choose **Use as Active SDK**.
4. Tab **Devices**: download your watches, e.g. Forerunner 945 and Forerunner 55.

## 3. Developer key

Run **Monkey C: Generate a Developer Key** in VS Code. Its path is stored in the VS Code setting `monkeyC.developerKeyPath`. Keep this file private and outside the repository, and always use the same key for an app.

## 4. Check the installation

1. Restart VS Code, so that Java is in the path.
2. `Ctrl+Shift+P` → **Monkey C: Verify Installation**. It must finish without errors.

## 5. Simulator

1. Open the folder `widget` in VS Code.
2. `F5` → choose a device. The simulator starts with the widget. Use it for the menu, the settings and the tests; sending a command needs a real watch.

## 6. Transfer to the watch (sideload)

1. `Ctrl+Shift+P` → **Monkey C: Build for Device** → choose a device and an output folder. The result is a `.prg` file.
2. Connect the watch over USB. It appears as a drive or as an MTP device.
3. Copy the `.prg` to `GARMIN\APPS\` and disconnect the watch. The widget is then in the widget loop.

To update, overwrite the file under the same name, otherwise the widget is there twice. To delete, remove the `.prg` from `GARMIN\APPS\`.

If the widget crashes (IQ logo with an exclamation mark), the reason is in `GARMIN\APPS\LOGS\CIQ_LOG.YML`.

## 7. Publishing

**Monkey C: Export Project** creates an `.iq` file for the Connect IQ Store (<https://apps.garmin.com/developer>).
