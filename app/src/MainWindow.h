// MainWindow.h - the controller GUI.

#pragma once

#include "vcam/CameraController.h"

#include <QMainWindow>
#include <QTimer>

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;

namespace vcam {

class ExtensionActivator;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);

protected:
    void closeEvent(QCloseEvent* event) override;

private slots:
    void browseSource();
    void startStopClicked();
    void updatePreview();
    void refreshPlatformStatus();
#if defined(Q_OS_MACOS)
    void activateExtensionClicked();
    void extensionActivationFinished(bool ok, const QString& message);
#endif

private:
    void buildUi();
    CameraConfig configFromUi() const;
    void applyConfig(const CameraConfig& cfg);
    void setRunningUi(bool running);
    void loadConfig();
    void saveConfig();
    QString sourceKind() const; // "video", "image" or "test pattern"

    CameraController m_controller;
    CameraConfig m_config;
    QStringList m_selectedFiles; // multi-selected images (browse dialog)

    QLineEdit* m_sourceEdit = nullptr;
    QLabel* m_sourceKindLabel = nullptr;
    QLabel* m_previewLabel = nullptr;
    QComboBox* m_resolutionCombo = nullptr;
    QComboBox* m_fpsCombo = nullptr;
    QCheckBox* m_loopCheck = nullptr;
    QPushButton* m_startStopButton = nullptr;
    QLabel* m_statusLabel = nullptr;
    QLabel* m_platformLabel = nullptr;
    QTimer m_previewTimer;
    QTimer m_statusTimer;

#if defined(Q_OS_MACOS)
    ExtensionActivator* m_extensionActivator = nullptr;
    QPushButton* m_extensionButton = nullptr;
#endif
};

} // namespace vcam
