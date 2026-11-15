import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import QtQuick.Window
import "../components"
import "../theme"
import "." as Pages

ColumnLayout {
    id: page
    objectName: "settingsPage"
    signal aiRequested()
    signal platformsRequested()
    signal appearanceRequested()
    signal shortcutsRequested()
    signal versionLogRequested()
    Layout.fillWidth: true
    Layout.fillHeight: true
    Layout.minimumWidth: 0
    Layout.minimumHeight: 0
    spacing: 0

    UiText { objectName:"settingsHeading";Layout.bottomMargin:16;text: "设置"; color: Theme.text; basePixelSize: 30; font.weight: Font.DemiBold }
    Rectangle { objectName:"settingsHeadingDivider";Layout.fillWidth: true; height: 1; color: Theme.border }
    ScrollView {
        id: scroll
        objectName: "settingsScroll"
        Layout.fillWidth: true
        Layout.fillHeight: true
        contentWidth: availableWidth
        contentHeight: settingsContent.implicitHeight + 32
        clip: true
        function stopAtEdges() { if (contentItem && contentItem.boundsBehavior !== undefined) { contentItem.boundsBehavior = Flickable.StopAtBounds; contentItem.boundsMovement = Flickable.StopAtBounds } }
        Component.onCompleted: Qt.callLater(stopAtEdges)
        onContentItemChanged: Qt.callLater(stopAtEdges)
        SmoothWheelHandler { flickable: scroll.contentItem as Flickable }
        ColumnLayout {
            id: settingsContent;y:16
            width: scroll.availableWidth
            height: implicitHeight
            spacing: 18
            SettingsCard {title:"外观";objectName:"settingsAppearanceCard"
                    SettingsGroup {
                        title:"主题与颜色";objectName:"settingsAppearanceGroup"
                        SettingsRow {
                            id:themeRow
                            objectName:"settingsAppearanceCell";label:"主题与配色";optionLabels:["浅色","深色","跟随系统","自定义"]
                            function chooseTheme(index){if(index===3){settingsBridge.beginAppearance();page.appearanceRequested()}else settingsBridge.setTheme(index)}
                            ChoiceBox {
                                visible:!themeRow.optionsInline
                                objectName:"qmlThemeSetting";width:Math.min(260,Math.max(120,parent.width-76))
                                model:["浅色","深色","跟随系统","自定义"];currentIndex:settingsBridge.values.theme
                                onActivated:themeRow.chooseTheme(currentIndex)
                            }
                            Repeater {
                                model:["浅色","深色","跟随系统","自定义"]
                                ShellButton {
                                    required property int index;required property string modelData
                                    objectName:"themeOption-"+index;visible:themeRow.optionsInline;text:modelData
                                    primary:settingsBridge.values.theme===index
                                    Accessible.role:Accessible.RadioButton;Accessible.checked:primary
                                    onClicked:themeRow.chooseTheme(index)
                                }
                            }
                            ShellButton {objectName:"editAppearance";visible:!themeRow.optionsInline;text:"编辑";onClicked:{settingsBridge.beginAppearance();page.appearanceRequested()}}
                        }
                        SettingsRow {id:lyricColorRow;label:"歌词颜色";optionLabels:["跟随文字颜色","自定义"]
                            ShellButton {objectName:"lyricColorSetting";visible:!lyricColorRow.optionsInline;text:"编辑颜色";onClicked:lyricColorEditor.open()}
                            ShellButton {objectName:"lyricColorFollowSetting";visible:lyricColorRow.optionsInline;text:"跟随文字颜色";primary:!settingsBridge.values.lyricColor;onClicked:settingsBridge.setPreference("lyricColor","")}
                            ShellButton {objectName:"lyricColorCustomSetting";visible:lyricColorRow.optionsInline;text:"自定义";primary:!!settingsBridge.values.lyricColor;onClicked:settingsBridge.chooseLyricColor(page.Window.window)}
                        }
                        SettingsRow {label:"展开播放背景";ToggleButton {objectName:"playerCoverBackground";text:"使用歌曲封面";checked:settingsBridge.values.playerCoverBackground;onClicked:settingsBridge.setPreference("playerCoverBackground",checked)}}
                    }
                    SettingsGroup {
                        title:"字体";objectName:"settingsFontGroup"
                        SettingsRow {label:"字体与应用区域";ShellButton {objectName:"uiFontSetting";text:"字体设置";onClicked:fontDialog.openSettings()}}
                        SettingsRow {label:"文字粗细";ToggleButton {objectName:"mutedBoldSetting";text:"右侧小字加粗";checked:settingsBridge.values.mutedBold;onClicked:settingsBridge.setPreference("mutedBold",checked)}}
                    }
                    SettingsGroup {
                        title:"界面与交互";objectName:"settingsInteractionGroup"
                        Repeater {model:[{key:"settingsEntryMode",label:"设置入口"},{key:"historyEntryMode",label:"历史记录入口"}]
                            SettingsRow {id:entryPositionRow;required property var modelData;label:modelData.label
                                Repeater {model:["列表","按钮"]
                                    ShellButton {required property int index;required property string modelData;objectName:entryPositionRow.modelData.key+"-"+index;text:modelData;primary:settingsBridge.values[entryPositionRow.modelData.key]===index;onClicked:settingsBridge.setPreference(entryPositionRow.modelData.key,index)}
                                }
                            }
                        }
                        SettingsRow {label:"音乐资料库侧栏";ShellButton {objectName:"customizeSidebar";text:"自定义侧栏";onClicked:sidebarEditor.openEditor()}}
                        SettingsRow {id:animationRow;label:"动画效果";optionLabels:["全量动画","少量动画","关闭动画"]
                            ChoiceBox {objectName:"animationModeSetting";visible:!animationRow.optionsInline;width:Math.min(280,parent.width);model:["全量动画","少量动画","关闭动画"];currentIndex:settingsBridge.values.animationMode;onActivated:settingsBridge.setPreference("animationMode",currentIndex)}
                            Repeater {model:["全量动画","少量动画","关闭动画"]
                                ShellButton {required property int index;required property string modelData;objectName:"animationOption-"+index;visible:animationRow.optionsInline;text:modelData;primary:settingsBridge.values.animationMode===index;onClicked:settingsBridge.setPreference("animationMode",index)}
                            }
                        }
                        SettingsRow {label:"歌单收起动画";ToggleButton {objectName:"playlistHeaderAnimationSetting";text:"滚动切换详情";checked:settingsBridge.values.playlistHeaderAnimation;onClicked:settingsBridge.setPreference("playlistHeaderAnimation",checked)}}
                        SettingsRow {label:"歌曲封面";ToggleButton {objectName:"playlistCoversSetting";text:"显示封面";checked:settingsBridge.values.playlistCovers;onClicked:settingsBridge.setPreference("playlistCovers",checked)}}
                        SettingsRow {label:"歌曲列表标记";ToggleButton {objectName:"showFavoritesSetting";text:"展示红心";checked:settingsBridge.values.showFavorites;onClicked:settingsBridge.setPreference("showFavorites",checked)}}
                        SettingsRow {id:closeRow;objectName:"settingsCloseCell";label:"关闭窗口时";optionLabels:["退出播放器","最小化到系统托盘"]
                            ChoiceBox {objectName:"closeSetting";visible:!closeRow.optionsInline;width:Math.min(280,parent.width);model:["退出播放器","最小化到系统托盘"];currentIndex:settingsBridge.values.closeToTray?1:0;onActivated:settingsBridge.setPreference("closeToTray",currentIndex===1)}
                            Repeater {
                                model:["退出播放器","最小化到系统托盘"]
                                ShellButton {
                                    required property int index;required property string modelData
                                    objectName:"closeOption-"+index;visible:closeRow.optionsInline;text:modelData
                                    primary:settingsBridge.values.closeToTray===(index===1)
                                    Accessible.role:Accessible.RadioButton;Accessible.checked:primary
                                    onClicked:settingsBridge.setPreference("closeToTray",index===1)
                                }
                            }
                        }
                    }
            }
            SettingsCard {title:"发现";objectName:"settingsDiscoveryCard"
                SettingsGroup {title:"发现页";objectName:"settingsDiscoveryGroup"
                    SettingsRow {label:"发现页歌曲快捷功能";ShellButton {objectName:"customizeDiscoveryActions";text:"自定义快捷功能";onClicked:discoveryActions.openEditor()}}
                    SettingsRow {id:vipModeRow;label:"会员标识"
                        ChoiceBox {objectName:"vipMarkModeSetting";visible:vipModeRow.stacked;width:Math.min(280,parent.width);model:["只标歌曲","只标账号","账号歌曲","全部关闭"];currentIndex:settingsBridge.values.vipMarkMode;onActivated:settingsBridge.setPreference("vipMarkMode",currentIndex)}
                        Repeater {model:["只标歌曲","只标账号","账号歌曲","全部关闭"]
                            ShellButton {required property int index;required property string modelData;objectName:"vipMarkOption-"+index;visible:!vipModeRow.stacked;text:modelData;primary:settingsBridge.values.vipMarkMode===index;onClicked:settingsBridge.setPreference("vipMarkMode",index)}
                        }
                    }
                }
            }
            SettingsCard {title:"播放";objectName:"settingsPlaybackCard"
                    SettingsGroup {
                        title:"播放行为";objectName:"settingsPlaybackGroup"
                        SettingsRow {objectName:"settingsThresholdCell";label:"有效播放比例"
                            SettingsNumber {objectName:"qmlThresholdSetting";from:70;to:100;value:settingsBridge.values.threshold;onValueModified:settingsBridge.setPreference("threshold",value)}
                        }
                        SettingsRow {label:"播放淡入淡出";ToggleButton {objectName:"playbackFadeSetting";text:"启用";checked:settingsBridge.values.playbackFade;onClicked:settingsBridge.setPreference("playbackFade",checked)}}
                        SettingsRow {id:onlineVideoRow;label:"在线视频播放";optionLabels:["视频","只音频"]
                            ChoiceBox {objectName:"onlineVideoModeSetting";visible:!onlineVideoRow.optionsInline;width:Math.min(260,parent.width);model:["视频","只音频"];currentIndex:settingsBridge.values.onlineVideoMode;onActivated:settingsBridge.setPreference("onlineVideoMode",currentIndex)}
                            Repeater {model:["视频","只音频"]
                                ShellButton {required property int index;required property string modelData;objectName:"onlineVideoMode-"+index;visible:onlineVideoRow.optionsInline;text:modelData;primary:settingsBridge.values.onlineVideoMode===index;onClicked:settingsBridge.setPreference("onlineVideoMode",index)}
                            }
                        }
                        SettingsRow {id:localVideoRow;label:"本地视频播放";optionLabels:["视频","只音频"]
                            ChoiceBox {objectName:"localVideoModeSetting";visible:!localVideoRow.optionsInline;width:Math.min(260,parent.width);model:["视频","只音频"];currentIndex:settingsBridge.values.localVideoMode;onActivated:settingsBridge.setPreference("localVideoMode",currentIndex)}
                            Repeater {model:["视频","只音频"]
                                ShellButton {required property int index;required property string modelData;objectName:"localVideoMode-"+index;visible:localVideoRow.optionsInline;text:modelData;primary:settingsBridge.values.localVideoMode===index;onClicked:settingsBridge.setPreference("localVideoMode",index)}
                            }
                        }
                        SettingsRow {label:"本地视频歌词";ToggleButton {objectName:"showLocalVideoLyricsSetting";text:"显示歌词";checked:settingsBridge.values.showLocalVideoLyrics;onClicked:settingsBridge.setPreference("showLocalVideoLyrics",checked)}}
                        SettingsRow {label:"首次播放视频时展开";ToggleButton {objectName:"autoExpandVideoSetting";text:"启用";checked:settingsBridge.values.autoExpandVideo;onClicked:settingsBridge.setPreference("autoExpandVideo",checked)}}
                        SettingsRow {label:"视频黑色留边";ToggleButton {objectName:"videoBlackBarsSetting";text:"启用";checked:settingsBridge.values.videoBlackBars;onClicked:settingsBridge.setPreference("videoBlackBars",checked)}}
                        SettingsRow {label:"播放进度记忆";ToggleButton {objectName:"rememberPositionSetting";text:"启用";checked:settingsBridge.values.rememberPosition;onClicked:settingsBridge.setPreference("rememberPosition",checked)}}
                    }
            }
            SettingsCard {title:"存储";objectName:"settingsStorageCard"
                    SettingsGroup {
                        title:"目录";objectName:"dataRootRow"
                        SettingsRow {label:"本地音乐目录"
                            UiText {objectName:"dataRootText";width:parent.width<420?parent.width:Math.max(80,parent.width-openDirectory.implicitWidth-8);height:Theme.controlHeight;text:settingsBridge.dataRoot;textFormat:Text.PlainText;elide:Text.ElideMiddle;verticalAlignment:Text.AlignVCenter;color:Theme.secondary;basePixelSize:13
                                HoverHandler {id:dataDirectoryHover} HintBubble {visible:dataDirectoryHover.hovered;text:settingsBridge.dataRoot}
                            }
                            ShellButton {id:openDirectory;objectName:"openDataRoot";text:"打开本地音乐目录";onClicked:settingsBridge.openDataRoot()}
                        }
                        SettingsRow {label:"默认下载目录"
                            UiText {objectName:"downloadDirectoryText";width:parent.width<420?parent.width:Math.max(80,parent.width-openDownloads.implicitWidth-8);height:Theme.controlHeight;text:onlineDownloads.downloadDirectory;textFormat:Text.PlainText;elide:Text.ElideMiddle;verticalAlignment:Text.AlignVCenter;color:Theme.secondary;basePixelSize:13
                                HoverHandler {id:downloadDirectoryHover} HintBubble {visible:downloadDirectoryHover.hovered;text:onlineDownloads.downloadDirectory}
                            }
                            ShellButton {id:openDownloads;objectName:"openDownloadDirectory";text:"打开默认下载目录";onClicked:onlineDownloads.openDownloadDirectory()}
                        }
                    }
                    SettingsGroup {title:"缓存";objectName:"settingsCacheGroup"
                        SettingsRow {label:"缓存位置"
                            UiText {objectName:"cacheRootText";width:parent.width<420?parent.width:Math.max(80,parent.width-openCacheDirectory.implicitWidth-8);height:Theme.controlHeight;verticalAlignment:Text.AlignVCenter;text:settingsBridge.cacheRoot;textFormat:Text.PlainText;elide:Text.ElideMiddle;color:Theme.secondary;basePixelSize:13
                                HoverHandler {id:cachePathHover} HintBubble {visible:cachePathHover.hovered;text:settingsBridge.cacheRoot}
                            }
                            ShellButton {id:openCacheDirectory;text:"打开缓存目录";onClicked:settingsBridge.openCacheRoot()}
                        }
                        SettingsRow {label:"缓存占用上限"
                            SettingsNumber {objectName:"cacheLimitSetting";controlPrefix:"cacheLimit";suffix:" MB";from:256;to:40960;stepSize:256;value:settingsBridge.values.cacheLimitMB;onValueModified:settingsBridge.setPreference("cacheLimitMB",value)}
                        }
                        SettingsRow {label:"原有缓存位置";visible:settingsBridge.legacyCacheRoot.length>0
                            UiText {width:parent.width;height:Theme.controlHeight;verticalAlignment:Text.AlignVCenter;text:settingsBridge.legacyCacheRoot;textFormat:Text.PlainText;elide:Text.ElideMiddle;color:Theme.secondary;basePixelSize:13
                                HoverHandler {id:legacyCacheHover} HintBubble {visible:legacyCacheHover.hovered;text:settingsBridge.legacyCacheRoot}
                            }
                        }
                    }
            }
            Rectangle {
                Layout.fillWidth:true;implicitHeight:platformCard.implicitHeight+36;radius:14;color:Theme.card
                ColumnLayout {
                    id:platformCard;anchors {left:parent.left;right:parent.right;top:parent.top;margins:18} spacing:8
                    RowLayout {
                        Layout.fillWidth:true
                        UiText {Layout.fillWidth:true;text:"音乐平台";basePixelSize:22;font.weight:Font.DemiBold;color:Theme.text}
                        ShellButton {objectName:"manageMusicPlatforms";text:"管理音乐平台";onClicked:page.platformsRequested()}
                    }
                    UiText {Layout.fillWidth:true;text:"管理音乐与视频平台的账号。";wrapMode:Text.Wrap;color:Theme.secondary;basePixelSize:14}
                }
            }
            Rectangle {
                Layout.fillWidth: true
                implicitHeight: aiCard.implicitHeight + 36
                radius: 14
                color: Theme.card
                ColumnLayout {
                    id: aiCard
                    anchors { left: parent.left; right: parent.right; top: parent.top; margins: 18 }
                    spacing: 8
                    RowLayout {
                        Layout.fillWidth: true
                        UiText { Layout.fillWidth: true; text: "AI 服务"; basePixelSize: 22; font.weight: Font.DemiBold; color: Theme.text }
                        ShellButton { objectName: "openAiServiceSettings"; text: "管理 AI 服务"; onClicked: page.aiRequested() }
                    }
                    UiText { Layout.fillWidth: true; text: "配置 DeepSeek、KIMI、Qwen 凭据、模型和歌词翻译。"; wrapMode: Text.Wrap; color: Theme.secondary; basePixelSize: 14 }
                }
            }
            ShortcutsPage { Layout.fillWidth: true }
            Rectangle {
                objectName: "settingsAboutCard"
                Layout.fillWidth: true
                implicitHeight: about.implicitHeight + 40
                radius: 14
                color: Theme.card
                ColumnLayout {
                    id: about
                    anchors { left: parent.left; right: parent.right; top: parent.top; margins: 20 }
                    spacing: 8
                    RowLayout{Layout.fillWidth:true;UiText { text: appBridge.version; color: Theme.text; basePixelSize: 22; font.weight: Font.DemiBold;Layout.fillWidth:true } ShellButton{objectName:"openVersionLog";text:"版本日志";flat:true;onClicked:page.versionLogRequested()}}
                    UiText { text: "为本地音乐留一个安静的位置。"; color: Theme.secondary; basePixelSize: 14 }
                }
            }
            UiText { Layout.fillWidth: true; visible:text.length>0; text: settingsBridge.status; wrapMode: Text.Wrap; color: Theme.accent; textFormat: Text.PlainText }
        }
    }
    LyricColorDialog {id:lyricColorEditor;parent:page.Overlay.overlay}
    Pages.FontDialog { id: fontDialog }
    DiscoveryActionsDialog {id:discoveryActions;parent:page.Overlay.overlay}
    SidebarEditor {id:sidebarEditor;parent:page.Overlay.overlay}
}
