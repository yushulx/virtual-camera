#include "MainWindow.h"

#include "SystemExtensionActivator.h"

#include <QCheckBox>
#include <QComboBox>
#include <QCoreApplication>
#include <QFileDialog>
#include <QFileInfo>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QImageReader>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPixmap>
#include <QPushButton>
#include <QSettings>
#include <QTimer>
#include <QVBoxLayout>

#if defined(Q_OS_LINUX)
#include "vcam/V4L2Writer.h"
#endif

namespace vcam {

namespace {

// Keep in sync with windows/driver/src/guiddefs.h.
constexpr char kWindowsDriverClsid[] =
    "{D4A1F5B2-6E38-4C7A-9B2E-1F8D3A5C7E64}";

QString cameraHintText()
{
#if defined(Q_OS_WIN)
    return QStringLiteral(
        "The camera appears as \"Virtual Camera\" in Chrome, Edge and Firefox "
        "and any DirectShow application. Open docs/web-camera-test.html in a "
        "browser to verify.");
#elif defined(Q_OS_LINUX)
    return QStringLiteral(
        "The camera appears as \"Virtual Camera\" (v4l2loopback) in Chrome, "
        "Firefox and any V4L2 application. Open docs/web-camera-test.html in "
        "a browser to verify.");
#else
    return QStringLiteral(
        "After the camera extension is approved, the camera appears as "
        "\"Virtual Camera\" in Chrome, Edge, Firefox and Safari. Open "
        "docs/web-camera-test.html in a browser to verify.");
#endif
}

QPixmap scaledPreview(const QImage& image, const QLabel* target)
{
    if (image.isNull()) {
        return QPixmap();
    }
    return QPixmap::fromImage(
        image.scaled(target->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
}

} // namespace

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
{
    setWindowTitle(QStringLiteral("Virtual Camera"));
    setMinimumSize(560, 720);
    buildUi();
    loadConfig();
    refreshPlatformStatus();

    connect(&m_controller, &CameraController::stateChanged, this,
            [this](bool running) {
                setRunningUi(running);
                m_statusLabel->setText(running
                                           ? tr("Camera is running.")
                                           : tr("Camera is stopped."));
            });
    connect(&m_controller, &CameraController::errorOccurred, this,
            [this](const QString& message) {
                m_statusLabel->setText(message);
            });

    m_previewTimer.setInterval(100);
    connect(&m_previewTimer, &QTimer::timeout, this, &MainWindow::updatePreview);
    m_previewTimer.start();

    m_statusTimer.setInterval(3000);
    connect(&m_statusTimer, &QTimer::timeout, this,
            &MainWindow::refreshPlatformStatus);
    m_statusTimer.start();

#if defined(Q_OS_MACOS)
    m_extensionActivator = new ExtensionActivator(this);
    connect(m_extensionActivator, &ExtensionActivator::finished, this,
            &MainWindow::extensionActivationFinished);
#endif
}

void MainWindow::buildUi()
{
    auto* central = new QWidget(this);
    auto* root = new QVBoxLayout(central);

    // --- Source ------------------------------------------------------------
    auto* sourceGroup = new QGroupBox(tr("Source"), central);
    auto* sourceLayout = new QVBoxLayout(sourceGroup);
    auto* sourceRow = new QHBoxLayout();
    m_sourceEdit = new QLineEdit(sourceGroup);
    m_sourceEdit->setPlaceholderText(
        tr("Leave empty for the built-in test pattern"));
    m_sourceEdit->setClearButtonEnabled(true);
    auto* browseButton = new QPushButton(tr("Browse..."), sourceGroup);
    sourceRow->addWidget(m_sourceEdit, 1);
    sourceRow->addWidget(browseButton);
    m_sourceKindLabel = new QLabel(sourceGroup);
    sourceLayout->addLayout(sourceRow);
    sourceLayout->addWidget(m_sourceKindLabel);
    root->addWidget(sourceGroup);

    // --- Preview ------------------------------------------------------------
    auto* previewGroup = new QGroupBox(tr("Preview"), central);
    auto* previewLayout = new QVBoxLayout(previewGroup);
    m_previewLabel = new QLabel(previewGroup);
    m_previewLabel->setMinimumSize(480, 270);
    m_previewLabel->setAlignment(Qt::AlignCenter);
    m_previewLabel->setStyleSheet(
        QStringLiteral("background-color: #101010; color: #ffffff;"));
    m_previewLabel->setText(tr("No source"));
    previewLayout->addWidget(m_previewLabel);
    root->addWidget(previewGroup, 1);

    // --- Settings ------------------------------------------------------------
    auto* settingsGroup = new QGroupBox(tr("Camera settings"), central);
    auto* settingsLayout = new QHBoxLayout(settingsGroup);
    settingsLayout->addWidget(new QLabel(tr("Resolution:"), settingsGroup));
    m_resolutionCombo = new QComboBox(settingsGroup);
    m_resolutionCombo->addItems(CameraConfig::standardResolutions());
    settingsLayout->addWidget(m_resolutionCombo, 1);
    settingsLayout->addWidget(new QLabel(tr("Frame rate:"), settingsGroup));
    m_fpsCombo = new QComboBox(settingsGroup);
    for (int fps : CameraConfig::standardFrameRates()) {
        m_fpsCombo->addItem(QString::number(fps) + QStringLiteral(" fps"), fps);
    }
    settingsLayout->addWidget(m_fpsCombo, 1);
    m_loopCheck = new QCheckBox(tr("Loop video"), settingsGroup);
    m_loopCheck->setChecked(true);
    settingsLayout->addWidget(m_loopCheck);
    root->addWidget(settingsGroup);

    // --- Control ------------------------------------------------------------
    auto* controlGroup = new QGroupBox(tr("Virtual camera"), central);
    auto* controlLayout = new QVBoxLayout(controlGroup);
    auto* controlRow = new QHBoxLayout();
    m_startStopButton = new QPushButton(tr("Start virtual camera"),
                                        controlGroup);
    m_startStopButton->setMinimumHeight(40);
    controlRow->addWidget(m_startStopButton);
    controlLayout->addLayout(controlRow);

#if defined(Q_OS_MACOS)
    auto* extensionRow = new QHBoxLayout();
    m_extensionButton =
        new QPushButton(tr("Install camera extension..."), controlGroup);
    extensionRow->addWidget(m_extensionButton);
    extensionRow->addStretch(1);
    controlLayout->addLayout(extensionRow);
#endif

    m_statusLabel = new QLabel(tr("Camera is stopped."), controlGroup);
    m_statusLabel->setWordWrap(true);
    controlLayout->addWidget(m_statusLabel);
    m_platformLabel = new QLabel(controlGroup);
    m_platformLabel->setWordWrap(true);
    controlLayout->addWidget(m_platformLabel);
    root->addWidget(controlGroup);

    auto* hintLabel = new QLabel(cameraHintText(), central);
    hintLabel->setWordWrap(true);
    hintLabel->setStyleSheet(QStringLiteral("color: gray;"));
    root->addWidget(hintLabel);

    setCentralWidget(central);

    // --- wiring ------------------------------------------------------------
    connect(browseButton, &QPushButton::clicked, this, &MainWindow::browseSource);
    connect(m_startStopButton, &QPushButton::clicked, this,
            &MainWindow::startStopClicked);
#if defined(Q_OS_MACOS)
    connect(m_extensionButton, &QPushButton::clicked, this,
            &MainWindow::activateExtensionClicked);
#endif
    auto markDirty = [this] {
        m_sourceKindLabel->setText(sourceKind());
    };
    connect(m_sourceEdit, &QLineEdit::textChanged, this, markDirty);
    m_sourceKindLabel->setText(sourceKind());
}

void MainWindow::closeEvent(QCloseEvent* event)
{
    m_controller.stop();
    saveConfig();
    event->accept();
}

CameraConfig MainWindow::configFromUi() const
{
    CameraConfig cfg;
    cfg.sourcePath = m_sourceEdit->text().trimmed();
    const QString resolution = m_resolutionCombo->currentText();
    const QStringList parts = resolution.split(QLatin1Char('x'));
    if (parts.size() == 2) {
        cfg.width = parts.at(0).toInt();
        cfg.height = parts.at(1).toInt();
    }
    cfg.fps = m_fpsCombo->currentData().toInt();
    if (cfg.fps <= 0) {
        cfg.fps = 30;
    }
    cfg.loop = m_loopCheck->isChecked();
    return cfg;
}

void MainWindow::applyConfig(const CameraConfig& cfg)
{
    m_sourceEdit->setText(cfg.sourcePath);
    const QString resolution =
        QStringLiteral("%1x%2").arg(cfg.width).arg(cfg.height);
    int index = m_resolutionCombo->findText(resolution);
    if (index < 0) {
        index = m_resolutionCombo->count();
        m_resolutionCombo->addItem(resolution);
    }
    m_resolutionCombo->setCurrentIndex(index);
    index = m_fpsCombo->findData(cfg.fps);
    if (index < 0) {
        index = m_fpsCombo->findData(30);
    }
    m_fpsCombo->setCurrentIndex(index);
    m_loopCheck->setChecked(cfg.loop);
    m_sourceKindLabel->setText(sourceKind());
}

void MainWindow::loadConfig()
{
    applyConfig(CameraConfig::load());
}

void MainWindow::saveConfig()
{
    CameraConfig cfg = configFromUi();
    cfg.save();
}

QString MainWindow::sourceKind() const
{
    const QString path = m_sourceEdit->text().trimmed();
    if (path.isEmpty()) {
        return tr("Source: built-in test pattern");
    }
    if (!QFileInfo::exists(path)) {
        return tr("Source: file not found");
    }
    QImageReader reader(path);
    if (reader.canRead()) {
        return tr("Source: image (repeated at the frame rate)");
    }
    return tr("Source: video (looped)");
}

void MainWindow::browseSource()
{
    const QString path = QFileDialog::getOpenFileName(
        this, tr("Select a video or image file"), QString(),
        tr("Media files (*.mp4 *.mov *.mkv *.webm *.avi *.m4v *.mpg *.mpeg "
           "*.ts *.flv *.wmv *.png *.jpg *.jpeg *.bmp *.webp *.gif *.tiff);; "
           "All files (*)"));
    if (!path.isEmpty()) {
        m_sourceEdit->setText(path);
        saveConfig();
    }
}

void MainWindow::startStopClicked()
{
    if (m_controller.isRunning()) {
        m_controller.stop(); // stateChanged updates the UI
        return;
    }
    saveConfig();
    const CameraConfig cfg = configFromUi();
    QString error;
    if (!m_controller.start(cfg, &error)) {
        QMessageBox::warning(this, tr("Virtual Camera"),
                             tr("Could not start the virtual camera:\n%1")
                                 .arg(error));
        return;
    }
}

void MainWindow::setRunningUi(bool running)
{
    m_startStopButton->setText(running ? tr("Stop virtual camera")
                                       : tr("Start virtual camera"));
    m_resolutionCombo->setEnabled(!running);
    m_fpsCombo->setEnabled(!running);
}

void MainWindow::updatePreview()
{
    if (m_controller.isRunning() && m_controller.source() != nullptr) {
        QImage frame;
        if (m_controller.source()->latestFrame(320, 180, &frame)) {
            m_previewLabel->setPixmap(scaledPreview(frame, m_previewLabel));
            return;
        }
    }
    // Show the selected image as a thumbnail when idle.
    const QString path = m_sourceEdit->text().trimmed();
    if (!path.isEmpty() && !m_previewLabel->pixmap()) {
        QImageReader reader(path);
        reader.setAutoTransform(true);
        reader.setScaledSize(QSize(480, 270));
        const QImage image = reader.read();
        if (!image.isNull()) {
            m_previewLabel->setPixmap(scaledPreview(image, m_previewLabel));
            return;
        }
    }
}

void MainWindow::refreshPlatformStatus()
{
    QString status;
#if defined(Q_OS_WIN)
    QSettings settings(
        QStringLiteral("HKEY_CLASSES_ROOT\\CLSID\\%1").arg(kWindowsDriverClsid),
        QSettings::NativeFormat);
    if (settings.contains(QStringLiteral("InprocServer32/Default"))) {
        status = tr("DirectShow filter: installed and registered.");
    } else {
        status = tr("DirectShow filter: NOT registered. Run "
                    "scripts\\register-driver.cmd as administrator "
                    "(or reinstall the setup).");
    }
#elif defined(Q_OS_LINUX)
    const QList<V4l2DeviceInfo> devices = listV4l2Devices();
    QString device;
    for (const V4l2DeviceInfo& info : devices) {
        if (info.card == QLatin1String(V4L2Writer::kCardLabel) && info.canOutput) {
            device = info.device;
            break;
        }
    }
    if (!device.isEmpty()) {
        status = tr("v4l2loopback device: %1 (\"Virtual Camera\")").arg(device);
    } else {
        status = tr("No \"Virtual Camera\" device node. Load the module with:\n"
                    "sudo modprobe v4l2loopback exclusive_caps=1 "
                    "card_label=\"Virtual Camera\" max_devices=1");
    }
#else
    status = tr("Use \"Install camera extension...\" once; macOS then shows a "
                "system extension approval in System Settings.");
#endif
    m_platformLabel->setText(status);
}

#if defined(Q_OS_MACOS)

void MainWindow::activateExtensionClicked()
{
    m_extensionButton->setEnabled(false);
    m_extensionActivator->activate(
        QStringLiteral("com.dynamsoft.virtualcamera.VirtualCameraExtension"));
}

void MainWindow::extensionActivationFinished(bool ok, const QString& message)
{
    m_extensionButton->setEnabled(true);
    m_statusLabel->setText(message);
    if (ok) {
        QMessageBox::information(
            this, tr("Virtual Camera"),
            tr("The camera extension is active. Select \"Virtual Camera\" "
               "in your browser or conferencing app."));
    }
}

#endif

} // namespace vcam
