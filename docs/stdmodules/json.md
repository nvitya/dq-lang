# `json`

The `json` module provides a mutable JSON tree with parsing and serialization.

```dq
use json

var j <- OJson()
j.AddStr("name", "dq")
j.AddNum("version", 1)
PrintLn("{}", [j.pretty_json])
```

Errors raise `EJson`.

## Node Kinds

```dq
enum NJsonNodeKind = (nkNull, nkBool, nkNumber, nkString, nkObject, nkArray)
```

An `OJson` node stores its kind, optional scalar value, parent pointer, name, and
children.

## Building Trees

```dq
var root <- OJson()
root.AddStr("title", "example")
root.AddBool("ok", true)

var items : OJson = root.AddArr("items")
items.AddStr("", "first")
items.AddNum("", 2)

var cfg : OJson = root.ForcePath("settings/theme")
cfg.as_str = "dark"
```

| Method | Meaning |
| --- | --- |
| `AddNull(name)` | append null child |
| `AddBool(name, value)` | append boolean child |
| `AddStr(name, value)` | append string child |
| `AddNum(name, value)` | append number child |
| `AddObj(name)` | append object child |
| `AddArr(name)` | append array child |
| `Clear()` | delete all children |

For array items, the name is usually `""`.

## Reading And Writing Values

| Property / method | Meaning |
| --- | --- |
| `.name` | node name, or `"0"` for root |
| `.parent` | parent node |
| `.as_str` | read/write scalar as string |
| `.as_num` | read/write scalar as float |
| `.as_bool` | read/write scalar as bool |
| `.is_null` | true for JSON null |
| `.json` | compact JSON string |
| `.pretty_json` | indented JSON string |
| `.length` | child count |
| `.child[index]` | child by index, or null |

```dq
var title : str = root.ChildByName("title").as_str
```

## Finding Nodes

```dq
var jv : OJson

// most practical:
if root.Find("settings.theme", jv):
    PrintLn("theme={}", [jv.as_str])
endif

// traditional:
jv = root.FindNode("settings/theme")
if jv <> null:
    PrintLn("theme={}", [jv.as_str])
endif

```

Paths can use `.` or `/` between object names. `ForcePath` creates missing object
nodes.

## Parsing And Files

```dq
var j <- OJson()
j.Parse('{"answer":42}')
j.LoadFromFile("config.json")
j.SaveToFile("out.json", true)
```

The root JSON text must be an object or array. Strings support JSON escapes,
including Unicode escapes and surrogate pairs.

