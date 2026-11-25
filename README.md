# TPSdemo / 糖块突围

基于 Unreal Engine 5.7 的第三人称糖果射击 Demo，包含：

- 第三人称移动、瞄准、武器与敌人 AI
- 6×6 背包：拾取、使用、丢弃
- 元素附着与反应：火 / 水 / 冰，蒸发 / 融化 / 冻结 / 碎冰
- 前台流程、战斗、暂停与结算界面
- 自动化测试覆盖战斗、背包、元素反应和流程

## 操作

| 按键 | 功能 |
| --- | --- |
| `WASD` | 移动 |
| 鼠标 | 瞄准；左键开火 |
| `Q` | 切换枪械元素（火 / 水 / 冰） |
| `R` | 装填 |
| `E` | 拾取地面物品 / 交互 |
| `B` | 打开 6×6 背包 |
| `ESC` | 关闭背包；战斗中打开暂停菜单 |
| `Tab` | 战斗暂停菜单 |

## 项目结构

- `Source/TPSdemo/CandyFight/Elements/`：元素附着、反应规则、冻结状态
- `Source/TPSdemo/CandyFight/Weapons/`：枪械、弹道、元素快照与反应接入
- `Source/TPSdemo/CandyFight/Inventory/`：6×6 背包、物品定义与地面拾取
- `Source/TPSdemo/CandyFight/UI/`：原生 C++ UMG 背包、准星、HUD
- `Source/TPSdemo/CandyFight/Tests/`：自动化测试
- `Docs/`：游戏流程、背包、元素反应的设计与验证文档

## 打开与运行

1. 使用 Unreal Engine 5.7 打开 `TPSdemo.uproject`。
2. 默认地图为前台菜单 `L_CandyFight_Frontend`；点击运行后从“开始游戏”进入备战，再进入战斗。
3. 直接验证元素反应可打开 `L_CandyFight`，正前方有 200 HP 靶子；按 `Q` 切换元素后射击。
4. 直接验证敌人与背包可打开 `L_CandyFight_AI_Test`。

## 构建与测试

生成工程文件后，使用 `TPSdemoEditor` / Win64 / Development 编译。

命令行运行全部 CandyFight 自动化测试（将 `UnrealEditor-Cmd.exe` 替换为你的引擎安装路径）：

```powershell
& 'UnrealEditor-Cmd.exe' `
  '.\TPSdemo.uproject' `
  -ExecCmds="Automation RunTests CandyFight" -unattended -nullrhi -nosplash `
  -TestExit="Automation Test Queue Empty" -log
```

元素反应测试：

```powershell
& 'UnrealEditor-Cmd.exe' `
  '.\TPSdemo.uproject' `
  -ExecCmds="Automation RunTests CandyFight.Elements" -unattended -nullrhi -nosplash `
  -TestExit="Automation Test Queue Empty" -log
```

## 文档

- [游戏流程原型：阅读与验证](Docs/游戏流程原型_阅读与验证.md)
- [背包功能：使用与验证](Docs/背包功能_使用与验证.md)
- [元素反应系统](Docs/元素反应系统.md)

## 说明

仓库保留了项目工程、Content 与可复现的源码；`Binaries/`、`Intermediate/`、`Saved/`、
`DerivedDataCache/` 和 IDE 生成文件已通过 `.gitignore` 排除。
`Plugins/` 中包含可选的 MCP 自动化与 Visual Studio 工具插件，可按需保留或删除。
