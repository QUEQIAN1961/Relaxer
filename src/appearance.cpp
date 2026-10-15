#include "appearance.h"
#include "widgets.h"
#include <QBuffer>
#include <QButtonGroup>
#include <QSignalBlocker>
#include <QPainterPath>
#include <cmath>
#include <QTimer>
#include <QtMath>
#include <QCheckBox>
#include <QColorDialog>
#include <QComboBox>
#include <QCryptographicHash>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QImageReader>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPainter>
#include <QSaveFile>
#include <QScrollArea>
#include <QSet>
#include <QUuid>
#include <QVBoxLayout>
namespace relaxer {
QColor autoInk(const QColor &color) {
    return color.redF() * .2126 + color.greenF() * .7152 + color.blueF() * .0722 > .53 ? QColor("#25252b")
                                                                                       : QColor("#f0edf1");
}
QImage backgroundImage(const QString &path, int maximum) {
    if (path.isEmpty())
        return {};
    QImageReader reader(path);
    reader.setAutoTransform(true);
    auto size = reader.size();
    if (!size.isValid() || qint64(size.width()) * size.height() > 100000000)
        return {};
    if (qMax(size.width(), size.height()) > maximum)
        reader.setScaledSize(size.scaled(maximum, maximum, Qt::KeepAspectRatio));
    return reader.read();
}
QImage fixedGaussianBlur(QImage image, int strength) {
    if (image.isNull() || !strength)
        return image;
    image = image.scaled(640, 640, Qt::KeepAspectRatio, Qt::SmoothTransformation)
                .convertToFormat(QImage::Format_ARGB32);
    // Three separable box passes approximate a Gaussian in linear time.
    int radius = qMax(1, qRound(strength / 3.0)), width = image.width(), height = image.height();
    for (int pass = 0; pass < 6; ++pass) {
        QImage output(image.size(), image.format());
        bool horizontal = pass % 2 == 0;
        int lines = horizontal ? height : width, length = horizontal ? width : height;
        for (int line = 0; line < lines; ++line) {
            auto pixel = [&](int pos) {
                int x = horizontal ? qBound(0, pos, width - 1) : line;
                int y = horizontal ? line : qBound(0, pos, height - 1);
                return reinterpret_cast<const QRgb *>(image.constScanLine(y))[x];
            };
            int r = 0, g = 0, b = 0, a = 0, count = 2 * radius + 1;
            auto add = [&](QRgb p, int sign) {
                r += sign * qRed(p);
                g += sign * qGreen(p);
                b += sign * qBlue(p);
                a += sign * qAlpha(p);
            };
            for (int i = -radius; i <= radius; ++i)
                add(pixel(i), 1);
            for (int pos = 0; pos < length; ++pos) {
                int x = horizontal ? pos : line, y = horizontal ? line : pos;
                reinterpret_cast<QRgb *>(output.scanLine(y))[x] =
                    qRgba(r / count, g / count, b / count, a / count);
                add(pixel(pos - radius), -1);
                add(pixel(pos + radius + 1), 1);
            }
        }
        image = std::move(output);
    }
    return image;
}
double blurRadius(double percent) {
    const double x=qBound(0.,percent,50.)/50.;
    return 17.*std::expm1(3.*x)/std::expm1(3.);
}
QImage gaussianBlur(QImage image,double strength) {
    if(image.isNull()||strength<=0)return image;
    // Keep fine detail at low values. Fractional box radii avoid a jump at 0.1%.
    image=image.convertToFormat(QImage::Format_ARGB32);
    const double radius=blurRadius(strength)*qMax(image.width(),image.height())/640.;
    const int lo=qFloor(radius),hi=lo+1;const double fraction=radius-lo;
    for(int pass=0;pass<6;++pass){
        QImage output(image.size(),image.format());const bool horizontal=pass%2==0;
        const int lines=horizontal?image.height():image.width(),length=horizontal?image.width():image.height();
        for(int line=0;line<lines;++line){
            auto pixel=[&](int p){const int x=horizontal?qBound(0,p,image.width()-1):line,y=horizontal?line:qBound(0,p,image.height()-1);return reinterpret_cast<const QRgb *>(image.constScanLine(y))[x];};
            int sums[2][4]{};
            auto add=[](int *sum,QRgb c,int sign){sum[0]+=sign*qRed(c);sum[1]+=sign*qGreen(c);sum[2]+=sign*qBlue(c);sum[3]+=sign*qAlpha(c);};
            for(int j=-hi;j<=hi;++j){const auto c=pixel(j);add(sums[1],c,1);if(j>=-lo&&j<=lo)add(sums[0],c,1);}
            for(int j=0;j<length;++j){
                int c[4];for(int k=0;k<4;++k)c[k]=qBound(0,qRound((1-fraction)*sums[0][k]/(2*lo+1)+fraction*sums[1][k]/(2*hi+1)),255);
                const int x=horizontal?j:line,y=horizontal?line:j;
                reinterpret_cast<QRgb *>(output.scanLine(y))[x]=qRgba(c[0],c[1],c[2],c[3]);
                add(sums[0],pixel(j-lo),-1);add(sums[0],pixel(j+lo+1),1);
                add(sums[1],pixel(j-hi),-1);add(sums[1],pixel(j+hi+1),1);
            }
        }
        image=std::move(output);
    }
    return image;
}

namespace {
QLabel *caption(const QString &text) {
    auto *label = new QLabel(text);
    label->setTextFormat(Qt::PlainText);
    label->setWordWrap(true);
    return label;
}
QPushButton *control(const QString &text) {
    auto *button = new QPushButton(text);
    button->setCursor(Qt::PointingHandCursor);
    return button;
}
} // namespace
void Surface::setLook(const QColor &color, const QString &file, double blur, QWidget *shared) {
    m_color = color;
    m_shared = shared;
    blur = qBound(0., blur, 50.);
    if (file != m_file || blur != m_blur) {
        m_file = file;
        m_blur = blur;
        m_image = gaussianBlur(backgroundImage(file), blur);
        m_canvas = {};
        m_scaled = {};
    }
    update();
}
void Surface::paintEvent(QPaintEvent *) {
    QPainter painter(this);
    painter.fillRect(rect(), m_color);
    if (m_image.isNull())
        return;
    auto *canvas = m_shared ? m_shared.data() : this;
    if (m_canvas != canvas->size()) {
        m_canvas = canvas->size();
        m_scaled = QPixmap::fromImage(
            m_image.scaled(m_canvas, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation));
    }
    auto offset = m_shared ? mapTo(canvas, QPoint()) : QPoint();
    painter.drawPixmap((canvas->width() - m_scaled.width()) / 2 - offset.x(),
                       (canvas->height() - m_scaled.height()) / 2 - offset.y(), m_scaled);
}
QJsonObject AppearanceStore::defaults() {
    return {{"mode", "palette"},    {"left", "#f5f5f7"},     {"right", "#ffffff"}, {"accent", "#e5455f"},
            {"leftInk", "#25252b"}, {"rightInk", "#25252b"}, {"muted", "#86818a"}, {"customText", false},
            {"mutedBold", false},   {"shareLR", false},      {"blurLeft", 0},      {"blurRight", 0}, {"blurExpanded", 15}, {"customProgressColors", false},
            {"customFooter", false}, {"footer", "#f4f4f6"}, {"shareFooter", true}};
}
QJsonObject AppearanceStore::state() const {
    auto raw = m_library->setting("appearanceHistory").toObject();
    QJsonArray cleaned;
    QSet<QString> ids;
    for (auto item : raw["items"].toArray()) {
        auto theme = item.toObject();
        auto id = theme["id"].toString();
        if (id.isEmpty() || ids.contains(id) || (theme["mode"] != "palette" && theme["mode"] != "image"))
            continue;
        ids.insert(id);
        cleaned.append(theme);
    }
    return {{"items", cleaned}, {"active", raw["active"].toString()}};
}
QJsonObject AppearanceStore::active() const {
    auto current = state();
    for (auto item : current["items"].toArray())
        if (item.toObject()["id"] == current["active"])
            return item.toObject();
    return defaults();
}
QString AppearanceStore::imagePath(const QJsonObject &theme, const QString &region) const {
    auto key = region == "left" ? "imageLeft" : region == "footer" ? "imageFooter" : "imageRight";
    auto rel = theme[key].toString();
    if (!rel.startsWith("Appearance/") || QDir::cleanPath(rel) != rel)
        return {};
    auto path = QFileInfo(m_library->absolute(rel)).canonicalFilePath();
    auto directory = QFileInfo(m_library->absolute("Appearance")).canonicalFilePath();
    return !directory.isEmpty() && path.startsWith(directory + '/') ? path : QString();
}
QJsonObject AppearanceStore::draft(QJsonObject theme) const {
    auto defaults = AppearanceStore::defaults();
    for (auto it = defaults.begin(); it != defaults.end(); ++it)
        if (!theme.contains(it.key()))
            theme[it.key()] = it.value();
    theme["sourceLeft"] = imagePath(theme, "left");
    theme["sourceRight"] = imagePath(theme, "right");
    theme["sourceFooter"] = imagePath(theme, "footer");
    return theme;
}
QJsonObject AppearanceStore::withProgressColors(QJsonObject theme, bool refreshRecommendation) const {
    if (!theme.contains("progressColorsCustomized")) theme["progressColorsCustomized"]=theme["customProgressColors"].toBool();
    const auto valid = [&theme](const char *key) { return QColor(theme[key].toString()).isValid(); };
    if (refreshRecommendation || !valid("recommendedProgressFill") || !valid("recommendedProgressTrack")) {
        QColor background(theme["right"].toString());
        if (!background.isValid()) background = QColor("#ffffff");
        if (theme["mode"] == "image") {
            const QString region = theme["shareLR"].toBool() ? "Left" : "Right";
            auto path = theme["source" + region].toString();
            if (path.isEmpty()) path = imagePath(theme, region.toLower());
            const auto image = backgroundImage(path, 32).convertToFormat(QImage::Format_ARGB32);
            if (!image.isNull()) {
                double r=0,g=0,b=0;
                for (int y=0;y<image.height();++y) for (int x=0;x<image.width();++x) {
                    const auto pixel=image.pixelColor(x,y);const auto a=pixel.alphaF();
                    r+=pixel.red()*a+background.red()*(1-a);
                    g+=pixel.green()*a+background.green()*(1-a);
                    b+=pixel.blue()*a+background.blue()*(1-a);
                }
                const int count=image.width()*image.height();
                background=QColor(qRound(r/count),qRound(g/count),qRound(b/count));
            }
        }
        // The footer uses the right background's derived card color in both playback modes.
        const bool dark=autoInk(background).lightness()>128;
        background=dark?background.lighter(115):background.darker(103);
        const auto ink=autoInk(background);
        const auto mix=[&](double amount) {
            return QColor(qRound(ink.red()*amount+background.red()*(1-amount)),
                          qRound(ink.green()*amount+background.green()*(1-amount)),
                          qRound(ink.blue()*amount+background.blue()*(1-amount))).name();
        };
        theme["recommendedProgressFill"]=mix(.72);
        theme["recommendedProgressTrack"]=mix(.25);
    }
    if (!theme.contains("customProgressColors")) theme["customProgressColors"]=false;
    if (!theme["progressColorsCustomized"].toBool() || !valid("progressFill")) theme["progressFill"]=theme.value("recommendedProgressFill");
    if (!theme["progressColorsCustomized"].toBool() || !valid("progressTrack")) theme["progressTrack"]=theme.value("recommendedProgressTrack");
    return theme;
}
bool AppearanceStore::initializeProgressColors() {
    auto current=state();auto items=current["items"].toArray();bool changed=false;
    for (int i=0;i<items.size();++i) {
        const auto old=items[i].toObject(),updated=withProgressColors(old);
        if (old!=updated) { items[i]=updated;changed=true; }
    }
    if (!changed) return true;
    current["items"]=items;
    return m_library->setSetting("appearanceHistory",current);
}
bool AppearanceStore::save(QJsonObject theme, const QString &replaceId, QString *error) {
    QStringList created;
    auto fail = [&](const QString &message) {
        for (auto path : created)
            QFile::remove(path);
        if (error)
            *error = message;
        return false;
    };
    if (theme["mode"] != "palette" && theme["mode"] != "image")
        return fail("请选择调色盘或图片模式。");
    for (const auto &key : {"left", "right", "accent", "leftInk", "rightInk", "muted"}) {
        QColor color(theme[key].toString());
        if (!color.isValid())
            return fail("颜色无效，请重新选择。");
        theme[key] = color.name();
    }
    if (theme["mode"] == "palette" && theme["customFooter"].toBool()) {
        QColor footer(theme["footer"].toString());
        if (!footer.isValid())
            return fail("底栏颜色无效，请重新选择。");
        theme["footer"] = footer.name();
    }
    auto current = state();
    auto items = current["items"].toArray();
    int replace = -1;
    for (int i = 0; i < items.size(); ++i)
        if (items[i].toObject()["id"] == replaceId)
            replace = i;
    if (!replaceId.isEmpty() && replace < 0)
        return fail("要覆盖的主题已不存在。");
    theme["id"] = replace < 0 ? QUuid::createUuid().toString(QUuid::WithoutBraces) : replaceId;
    theme["name"] =
        replace < 0 ? QString("主题 %1").arg(items.size() + 1) : items[replace].toObject()["name"].toString();
    if (theme["mode"] == "image") {
        QMap<QString, QString> reused;
        QStringList regions{"Left", "Right"};
        if (!theme["shareFooter"].toBool())
            regions << "Footer";
        for (const auto &region : regions) {
            auto source =
                theme["source" + (region == "Right" && theme["shareLR"].toBool() ? QString("Left") : region)]
                    .toString();
            if (source.isEmpty())
                return fail(region == "Footer" ? "请选择底栏背景图片。" : "请选择左侧和右侧背景图片。");
            auto image = backgroundImage(source);
            if (image.isNull())
                return fail("背景图片打不开或尺寸过大：" + source);
            QString rel = reused.value(source);
            if (rel.isEmpty()) {
                QByteArray bytes;
                QBuffer buffer(&bytes);
                buffer.open(QIODevice::WriteOnly);
                if (!image.save(&buffer, "PNG"))
                    return fail("背景图片编码失败。");
                rel =
                    "Appearance/" +
                    QString::fromLatin1(QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex()) +
                    ".png";
                if (!QDir().mkpath(m_library->absolute("Appearance")))
                    return fail("无法创建背景图片目录。");
                auto destination = m_library->absolute(rel);
                if (!QFile::exists(destination)) {
                    QSaveFile file(destination);
                    if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size() ||
                        !file.commit())
                        return fail("背景图片保存失败。");
                    created << destination;
                }
                reused[source] = rel;
            }
            theme["image" + region] = rel;
        }
    } else {
        theme.remove("imageLeft");
        theme.remove("imageRight");
        theme.remove("imageFooter");
    }
    theme=withProgressColors(theme,true);
    theme.remove("sourceLeft");
    theme.remove("sourceRight");
    theme.remove("sourceFooter");
    theme["blurLeft"] = qBound(0., theme["blurLeft"].toDouble(), 50.);
    theme["blurRight"] = qBound(0., theme["blurRight"].toDouble(), 50.);
    theme["blurExpanded"] = qBound(0., theme["blurExpanded"].toDouble(15), 50.);
    if (replace < 0)
        items.append(theme);
    else
        items[replace] = theme;
    current["items"] = items;
    current["active"] = theme["id"];
    if (!m_library->setSettings({{"appearanceHistory", current}, {"theme", 3}}))
        return fail(m_library->lastError());
    return true;
}
bool AppearanceStore::use(const QString &id) {
    auto current = state();
    for (auto theme : current["items"].toArray())
        if (theme.toObject()["id"] == id) {
            current["active"] = id;
            return m_library->setSettings({{"appearanceHistory", current}, {"theme", 3}});
        }
    return false;
}
bool AppearanceStore::rename(const QString &id, const QString &name) {
    if (name.trimmed().isEmpty())
        return false;
    auto current = state();
    auto items = current["items"].toArray();
    for (int i = 0; i < items.size(); ++i)
        if (items[i].toObject()["id"] == id) {
            auto theme = items[i].toObject();
            theme["name"] = name.trimmed().left(80);
            items[i] = theme;
            current["items"] = items;
            return m_library->setSetting("appearanceHistory", current);
        }
    return false;
}
bool AppearanceStore::remove(const QString &id) {
    auto current = state();
    QJsonArray items;
    bool found = false;
    for (auto theme : current["items"].toArray()) {
        if (theme.toObject()["id"] == id)
            found = true;
        else
            items.append(theme);
    }
    if (!found)
        return false;
    current["items"] = items;
    QJsonObject changes{{"appearanceHistory", current}};
    if (current["active"] == id) {
        current["active"] = "";
        changes["appearanceHistory"] = current;
        changes["theme"] = 0;
    }
    // Keep unreferenced image files; removing a theme never deletes originals.
    return m_library->setSettings(changes);
}
AppearancePage::AppearancePage(Library *library, std::function<void(const QJsonObject &)> preview,
                               std::function<void()> restore, QWidget *parent, QJsonObject initial)
    : QWidget(parent), m_library(library), m_store(library),
      m_draft(initial.isEmpty() ? m_store.draft(m_store.active()) : initial),
      m_active(initial.isEmpty() ? m_store.state()["active"].toString() : initial["id"].toString()),
      m_preview(std::move(preview)), m_restore(std::move(restore)) {
    setObjectName("appearancePage");
    m_draft=m_store.withProgressColors(m_draft);
    const int theme = qBound(0, library->setting("theme", 0).toInt(), 3);
    m_mode = theme < 3 ? theme : m_draft["mode"] == "image" ? 4 : 3;
    m_previewTimer = new QTimer(this);
    m_previewTimer->setSingleShot(true);
    m_previewTimer->setInterval(100);
    connect(m_previewTimer, &QTimer::timeout, this, &AppearancePage::previewDraft);
    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(34, 28, 34, 28);
    outer->setSpacing(18);
    auto *top = new QHBoxLayout;
    auto *back = control("返回设置");
    back->setObjectName("appearanceBack");
    back->setIcon(symbol("back", palette().color(QPalette::WindowText)));
    top->addWidget(back);
    auto *heading = caption("自定义外观");
    heading->setObjectName("profileName");
    top->addWidget(heading);
    top->addStretch();
    m_newSave = control("新建并保存");
    m_overwrite = control("覆盖并保存");
    m_savePreset = control("保存并退出");
    m_newSave->setObjectName("appearanceNewSave");
    m_overwrite->setObjectName("appearanceOverwriteSave");
    m_savePreset->setObjectName("appearanceSavePreset");
    for(auto *button:QList<QPushButton *>{back,m_newSave,m_overwrite,m_savePreset})button->setProperty("toolbarRole",true);
    top->addWidget(m_newSave);
    top->addWidget(m_overwrite);
    top->addWidget(m_savePreset);
    outer->addLayout(top);
    auto *modes = new QHBoxLayout;
    auto *group = new QButtonGroup(this);
    const QStringList modeNames{"浅色", "深色", "跟随系统", "调色盘", "图片"};
    QVector<QPushButton *> modeButtons;
    for (int i = 0; i < modeNames.size(); ++i) {
        auto *button = control(modeNames[i]);
        button->setObjectName("appearanceMode" + QString::number(i));
        button->setProperty("appearanceModeButton", true);
        button->setCheckable(true);
        button->setChecked(i == m_mode);
        group->addButton(button, i);
        modeButtons.append(button);
        modes->addWidget(button, 1);
    }
    outer->addLayout(modes);
    auto *scroll = new QScrollArea;
    scroll->setObjectName("appearanceScroll");
    scroll->setWidgetResizable(true);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setFrameShape(QFrame::NoFrame);
    auto *body = new QWidget;
    auto *layout = new QVBoxLayout(body);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(18);
    auto *custom = new QWidget;
    custom->setObjectName("appearanceCustomOptions");
    auto *options = new QVBoxLayout(custom);
    options->setContentsMargins(0, 0, 0, 0);
    options->setSpacing(16);
    auto *colorRow = new QHBoxLayout;
    QMap<QString, QPushButton *> swatches;
    auto updateSwatches = [this, &swatches] {
        for (auto it = swatches.begin(); it != swatches.end(); ++it) {
            const auto key=it.key();
            const auto effective=!m_draft["customProgressColors"].toBool()&&key=="progressFill"?QString("recommendedProgressFill"):
                                 !m_draft["customProgressColors"].toBool()&&key=="progressTrack"?QString("recommendedProgressTrack"):key;
            const QColor color(m_draft[effective].toString());
            it.value()->setText(it.value()->property("swatchName").toString() + "  " + color.name());
            it.value()->setStyleSheet("background:" + color.name() + ";color:" + autoInk(color).name() + ";");
        }
    };
    auto addColor = [&](const QString &key, const QString &name, QHBoxLayout *row) {
        auto *button = control(name);
        button->setObjectName("appearanceColor-" + key);
        button->setProperty("swatchName", name);
        swatches[key] = button;
        row->addWidget(button, 1);
        connect(button, &QPushButton::clicked, this, [this, key, button, name] {
            const auto color = QColorDialog::getColor(QColor(m_draft[key].toString()), this,
                                                       "选择颜色", QColorDialog::DontUseNativeDialog);
            if (!color.isValid()) return;
            m_draft[key] = color.name();
            if(key=="progressFill"||key=="progressTrack")m_draft["progressColorsCustomized"]=true;
            button->setText(name + "  " + color.name());
            button->setStyleSheet("background:" + color.name() + ";color:" + autoInk(color).name() + ";");
            m_dirty = true;
            previewDraft();
        });
    };
    addColor("left", "左侧底色", colorRow);
    addColor("right", "右侧底色", colorRow);
    addColor("accent", "主题色", colorRow);
    options->addLayout(colorRow);
    auto *customText = new QCheckBox("自定义文字颜色");
    customText->setChecked(m_draft["customText"].toBool());
    options->addWidget(customText);
    auto *ink = new QWidget;
    auto *inkRow = new QHBoxLayout(ink);
    inkRow->setContentsMargins(0, 0, 0, 0);
    addColor("leftInk", "左侧文字", inkRow);
    addColor("rightInk", "右侧主要文字", inkRow);
    addColor("muted", "右侧小字", inkRow);
    ink->setVisible(customText->isChecked());
    options->addWidget(ink);
    auto *customProgress = new QCheckBox("自定义进度条颜色");
    customProgress->setObjectName("appearanceCustomProgressColors");
    customProgress->setChecked(m_draft["customProgressColors"].toBool());
    options->addWidget(customProgress);
    auto *progressOptions = new QWidget;
    progressOptions->setObjectName("appearanceProgressColors");
    auto *progressRow = new QHBoxLayout(progressOptions);
    progressRow->setContentsMargins(0, 0, 0, 0);
    addColor("progressFill", "已填充与滑块", progressRow);
    addColor("progressTrack", "未填充", progressRow);
    options->addWidget(progressOptions);
    options->addWidget(caption("同时用于歌曲进度条和音量条；未勾选时使用系统推荐颜色。"));
    progressOptions->setEnabled(customProgress->isChecked());
    connect(customProgress, &QCheckBox::toggled, this, [this, progressOptions](bool on) {
        m_draft["customProgressColors"]=on;
        progressOptions->setEnabled(on);
        m_dirty=true;
        previewDraft();
    });
    auto *customFooter=new QCheckBox("自定义底栏颜色");
    customFooter->setObjectName("appearanceCustomFooter");
    customFooter->setChecked(m_draft["customFooter"].toBool());
    options->addWidget(customFooter);
    auto *footerOptions=new QWidget;
    footerOptions->setObjectName("appearanceFooterColors");
    auto *footerRow=new QHBoxLayout(footerOptions);
    footerRow->setContentsMargins(0,0,0,0);
    addColor("footer","底栏颜色",footerRow);
    options->addWidget(footerOptions);
    footerOptions->setEnabled(customFooter->isChecked());
    connect(customFooter,&QCheckBox::toggled,this,[this,footerOptions](bool on){m_draft["customFooter"]=on;footerOptions->setEnabled(on);m_dirty=true;previewDraft();});
    updateSwatches();
    auto *bold = new QCheckBox("右侧小字加粗");
    bold->setChecked(m_draft["mutedBold"].toBool());
    options->addWidget(bold);
    auto *images = new QWidget;
    images->setObjectName("appearanceImageOptions");
    auto *imageLayout = new QVBoxLayout(images);
    imageLayout->setContentsMargins(0, 0, 0, 0);
    auto *shared = new QCheckBox("左右同一张（连续铺图）");
    shared->setObjectName("appearanceShareLR");
    shared->setChecked(m_draft["shareLR"].toBool());
    imageLayout->addWidget(shared);
    QMap<QString, QLineEdit *> edits;
    QWidget *rightRow = nullptr;
    for (const auto &region : {QString("Left"), QString("Right")}) {
        auto *host = new QWidget;
        auto *row = new QHBoxLayout(host);
        row->setContentsMargins(0, 0, 0, 0);
        row->addWidget(caption(region == "Left" ? "左侧图片" : "右侧图片"));
        auto *edit = new QLineEdit(m_draft["source" + region].toString());
        edit->setObjectName("appearanceImage" + region);
        edits[region] = edit;
        row->addWidget(edit, 1);
        auto *choose = control("选择");
        row->addWidget(choose);
        connect(choose, &QPushButton::clicked, this, [this, edit] {
            const auto path = QFileDialog::getOpenFileName(this, "选择背景图片", {},
                                                           "图片 (*.png *.jpg *.jpeg *.webp *.bmp)");
            if (!path.isEmpty()) edit->setText(path);
        });
        connect(edit, &QLineEdit::textChanged, this, [this, region](const QString &path) {
            m_draft["source" + region] = path;
            m_dirty = true;
            previewDraft();
        });
        imageLayout->addWidget(host);
        if (region == "Right") rightRow = host;
    }
    rightRow->setVisible(!shared->isChecked());
    auto *shareFooter=new QCheckBox("底栏与其他区域同一张（连续铺图）");
    shareFooter->setObjectName("appearanceShareFooter");
    shareFooter->setChecked(!m_draft.contains("shareFooter")||m_draft["shareFooter"].toBool());
    imageLayout->addWidget(shareFooter);
    auto *footerImage=new QWidget;footerImage->setObjectName("appearanceFooterImage");
    auto *footerImageRow=new QHBoxLayout(footerImage);footerImageRow->setContentsMargins(0,0,0,0);
    footerImageRow->addWidget(caption("底栏图片"));
    auto *footerEdit=new QLineEdit(m_draft["sourceFooter"].toString());footerEdit->setObjectName("appearanceImageFooter");edits["Footer"]=footerEdit;footerImageRow->addWidget(footerEdit,1);
    auto *footerChoose=control("选择");footerImageRow->addWidget(footerChoose);
    connect(footerChoose,&QPushButton::clicked,this,[footerEdit]{const auto path=QFileDialog::getOpenFileName(footerEdit,"选择底栏图片",{},"图片 (*.png *.jpg *.jpeg *.webp *.bmp)");if(!path.isEmpty())footerEdit->setText(path);});
    connect(footerEdit,&QLineEdit::textChanged,this,[this](const QString &path){m_draft["sourceFooter"]=path;m_dirty=true;previewDraft();});
    imageLayout->addWidget(footerImage);
    footerImage->setVisible(!shareFooter->isChecked());
    options->addWidget(images);
    auto *blurRow = new QWidget;
    blurRow->setObjectName("appearanceBlurRow");
    auto *blurLayout = new QHBoxLayout(blurRow);
    blurLayout->setContentsMargins(0, 0, 0, 0);
    blurLayout->setSpacing(16);
    QMap<QString, BlurPercentSpin *> spins;
    const QStringList blurNames{"左侧列表模糊", "右侧列表模糊", "展开播放时背景模糊度"};
    const QStringList blurKeys{"Left", "Right", "Expanded"};
    for (int i = 0; i < blurKeys.size(); ++i) {
        const auto key = blurKeys[i];
        auto *host = new QWidget;
        host->setObjectName("appearanceBlurGroup" + key);
        auto *column = new QVBoxLayout(host);
        column->setContentsMargins(0, 0, 0, 0);
        column->addWidget(caption(blurNames[i]));
        auto *row = new QHBoxLayout;
        auto *slider = new Slider;
        slider->setRange(0, 500);
        slider->setObjectName("appearanceBlur" + key);
        slider->setMinimumWidth(36);
        auto *spin = new BlurPercentSpin;
        spin->setObjectName("appearanceBlurValue" + key);
        spin->setDecimals(1);
        spin->setRange(0, 50);
        spin->setSingleStep(.1);
        spin->setSuffix(" %");
        spin->setFixedWidth(94);
        spin->setValue(m_draft["blur" + key].toDouble(key == "Expanded" ? 15 : 0));
        slider->setValue(qRound(spin->value() * 10));
        spins[key] = spin;
        row->addWidget(slider, 1);
        row->addWidget(spin);
        column->addLayout(row);
        blurLayout->addWidget(host, 1);
        connect(slider, &QSlider::valueChanged, spin, [spin](int value) { spin->setValue(value / 10.); });
        connect(spin, &QDoubleSpinBox::valueChanged, slider, [slider](double value) { slider->setValue(qRound(value * 10)); });
        connect(spin, &QDoubleSpinBox::valueChanged, this, [this, key](double value) {
            m_draft["blur" + key] = value;
            m_dirty = true;
            m_previewTimer->start();
        });
    }
    options->addWidget(blurRow);
    auto *reset = control("重置当前配置");
    reset->setObjectName("appearanceReset");
    options->addWidget(reset);
    layout->addWidget(custom);
    const auto updateMode = [this, custom, images, modeButtons] {
        custom->setVisible(m_mode >= 3);
        images->setVisible(m_mode == 4);
        m_newSave->setVisible(m_mode >= 3);
        m_overwrite->setVisible(m_mode >= 3);
        m_overwrite->setEnabled(!m_active.isEmpty());
        m_savePreset->setVisible(m_mode < 3);
        for (int i = 0; i < modeButtons.size(); ++i) modeButtons[i]->setChecked(i == m_mode);
        setProperty("appearanceMode", m_mode);
    };
    updateMode();
    for (int i = 0; i < modeButtons.size(); ++i) {
        connect(modeButtons[i], &QPushButton::clicked, this, [this, i, updateMode] {
            if (i == m_mode) return;
            m_mode = i;
            if (i >= 3) m_draft["mode"] = i == 4 ? "image" : "palette";
            m_dirty = true;
            updateMode();
            previewDraft();
        });
    }
    connect(shared, &QCheckBox::toggled, this, [this, rightRow](bool value) {
        m_draft["shareLR"] = value;
        rightRow->setVisible(!value);
        m_dirty = true;
        previewDraft();
    });
    connect(shareFooter,&QCheckBox::toggled,this,[this,footerImage](bool value){m_draft["shareFooter"]=value;footerImage->setVisible(!value);m_dirty=true;previewDraft();});
    connect(customText, &QCheckBox::toggled, this, [this, ink](bool value) {
        m_draft["customText"] = value;
        ink->setVisible(value);
        m_dirty = true;
        previewDraft();
    });
    connect(bold, &QCheckBox::toggled, this, [this](bool value) {
        m_draft["mutedBold"] = value;
        m_dirty = true;
        previewDraft();
    });
    connect(reset, &QPushButton::clicked, this, [this, edits, spins, customText, bold, shared,
                                               swatches, updateMode, ink, rightRow, customProgress, progressOptions] {
        const auto id = m_draft.value("id"), name = m_draft.value("name");
        m_draft = m_store.withProgressColors(AppearanceStore::defaults());
        m_draft["id"] = id;
        m_draft["name"] = name;
        m_mode = 3;
        updateMode();
        { QSignalBlocker block(customProgress); customProgress->setChecked(false); }
        progressOptions->setEnabled(false);
        if(auto *footer=findChild<QCheckBox *>("appearanceCustomFooter")){QSignalBlocker block(footer);footer->setChecked(false);}
        if(auto *colors=findChild<QWidget *>("appearanceFooterColors"))colors->setEnabled(false);
        if(auto *footer=findChild<QWidget *>("appearanceFooterImage"))footer->setVisible(false);
        if(auto *share=findChild<QCheckBox *>("appearanceShareFooter")){QSignalBlocker block(share);share->setChecked(true);}
        { QSignalBlocker block(customText); customText->setChecked(false); }
        { QSignalBlocker block(bold); bold->setChecked(false); }
        { QSignalBlocker block(shared); shared->setChecked(false); }
        for (auto *edit : edits) { QSignalBlocker block(edit); edit->clear(); }
        for (auto it = spins.begin(); it != spins.end(); ++it) {
            const QSignalBlocker block(it.value());
            it.value()->setValue(it.key() == "Expanded" ? 15 : 0);
            findChild<Slider *>("appearanceBlur" + it.key())->setValue(qRound(it.value()->value() * 10));
        }
        for (auto it = swatches.begin(); it != swatches.end(); ++it) {
            const auto key=it.key();
            const auto effective=!m_draft["customProgressColors"].toBool()&&key=="progressFill"?QString("recommendedProgressFill"):
                                 !m_draft["customProgressColors"].toBool()&&key=="progressTrack"?QString("recommendedProgressTrack"):key;
            const QColor color(m_draft[effective].toString());
            it.value()->setText(it.value()->property("swatchName").toString() + "  " + color.name());
            it.value()->setStyleSheet("background:" + color.name() + ";color:" + autoInk(color).name() + ";");
        }
        ink->setVisible(false);
        rightRow->setVisible(true);
        m_dirty = true;
        previewDraft();
    });
    layout->addWidget(caption("已保存的外观"));
    auto *history = new QWidget;
    history->setObjectName("appearanceHistory");
    m_historyLayout = new QVBoxLayout(history);
    m_historyLayout->setContentsMargins(0, 0, 0, 0);
    m_historyLayout->setSpacing(12);
    layout->addWidget(history);
    rebuildHistory();
    layout->addStretch();
    scroll->setWidget(body);
    outer->addWidget(scroll, 1);
    connect(back, &QPushButton::clicked, this, [this] { if (confirmLeave()) emit finished(); });
    connect(m_newSave, &QPushButton::clicked, this, [this] { if (save()) emit finished(); });
    connect(m_overwrite, &QPushButton::clicked, this, [this] { if (save(m_active)) emit finished(); });
    connect(m_savePreset, &QPushButton::clicked, this, [this] { if (save()) emit finished(); });
}
void AppearancePage::previewDraft() {
    m_draft=m_store.withProgressColors(m_draft,true);
    const bool custom=m_draft["customProgressColors"].toBool();
    for (const auto &key : {QString("progressFill"),QString("progressTrack")}) {
        auto *button=findChild<QPushButton *>("appearanceColor-"+key);
        if (!button) continue;
        const auto recommended=key=="progressFill"?"recommendedProgressFill":"recommendedProgressTrack";
        const QColor color(m_draft[custom?key:QString(recommended)].toString());
        button->setText(button->property("swatchName").toString()+"  "+color.name());
        button->setStyleSheet("background:"+color.name()+";color:"+autoInk(color).name()+";");
    }
    auto draft = m_draft;
    draft["preset"] = m_mode < 3 ? m_mode : 3;
    m_preview(draft);
}
bool AppearancePage::save(const QString &replaceId) {
    m_previewTimer->stop();
    QString error;
    const bool saved = m_mode < 3 ? m_library->setSetting("theme", m_mode)
                                 : m_store.save(m_draft, replaceId, &error);
    if (!saved) {
        QMessageBox::warning(this, "外观未保存", error.isEmpty() ? m_library->lastError() : error);
        return false;
    }
    m_dirty = false;
    m_restore();
    return true;
}
bool AppearancePage::confirmLeave() {
    if (m_dirty) {
        QMessageBox question(QMessageBox::Question, "外观尚未保存", "修改尚未保存，是否退出？",
                             QMessageBox::NoButton, this);
        question.setObjectName("appearanceUnsavedPrompt");
        auto *discard = question.addButton("不保存", QMessageBox::DestructiveRole);
        auto *saveButton = question.addButton("保存", QMessageBox::AcceptRole);
        question.addButton("取消", QMessageBox::RejectRole);
        question.setDefaultButton(saveButton);
        question.exec();
        if (question.clickedButton() == saveButton) return save();
        if (question.clickedButton() != discard) return false;
    }
    m_previewTimer->stop();
    m_dirty = false;
    m_restore();
    return true;
}
void AppearancePage::rebuildHistory() {
    while (auto *item = m_historyLayout->takeAt(0)) {
        delete item->widget();
        delete item;
    }
    for (const auto value : m_store.state()["items"].toArray()) {
        const auto theme = value.toObject();
        const auto id = theme["id"].toString();
        auto *host = new QWidget;
        host->setObjectName("appearanceHistoryItem");
        auto *row = new QHBoxLayout(host);
        row->setContentsMargins(10, 8, 10, 8);
        row->setSpacing(8);
        auto *name = caption(theme["name"].toString() + (theme["mode"] == "image" ? " · 图片" : " · 调色盘"));
        name->setMinimumWidth(0);
        name->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
        row->addWidget(name, 1);
        QSet<QString> shownImages;
        if (theme["mode"] == "image") for (const auto &region : {QString("left"), QString("right")}) {
            const auto path = m_store.imagePath(theme, theme["shareLR"].toBool() ? "left" : region);
            if (path.isEmpty() || shownImages.contains(path)) continue;
            shownImages.insert(path);
            auto *image = new QLabel;
            image->setObjectName("appearanceHistoryImage");
            image->setToolTip(region == "left" ? "左侧图片" : "右侧图片");
            image->setFixedSize(64, 42);
            const auto scaled = backgroundImage(path, 160).scaled(64, 42, Qt::KeepAspectRatioByExpanding,
                                                                 Qt::SmoothTransformation);
            QPixmap preview(64, 42);
            preview.fill(Qt::transparent);
            QPainter painter(&preview);
            painter.setRenderHint(QPainter::Antialiasing);
            QPainterPath mask;
            mask.addRoundedRect(preview.rect(), 5, 5);
            painter.setClipPath(mask);
            painter.drawImage(QPoint((64 - scaled.width()) / 2, (42 - scaled.height()) / 2), scaled);
            painter.end();
            image->setPixmap(preview);
            row->addWidget(image);
        }
        host->setProperty("historyImageCount", shownImages.size());
        for (const auto &key : {QString("left"), QString("right"), QString("accent")}) {
            auto *swatch = new QLabel;
            swatch->setObjectName("appearanceHistoryColor-" + key);
            swatch->setFixedSize(22, 34);
            const QColor color(theme[key].toString());
            const QString title = key == "left" ? "左侧底色" : key == "right" ? "右侧底色" : "主题色";
            swatch->setToolTip(title + "  " + color.name());
            swatch->setStyleSheet("background:" + color.name() + ";border:1px solid #808080;border-radius:4px;");
            row->addWidget(swatch);
        }
        auto *use = control("使用"), *rename = control("重命名"), *remove = control("删除");
        for (auto *button : {use, rename, remove}) {
            button->setProperty("appearanceHistoryAction", true);
            row->addWidget(button);
        }
        connect(use, &QPushButton::clicked, this, [this, id] {
            if (!m_store.use(id)) { QMessageBox::warning(this, "外观未应用", m_library->lastError()); return; }
            m_previewTimer->stop();
            m_dirty = false;
            m_restore();
            emit finished();
        });
        connect(rename, &QPushButton::clicked, this, [this, id, theme] {
            bool ok = false;
            const auto name = QInputDialog::getText(this, "重命名外观", "名称", QLineEdit::Normal,
                                                   theme["name"].toString(), &ok);
            if (!ok || name.trimmed().isEmpty()) return;
            if (m_store.rename(id, name)) rebuildHistory();
            else QMessageBox::warning(this, "外观未重命名", m_library->lastError());
        });
        connect(remove, &QPushButton::clicked, this, [this, id] {
            if (!m_store.remove(id)) { QMessageBox::warning(this, "外观未删除", m_library->lastError()); return; }
            m_active = m_store.state()["active"].toString();
            m_overwrite->setEnabled(!m_active.isEmpty());
            m_restore();
            if (m_dirty) previewDraft();
            rebuildHistory();
        });
        m_historyLayout->addWidget(host);
    }
}
} // namespace relaxer
