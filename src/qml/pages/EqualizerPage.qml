import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window
import "../components"
import "../theme"
Item {
    id:panel;objectName:"parametricEqualizerPage"
    property string backText:"返回设置"
    signal backRequested()
    function prepareToLeave(){if(songSearchControl)songSearchControl.close();panel.forceActiveFocus()}
    function requestBack(){prepareToLeave();backRequested()}
    property Item backdropSource:panel
    onVisibleChanged:if(!visible&&songSearchControl)songSearchControl.close()
    property bool helpOpen:false
    property int selectedBand:0
    property int presetIndex:0
    readonly property int targetIndex:equalizerBridge.targets.findIndex(row=>row.key===equalizerBridge.targetKey)
    readonly property var scope:equalizerBridge.target
    readonly property string scopeDescription:equalizerBridge.targetKey==="global"?"全局默认，未单独适配的内容使用此配置。":scope.configured?"独立配置，调整后实时试听，点击保存配置后记住。关闭此配置时也会优先命中并旁路。":"继承："+scope.inheritedName+"。调整参数或应用预设即可建立独立配置。"
    readonly property bool customPreset:presetIndex>=4&&presetIndex<equalizerBridge.presets.length
    Connections {target:equalizerBridge;property string previousKey:"";function onChanged(){if(previousKey!==equalizerBridge.targetKey){previousKey=equalizerBridge.targetKey;panel.presetIndex=0}}}
    readonly property var config:equalizerBridge.settings
    readonly property var band:config.bands[selectedBand]
    readonly property var chartResponse:equalizerBridge.response
    readonly property var bandColors:["#ef667c","#e99b51","#c0ae48","#64b98b","#4eb6c2","#668ed7","#a17ad4","#cb79b0"]
    component EqChoice:Item {
        id:choice
        property var model:[]
        property string textRole:"name"
        property int currentIndex:0
        signal activated(int index)
        function label(row){return typeof row==="string"?row:String(row[textRole]||"")}
        readonly property string currentText:currentIndex>=0&&currentIndex<model.length?label(model[currentIndex]):""
        readonly property real naturalWidth:{const font=choiceMetrics.font;let total=0;for(const row of model)total+=Math.ceil(choiceMetrics.advanceWidth(label(row)))+16;return total+Math.max(0,model.length-1)*4}
        readonly property bool tiled:model.length>0&&Math.ceil(naturalWidth)<=Math.floor(width)
        readonly property bool hovered:choiceHover.hovered
        implicitHeight:Theme.controlHeight;implicitWidth:Math.min(naturalWidth,240*Theme.fontScale)
        Accessible.role:Accessible.Grouping
        FontMetrics{id:choiceMetrics;font.family:Theme.fontFamily;font.pixelSize:Theme.fontSize;font.weight:Font.DemiBold}
        HoverHandler{id:choiceHover}
        onTiledChanged:if(dropdown&&dropdown.popup&&dropdown.popup.visible)dropdown.popup.close()
        ChoiceBox {
            id:dropdown;objectName:choice.objectName+"Dropdown";anchors.fill:parent;visible:!choice.tiled
            model:choice.model;textRole:typeof choice.model[0]==="string"?"":choice.textRole;currentIndex:choice.currentIndex
            Accessible.name:choice.Accessible.name;Accessible.description:choice.Accessible.description
            onActivated:index=>{choice.activated(index);dropdown.currentIndex=Qt.binding(function(){return choice.currentIndex})}
        }
        Loader {
            anchors.fill:parent;active:choice.tiled
            sourceComponent:Row {
                spacing:4
                Repeater {
                    model:choice.model
                    Button {
                        id:option;required property int index;required property var modelData
                        objectName:choice.objectName+"Option-"+index
                        width:Math.ceil(choiceMetrics.advanceWidth(choice.label(modelData)))+16;height:Theme.controlHeight
                        leftPadding:8;rightPadding:8;hoverEnabled:true;checked:index===choice.currentIndex
                        Accessible.role:Accessible.RadioButton;Accessible.checked:checked;Accessible.name:choice.label(modelData)
                        onClicked:choice.activated(index)
                        contentItem:UiText{text:choice.label(option.modelData);textFormat:Text.PlainText;font:choiceMetrics.font;horizontalAlignment:Text.AlignHCenter;verticalAlignment:Text.AlignVCenter;color:option.checked?Theme.text:Theme.secondary;elide:Text.ElideRight}
                        background:Rectangle{radius:8;color:option.checked?Theme.selected:option.down?Theme.pressed:option.hovered?Theme.hover:Theme.background;border.color:option.checked||option.activeFocus?Theme.accent:Theme.border}
                    }
                }
            }
        }
    }
    component EqValue:SpinBox {
        id:valueControl
        property real amount:0
        property int scaleFactor:1
        property string unit:""
        signal edited(real amount)
        implicitWidth:Math.round(124*Theme.fontScale);implicitHeight:Theme.controlHeight;editable:true
        leftPadding:24;rightPadding:24
        font.family:Theme.fontFamily;font.pixelSize:Math.max(12,Math.round(14*Theme.fontScale));font.weight:Font.Medium
        value:Math.round(amount*scaleFactor);stepSize:1
        validator:RegularExpressionValidator{regularExpression:/^-?\d*([.]\d*)?\s*(Hz|dB)?$/}
        textFromValue:(value,locale)=>(value/scaleFactor).toFixed(scaleFactor===1?0:scaleFactor===10?1:2)+unit
        valueFromText:(text,locale)=>Math.round(Number(text.replace(unit,"").trim())*scaleFactor)
        onValueModified:edited(value/scaleFactor)
        contentItem:TextInput{text:valueControl.textFromValue(valueControl.value,valueControl.locale);font:valueControl.font;color:valueControl.enabled?Theme.text:Theme.disabled;horizontalAlignment:Text.AlignHCenter;verticalAlignment:Text.AlignVCenter;readOnly:!valueControl.editable;validator:valueControl.validator;selectByMouse:true}
        background:Rectangle{radius:8;color:Theme.background;border.color:valueControl.activeFocus?Theme.accent:Theme.border}
        up.indicator:Rectangle {
            x:valueControl.width-width;width:22;height:valueControl.height;radius:7
            color:valueControl.up.pressed?Theme.pressed:valueControl.up.hovered?Theme.hover:"transparent"
            UiText{anchors.centerIn:parent;text:"+";basePixelSize:18;color:valueControl.enabled?Theme.secondary:Theme.disabled}
        }
        down.indicator:Rectangle {
            width:22;height:valueControl.height;radius:7
            color:valueControl.down.pressed?Theme.pressed:valueControl.down.hovered?Theme.hover:"transparent"
            UiText{anchors.centerIn:parent;text:"−";basePixelSize:18;color:valueControl.enabled?Theme.secondary:Theme.disabled}
        }
    }
    ColumnLayout {
        anchors.fill:parent;anchors.leftMargin:Theme.pageMargin;anchors.rightMargin:Theme.pageMargin
        anchors.topMargin:28;anchors.bottomMargin:16;spacing:14
        RowLayout {
            Layout.fillWidth:true;spacing:10
            UiText {objectName:"equalizerHeading";Layout.minimumWidth:0;Layout.fillWidth:true;text:"参数均衡器";basePixelSize:28;font.weight:Font.DemiBold;elide:Text.ElideRight}
            EqualizerSongSearch {
                id:songSearchControl;objectName:"peqSongSearchControl";Layout.preferredWidth:36;Layout.minimumWidth:36;Layout.maximumWidth:36
                Layout.preferredHeight:Theme.controlHeight;backdrop:panel.backdropSource
                onSongSelected:panel.forceActiveFocus()
            }
            ToolButton {
                objectName:"peqHelp";Layout.preferredWidth:24;Layout.preferredHeight:24;text:"?";font.pixelSize:14;hoverEnabled:true;focusPolicy:Qt.TabFocus
                Accessible.name:"均衡器使用说明";checkable:true;checked:panel.helpOpen;onClicked:panel.helpOpen=checked
                background:Rectangle{radius:12;color:panel.helpOpen?Theme.selected:Theme.hover;border.color:parent.activeFocus?Theme.accent:Theme.border}
                HintBubble{visible:parent.hovered;text:"均衡器使用说明"}
            }
            ShellButton {objectName:"equalizerBack";text:panel.backText;flat:true;onClicked:panel.requestBack()}
        }
        ScrollView {
            id:scroll;Layout.fillWidth:true;Layout.fillHeight:true;contentWidth:availableWidth;contentHeight:body.implicitHeight;clip:true
            ScrollBar.horizontal.policy:ScrollBar.AlwaysOff
            SmoothWheelHandler{flickable:scroll.contentItem as Flickable}
            ColumnLayout {
                id:body;width:scroll.availableWidth;spacing:12
                Rectangle {
                    Layout.fillWidth:true;implicitHeight:targetContent.implicitHeight+24;radius:12;color:Theme.card
                    ColumnLayout {
                        id:targetContent;anchors.left:parent.left;anchors.right:parent.right;anchors.top:parent.top;anchors.margins:12;spacing:8
                        Flow {
                            id:targetTools;Layout.fillWidth:true;spacing:8
                            readonly property real overrideWidth:createOverride.visible?createOverride.implicitWidth:inheritOverride.visible?inheritOverride.implicitWidth:0
                            readonly property real actionWidth:overrideWidth+(overrideWidth?8:0)+saveConfig.implicitWidth
                            readonly property real songButtonWidth:currentSong.visible?currentSong.width+6:0
                            readonly property bool fullRowTiles:targetChoice.naturalWidth+songButtonWidth<=width&&targetChoice.naturalWidth+songButtonWidth+actionWidth+8>width
                            readonly property bool inlineActions:!fullRowTiles&&width>=Math.min(targetChoice.naturalWidth,Math.round(150*Theme.fontScale))+songButtonWidth+actionWidth+8
                            RowLayout {
                                width:scopeActions.visible&&targetTools.inlineActions?targetTools.width-targetTools.actionWidth-8:targetTools.width;spacing:6
                                EqChoice {
                                    id:targetChoice;objectName:"peqTarget";Layout.fillWidth:true;Layout.minimumWidth:0;model:equalizerBridge.targets;textRole:"name"
                                    Accessible.name:"适配对象";Accessible.description:panel.scopeDescription
                                    currentIndex:panel.targetIndex;onActivated:index=>equalizerBridge.selectTarget(model[index].key)
                                    HintBubble{objectName:"peqScopeHint";visible:targetChoice.hovered;text:targetChoice.currentText+"\n"+panel.scopeDescription}
                                }
                                IconButton {id:currentSong;objectName:"peqCurrentSong";Layout.preferredWidth:32;Layout.preferredHeight:Theme.controlHeight;width:32;text:"当前歌曲";symbol:"music";tip:"编辑当前歌曲的适配";showTooltip:true;visible:equalizerBridge.currentSongKey.length>0;onClicked:equalizerBridge.selectTarget(equalizerBridge.currentSongKey)}
                            }
                            Flow {
                                id:scopeActions
                                width:targetTools.inlineActions?targetTools.actionWidth:targetTools.width;spacing:8
                                UiText {width:Math.max(60*Theme.fontScale,scopeActions.width-targetTools.actionWidth-8);height:Theme.controlHeight;verticalAlignment:Text.AlignVCenter;visible:!targetTools.inlineActions;text:equalizerBridge.targetKey==="global"?"全局默认":panel.scope.configured?"独立配置":"继承 · "+panel.scope.inheritedName;basePixelSize:12;color:Theme.secondary;elide:Text.ElideRight}
                                ShellButton {id:createOverride;objectName:"peqCreateOverride";text:"独立配置";visible:!panel.scope.configured;onClicked:equalizerBridge.createOverride();HintBubble{visible:parent.hovered;text:panel.scopeDescription}}
                                ShellButton {id:inheritOverride;objectName:"peqInherit";text:"恢复继承";flat:true;visible:equalizerBridge.targetKey!=="global"&&panel.scope.configured;onClicked:equalizerBridge.removeOverride()}
                                ShellButton {id:saveConfig;objectName:"peqSaveChanges";text:equalizerBridge.unsavedChanges?"保存配置 *":"保存配置";implicitWidth:Math.ceil(saveLabelMeasure.advanceWidth)+30;primary:equalizerBridge.unsavedChanges;onClicked:{panel.prepareToLeave();equalizerBridge.saveChanges()}TextMetrics{id:saveLabelMeasure;font:saveConfig.font;text:"保存配置 *"}}
                            }
                        }
                        Flow {
                            id:presetTools;Layout.fillWidth:true;spacing:8
                            readonly property real deleteWidth:deletePreset.visible?deletePreset.implicitWidth+6:0
                            readonly property real saveMinimum:Math.round(125*Theme.fontScale)+savePreset.implicitWidth+6
                            readonly property real tileMinimum:presetChoice.naturalWidth+deleteWidth
                            readonly property real dropdownMinimum:Math.round(170*Theme.fontScale)+Math.ceil(presetLabel.implicitWidth)+6+deleteWidth
                            readonly property bool fullRowTiles:tileMinimum<=width&&tileMinimum+saveMinimum+8>width
                            readonly property real chooseMinimum:tileMinimum+saveMinimum+8<=width?tileMinimum:dropdownMinimum
                            readonly property bool inlineTools:!fullRowTiles&&width>=chooseMinimum+saveMinimum+8
                            RowLayout {
                                id:presetChooser;width:presetTools.inlineTools?Math.min(presetTools.width-presetTools.saveMinimum-8,Math.max(presetTools.chooseMinimum,presetTools.width*.48)):presetTools.width;spacing:6
                                UiText {id:presetLabel;visible:presetChooser.width-presetTools.deleteWidth<presetChoice.naturalWidth;text:"预设";basePixelSize:12;color:Theme.secondary}
                                EqChoice {id:presetChoice;objectName:"peqPresets";Layout.fillWidth:true;Layout.minimumWidth:0;model:equalizerBridge.presets;textRole:"name";currentIndex:Math.min(panel.presetIndex,model.length-1);onActivated:index=>{panel.presetIndex=index;equalizerBridge.usePreset(model[index].id)}}
                                ShellButton {id:deletePreset;objectName:"peqRemovePreset";text:"删除";flat:true;visible:panel.customPreset;onClicked:if(equalizerBridge.removePreset(equalizerBridge.presets[panel.presetIndex].id))panel.presetIndex=0}
                            }
                            RowLayout {
                                width:presetTools.inlineTools?presetTools.width-presetChooser.width-8:presetTools.width;spacing:6
                                EntryField{id:presetName;objectName:"peqPresetName";Layout.fillWidth:true;Layout.minimumWidth:0;placeholderText:"新预设名称";maximumLength:40;Accessible.name:"自定义预设名称";text:equalizerBridge.presetDraft;onTextEdited:equalizerBridge.presetDraft=text;onAccepted:equalizerBridge.savePreset(text)}
                                ShellButton{id:savePreset;objectName:"peqSavePreset";text:"保存预设";onClicked:equalizerBridge.savePreset(equalizerBridge.presetDraft)}
                            }
                        }
                        UiText {
                            id:activeSummary;objectName:"peqActiveScope";Layout.fillWidth:true;visible:equalizerBridge.active.trackTitle.length>0
                            text:"播放 · "+equalizerBridge.active.trackTitle+"  /  "+equalizerBridge.active.name+(equalizerBridge.active.enabled?"":" · 旁路")
                            basePixelSize:12;color:Theme.secondary;elide:Text.ElideRight
                            HoverHandler{id:summaryHover}
                            HintBubble{visible:summaryHover.hovered;text:activeSummary.text}
                        }
                    }
                }
                Rectangle {
                    Layout.fillWidth:true;implicitHeight:helpText.implicitHeight+24;visible:panel.helpOpen;radius:12;color:Theme.card
                    UiText {
                        id:helpText;objectName:"peqHelpText";anchors.left:parent.left;anchors.right:parent.right;anchors.top:parent.top;anchors.margins:12;basePixelSize:13;wrapMode:Text.Wrap;color:Theme.secondary
                        text:"浅灰色横线是固定的 0 dB 基线；彩色线预览各频段的合成滤波曲线，不含前级和自动余量的整体衰减，旁路时也可预览。横向拖动节点调整频率，纵向调整增益；Q 值越大，影响范围越窄。高通、低通和陷波只需调整频率与 Q。前级作用于整套配置。\n适配优先级：歌曲 → 播放来源歌单 → 发现平台 → 全局。选择对象只切换编辑内容；调整参数或应用预设会建立独立配置，恢复继承可撤销它。独立配置关闭时也会优先命中并旁路。\n可通过搜索框按歌名、歌手或专辑选择单曲，结果只打开适配配置，不会播放。固定云端歌单按已加载或已缓存的内容检索，缺少缓存时可先进入该歌单。\n参数实时试听，点击“保存配置”后持久保存；离开未保存的配置时可选择保存、放弃或继续编辑。保存预设也会保存当前配置。覆盖本地、在线、视频音轨和 MIDI。自动余量估算频响峰值后衰减，可降低过载风险；它不是峰值限制器。"
                    }
                }
                Rectangle {
                    Layout.fillWidth:true;implicitHeight:processor.implicitHeight+24;radius:12;color:Theme.card
                    ColumnLayout {
                        id:processor;anchors.left:parent.left;anchors.right:parent.right;anchors.top:parent.top;anchors.margins:12;spacing:10
                        RowLayout {
                            Layout.fillWidth:true;spacing:8
                            UiText{text:"频率响应";font.weight:Font.DemiBold;basePixelSize:14}
                            UiText{text:(equalizerBridge.sampleRate/1000).toFixed(1)+" kHz";basePixelSize:11;color:Theme.secondary}
                            Item{Layout.fillWidth:true}
                            ToggleButton {objectName:"peqEnabled";enabled:equalizerBridge.mediaAvailable;text:panel.config.enabled?"已启用":"已旁路";checked:panel.config.enabled;onClicked:equalizerBridge.setOption("enabled",checked)}
                            IconButton {objectName:"peqReset";Layout.preferredWidth:30;Layout.preferredHeight:Theme.controlHeight;text:"恢复平直";symbol:"refresh";tip:"恢复此配置为平直";showTooltip:true;onClicked:equalizerBridge.reset()}
                        }
                        Rectangle {
                            id:chart;objectName:"peqResponseChart";Layout.fillWidth:true;implicitHeight:Math.min(252,Math.max(178,width*.32));radius:10;color:Theme.background;border.color:Theme.border
                            readonly property real plotLeft:36
                            readonly property real plotRight:12
                            readonly property real plotTop:16
                            readonly property real plotBottom:26
                            readonly property real plotWidth:Math.max(1,width-plotLeft-plotRight)
                            readonly property real plotHeight:Math.max(1,height-plotTop-plotBottom)
                            function freqX(f){return plotLeft+Math.log(f/20)/Math.log(1000)*plotWidth}
                            function gainY(g){return plotTop+(24-g)/48*plotHeight}
                            function pointFrequency(x){return Math.max(20,Math.min(20000,20*Math.pow(1000,(x-plotLeft)/plotWidth)))}
                            function pointGain(y){return Math.max(-18,Math.min(18,24-(y-plotTop)/plotHeight*48))}
                            Canvas {
                                id:curve;anchors.fill:parent;onWidthChanged:requestPaint();onHeightChanged:requestPaint()
                                Connections{target:equalizerBridge;function onChanged(){curve.requestPaint()}}
                                Connections{target:settingsBridge;function onSettingsChanged(){curve.requestPaint()}}
                                onPaint:{
                                    const ctx=getContext("2d");ctx.clearRect(0,0,width,height)
                                    ctx.font="10px sans-serif";ctx.lineWidth=1;ctx.fillStyle=Theme.secondary;ctx.strokeStyle=Qt.rgba(Theme.border.r,Theme.border.g,Theme.border.b,.6)
                                    for(const f of [20,50,100,200,500,1000,2000,5000,10000,20000]){const x=chart.freqX(f);ctx.beginPath();ctx.moveTo(x,chart.plotTop);ctx.lineTo(x,height-chart.plotBottom);ctx.stroke();if([20,100,1000,10000].indexOf(f)>=0)ctx.fillText(f>=1000?(f/1000)+"k":String(f),x-8,height-8)}
                                    for(const g of [-24,-12,0,12,24]){const y=chart.gainY(g);ctx.beginPath();ctx.moveTo(chart.plotLeft,y);ctx.lineTo(width-chart.plotRight,y);ctx.stroke();ctx.fillText((g>0?"+":"")+g,4,y+4)}
                                    ctx.save();ctx.beginPath();ctx.rect(chart.plotLeft,chart.plotTop,chart.plotWidth,chart.plotHeight);ctx.clip()
                                    const ink=Theme.accent,response=panel.chartResponse
                                    const fill=ctx.createLinearGradient(0,chart.plotTop,0,height-chart.plotBottom)
                                    fill.addColorStop(0,Qt.rgba(ink.r,ink.g,ink.b,.14));fill.addColorStop(1,Qt.rgba(ink.r,ink.g,ink.b,.015))
                                    ctx.fillStyle=fill;ctx.beginPath();ctx.moveTo(chart.freqX(20),chart.gainY(0))
                                    for(const point of response)ctx.lineTo(chart.freqX(point.frequency),chart.gainY(point.gain))
                                    ctx.lineTo(chart.freqX(20000),chart.gainY(0));ctx.closePath();ctx.fill()
                                    ctx.restore()
                                }
                            }
                            Rectangle {
                                id:zeroBaseline;objectName:"peqZeroDbBaseline";x:chart.plotLeft;y:Math.round(chart.gainY(0));width:chart.plotWidth;height:1
                                color:Theme.dark?"#96969f":"#b9b9c2"
                            }
                            Canvas {
                                id:responseLine;objectName:"peqFilterResponseLine";anchors.fill:parent;onWidthChanged:requestPaint();onHeightChanged:requestPaint()
                                Connections{target:equalizerBridge;function onChanged(){responseLine.requestPaint()}}
                                Connections{target:settingsBridge;function onSettingsChanged(){responseLine.requestPaint()}}
                                onPaint:{
                                    const ctx=getContext("2d");ctx.clearRect(0,0,width,height);ctx.save();ctx.beginPath();ctx.rect(chart.plotLeft,chart.plotTop,chart.plotWidth,chart.plotHeight);ctx.clip()
                                    ctx.strokeStyle=Theme.accent;ctx.lineWidth=2.2;ctx.beginPath();const response=panel.chartResponse
                                    for(let i=0;i<response.length;++i){const point=response[i],x=chart.freqX(point.frequency),y=chart.gainY(point.gain);if(i===0)ctx.moveTo(x,y);else ctx.lineTo(x,y)}ctx.stroke();ctx.restore()
                                }
                            }
                            Repeater {
                                model:panel.config.bands
                                delegate:Rectangle {
                                    required property int index;required property var modelData
                                    objectName:"peqBandHandle-"+index;width:panel.selectedBand===index?18:14;height:width;radius:width/2
                                    x:chart.freqX(modelData.frequency)-width/2;y:chart.gainY(modelData.type<3?modelData.gain:0)-height/2
                                    color:panel.bandColors[index];opacity:modelData.enabled?1:.4;border.width:panel.selectedBand===index?2:1;border.color:Theme.background
                                    UiText{anchors.centerIn:parent;text:index+1;basePixelSize:9;color:"white"}
                                }
                            }
                            MouseArea {
                                id:chartDrag;objectName:"peqCurveDrag";anchors.fill:parent;preventStealing:true
                                property int dragBand:-1
                                property int hoveredBand:-1
                                hoverEnabled:true;cursorShape:dragBand>=0||hoveredBand>=0?Qt.SizeAllCursor:Qt.ArrowCursor
                                function bandAt(x,y){let best=28,found=-1;for(let i=0;i<panel.config.bands.length;++i){const b=panel.config.bands[i],dx=chart.freqX(b.frequency)-x,dy=chart.gainY(b.type<3?b.gain:0)-y,d=Math.sqrt(dx*dx+dy*dy);if(d<best){best=d;found=i}}return found}
                                onPressed:mouse=>{dragBand=bandAt(mouse.x,mouse.y);if(dragBand<0){mouse.accepted=false;return}panel.selectedBand=dragBand}
                                onPositionChanged:mouse=>{hoveredBand=bandAt(mouse.x,mouse.y);if(pressed&&dragBand>=0)equalizerBridge.setBandPoint(dragBand,chart.pointFrequency(mouse.x),chart.pointGain(mouse.y))}
                                onReleased:dragBand=-1
                                onCanceled:dragBand=-1
                                onExited:hoveredBand=-1
                                HintBubble{objectName:"peqNodeDragHint";visible:parent.hoveredBand>=0&&!parent.pressed;text:"左右：调整频率；上下：调整增益。Q 值在下方调整。"}
                            }
                        }
                        RowLayout {
                            id:bandStrip;objectName:"peqBandStrip";Layout.fillWidth:true;spacing:5
                            Repeater {model:8
                                Button {
                                    id:bandButton;required property int index;readonly property color bandColor:panel.bandColors[index];objectName:"peqBandSelect-"+index
                                    Layout.fillWidth:true;Layout.minimumWidth:0;Layout.preferredWidth:1;implicitHeight:Theme.controlHeight;hoverEnabled:true
                                    checked:panel.selectedBand===index;Accessible.role:Accessible.RadioButton;Accessible.checked:checked;Accessible.name:"第 "+(index+1)+" 段";onClicked:panel.selectedBand=index
                                    contentItem:Item {
                                        Row {
                                            anchors.centerIn:parent;spacing:4
                                            Rectangle{width:5;height:5;radius:2.5;anchors.verticalCenter:parent.verticalCenter;color:panel.bandColors[bandButton.index];opacity:panel.config.bands[bandButton.index].enabled?1:.35}
                                            UiText{text:bandButton.index+1;basePixelSize:13;font.weight:bandButton.checked?Font.DemiBold:Font.Medium;color:bandButton.checked?Theme.text:Theme.secondary}
                                        }
                                    }
                                    background:Rectangle{radius:8;color:bandButton.checked?Qt.rgba(bandButton.bandColor.r,bandButton.bandColor.g,bandButton.bandColor.b,.12):bandButton.hovered?Theme.hover:"transparent";border.width:bandButton.checked||bandButton.activeFocus?1:0;border.color:bandButton.activeFocus?Theme.accent:panel.bandColors[bandButton.index]}
                                }
                            }
                        }
                        Flow {
                            id:modeTools;Layout.fillWidth:true;spacing:8
                            readonly property real controlsMinimum:bandEnable.implicitWidth+autoHeadroom.implicitWidth+Math.max(headroomValue.implicitWidth,60*Theme.fontScale)+16
                            readonly property bool inlineTools:width>=controlsMinimum+filterChoice.naturalWidth+8
                            Flow {
                                width:modeTools.inlineTools?modeTools.controlsMinimum:modeTools.width;spacing:8
                                ToggleButton {id:bandEnable;objectName:"peqBandEnabled";text:"频段"+(panel.selectedBand+1);checked:panel.band.enabled;onClicked:equalizerBridge.setBand(panel.selectedBand,"enabled",checked)}
                                RowLayout {
                                    width:autoHeadroom.implicitWidth+Math.max(headroomValue.implicitWidth,60*Theme.fontScale)+8;spacing:8
                                    ToggleButton {id:autoHeadroom;objectName:"peqAutoHeadroom";text:"自动余量";checked:panel.config.autoHeadroom;onClicked:equalizerBridge.setOption("autoHeadroom",checked)}
                                    UiText {id:headroomValue;objectName:"peqHeadroomHint";text:panel.config.autoHeadroom?"−"+equalizerBridge.headroom.toFixed(1)+" dB":"手动前级";basePixelSize:12;color:Theme.secondary}
                                }
                            }
                            EqChoice {
                                id:filterChoice;objectName:"peqBandType";width:modeTools.inlineTools?modeTools.width-modeTools.controlsMinimum-8:modeTools.width
                                Accessible.name:"滤波类型";model:["峰值","低架","高架","高通","低通","陷波"];currentIndex:panel.band.type
                                onActivated:index=>equalizerBridge.setBand(panel.selectedBand,"type",index)
                            }
                        }
                        GridLayout {
                            id:parameters;objectName:"peqParameters";Layout.fillWidth:true;columnSpacing:10;rowSpacing:8
                            columns:width>=4*Math.round(146*Theme.fontScale)+30?4:width>=2*Math.round(146*Theme.fontScale)+10?2:1
                            RowLayout {
                                Layout.fillWidth:true;Layout.minimumWidth:0;spacing:6
                                UiText{text:"频率";basePixelSize:12;color:Theme.secondary}
                                EqValue{objectName:"peqFrequency";Layout.fillWidth:true;Layout.minimumWidth:0;from:20;to:20000;amount:panel.band.frequency;unit:" Hz";Accessible.name:"频率，赫兹";onEdited:amount=>equalizerBridge.setBand(panel.selectedBand,"frequency",amount)}
                            }
                            RowLayout {
                                Layout.fillWidth:true;Layout.minimumWidth:0;spacing:6
                                UiText{text:"增益";basePixelSize:12;color:Theme.secondary}
                                EqValue{objectName:"peqGain";Layout.fillWidth:true;Layout.minimumWidth:0;from:-180;to:180;scaleFactor:10;amount:panel.band.gain;unit:" dB";enabled:panel.band.type<3;Accessible.name:"频段增益，分贝";onEdited:amount=>equalizerBridge.setBand(panel.selectedBand,"gain",amount)}
                            }
                            RowLayout {
                                Layout.fillWidth:true;Layout.minimumWidth:0;spacing:6
                                UiText{text:"Q 值";basePixelSize:12;color:Theme.secondary}
                                EqValue{objectName:"peqQ";Layout.fillWidth:true;Layout.minimumWidth:0;from:10;to:1800;scaleFactor:100;stepSize:5;amount:panel.band.q;Accessible.name:"Q 值";onEdited:amount=>equalizerBridge.setBand(panel.selectedBand,"q",amount)}
                            }
                            RowLayout {
                                Layout.fillWidth:true;Layout.minimumWidth:0;spacing:6
                                UiText{text:"前级";basePixelSize:12;color:Theme.secondary}
                                EqValue{objectName:"peqPreamp";Layout.fillWidth:true;Layout.minimumWidth:0;from:-240;to:120;scaleFactor:10;amount:panel.config.preamp;unit:" dB";Accessible.name:"整套配置的前级增益，分贝";onEdited:amount=>equalizerBridge.setOption("preamp",amount)}
                            }
                        }
                    }
                }
                UiText{objectName:"peqStatus";Layout.fillWidth:true;visible:text.length>0;text:equalizerBridge.status;textFormat:Text.PlainText;wrapMode:Text.Wrap;color:Theme.accent;basePixelSize:12}
            }
        }
    }
}
