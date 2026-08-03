
## Group C — write probe, started 2026-08-03T01:16:35.344313Z

### `CreateWidgetBlueprint`
```json
args   : {"folderPath": "/Game/UI/_Probe", "assetName": "WBP_Probe", "parentClass": {"refPath": "/Script/Breachpoint.BRActivatableWidget"}}
result : {"returnValue":{"refPath":"/Game/UI/_Probe/WBP_Probe.WBP_Probe"}}
```

WBP handle: `{'refPath': '/Game/UI/_Probe/WBP_Probe.WBP_Probe'}`

### `GetWidgets`
```json
args   : {"widgetBlueprint": {"refPath": "/Game/UI/_Probe/WBP_Probe.WBP_Probe"}}
result : {"returnValue":{"info":{"parentClass":{"refPath":"/Script/Breachpoint.BRActivatableWidget"},"rootWidgetClass":"None","widgetCount":0,"inheritedWidgetCount":0,"namedSlotCount":0},"widgets":[]}}
```

### `AddWidget`
```json
args   : {"widgetBlueprint": {"refPath": "/Game/UI/_Probe/WBP_Probe.WBP_Probe"}, "widgetClass": {"refPath": "/Script/UMG.CanvasPanel"}, "widgetDisplayName": "RootCanvas", "parentWidget": null, "childIndex": -1}
result : {"returnValue":{"widget":{"refPath":"/Game/UI/_Probe/WBP_Probe.WBP_Probe:WidgetTree.RootCanvas"},"parent":"None","slot":"None","namedSlotHost":"None","widgetClassPath":{"refPath":"/Script/UMG.CanvasPanel"},"widgetName":"RootCanvas","bIsVariable":false,"bInherited":false,"uIComponents":[]}}
```

### `AddWidget`
```json
args   : {"widgetBlueprint": {"refPath": "/Game/UI/_Probe/WBP_Probe.WBP_Probe"}, "widgetClass": {"refPath": "/Script/UMG.Border"}, "widgetDisplayName": "TestBorder", "parentWidget": {"refPath": "/Game/UI/_Probe/WBP_Probe.WBP_Probe:WidgetTree.RootCanvas"}, "childIndex": -1}
result : {"returnValue":{"widget":{"refPath":"/Game/UI/_Probe/WBP_Probe.WBP_Probe:WidgetTree.TestBorder"},"parent":{"refPath":"/Game/UI/_Probe/WBP_Probe.WBP_Probe:WidgetTree.RootCanvas"},"slot":{"refPath":"/Game/UI/_Probe/WBP_Probe.WBP_Probe:WidgetTree.RootCanvas.CanvasPanelSlot_0"},"namedSlotHost":"None","widgetClassPath":{"refPath":"/Script/UMG.Border"},"widgetName":"TestBorder","bIsVariable":false,"bInherited":false,"uIComponents":[]}}
```

### Handles returned by AddWidget
```json
canvas : {"widget": {"refPath": "/Game/UI/_Probe/WBP_Probe.WBP_Probe:WidgetTree.RootCanvas"}, "parent": "None", "slot": "None", "namedSlotHost": "None", "widgetClassPath": {"refPath": "/Script/UMG.CanvasPanel"}, "widgetName": "RootCanvas", "bIsVariable": false, "bInherited": false, "uIComponents": []}
border : {"widget": {"refPath": "/Game/UI/_Probe/WBP_Probe.WBP_Probe:WidgetTree.TestBorder"}, "parent": {"refPath": "/Game/UI/_Probe/WBP_Probe.WBP_Probe:WidgetTree.RootCanvas"}, "slot": {"refPath": "/Game/UI/_Probe/WBP_Probe.WBP_Probe:WidgetTree.RootCanvas.CanvasPanelSlot_0"}, "namedSlotHost": "None", "widgetClassPath": {"refPath": "/Script/UMG.Border"}, "widgetName": "TestBorder", "bIsVariable": false, "bInherited": false, "uIComponents": []}
```

#### C5a — list_properties on the SLOT (UCanvasPanelSlot)
### `list_properties`
```json
args   : {"object": {"refPath": "/Game/UI/_Probe/WBP_Probe.WBP_Probe:WidgetTree.RootCanvas.CanvasPanelSlot_0"}}
result : **FAILED** Function "list_properties", input param "instance" is required by the function input schema Json, but is missing from the incoming function input params Json.
Function schema Json -
{"type":"object","properties":{"instance":{"type":"object","title":"/Script/CoreUObject.Object","description":"Represents a reference to a UObject or UClass.","properties":{"refPath":{"type":"string","description":"The reference stored as a soft path string."}},"required":["refPath"]}},"required":["instance"]}
Function input params Json -
{"object":{"refPath":"/Game/UI/_Probe/WBP_Probe.WBP_Probe:WidgetTree.RootCanvas.CanvasPanelSlot_0"}}
```

#### C5b — SET Anchors / Offsets / Alignment on the slot
### `set_properties`
```json
args   : {"object": {"refPath": "/Game/UI/_Probe/WBP_Probe.WBP_Probe:WidgetTree.RootCanvas.CanvasPanelSlot_0"}, "properties": {"LayoutData": "(Anchors=(Minimum=(X=0.000000,Y=0.000000),Maximum=(X=0.000000,Y=0.000000)),Offsets=(Left=69.000000,Top=138.000000,Right=349.000000,Bottom=510.000000),Alignment=(X=0.000000,Y=0.000000))"}}
result : **FAILED** Function "set_properties", input param "instance" is required by the function input schema Json, but is missing from the incoming function input params Json.
Function schema Json -
{"type":"object","properties":{"instance":{"type":"object","title":"/Script/CoreUObject.Object","description":"Represents a reference to a UObject or UClass.","properties":{"refPath":{"type":"string","description":"The reference stored as a soft path string."}},"required":["refPath"]},"values":{"type":"string"}},"required":["instance","values"]}
Function input params Json -
{"object":{"refPath":"/Game/UI/_Probe/WBP_Probe.WBP_Probe:WidgetTree.RootCanvas.CanvasPanelSlot_0"},"properties":{"LayoutData":"(Anchors=(Minimum=(X=0.000000,Y=0.000000),Maximum=(X=0.000000,Y=0.000000)),Offsets=(Left=69.000000,Top=138.000000,Right=349.000000,Bottom=510.000000),Alignment=(X=0.000000,Y=0.000000))"}}
```

#### C5c — read it back
### `get_properties`
```json
args   : {"object": {"refPath": "/Game/UI/_Probe/WBP_Probe.WBP_Probe:WidgetTree.RootCanvas.CanvasPanelSlot_0"}, "property_names": ["LayoutData"]}
result : **FAILED** Function "get_properties", input param "instance" is required by the function input schema Json, but is missing from the incoming function input params Json.
Function schema Json -
{"type":"object","properties":{"instance":{"type":"object","title":"/Script/CoreUObject.Object","description":"Represents a reference to a UObject or UClass.","properties":{"refPath":{"type":"string","description":"The reference stored as a soft path string."}},"required":["refPath"]},"properties":{"type":"array","items":{"type":"string"}}},"required":["instance","properties"]}
Function input params Json -
{"object":{"refPath":"/Game/UI/_Probe/WBP_Probe.WBP_Probe:WidgetTree.RootCanvas.CanvasPanelSlot_0"},"property_names":["LayoutData"]}
```

#### C5d — set an appearance property on the WIDGET (Border tint)
### `set_properties`
```json
args   : {"object": {"refPath": "/Game/UI/_Probe/WBP_Probe.WBP_Probe:WidgetTree.TestBorder"}, "properties": {"BrushColor": "(R=0.020000,G=0.031000,B=0.047000,A=1.000000)"}}
result : **FAILED** Function "set_properties", input param "instance" is required by the function input schema Json, but is missing from the incoming function input params Json.
Function schema Json -
{"type":"object","properties":{"instance":{"type":"object","title":"/Script/CoreUObject.Object","description":"Represents a reference to a UObject or UClass.","properties":{"refPath":{"type":"string","description":"The reference stored as a soft path string."}},"required":["refPath"]},"values":{"type":"string"}},"required":["instance","values"]}
Function input params Json -
{"object":{"refPath":"/Game/UI/_Probe/WBP_Probe.WBP_Probe:WidgetTree.TestBorder"},"properties":{"BrushColor":"(R=0.020000,G=0.031000,B=0.047000,A=1.000000)"}}
```

### `CompileWidgetBlueprint`
```json
args   : {"widgetBlueprint": {"refPath": "/Game/UI/_Probe/WBP_Probe.WBP_Probe"}}
result : {"returnValue":true}
```

### `GetWidgetDescription`
```json
args   : {"widgetBlueprint": {"refPath": "/Game/UI/_Probe/WBP_Probe.WBP_Probe"}, "startWidget": null, "maxDepth": -1}
result : {"returnValue":{"description":"[0] CanvasPanel RootCanvas\n  [1] Border TestBorder  Slot:/Script/UMG.CanvasPanelSlot'/Game/UI/_Probe/WBP_Probe.WBP_Probe:WidgetTree.RootCanvas.CanvasPanelSlot_0'\n","widgets":[{"widget":{"refPath":"/Game/UI/_Probe/WBP_Probe.WBP_Probe:WidgetTree.RootCanvas"},"parent":"None","slot":"None","namedSlotHost":"None","widgetClassPath":{"refPath":"/Script/UMG.CanvasPanel"},"widgetName":"RootCanvas","bIsVariable":false,"bInherited":false,"uIComponents":[]},{"widget":{"refPath":"/Game/UI/_Probe/WBP_Probe.WBP_Probe:WidgetTree.TestBorder"},"parent":{"refPath":"/Game/UI/_Probe/WBP_Probe.WBP_Probe:WidgetTree.RootCanvas"},"slot":{"refPath":"/Game/UI/_Probe/WBP_Probe.WBP_Probe:WidgetTree.RootCanvas.CanvasPanelSlot_0"},"namedSlotHost":"None","widgetClassPath":{"refPath":"/Script/UMG.Border"},"widgetName":"TestBorder","bIsVariable":false,"bInherited":false,"uIComponents":[]}]}}
```


**Probe complete. C7 (delete /Game/UI/_Probe) runs separately and is asserted.**
