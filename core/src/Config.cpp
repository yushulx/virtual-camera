#include "vcam/Config.h"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>

namespace vcam {

bool CameraConfig::isValid() const
{
    return width > 0 && height > 0 && fps > 0 && fps <= 240;
}

QStringList CameraConfig::standardResolutions()
{
    return {"640x480", "854x480", "1280x720", "1920x1080"};
}

QList<int> CameraConfig::standardFrameRates()
{
    return {10, 15, 24, 25, 30, 50, 60};
}

QString CameraConfig::configFilePath()
{
    if (qEnvironmentVariableIsSet("VCAM_CONFIG_PATH")) {
        return qEnvironmentVariable("VCAM_CONFIG_PATH");
    }
    const QString base =
        QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    return QDir(base).filePath(QStringLiteral("config.json"));
}

CameraConfig CameraConfig::load()
{
    CameraConfig cfg;
    QFile file(configFilePath());
    if (!file.open(QIODevice::ReadOnly)) {
        return cfg; // defaults
    }
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    if (!doc.isObject()) {
        return cfg;
    }
    const QJsonObject obj = doc.object();
    cfg.sourcePath = obj.value("sourcePath").toString();
    for (const QJsonValue& value : obj.value("sourceFiles").toArray()) {
        cfg.sourceFiles.append(value.toString());
    }
    cfg.width = obj.value("width").toInt(cfg.width);
    cfg.height = obj.value("height").toInt(cfg.height);
    cfg.fps = obj.value("fps").toInt(cfg.fps);
    cfg.loop = obj.value("loop").toBool(cfg.loop);
    if (!cfg.isValid()) {
        return CameraConfig{}; // corrupted file: fall back to defaults
    }
    return cfg;
}

void CameraConfig::save() const
{
    if (!isValid()) {
        return;
    }
    const QString path = configFilePath();
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return; // best effort
    }
    QJsonObject obj;
    obj.insert("sourcePath", sourcePath);
    QJsonArray files;
    for (const QString& file : sourceFiles) {
        files.append(file);
    }
    obj.insert("sourceFiles", files);
    obj.insert("width", width);
    obj.insert("height", height);
    obj.insert("fps", fps);
    obj.insert("loop", loop);
    file.write(QJsonDocument(obj).toJson(QJsonDocument::Indented));
}

} // namespace vcam
