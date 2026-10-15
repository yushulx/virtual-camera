// Config.h - the persistent controller configuration (config.json).

#pragma once

#include <QString>
#include <QStringList>
#include <QList>

namespace vcam {

struct CameraConfig {
    QString sourcePath; // video, image or folder; empty = built-in test pattern
    QStringList sourceFiles; // multi-selected images (cycled like a folder)
    int width = 1280;
    int height = 720;
    int fps = 30;
    bool loop = true; // loop video files

    bool isValid() const;

    // Standard presets offered by the GUI.
    static QStringList standardResolutions(); // "640x480", ...
    static QList<int> standardFrameRates();    // 10, 15, 24, 25, 30, 50, 60

    // Path of config.json inside the user's application configuration
    // location. Overridable with the VCAM_CONFIG_PATH environment variable
    // (used by the unit tests).
    static QString configFilePath();

    static CameraConfig load();  // returns defaults when no file exists
    void save() const;           // no-op when the file cannot be written
};

} // namespace vcam
