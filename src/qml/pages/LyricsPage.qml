import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../components"
import "../theme"
Item {
    id:page
    objectName:"lyricsPage"
    property bool fullScreen:false
    property bool metadataTravelling:false
    property alias metadataTitle:largeTitle
    property alias metadataArtist:largeArtist
    property bool artworkTravelling:false
    property real detailsOpacity:1
    property int trackDirection:1
    property alias largeArtwork:art
    property bool toolsVisible:false
    property bool delayOpen:false
    signal collapseRequested()
    signal fullscreenRequested()
    signal translationRequested()
    Rectangle { anchors.fill:parent;color:Theme.background }
    Image {
        anchors.fill:parent
        source:settingsBridge.values.playerCoverBackground ? "image://relaxer/blur?file="+encodeURIComponent(playbackBridge.coverUrl)+"&seed="+encodeURIComponent(playbackBridge.hasTrack?playbackBridge.title:"Relaxer")+"&rev="+playbackBridge.artworkRevision : String(settingsBridge.palette.expandedImage||"").length?"image://relaxer/background?file="+encodeURIComponent(String(settingsBridge.palette.expandedImage))+"&blur="+Number(settingsBridge.palette.blurExpanded||0):""
        asynchronous:true;cache:false;retainWhileLoading:false
        sourceSize.width:720;sourceSize.height:720;fillMode:Image.PreserveAspectCrop
        visible:source.toString().length>0
    }
    Rectangle{anchors.fill:parent;color:Theme.background;opacity:settingsBridge.values.playerCoverBackground?.68:0}
    RowLayout{
        x:34;y:28;width:parent.width-68;height:34;opacity:page.detailsOpacity
        ShellButton{objectName:"playerCollapse";text:"收起";flat:true;symbol:"collapse";onClicked:page.collapseRequested()}
        Item{Layout.fillWidth:true}
        IconButton{objectName:"fullscreenButton";symbol:page.fullScreen?"contract":"expand";tip:page.fullScreen?"退出窗口全屏":"窗口全屏";onClicked:page.fullscreenRequested()}
    }
    RowLayout{
        anchors.left:parent.left;anchors.right:parent.right;anchors.top:parent.top;anchors.bottom:parent.bottom
        anchors.leftMargin:34;anchors.rightMargin:34;anchors.topMargin:76;anchors.bottomMargin:20;spacing:32
        Item{
            Layout.fillWidth:true;Layout.fillHeight:true;Layout.preferredWidth:page.width/2
            Column{
                width:parent.width;anchors.verticalCenter:parent.verticalCenter;spacing:0
                Artwork{
                    id:art;objectName:"currentArtwork";width:Math.max(160,Math.min(360,parent.width-40,page.height-310));height:width;anchors.horizontalCenter:parent.horizontalCenter
                    file:playbackBridge.coverUrl;seed:playbackBridge.hasTrack?playbackBridge.title:"Relaxer";revision:playbackBridge.artworkRevision;radius:24
                    animated:page.visible&&!page.artworkTravelling;direction:page.trackDirection
                    opacity:page.artworkTravelling?0:1
                }
                Item{height:22;width:1}
                UiText {
                    id:largeTitle
                    width:parent.width-40;anchors.horizontalCenter:parent.horizontalCenter;text:playbackBridge.hasTrack?playbackBridge.title:"还没有音乐正在播放";textFormat:Text.PlainText
                    wrapMode:Text.Wrap;maximumLineCount:2;elide:Text.ElideRight;horizontalAlignment:Text.AlignHCenter
                    font.pixelSize:27;font.weight:Font.DemiBold;color:Theme.text;opacity:page.metadataTravelling?0:page.detailsOpacity
                }
                Item{height:8;width:1}
                UiText {id:largeArtist;width:parent.width-40;anchors.horizontalCenter:parent.horizontalCenter;text:playbackBridge.hasTrack?playbackBridge.artist:"选一首喜欢的歌，慢慢听。";textFormat:Text.PlainText;wrapMode:Text.Wrap;maximumLineCount:2;elide:Text.ElideRight;horizontalAlignment:Text.AlignHCenter;font.pixelSize:16;color:Theme.secondary;opacity:page.metadataTravelling?0:page.detailsOpacity}
            }
        }
        Item{
            id:lyricSide;Layout.fillWidth:true;Layout.fillHeight:true;Layout.preferredWidth:page.width/2;opacity:page.detailsOpacity
            LyricsList{id:lyrics;anchors.fill:parent}
            HoverHandler{onPointChanged:{page.toolsVisible=true;if(!tools.opened)idle.restart()} onHoveredChanged:if(hovered){page.toolsVisible=true;if(!tools.opened)idle.restart()}}
            ToolButton{
                id:question;objectName:"lyricToolsQuestion";width:20;height:20;anchors.right:parent.right;anchors.bottom:parent.bottom;anchors.rightMargin:26;anchors.bottomMargin:6
                text:"?";font.pixelSize: 14;opacity:page.toolsVisible||tools.opened?1:0;enabled:opacity>0
                background:Rectangle{radius:10;color:Theme.hover;border.color:Theme.border}
                onHoveredChanged: { if(hovered) {page.toolsVisible=true;idle.stop()} else if(!tools.hovered) idle.restart() }
                onClicked:{if(tools.opened)tools.close();else{page.delayOpen=false;tools.open()}}
                Behavior on opacity{NumberAnimation{duration:settingsBridge.values.reduceMotion?0:140}}
            }
            Timer{id:idle;interval:3000;onTriggered:if(!question.hovered&&!tools.hovered){page.toolsVisible=false;tools.close()}}
            Popup { popupType: Popup.Item;
                id:tools;objectName:"lyricToolsMenu";property bool hovered:false
                x:Math.max(0,lyricSide.width-width-28);y:Math.max(0,lyricSide.height-height-30)
                width:page.delayOpen?320:210;padding:12
                closePolicy:Popup.CloseOnEscape|Popup.CloseOnPressOutside
                background:Rectangle{radius:12;color:Theme.card;border.color:Theme.border}
                HoverHandler{onHoveredChanged:{tools.hovered=hovered;if(hovered)idle.stop();else if(!question.hovered)idle.restart()}}
                onClosed:{page.delayOpen=false;idle.restart()}
                contentItem:ColumnLayout{
                    spacing:8
                    ShellButton{objectName:"lyricDelayEntry";Layout.fillWidth:true;visible:!page.delayOpen;text:lyricsModel.timed?"歌词延时":"歌词延时 · 查找滚动歌词";enabled:playbackBridge.hasTrack;onClicked:{if(lyricsModel.timed){page.delayOpen=true;idle.stop()}else{tools.close();page.translationRequested()}}}
                    CheckBox{visible:!page.delayOpen;text:"显示译文";checked:settingsBridge.values.showLyricTranslation;onClicked:settingsBridge.setPreference("showLyricTranslation",checked)}
                    ShellButton{visible:!page.delayOpen;Layout.fillWidth:true;text:"查找歌词…";enabled:playbackBridge.hasTrack;onClicked:{tools.close();page.translationRequested()}}
                    ShellButton{visible:page.delayOpen;text:"‹ 返回";onClicked:page.delayOpen=false}
                    RowLayout{
                        visible:page.delayOpen
                        UiText {text:"歌词延时";color:Theme.secondary}
                        EntryField {
                            id:offset;objectName:"lyricsOffset";Layout.fillWidth:true;text:(lyricsModel.offsetMs/1000).toFixed(2)+" s";enabled:lyricsModel.timed
                            onEditingFinished:{const v=Number(text.replace("s","").trim());if(!isNaN(v))lyricsModel.setOffsetMs(Math.max(-5000,Math.min(5000,Math.round(v*1000))));text=(lyricsModel.offsetMs/1000).toFixed(2)+" s"}
                        }
                        Column{
                            IconButton{width:20;height:16;symbol:"up";onClicked:lyricsModel.setOffsetMs(Math.min(5000,lyricsModel.offsetMs+10))}
                            IconButton{width:20;height:16;symbol:"down";onClicked:lyricsModel.setOffsetMs(Math.max(-5000,lyricsModel.offsetMs-10))}
                        }
                        ShellButton{text:"归零";onClicked:lyricsModel.setOffsetMs(0)}
                    }
                }
            }
        }
    }
}
