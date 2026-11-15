import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../theme"

ColumnLayout {
    id: accounts
    objectName:"platformAccounts"
    signal backRequested()
    Layout.fillWidth:true
    spacing:18
    property var loginSource:onlineMusic
    property string loginName:"网易云音乐"
    readonly property bool showPlatformNames:{
        for(let i=0;i<platformRows.count;++i){
            const row=platformRows.itemAt(i)
            if(row&&row.nameSpaceWithPlatforms<row.requiredNameWidth)return false
        }
        return true
    }
    property bool awaitingLogin:false
    readonly property var platforms:[
        {name:"网易云音乐",url:"https://music.163.com/",logo:"qrc:/music-platforms/netease.ico",bridge:onlineMusic},
        {name:"QQ 音乐",url:"https://y.qq.com/",logo:"qrc:/music-platforms/qq.ico",bridge:qqMusic},
        {name:"汽水音乐",url:"https://music.douyin.com/",logo:"qrc:/music-platforms/qishui.svg",bridge:sodaMusic},
        {name:"酷狗音乐",url:"https://www.kugou.com/",logo:"qrc:/music-platforms/kugou.png",clip:Qt.rect(0,0,160,160),bridge:kugouMusic},
        {name:"哔哩哔哩",url:"https://www.bilibili.com/",logo:"image://relaxer/icon/video?color=%2300a1d6",bridge:bilibili}
    ]
    function startLogin(source,name){
        loginSource=source;loginName=name;awaitingLogin=true;login.open();source.startLogin()
    }
    function activatePlatforms(){onlineMusic.activate();qqMusic.activate();sodaMusic.activate();kugouMusic.activate();bilibili.activate()}
    RowLayout {
        Layout.fillWidth:true
        UiText {Layout.fillWidth:true;text:"音乐平台";basePixelSize:25;font.weight:Font.DemiBold}
        ShellButton {objectName:"platformSettingsBack";text:"返回设置";onClicked:accounts.backRequested()}
    }
    ColumnLayout {
        Layout.fillWidth:true
        spacing:12
        Repeater {
            id:platformRows
            model:accounts.platforms
            delegate:Rectangle {
                id:platformRow
                objectName:"platformRow-"+index
                required property var modelData
                required property int index
                readonly property string accountName:modelData.bridge.loggedIn?(modelData.bridge.nickname||"已登录账号"):"未登录"
                readonly property var nameCharacters:Array.from(accountName)
                readonly property string tenCharacterName:nameCharacters.slice(0,10).join("")+"..."
                readonly property string fiveCharacterName:nameCharacters.slice(0,5).join("")+"..."
                readonly property real fullNameWidth:Math.ceil(fullNameMetrics.advanceWidth)
                readonly property real requiredNameWidth:nameCharacters.length>10?Math.ceil(tenNameMetrics.advanceWidth):fullNameWidth
                readonly property real platformNameWidth:Math.max(platformLabel.implicitWidth,platformLabel.Layout.preferredWidth)
                // Measure the same hypothetical layout regardless of which names
                // are currently visible, so collapsing cannot trigger expansion.
                readonly property real nameSpaceWithPlatforms:accountRow.width-platformLogo.Layout.preferredWidth-platformNameWidth-1-accountAvatar.Layout.preferredWidth-platformLoginButton.implicitWidth-platformWebsite.implicitWidth-accountRow.spacing*6
                readonly property real nameSpace:nameSpaceWithPlatforms+(accounts.showPlatformNames?0:platformNameWidth+accountRow.spacing)
                readonly property string displayedName:{
                    if(fullNameWidth<=nameSpace)return accountName
                    if(accounts.showPlatformNames)return tenCharacterName
                    if(nameCharacters.length>=6&&Math.ceil(fiveNameMetrics.advanceWidth)<=nameSpace)return fiveCharacterName
                    return ""
                }
                TextMetrics {id:fullNameMetrics;font:accountText.font;text:platformRow.accountName}
                TextMetrics {id:tenNameMetrics;font:accountText.font;text:platformRow.tenCharacterName}
                TextMetrics {id:fiveNameMetrics;font:accountText.font;text:platformRow.fiveCharacterName}
                Layout.fillWidth:true;implicitHeight:72;radius:10;color:Theme.card
                RowLayout {
                    id:accountRow
                    anchors.fill:parent;anchors.margins:accounts.width<520?6:10;spacing:accounts.width<520?4:6
                    Image {id:platformLogo;Layout.preferredWidth:accounts.width<520?20:24;Layout.preferredHeight:accounts.width<520?20:24;objectName:"platformLogo-"+platformRow.index;source:platformRow.modelData.logo;sourceClipRect:platformRow.modelData.clip||Qt.rect(0,0,0,0);fillMode:Image.PreserveAspectFit;smooth:true;mipmap:true;asynchronous:true
                        HoverHandler {id:platformLogoHover} HintBubble {visible:platformLogoHover.hovered;text:platformRow.modelData.name}
                    }
                    UiText {id:platformLabel;objectName:"platformName-"+index;visible:accounts.showPlatformNames;text:platformRow.modelData.name;color:Theme.text;font.weight:Font.DemiBold;Layout.minimumWidth:implicitWidth;Layout.preferredWidth:accounts.width<520?76:90;wrapMode:Text.NoWrap}
                    Rectangle {width:1;Layout.preferredHeight:32;color:Theme.border}
                    Avatar {id:accountAvatar;objectName:"platformAvatar-"+index;Layout.preferredWidth:accounts.width<520?24:32;Layout.preferredHeight:accounts.width<520?24:32;file:platformRow.modelData.bridge.avatarUrl;seed:platformRow.modelData.bridge.nickname||platformRow.modelData.name
                        HoverHandler {id:accountAvatarHover} HintBubble {visible:accountAvatarHover.hovered;text:platformRow.modelData.name+" · "+(platformRow.modelData.bridge.loggedIn?platformRow.modelData.bridge.nickname:"未登录")}
                    }
                    ColumnLayout {
                        objectName:"platformAccountName-"+index;visible:platformRow.displayedName.length>0
                        Layout.fillWidth:true;Layout.minimumWidth:0;spacing:2
                        UiText {id:accountText;objectName:"platformAccountText-"+index;Layout.fillWidth:true;text:platformRow.displayedName;textFormat:Text.PlainText;color:Theme.text
                            HoverHandler {id:accountNameHover} HintBubble {visible:accountNameHover.hovered&&platformRow.displayedName!==platformRow.accountName;text:platformRow.accountName}
                        }
                        UiText {Layout.fillWidth:true;visible:platformRow.modelData.bridge===qqMusic&&qqMusic.loggedIn;text:qqMusic.membership;elide:Text.ElideRight;color:Theme.secondary;basePixelSize:12}
                    }
                    Item {Layout.fillWidth:true;visible:!platformRow.displayedName.length}
                    ShellButton {id:platformLoginButton;objectName:"platformLogin-"+index;implicitWidth:contentItem.implicitWidth+leftPadding+rightPadding;Layout.minimumWidth:implicitWidth;leftPadding:accounts.width<520?6:15;rightPadding:leftPadding;text:platformRow.modelData.bridge.loggedIn?"退出":"登录"
                        onClicked:if(platformRow.modelData.bridge.loggedIn)platformRow.modelData.bridge.logout();else accounts.startLogin(platformRow.modelData.bridge,platformRow.modelData.name)
                    }
                    ShellButton {id:platformWebsite;objectName:"platformWebsite-"+index;text:"官网";implicitWidth:contentItem.implicitWidth+leftPadding+rightPadding;Layout.minimumWidth:implicitWidth;leftPadding:accounts.width<520?6:15;rightPadding:leftPadding;Accessible.name:"打开"+platformRow.modelData.name+"官网";flat:true;onClicked:Qt.openUrlExternally(platformRow.modelData.url)}
                }
            }
        }
    }
    ConfirmDialog {
        id:login;objectName:"platformLoginDialog";parent:accounts.Overlay.overlay;width:Math.min(360,parent.width-24);anchors.centerIn:parent
        modal:true;title:accounts.loginName+"扫码登录";standardButtons:Dialog.Cancel
        onClosed:{accounts.awaitingLogin=false;accounts.loginSource.stopLogin()}
        contentItem:ColumnLayout {
            spacing:12
            ChoiceBox {
                Layout.fillWidth:true;visible:accounts.loginSource===qqMusic
                model:["手机 QQ 扫码","微信扫码"];currentIndex:qqMusic.loginChannel==="wx"?1:0
                onActivated:{qqMusic.loginChannel=currentIndex===1?"wx":"qq";qqMusic.startLogin()}
            }
            Rectangle {
                Layout.alignment:Qt.AlignHCenter;Layout.preferredWidth:220;Layout.preferredHeight:220;color:"white";radius:8
                Image {anchors.fill:parent;anchors.margins:4;source:accounts.loginSource.qrImage;fillMode:Image.PreserveAspectFit;smooth:false}
            }
            UiText {Layout.fillWidth:true;text:accounts.loginSource.status;textFormat:Text.PlainText;wrapMode:Text.Wrap;color:Theme.secondary}
            ShellButton {text:"刷新二维码";onClicked:accounts.loginSource.startLogin()}
        }
    }
    Connections {target:accounts.loginSource;function onLoginCompleted(){if(accounts.awaitingLogin)login.close()}}
    onVisibleChanged:{
        if(visible)activatePlatforms()
        else if(login.visible)login.close()
    }
    Component.onCompleted:if(visible)activatePlatforms()
}
