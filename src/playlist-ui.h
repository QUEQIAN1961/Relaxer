#pragma once
#include "widgets.h"
#include <QCheckBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QLabel>
#include <QPlainTextEdit>
#include <QVariantAnimation>
namespace relaxer {
class DescriptionPreview : public QWidget {
    Q_OBJECT
public:
    explicit DescriptionPreview(QWidget *parent=nullptr);
    void setText(const QString &text);
    void setSingleLine(bool enabled){if(m_singleLine!=enabled){m_singleLine=enabled;update();}}
    int contentHeight(int width) const;
    QStringList displayLines() const;
    bool isElided() const;
    int singleLineWidth() const {return fontMetrics().horizontalAdvance(m_text.isEmpty()?QString("未设置"):m_text.simplified());}
signals:
    void layoutChanged();
protected:
    void paintEvent(QPaintEvent *) override;
    void changeEvent(QEvent *) override;
private:
    QString m_text;
    bool m_singleLine=false;
    QStringList linesForWidth(int width, bool *elided=nullptr) const;
};
class PlaylistHeader : public QWidget {
    Q_OBJECT
public:
    explicit PlaylistHeader(QWidget *parent=nullptr);
    void setPlaylist(const Playlist &,const QString &root);
    void collapseTo(qreal target);
    qreal target() const {return m_target;}
    qreal progress() const {return m_progress;}
    Cover *cover() const { return m_cover; }
    QPushButton *options() const { return m_options; }
    QPushButton *up() const { return m_up; }
    QPushButton *down() const { return m_down; }
signals:
    void playRequested();
    void shuffleRequested();
    void tagsRequested();
    void descriptionRequested();
protected:
    void resizeEvent(QResizeEvent *) override;
    void showEvent(QShowEvent *) override;
    void changeEvent(QEvent *) override;
private:
    Cover *m_cover=nullptr;
    QWidget *m_rows[4]{};
    QPushButton *m_options,*m_up,*m_down,*m_descriptionLink,*m_tagsLink;
    QLabel *m_counts,*m_tags;
    DescriptionPreview *m_description=nullptr;
    QString m_tagText;
    bool m_arranging=false;
    qreal m_progress=0,m_target=0;
    QVariantAnimation m_animation;
    void arrange();
};
class PlaylistTagsDialog : public QDialog {
    Q_OBJECT
public:
    explicit PlaylistTagsDialog(const QStringList &tags,QWidget *parent=nullptr);
    QStringList selectedTags() const;
private:
    QList<QCheckBox *> m_checks;
    QLabel *m_count;
    QDialogButtonBox *m_buttons;
    void updateSelection();
};
class PlaylistDescriptionDialog : public QDialog {
    Q_OBJECT
public:
    explicit PlaylistDescriptionDialog(const QString &text,QWidget *parent=nullptr);
    QString description() const { return m_editor->toPlainText(); }
private:
    QPlainTextEdit *m_editor;
    QLabel *m_remaining;
    void updateText();
};
}
