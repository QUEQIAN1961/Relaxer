#include "ai-settings-page.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QFrame>
#include <QGridLayout>
#include <QHideEvent>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QScrollBar>
#include <QSizePolicy>
#include <QStandardItemModel>
#include <QStackedWidget>
#include <QVBoxLayout>

namespace relaxer::ai {
namespace {
QLabel *heading(const QString &text, QWidget *parent = nullptr) {
    auto *label = new QLabel(text, parent);
    QFont font = label->font();
    font.setBold(true);
    font.setPointSize(font.pointSize() + 2);
    label->setFont(font);
    label->setWordWrap(true);
    return label;
}

QLabel *hint(const QString &text, QWidget *parent = nullptr) {
    auto *label = new QLabel(text, parent);
    label->setObjectName("aiHint");
    label->setWordWrap(true);
    label->setMinimumWidth(0);
    return label;
}

QPushButton *button(const QString &text, const QString &name, QWidget *parent = nullptr) {
    auto *result = new QPushButton(text, parent);
    result->setObjectName(name);
    result->setCursor(Qt::PointingHandCursor);
    return result;
}

QWidget *rowHost(QWidget *parent = nullptr) {
    auto *row = new QWidget(parent);
    row->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    auto *layout = new QHBoxLayout(row);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(8);
    return row;
}

QHBoxLayout *rowLayout(QWidget *row) { return qobject_cast<QHBoxLayout *>(row->layout()); }

QString effortLabel(const QString &effort) {
    if (effort == "low") return "Low";
    if (effort == "high") return "High";
    if (effort == "max") return "Max";
    return effort;
}

QString currentEffort(const AiTaskConfiguration &configuration) {
    return configuration.thinkingEnabled ? configuration.reasoningEffort : QStringLiteral("off");
}

void setEffort(AiTaskConfiguration &configuration, const QString &effort) {
    configuration.thinkingEnabled = effort != "off";
    configuration.reasoningEffort = configuration.thinkingEnabled ? effort : QString();
}

QWidget *makeScrollPage(QWidget *content, QScrollArea **scrollOut, QWidget *parent) {
    auto *scroll = new QScrollArea(parent);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    scroll->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    scroll->setWidget(content);
    *scrollOut = scroll;
    return scroll;
}

QWidget *makeHeader(const QString &title, const QString &backText, const QString &backName,
                    QPushButton **backButton, QWidget *parent) {
    auto *host = new QWidget(parent);
    auto *layout = new QHBoxLayout(host);
    layout->setContentsMargins(0, 0, 0, 12);
    layout->setSpacing(12);
    auto *titleLabel = heading(title, host);
    titleLabel->setObjectName("aiPageTitle");
    layout->addWidget(titleLabel, 1);
    *backButton = button(backText, backName, host);
    layout->addWidget(*backButton, 0, Qt::AlignTop);
    return host;
}

QString modelDisplay(const ModelInfo &model) {
    QString text = model.displayName;
    if (model.availability == ModelAvailability::Unverified)
        text += "（尚未验证）";
    else if (model.availability == ModelAvailability::Unavailable)
        text += "（当前不可用）";
    return text;
}
} // namespace

AiSettingsPage::AiSettingsPage(AiRuntime *runtime, QWidget *parent)
    : QWidget(parent), m_runtime(runtime) {
    setObjectName("aiServicePage");
    setMinimumSize(0, 0);
    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(34, 26, 34, 24);
    outer->setSpacing(0);
    m_stack = new QStackedWidget(this);
    m_stack->setObjectName("aiSettingsStack");
    outer->addWidget(m_stack, 1);
    buildHome();
    buildTranslation();
    m_stack->addWidget(m_home);
    m_stack->addWidget(m_translation);
    if (m_runtime) {
    connect(m_runtime, &AiRuntime::configurationChanged, this, &AiSettingsPage::populate);
    connect(m_runtime, &AiRuntime::modelsChanged, this, &AiSettingsPage::populate);
    connect(m_runtime->requests(), &AiRequestCoordinator::modelsFinished, this,
            [this](quint64 taskId, const QString &, const QList<ModelInfo> &models, const ProviderError &error) {
        if (taskId != m_pendingRefresh)
            return;
        m_pendingRefresh = 0;
        setRequestState(false);
        if (!error.isEmpty()) {
            m_providerStatus->setText(error.message);
            m_translationStatus->setText(error.message);
            return;
        }
        if (models.isEmpty()) {
            m_providerStatus->setText("连接成功，但供应商未返回可用模型");
            m_translationStatus->setText("模型列表为空，请检查供应商状态");
        } else {
            m_providerStatus->setText(QString("连接成功，已读取 %1 个模型").arg(models.size()));
            m_translationStatus->setText(QString("模型列表已更新（%1 个）").arg(models.size()));
        }
        updateCredentialStatus();
        updateModels();
    });
    connect(m_runtime, &QObject::destroyed, this, [this] {
        m_pendingRefresh = 0;
        setEnabled(false);
    });
    } else {
        setEnabled(false);
    }
    populate();
}

AiSettingsPage::~AiSettingsPage() { cancelRefresh(); }
void AiSettingsPage::hideEvent(QHideEvent *event) {
    cancelRefresh();
    QWidget::hideEvent(event);
}
void AiSettingsPage::cancelRefresh() {
    const auto taskId = m_pendingRefresh;
    m_pendingRefresh = 0;
    if (taskId && m_runtime) m_runtime->requests()->cancel(taskId);
    setRequestState(false);
}

void AiSettingsPage::buildHome() {
    m_home = new QWidget(this);
    m_home->setObjectName("aiServiceHome");
    auto *outer = new QVBoxLayout(m_home);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(12);
    QPushButton *back = nullptr;
    outer->addWidget(makeHeader("AI 服务", "返回设置", "aiSettingsBack", &back, m_home));
    connect(back, &QPushButton::clicked, this, &AiSettingsPage::backRequested);

    auto *body = new QWidget;
    body->setObjectName("aiServiceHomeContent");
    auto *bodyLayout = new QVBoxLayout(body);
    bodyLayout->setContentsMargins(0, 0, 8, 4);
    bodyLayout->setSpacing(14);

    auto *providerCard = new QGroupBox("供应商管理", body);
    providerCard->setObjectName("aiProviderCard");
    auto *providerLayout = new QVBoxLayout(providerCard);
    providerLayout->setSpacing(10);
    auto *providerForm = new QFormLayout;
    providerForm->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    providerForm->setRowWrapPolicy(QFormLayout::WrapLongRows);
    m_provider = new QComboBox(providerCard);
    m_provider->setObjectName("aiProviderSelection");
    m_provider->addItem("请选择供应商", QString());
    m_provider->addItem("DeepSeek 官方 API", QString::fromLatin1(DeepSeekProviderId));
    m_provider->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    m_provider->setMinimumContentsLength(18);
    providerForm->addRow("供应商", m_provider);

    auto *keyRow = rowHost(providerCard);
    m_keyInput = new QLineEdit(keyRow);
    m_keyInput->setObjectName("aiCredentialInput");
    m_keyInput->setEchoMode(QLineEdit::Password);
    m_keyInput->setPlaceholderText("在 Relaxer 中输入 API Key");
    m_keyInput->setClearButtonEnabled(true);
    connect(m_keyInput, &QLineEdit::textChanged, this, [this] { updateCredentialStatus(); });
    rowLayout(keyRow)->addWidget(m_keyInput, 1);
    m_saveKey = button("保存 Key", "aiCredentialSave", keyRow);
    rowLayout(keyRow)->addWidget(m_saveKey);
    m_deleteKey = button("删除 Key", "aiCredentialDelete", keyRow);
    rowLayout(keyRow)->addWidget(m_deleteKey);
    providerForm->addRow("API Key", keyRow);
    m_credentialStatus = hint("未配置", providerCard);
    m_credentialStatus->setObjectName("aiCredentialStatus");
    providerForm->addRow("凭据状态", m_credentialStatus);
    providerLayout->addLayout(providerForm);

    auto *networkRow = rowHost(providerCard);
    m_verify = button("验证连接", "aiProviderVerify", networkRow);
    m_refresh = button("刷新模型", "aiModelRefresh", networkRow);
    rowLayout(networkRow)->addWidget(m_verify);
    rowLayout(networkRow)->addWidget(m_refresh);
    rowLayout(networkRow)->addStretch(1);
    providerLayout->addWidget(networkRow);
    m_providerStatus = hint("需要先选择供应商并配置 API Key。连接验证只读取模型列表，不会请求翻译。", providerCard);
    m_providerStatus->setObjectName("aiProviderStatus");
    providerLayout->addWidget(m_providerStatus);
    bodyLayout->addWidget(providerCard);

    auto *modelCard = new QGroupBox("默认模型配置", body);
    modelCard->setObjectName("aiDefaultModelCard");
    auto *modelForm = new QFormLayout(modelCard);
    modelForm->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    modelForm->setRowWrapPolicy(QFormLayout::WrapLongRows);
    m_model = new QComboBox(modelCard);
    m_model->setObjectName("aiDefaultModel");
    m_model->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    m_model->setMinimumContentsLength(24);
    m_model->setToolTip("模型显示名称与实际请求 ID 分开保存。");
    modelForm->addRow("模型", m_model);
    m_thinking = new QComboBox(modelCard);
    m_thinking->setObjectName("aiDefaultThinking");
    modelForm->addRow("思考模式", m_thinking);
    m_modelCapabilities = hint("刷新模型后显示供应商提供的上下文、输出和思考能力。", modelCard);
    m_modelCapabilities->setObjectName("aiDefaultModelCapabilities");
    modelForm->addRow("能力", m_modelCapabilities);
    bodyLayout->addWidget(modelCard);

    auto *features = new QGroupBox("AI 功能", body);
    features->setObjectName("aiFeaturesCard");
    auto *featureLayout = new QVBoxLayout(features);
    auto *translation = button("歌词翻译设置", "aiLyricsTranslationSettings", features);
    translation->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Preferred);
    featureLayout->addWidget(translation, 0, Qt::AlignLeft);
    featureLayout->addWidget(hint("歌词翻译会使用独立配置（或全局默认），并在请求完成后先预览再保存。", features));
    bodyLayout->addWidget(features);
    bodyLayout->addStretch(1);

    m_homeScroll = nullptr;
    outer->addWidget(makeScrollPage(body, &m_homeScroll, m_home), 1);
    m_homeScroll->setObjectName("aiServiceScroll");
    connect(m_provider, &QComboBox::currentIndexChanged, this, [this](int) {
        if (m_populating || !m_runtime)
            return;
        cancelRefresh();
        auto configuration = m_runtime->configuration();
        configuration.defaults.providerId = m_provider->currentData().toString();
        if (configuration.defaults.providerId == DeepSeekProviderId && configuration.defaults.modelId.isEmpty())
            configuration.defaults.modelId = "deepseek-flash";
        if (configuration.translationUsesDefaults && configuration.translationOverride.providerId.isEmpty())
            configuration.translationOverride = configuration.defaults;
        saveConfiguration(configuration, m_providerStatus);
        updateCredentialStatus();
        updateModels();
    });
    connect(m_model, &QComboBox::currentIndexChanged, this, [this](int) {
        if (m_populating || !m_runtime)
            return;
        auto configuration = m_runtime->configuration();
        configuration.defaults.modelId = m_model->currentData().toString();
        saveConfiguration(configuration, m_providerStatus);
        updateThinkingOptions(m_thinking, configuration.defaults,
                              m_runtime->model(configuration.defaults.providerId, configuration.defaults.modelId));
        updateCapabilityText();
    });
    connect(m_thinking, &QComboBox::currentIndexChanged, this, [this](int) {
        if (m_populating || !m_runtime)
            return;
        auto configuration = m_runtime->configuration();
        setEffort(configuration.defaults, m_thinking->currentData().toString());
        saveConfiguration(configuration, m_providerStatus);
        if (!configuration.translationUsesDefaults)
            populate();
    });
    connect(m_saveKey, &QPushButton::clicked, this, [this] {
        if (!m_runtime) return;
        const auto providerId = m_provider->currentData().toString();
        auto secret = m_keyInput->text().trimmed();
        if (providerId.isEmpty()) {
            m_credentialStatus->setText("请先选择供应商");
            return;
        }
        if (secret.isEmpty()) {
            m_credentialStatus->setText("请输入 API Key");
            return;
        }
        QString error;
        const bool stored = m_runtime->credentials()->store(providerId, secret, &error);
        secret.fill(QChar());
        m_keyInput->clear();
        if (!stored) {
            m_credentialStatus->setText(error.isEmpty() ? "保存凭据失败" : error);
            return;
        }
        m_credentialStatus->setText("已安全保存（完整 Key 不会显示）");
        updateCredentialStatus();
    });
    connect(m_deleteKey, &QPushButton::clicked, this, [this] {
        if (!m_runtime) return;
        const auto providerId = m_provider->currentData().toString();
        if (providerId.isEmpty()) {
            m_credentialStatus->setText("请先选择供应商");
            return;
        }
        QString error;
        if (!m_runtime->credentials()->remove(providerId, &error)) {
            m_credentialStatus->setText(error.isEmpty() ? "删除凭据失败" : error);
            return;
        }
        m_keyInput->clear();
        m_credentialStatus->setText("已删除安全凭据");
        updateCredentialStatus();
    });
    connect(m_verify, &QPushButton::clicked, this, [this] { requestModelList("验证连接"); });
    connect(m_refresh, &QPushButton::clicked, this, [this] { requestModelList("刷新模型列表"); });
    connect(translation, &QPushButton::clicked, this, [this] { selectSubpage(true); });
}

void AiSettingsPage::buildTranslation() {
    m_translation = new QWidget(this);
    m_translation->setObjectName("aiLyricsTranslationSettingsPage");
    auto *outer = new QVBoxLayout(m_translation);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(12);
    QPushButton *back = nullptr;
    outer->addWidget(makeHeader("歌词翻译", "返回 AI 服务", "aiTranslationBack", &back, m_translation));
    connect(back, &QPushButton::clicked, this, [this] { selectSubpage(false); });

    auto *body = new QWidget;
    body->setObjectName("aiLyricsTranslationContent");
    auto *layout = new QVBoxLayout(body);
    layout->setContentsMargins(0, 0, 8, 4);
    layout->setSpacing(14);
    auto *configCard = new QGroupBox("功能配置", body);
    configCard->setObjectName("aiLyricsTranslationConfigCard");
    auto *form = new QFormLayout(configCard);
    form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    form->setRowWrapPolicy(QFormLayout::WrapLongRows);
    m_translationUsesDefaults = new QCheckBox("使用全局默认配置", configCard);
    m_translationUsesDefaults->setObjectName("aiTranslationUseGlobal");
    form->addRow("模型配置", m_translationUsesDefaults);
    m_translationProvider = new QComboBox(configCard);
    m_translationProvider->setObjectName("aiTranslationProvider");
    m_translationProvider->addItem("请选择供应商", QString());
    m_translationProvider->addItem("DeepSeek 官方 API", QString::fromLatin1(DeepSeekProviderId));
    form->addRow("供应商覆盖", m_translationProvider);
    m_translationModel = new QComboBox(configCard);
    m_translationModel->setObjectName("aiTranslationModel");
    m_translationModel->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    m_translationModel->setMinimumContentsLength(24);
    form->addRow("模型覆盖", m_translationModel);
    m_translationThinking = new QComboBox(configCard);
    m_translationThinking->setObjectName("aiTranslationThinking");
    form->addRow("思考模式覆盖", m_translationThinking);
    m_language = new QComboBox(configCard);
    m_language->setObjectName("aiTranslationLanguage");
    m_language->addItem("简体中文", "zh-CN");
    form->addRow("目标语言", m_language);
    layout->addWidget(configCard);

    auto *styleCard = new QGroupBox("翻译风格", body);
    styleCard->setObjectName("aiTranslationStyleCard");
    auto *styleLayout = new QVBoxLayout(styleCard);
    styleLayout->setSpacing(8);
    styleLayout->addWidget(hint("此处只调整表达风格。行 ID、JSON 结构、行数、时间轴和供应商安全边界由程序固定校验。", styleCard));
    m_stylePrompt = new QPlainTextEdit(styleCard);
    m_stylePrompt->setObjectName("aiTranslationStylePrompt");
    m_stylePrompt->setPlaceholderText(defaultTranslationStylePrompt());
    m_stylePrompt->setMinimumHeight(120);
    m_stylePrompt->setTabChangesFocus(true);
    styleLayout->addWidget(m_stylePrompt, 1);
    auto *styleActions = rowHost(styleCard);
    auto *restore = button("恢复默认提示词", "aiTranslationStyleReset", styleActions);
    rowLayout(styleActions)->addWidget(restore);
    rowLayout(styleActions)->addStretch(1);
    styleLayout->addWidget(styleActions);
    layout->addWidget(styleCard);
    layout->addWidget(hint("隐私与费用：用户启动翻译后，整首歌词文本和歌曲名、歌手会发送至已选择的供应商。每次只发起一条整首请求；取消只停止本地处理，供应商可能已收到请求。", body));
    m_translationStatus = hint("翻译设置保存在现有 Library 设置中。", body);
    m_translationStatus->setObjectName("aiTranslationSettingsStatus");
    layout->addWidget(m_translationStatus);
    layout->addStretch(1);
    m_translationScroll = nullptr;
    outer->addWidget(makeScrollPage(body, &m_translationScroll, m_translation), 1);
    m_translationScroll->setObjectName("aiTranslationSettingsScroll");

    connect(m_translationUsesDefaults, &QCheckBox::toggled, this, [this](bool useDefaults) {
        if (m_populating || !m_runtime)
            return;
        auto configuration = m_runtime->configuration();
        configuration.translationUsesDefaults = useDefaults;
        if (!useDefaults && configuration.translationOverride.providerId.isEmpty())
            configuration.translationOverride = configuration.defaults;
        saveConfiguration(configuration, m_translationStatus);
        populate();
    });
    connect(m_translationProvider, &QComboBox::currentIndexChanged, this, [this](int) {
        if (m_populating || !m_runtime)
            return;
        auto configuration = m_runtime->configuration();
        configuration.translationOverride.providerId = m_translationProvider->currentData().toString();
        if (configuration.translationOverride.providerId == DeepSeekProviderId && configuration.translationOverride.modelId.isEmpty())
            configuration.translationOverride.modelId = "deepseek-flash";
        saveConfiguration(configuration, m_translationStatus);
        updateModels();
    });
    connect(m_translationModel, &QComboBox::currentIndexChanged, this, [this](int) {
        if (m_populating || !m_runtime)
            return;
        auto configuration = m_runtime->configuration();
        configuration.translationOverride.modelId = m_translationModel->currentData().toString();
        saveConfiguration(configuration, m_translationStatus);
        updateThinkingOptions(m_translationThinking, configuration.translationOverride,
                              m_runtime->model(configuration.translationOverride.providerId,
                                               configuration.translationOverride.modelId), !configuration.translationUsesDefaults);
    });
    connect(m_translationThinking, &QComboBox::currentIndexChanged, this, [this](int) {
        if (m_populating || !m_runtime)
            return;
        auto configuration = m_runtime->configuration();
        setEffort(configuration.translationOverride, m_translationThinking->currentData().toString());
        saveConfiguration(configuration, m_translationStatus);
    });
    connect(m_language, &QComboBox::currentIndexChanged, this, [this](int) {
        if (m_populating || !m_runtime)
            return;
        auto configuration = m_runtime->configuration();
        configuration.targetLanguage = m_language->currentData().toString();
        saveConfiguration(configuration, m_translationStatus);
    });
    connect(m_stylePrompt, &QPlainTextEdit::textChanged, this, [this] {
        if (m_populating || !m_runtime)
            return;
        auto configuration = m_runtime->configuration();
        configuration.translationStylePrompt = m_stylePrompt->toPlainText();
        saveConfiguration(configuration, m_translationStatus);
    });
    connect(restore, &QPushButton::clicked, this, [this] {
        m_stylePrompt->setPlainText(defaultTranslationStylePrompt());
    });
}

void AiSettingsPage::populate() {
    if (!m_runtime)
        return;
    m_populating = true;
    const auto &configuration = m_runtime->configuration();
    {
        const QSignalBlocker blocker(m_provider);
        const auto index = m_provider->findData(configuration.defaults.providerId);
        m_provider->setCurrentIndex(index >= 0 ? index : 0);
    }
    {
        const QSignalBlocker blocker(m_translationUsesDefaults);
        m_translationUsesDefaults->setChecked(configuration.translationUsesDefaults);
    }
    {
        const QSignalBlocker blocker(m_translationProvider);
        const auto index = m_translationProvider->findData(configuration.translationOverride.providerId);
        m_translationProvider->setCurrentIndex(index >= 0 ? index : 0);
    }
    {
        const QSignalBlocker blocker(m_language);
        const auto index = m_language->findData(configuration.targetLanguage);
        m_language->setCurrentIndex(index >= 0 ? index : 0);
    }
    {
        const QSignalBlocker blocker(m_stylePrompt);
        m_stylePrompt->setPlainText(configuration.translationStylePrompt);
    }
    m_populating = false;
    updateCredentialStatus();
    updateModels();
    const bool overrideEnabled = !configuration.translationUsesDefaults;
    m_translationProvider->setEnabled(overrideEnabled);
}

void AiSettingsPage::updateCredentialStatus() {
    if (!m_runtime) return;
    const auto providerId = m_provider->currentData().toString();
    const bool selected = !providerId.isEmpty();
    m_keyInput->setEnabled(selected);
    m_saveKey->setEnabled(selected && !m_keyInput->text().trimmed().isEmpty());
    m_deleteKey->setEnabled(selected);
    m_verify->setEnabled(selected && !m_pendingRefresh);
    m_refresh->setEnabled(selected && !m_pendingRefresh);
    if (!selected) {
        m_credentialStatus->setText("未选择供应商");
        return;
    }
    QString error;
    const bool exists = m_runtime->hasCredential(providerId, &error);
    if (!error.isEmpty())
        m_credentialStatus->setText(error);
    else
        m_credentialStatus->setText(exists ? "已配置（完整 Key 已隐藏）" : "未配置");
}

void AiSettingsPage::updateModels() {
    if (!m_runtime)
        return;
    const auto configuration = m_runtime->configuration();
    const auto globalProvider = configuration.defaults.providerId;
    const auto globalModelId = configuration.defaults.modelId;
    const auto translation = configuration.translationOverride;
    const auto fill = [this](QComboBox *combo, const QString &providerId, const QString &selectedId) {
        const QSignalBlocker blocker(combo);
        combo->clear();
        combo->addItem(providerId.isEmpty() ? "先选择供应商" : "刷新模型列表后选择", QString());
        const auto choices = m_runtime->models(providerId);
        for (const auto &model : choices) {
            combo->addItem(modelDisplay(model), model.id);
            const auto index = combo->count() - 1;
            combo->setItemData(index, QString("%1\n请求 ID: %2").arg(model.displayName, model.id), Qt::ToolTipRole);
        }
        auto index = combo->findData(selectedId);
        if (!selectedId.isEmpty() && index < 0) {
            combo->addItem(selectedId + "（尚未验证）", selectedId);
            index = combo->count() - 1;
        }
        combo->setCurrentIndex(index >= 0 ? index : 0);
        combo->setEnabled(!providerId.isEmpty());
    };
    m_populating = true;
    fill(m_model, globalProvider, globalModelId);
    fill(m_translationModel, translation.providerId, translation.modelId);
    updateThinkingOptions(m_thinking, configuration.defaults,
                          m_runtime->model(globalProvider, globalModelId));
    updateThinkingOptions(m_translationThinking, translation,
                          m_runtime->model(translation.providerId, translation.modelId), !configuration.translationUsesDefaults);
    const bool overrideEnabled = !configuration.translationUsesDefaults;
    m_translationProvider->setEnabled(overrideEnabled);
    m_translationModel->setEnabled(overrideEnabled && !translation.providerId.isEmpty());
    m_populating = false;
    updateCapabilityText();
    const auto error = m_runtime->validateConfiguration(m_runtime->translationConfiguration());
    if (!error.isEmpty() && m_translationStatus->text().startsWith("模型列表已更新"))
        m_translationStatus->setText(error);
}

void AiSettingsPage::updateThinkingOptions(QComboBox *combo, const AiTaskConfiguration &selection,
                                            const ModelInfo *model, bool featureEnabled) {
    const QSignalBlocker blocker(combo);
    const auto requested = currentEffort(selection);
    combo->clear();
    combo->addItem("关闭思考", "off");
    if (model && model->capabilities.reasoningMetadataKnown) {
        for (const auto &effort : model->capabilities.reasoningEfforts)
            combo->addItem(effortLabel(effort), effort);
    }
    auto index = combo->findData(requested);
    if (index < 0 && requested != "off") {
        combo->insertItem(0, QString("需重新选择（%1）").arg(effortLabel(requested)), "invalid");
        index = 0;
        if (auto *modelObject = qobject_cast<QStandardItemModel *>(combo->model()))
            if (auto *item = modelObject->item(0))
                item->setEnabled(false);
    }
    combo->setCurrentIndex(index >= 0 ? index : 0);
    combo->setToolTip(model && model->capabilities.reasoningMetadataKnown
        ? QString("此模型支持：%1").arg(model->capabilities.reasoningEfforts.join(", "))
        : QStringLiteral("尚未确认此模型支持的思考强度"));
    combo->setEnabled(featureEnabled && model && model->availability == ModelAvailability::Available &&
                      model->protocol.supportsStructuredChat() && model->capabilities.reasoningMetadataKnown &&
                      !model->capabilities.reasoningEfforts.isEmpty());
}

void AiSettingsPage::updateCapabilityText() {
    const auto selection = m_runtime->configuration().defaults;
    const auto *model = m_runtime->model(selection.providerId, selection.modelId);
    if (!model) {
        m_modelCapabilities->setText("模型能力未知。刷新列表后才能验证并使用模型。");
        return;
    }
    QStringList parts;
    parts << modelAvailabilityText(model->availability);
    if (model->capabilities.contextWindow > 0)
        parts << QString("上下文 %1 tokens").arg(model->capabilities.contextWindow);
    if (model->capabilities.maxOutputTokens > 0)
        parts << QString("输出上限 %1 tokens").arg(model->capabilities.maxOutputTokens);
    if (model->capabilities.reasoningMetadataKnown)
        parts << (model->capabilities.reasoningEfforts.isEmpty()
                      ? QStringLiteral("未提供思考强度")
                      : QString("思考强度 %1").arg(model->capabilities.reasoningEfforts.join(" / ")));
    else
        parts << "思考能力未知";
    parts << (model->protocol.supportsStructuredChat()
                  ? QStringLiteral("请求协议已确认")
                  : QStringLiteral("请求协议未确认，暂不可调用"));
    m_modelCapabilities->setText(parts.join(" · "));
    m_modelCapabilities->setToolTip(model->id);
    m_model->setToolTip(QString("%1\n请求 ID：%2").arg(model->displayName, model->id));
}

void AiSettingsPage::saveConfiguration(const AiServiceConfiguration &configuration, QLabel *status) {
    QString error;
    if (!m_runtime->saveConfiguration(configuration, &error))
        status->setText(error.isEmpty() ? "保存 AI 服务设置失败" : error);
}

void AiSettingsPage::selectSubpage(bool translation) {
    m_stack->setCurrentWidget(translation ? m_translation : m_home);
    if (translation && m_translationScroll)
        m_translationScroll->verticalScrollBar()->setValue(0);
}

void AiSettingsPage::setRequestState(bool running, const QString &message) {
    m_verify->setEnabled(!running && !m_provider->currentData().toString().isEmpty());
    m_refresh->setEnabled(!running && !m_provider->currentData().toString().isEmpty());
    if (running) {
        m_providerStatus->setText(message);
        m_verify->setText("正在连接…");
        m_refresh->setText("正在刷新…");
    } else {
        m_verify->setText("验证连接");
        m_refresh->setText("刷新模型");
        updateCredentialStatus();
    }
}

void AiSettingsPage::requestModelList(const QString &purpose) {
    if (m_pendingRefresh || !m_runtime)
        return;
    const auto providerId = m_provider->currentData().toString();
    if (providerId.isEmpty()) {
        m_providerStatus->setText("请先明确选择供应商");
        return;
    }
    QString error;
    if (!m_runtime->hasCredential(providerId, &error)) {
        m_providerStatus->setText(error.isEmpty() ? "请先安全保存 API Key" : error);
        return;
    }
    setRequestState(true, purpose + "中…");
    m_pendingRefresh = m_runtime->requests()->fetchModels(providerId);
}

void AiSettingsPage::setSubpageForTest(const QString &name) {
    selectSubpage(name == "translation");
}
} // namespace relaxer::ai
