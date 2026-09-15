# 合并策略分析（VS2022 + Qt6.0 重写版）

## 〇、目标与起点

**目标仓库**：`funq\`。执行合并前的基线是 `c6c0129`；当前 HEAD 是 `1859c5a`。`c6c0129` 是本轮操作开始时的基线，不是当前 HEAD。

**编译环境目标**：
- **编译器**：MSVC v143（VS2022 17.0 或更新）
- **Qt 版本**：Qt 6.0（主兼容 Qt5.12+）
- **平台**：Windows x64
- **CMake**：3.20+

**funq 当前真实状态**（基于 `Test-Path`、`get-content`、`git show` 实地核对，不是版本号猜的）：

| 路径 | 是否存在 | 备注 |
|------|:--------:|------|
| `client/funq/noseplugin.py` | 存在 | **仍用 nose** |
| `client/funq/pytestplugin.py` | 存在 | pytest 插件；保留 nose 插件兼容性 |
| `client/funq/pytest_plugin.py` | **不存在** | — |
| `tests-functionnal/funq-test-qml-app/` | 存在 | PR #27 的递归 children 测试程序 |
| `tests-functionnal/funq-qml-test-app/` | 存在 | focus cycling 测试程序 |
| `server/libFunq/player_commands_gitems.cpp` | 存在 | PR #17 拆分结果 |
| `server/libFunq/player_utils.cpp` | 存在 | PR #17 拆分结果 |
| `server/libFunq/WindowsInjector.cpp` | 存在 | — |
| `server/libFunq/ldPreloadInjector.cpp` | 存在 | — |
| `.github/workflows/main.yml` | 存在 | — |
| `.travis.yml` | **不存在** | 已删 |
| `server/CMakeLists.txt` | 存在 | 已支持 Qt5/Qt6 自动检测 |

**funq 当前 Qt6 兼容性**（基于 `git show` master commits）：

master 上已经合并了以下 LibrePCB fork 路径的 Qt6 兼容 commits：

| commit | 说明 |
|--------|------|
| `bbb435c` Support building with Qt6 | CMakeLists 加 Qt5/Qt6 检测，C++11/Qt6→C++17 切换 |
| `a341ecb` Fix failed functional test on Qt6 | 功能测试修复 |
| `b9adcfc` Merge PR #8 (fix-qt6-quit) | qApp->quit() → qApp->exit() |
| `ba8ac1e` Merge PR #7 (LibrePCB/qt6) | pick.cpp 用 QScreen 替代 QDesktopWidget 等 |

`player.cpp` 现有的 Qt6 兼容分支：

```cpp
#if QT_VERSION_MAJOR >= 6
    #include <QScreen>
#else
    #include <QDesktopWidget>
#endif
```

```cpp
// Qt5: path.swap(); Qt6: path.swapItemsAt()
for (int k = 0, s = path.size(), max = (s / 2); k < max; k++) {
#if QT_VERSION_MAJOR >= 6
    path.swapItemsAt(k, s - (1 + k));
#else
    path.swap(k, s - (1 + k));
#endif
}
```

**重要发现**：funq 当前 master 已经有 Qt6 兼容基础。本轮又合入了 pytest 插件、`player.cpp` 拆分、两个 QML 测试程序、`quick_item_children`、`quick_items_find` 和 QtQuick 交互命令。`noseplugin.py` 没有删除，以便旧测试继续运行。

---

## 一、各 fork 真实改动 + Qt6 兼容性分析

### 1.1 fork 与 funq 的 base 关系（`merge-base` 实地核对）

| 仓库 | HEAD | base | 与 funq (c6c0129) 关系 |
|------|------|------|------------------------|
| funq | c6c0129 | — | 完全一致（与 parkouss/master 同步） |
| grandag | 8d5c9eb | ce12212 | **merge-base 为空**——独立 fork，未拉过 parkouss/funq 任何改进 |
| jer-chao | 3a0173f | c6c0129 | **基于 c6c0129 + 1 commit**（Compatible with VS 2019, Qt 5.15, 64-bit Windows version.） |
| retsubhtym | 16b5bed | ce12212 | **merge-base 为空**——独立 fork，未拉过 parkouss/funq 任何改进，独立演进到 1.5.2 |

**纠正之前错误**：`v1.2.0` 不是 git 引用，只是 `__version__` 字符串。grandag 和 retsubhtym 是 2014 年独立 fork。

### 1.2 grandag —— 21 文件，纯 Python + 简单 C++ 改动

文件清单见 merge.md 之前版本。**Qt6 兼容性**：仅改动 player.cpp/h 的 list_actions → list_commands 重命名和 widgets_list recursive 参数，**不涉及 Qt6 API 差异**。Python 端的 f-string、object 基类、super() 无参等改动与 Qt6 无关。**结论：Qt6 兼容，无冲突。**

### 1.3 jer-chao —— 3 源码 + 6 VS 工程 + 大量编译产物

#### 源码改动（3 个文件）

**`server/CMakeLists.txt`**（+16/-2）：
```cmake
# Qt5/Qt6 自动检测已有，不变
if(MSVC)
  add_compile_options(/W4)
  add_compile_options(/permissive-)  # 启用标准符合性检查
else()
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()
if(BUILD_DISALLOW_WARNINGS)
  if(MSVC)
    add_compile_options(/WX)  # MSVC 警告即错误
  else()
    add_compile_options(-Werror)
  endif()
endif()
```
**Qt6 兼容性**：✅ MSVC 分支标准做法，VS2022 完全支持。

**`server/libFunq/CMakeLists.txt`**（+4/-0）：
```cmake
if(WIN32)
    set_target_properties(Funq PROPERTIES OUTPUT_NAME "libFunq")
endif()
```
**Qt6 兼容性**：✅ Windows DLL 命名。

**`server/setup.py`**（+66/-17）：Debug 模式灵活切换（+FUNQ_DEBUG 环境变量）。**与 Qt6 无关**。

**致命问题**：`server/setup.py` 硬编码 VS2019 生成器：
```python
cmake_cmd.extend(['-G', 'Visual Studio 16 2019', '-A', 'x64'])
```
**对 VS2022 目标必须改为 `'Visual Studio 17 2022'`**（这是 jer-chao 唯一不能直接合并的改动）。

#### 新增 VS 工程文件（6 个）

- `server/funq.sln`（67 行）
- `server/ALL_BUILD.vcxproj`（189 行）+ `.filters`（8 行）
- `server/ZERO_CHECK.vcxproj`（179 行）+ `.filters`（13 行）
- `server/libFunq/Funq.vcxproj`（506 行）+ `.filters`（93 行）
- `server/libFunq/FunqStatic.vcxproj`（441 行）+ `.filters`（93 行）

**注意**：这些是 jer-chao 在本地用 VS2019 编译生成的工程文件，**没有针对 VS2022 重新生成**。直接合并到 VS2022 目标会有问题（VS2022 可能不识别 vcxproj 里的某些属性）。

**建议**：**不合并 jer-chao 的 VS 工程文件**，而是用 VS2022 打开 CMake 项目（CMakeLists.txt 已经是 Qt5/Qt6 兼容），让 VS2022 自行生成新的 vcxproj。

#### 编译产物（**完全跳过**）

`.tlog`、`.obj`、`.recipe`、`.lastbuildstate`、`Release/`、`x64/Release/`、`Funq.dir/`、`FunqStatic.dir/` 等——VS 编译临时产物，不进版本库。

### 1.4 retsubhtym —— 18 文件改 + 5 文件新增，服务端大改

#### pick.cpp（541 行删 + 7 行增）—— 大规模重写

retsubhtym 把原 pick.cpp（基于 `Pick::handleEvent`、`print_object_props`、`PickFormatter::handle`）**整文件删掉重写**。

**新增文件结构**：
```cpp
#include <QApplication>
#include <QBackingStore>     // Qt5/Qt6 通用
#include <QExposeEvent>      // Qt5/Qt6 通用
#include <QGuiApplication>
#include <QPainter>
#include <QPaintEvent>
#include <QQuickItem>        // QT_QUICK_LIB 守卫
#include <QQuickWindow>      // QT_QUICK_LIB 守卫
#include <QResizeEvent>
#include <QScopedPointer>
#include <QSurface>          // Qt5/Qt6 通用
#include <QSurfaceFormat>    // Qt5/Qt6 通用
#include <QWindow>

// 新增 WindowHighlightOverlay 类（QWidget 派生，用于高亮显示）
// 重写 Pick::mouse(QPoint globalPos, bool buttonsOnly, ...) 使用 QGuiApplication::topLevelWindows()
```

**Qt6 兼容性**：✅ 用 QGuiApplication、QSurface、QBackingStore 等 Qt5/Qt6 通用 API。**比 master 当前实现（仍用 QApplication::topLevelWidgets）的 Qt6 兼容性更好**。这是 retsubhtym 最有价值的改动。

**问题**：与 funq master 当前的 pick.cpp 实现冲突（master 也是 Qt6 兼容但实现路径不同）。

#### player.cpp（224 行删 + 6 行增）—— 大规模重写

retsubhtym 的 player.cpp 也有 Qt6 兼容代码（用 `QT_VERSION_MAJOR >= 6` 分支）：

```cpp
#if QT_VERSION_MAJOR >= 6
    #include <QScreen>
#else
    #include <QDesktopWidget>
#endif

// Qt5/Qt6 mouse_click 分支
template <class T>
void mouse_click(T * w, const QPoint & pos, Qt::MouseButton button) {
#if QT_VERSION_MAJOR >= 6
    QTest::mouseClick(w, button, Qt::NoModifier, pos, 10);
#else
    QPoint global_pos = w->mapToGlobal(pos);
    qApp->postEvent(w, new QMouseEvent(QEvent::MouseButtonPress, pos, global_pos, button, Qt::NoButton, Qt::NoModifier));
    qApp->postEvent(w, new QMouseEvent(QEvent::MouseButtonRelease, pos, global_pos, button, Qt::NoButton, Qt::NoModifier));
#endif
}

// QML QuickItem 命令新增
QtJson::JsonObject Player::quick_items_find(const QtJson::JsonObject & command) { ... }
QtJson::JsonObject Player::quick_item_find_by_property(...) { ... }
QtJson::JsonObject Player::quick_item_key_click(...) { ... }
QtJson::JsonObject Player::quick_item_key_press(...) { ... }
```

**Qt6 兼容性**：✅ 自己有 Qt5/Qt6 分支。

**问题**：与 master 现有的 Qt6 兼容代码（路径相同但实现略不同）**冗余**。合并时**保留 master 版本，丢弃 retsubhtym 的 Qt6 分支代码**——只取 retsubhtym 独有的 QML 集成代码。

#### player.h —— 仅 +4 行

新增 4 个命令声明：`quick_items_find`、`quick_item_find_by_property`、`quick_item_key_click`、`quick_item_key_press`。

#### objectpath.cpp/h —— 仅新增 `findQuickItems`

retsubhtym 的 `ObjectPath::findQuickItems(QQuickWindow*, QString)`（多重匹配 QQuickItem）。用 Qt5/Qt6 通用 API。✅ Qt6 兼容。

#### funq.cpp —— `eventFilter` 改为返回 handled

```cpp
// 原版
bool Funq::eventFilter(QObject * receiver, QEvent * event) {
    m_pick->handleEvent(receiver, event);
    return false;
}

// retsubhtym
bool Funq::eventFilter(QObject * receiver, QEvent * event) {
    bool handled = m_pick->handleEvent(receiver, event);
    if (handled) { event->accept(); }
    return handled;
}
```

**Qt6 兼容性**：✅ Qt5/Qt6 通用。**这是 retsubhtym 重要的逻辑改进**——pick 命中时正确接受事件。

#### models.py —— 新增 5 个方法

```python
Widget.click(xpos=-1, ypos=-1)  # 加坐标参数
Widget.dclick()
Widget.key_click(key, modifiers)
Widget.key_press(key, modifiers, duration)
Widget.find_item_by_property(property_name, property_value)
Widget.items(alias=None, path=None)
```

#### tools.py —— 新增 QtKeyDict

```python
QtKeyDict = {
    "Key_Space": "0x20",
    "Key_Exclam": "0x21",
    # ... 完整 Qt 键位映射（Key_0 到 Key_Z、Key_F1 到 Key_F35、修饰键等）
}
```

**Qt6 兼容性**：✅ Python 端，与 Qt 版本无关。

#### 其他 Python 改动

`client/funq/__init__.py`、`server/funq_server/__init__.py`：版本号 1.2.0 → 1.5.1。
`client/setup.py`：加 pytest11 entry point。
`README.rst`、`client/doc/localise_widget.rst`：文档更新。
`.gitignore`：扩展 git ignore 规则（278/17 大删，但**主要新增应该是构建产物排除规则**）。
`.clang-format`：格式化调整。

#### 新增文件（5 个）

- `client/funq/pytestplugin.py`（203 行）
- `tests-functionnal/funq-qml-test-app/CMakeLists.txt`（40 行）
- `tests-functionnal/funq-qml-test-app/main.cpp`（41 行）
- `tests-functionnal/test_focus_cycling.py`（25 行）
- `tests-functionnal/test_focus_cycling_qml.py`（16 行）

---

## 二、PR 改动分析

### 2.1 PR #7 (floufen) —— "Recursively iterate throught GItems data"

**真实情况**：2 个 commit，**7 个文件**（pr.md 里"81 文件"是 base 跨度造成的假象）。

#### Commit 1 (40e417e) "At Widget creation, recursively iterate throught GItems data"

`client/funq/models.py`（+27/-1）：新增内部函数 `get_recursive_items(data)`。

**问题**：函数末尾 `return []` 是 dead code；递归调用语义不一致（首次传 `[data]`，递归时传 `items['items']`），逻辑有 bug。

#### Commit 2 (f33f29a) "Add a mouse move feature"

**6 个文件改动，+89/-28**：

1. `client/funq/client.py`（+25）：新增 `FunqClient.mouse_move(ref_widget, src_pos, dest_pos, key_press)` 方法，发送 `widget_mouse_move` 命令。

2. `client/funq/models.py`（+15/-28）：**删除 commit 1 的整个 `get_recursive_items` 块和 GItem 条件判断**，恢复成原始的列表推导。同时在 widget 类新增 `mouse_move(self, src_pos, dest_pos)` 方法。

3. `server/libFunq/dragndropresponse.cpp`（+1/-1）：把默认参数 `=4` 从 .cpp 移到 .h 声明。

4. `server/libFunq/dragndropresponse.h`（+3）：声明 `calculate_drag_n_drop_moves`（带 `=4`）和 `pointFromString`。

5. `server/libFunq/player.cpp`（+43）：
   - 新增辅助函数 `mouse_move(w, srcpos, destpos)`：把位移分成多步，每步 post QMouseEvent，**最后调一次 mouse_click**。
   - 新增命令处理器 `Player::widget_mouse_move(command)`：解析 srcpos/destpos 字符串。

6. `server/libFunq/player.h`（+1）：声明 `widget_mouse_move` 命令。

**关键发现**：
- **作者在 commit 2 自己撤回 commit 1**——PR 标题"递归 GItem"是假的。
- **patch 不完整**：`pointFromString` 只声明不定义（缺 .cpp 实现），直接合上会**编译失败**。
- **bug**：`widget_mouse_move` 里 destPos 是 srcPos + 增量，与 docstring 描述的"ending position"不一致。
- **bug**：`key_press` 参数被客户端发出但服务端**完全没用**。

**Qt6 兼容性**：用 `QPoint`（不是 QPointF），`QMouseEvent` 构造调用。Qt6 中 `QMouseEvent` 构造 API 有变化（QPointF 替代 QPoint），**直接用此 patch 在 Qt6 上编译可能失败**。master 当前 mouse_click/mouse_dclick 已有 Qt5/Qt6 分支，但 PR #7 的 mouse_move 没有分支——**需要在合并时加 `#if QT_VERSION_MAJOR >= 6` 分支**。

**合并建议**：**丢弃整个 PR #7**。理由：
1. patch 不完整（缺 pointFromString 实现）
2. PR 标题是撤回的承诺
3. mouse_move 的 key_press 参数无效
4. 直接合到 Qt6 上需要额外修补

如果坚持要 mouse_move 功能，**从 retsubhtym 抄 `Widget.mouse_move()` 实现**（retsubhtym 已经有 Qt5/Qt6 双兼容版本，且通过 `QTest::mouseClick` 实现更标准）。

### 2.2 PR #17 (parkouss) —— "refactor player.h and player.cpp"

**1 个 commit**（base=bf398436 release-1.1.5）：player.cpp 从 816 行拆成 7 个文件，纯结构重构。

拆分结构：

| 文件 | 行数 | 内容 |
|------|-----:|------|
| `player.cpp` | 307 | 主类 |
| `player_commands_gitems.cpp` | 153 | GItems 相关 |
| `player_commands_itemmodel.cpp` | 303 | Model/View |
| `player_commands_quickitems.cpp` | 122 | QuickItem |
| `player_utils.cpp/h` | 90/122 | 工具函数 |

**Qt6 兼容性**：纯结构重构，不改变 API。✅ Qt6 兼容。

**合并难度**：必须手工合并。理由：base=release-1.1.5 跨度大，`git am` 几乎肯定失败；且与 retsubhtym 的 player.cpp 大改、PR #27、grandag 的 widgets_list 改动三方冲突。

### 2.3 PR #27 (rafaeldelucena) —— "Add quick item childen (can be nested)"

**11 个 commits**（base=master HEAD）。

核心 commit (1691774)：
- `server/libFunq/player.cpp`（+41）：新增 `quick_item_children` 命令处理器，递归调用 `dump_quick_items(this, ctx.item->childItems(), ctx.id, recursive, result)`。
- `server/libFunq/player.h`（+1）：声明 `quick_item_children`。
- `client/funq/models.py`（+46）：新增 `QuickItem.children` 属性。
- `client/doc/*.rst`（+4）：文档。

后续 commits（c16fd55、cbe445a、8f7a2b8、35a38df、eb25bd9）：新增 `tests-functionnal/funq-test-qml-app/`（CMakeLists.txt + main.cpp + qml/children.qml + resources.qrc）+ `test_qml_item_children.py`，CMakeLists.txt 加 Qt5/Qt6 双兼容：

```cmake
find_package(Qt6 QUIET COMPONENTS Widgets Quick Qml)
if (NOT Qt6_FOUND)
    find_package(Qt5 REQUIRED COMPONENTS Widgets Quick Qml)
    set(QT_VERSION_MAJOR 5)
else()
    set(QT_VERSION_MAJOR 6)
endif()
# ...
if (QT_VERSION_MAJOR EQUAL 5)
    qt5_add_resources(QT_RESOURCES resources.qrc)
    target_link_libraries(${PROJECT_NAME} Qt5::Widgets Qt5::Quick Qt5::Qml)
elseif (QT_VERSION_MAJOR EQUAL 6)
    qt_add_resources(QT_RESOURCES resources.qrc)
    target_link_libraries(${PROJECT_NAME} Qt6::Widgets Qt6::Quick Qt6::Qml)
endif()
```

**Qt6 兼容性**：✅ `QQuickItem::childItems()` Qt5/Qt6 通用。

**与 master 关系**：master 已经有 `quick_item_find` 和 `quick_item_click`，但**没有 `quick_item_children`**。PR #27 是补充。

### 2.4 PR #60 (dbrgn) —— "Python 2/3 compatibility without 2to3"

**1 个 commit**（base=master HEAD）。23 文件改动。

**核心内容**：用 `six` 同时支持 Py2/Py3，删除 `2to3` 选项。

**冲突**：与 retsubhtym 的 drop Py2 方向相反。**合并建议：丢弃**。

### 2.5 PR #79 (rafaeldelucena) —— "Drop python2 support"

**21 个 commits**（base=master HEAD）。33 文件改动。

**核心改动**：
- 删 `client/funq/noseplugin.py`（202 行）
- 删 `client/funq/tests/test_noseplugin.py`（93 行）
- 新增 `client/funq/pytest_plugin.py`（90 行，下划线）
- 新增 `.github/workflows/github-actions.yml`（88 行）
- 删 `.travis.yml`
- 新增 `requirements.txt`

**Qt6 兼容性**：纯 Python 端，✅ 无关。

**冲突**：与 PR #83 重叠（pytest 迁移方向相同）；与 retsubhtym 的 `pytestplugin.py`（无下划线）冲突。

### 2.6 PR #83 (rafaeldelucena, draft) —— "Migrating from nosetests to pytest"

**18 个 commits**（base=master HEAD）。19 文件改动。

**核心改动**：
- 删 `client/funq/noseplugin.py`
- 删 `client/funq/tests/test_noseplugin.py`
- 新增 `client/requirements-dev.txt`
- `client/setup.py` 加 pytest

**Qt6 兼容性**：纯 Python 端，✅ 无关。

**冲突**：是 PR #79 的子集（删了 nose 但没加 pytest plugin）。**合并建议：丢弃**。

---

## 三、文件冲突交叉表（VS2022 + Qt6.0 视角）

| 文件 | grandag | jer-chao | retsubhtym | PR #7 | PR #17 | PR #27 | PR #60 | PR #79 | PR #83 | Qt6 风险 |
|------|:-------:|:---:|:---:|:---:|:---:|:---:|:---:|:---:|:---:|:--------:|
| `client/funq/noseplugin.py` | M 8/8 | — | — | — | — | — | M | **D** | **D** | — |
| `client/funq/pytestplugin.py` | — | — | **+ 203** | — | — | — | — | — | — | — |
| `client/funq/pytest_plugin.py` | — | — | — | — | — | — | — | **+ 90** | — | — |
| `client/funq/models.py` | M 172/140 | — | M 107/3 | — | — | M | M | M | — | — |
| `client/funq/tools.py` | M 19/20 | — | M 241/3 | — | — | — | M | M | — | — |
| `client/setup.py` | — | — | M 2/1 | — | — | — | M | M | M | — |
| `server/setup.py` | M 2/2 | M 66/17 | — | — | — | — | — | M | M | — |
| `server/CMakeLists.txt` | — | M 16/2 | — | — | — | — | — | — | — | MSVC 分支 |
| `server/libFunq/CMakeLists.txt` | — | M 4/0 | — | — | — | — | — | — | — | WIN32 |
| `server/libFunq/player.cpp` | M 36/6 | — | M 224/6 | M | **拆** | M | — | — | — | **🔴 4 源冲突 + Qt6 兼容** |
| `server/libFunq/player.h` | M 2/1 | — | M 4/0 | M | **拆** | M | — | — | — | — |
| `server/libFunq/pick.cpp` | — | — | M 541/7 | — | — | — | — | — | — | **🔴 大改，与 master Qt6 兼容冗余** |
| `server/libFunq/pick.h` | — | — | M 71/0 | — | — | — | — | — | — | — |
| `server/libFunq/objectpath.cpp` | — | — | M 33/0 | — | — | — | — | — | — | — |
| `server/libFunq/objectpath.h` | — | — | M 1/0 | — | — | — | — | — | — | — |
| `server/libFunq/funq.cpp` | — | — | M 5/2 | — | — | — | — | — | — | — |
| `server/funq_server/runner.py` | M 3/3 | — | M 4/0 | — | — | — | — | M | — | — |
| `tests-functionnal/funq.conf` | — | — | M 19/2 | — | — | M | — | — | — | — |
| `.github/workflows/main.yml` | M 1/1 | — | — | — | — | M 51/51 | — | M | M | — |
| `.gitignore` | M 2/0 | — | M 278/17 | M | — | — | — | — | — | — |

**Qt6 风险标记**：
- 🔴 红色：服务端 C++ 代码，与 Qt6 API 差异相关，必须用 QT_VERSION_MAJOR 分支处理
- 黄色：可能涉及 Qt6（已修复）

---

## 四、VS2022 + Qt6.0 特定调整

### 4.1 jer-chao 的 VS2019 生成器适配

**修改 `server/setup.py`**：

```python
# 原（VS2019）：
cmake_cmd.extend(['-G', 'Visual Studio 16 2019', '-A', 'x64'])

# 改为（VS2022）：
cmake_cmd.extend(['-G', 'Visual Studio 17 2022', '-A', 'x64'])
```

或者用环境变量控制：

```python
# 更灵活的做法
if sys.platform == 'win32':
    cmake_generator = os.environ.get('FUNQ_CMAKE_GENERATOR', 'Visual Studio 17 2022')
    cmake_arch = os.environ.get('FUNQ_CMAKE_ARCH', 'x64')
    cmake_cmd.extend(['-G', cmake_generator, '-A', cmake_arch])
```

### 4.2 CMake 最低版本放宽

funq master 当前 `cmake_minimum_required(VERSION 3.5...3.19)`——VS2022 自带 CMake 通常是 3.20+，上限 3.19 不会触发警告但稍嫌保守。建议放宽到 `3.5...3.27`。

### 4.3 Qt6 组件检查

master 当前 `find_package`：
```cmake
find_package(${QT} REQUIRED COMPONENTS Core Gui Network Widgets Test
             OPTIONAL_COMPONENTS Quick)
```

对 Qt6.0 需要确认所有组件都可用：
- `Qt6::Core` ✅
- `Qt6::Gui` ✅
- `Qt6::Network` ✅
- `Qt6::Widgets` ✅
- `Qt6::Test` ✅（用于 QTest::mouseClick、QTest::keyClick 等）
- `Qt6::Quick` ✅（可选，需要 QML 支持时）

**retsubhtym 的代码**大量使用 `QTest::mouseClick`、`QTest::keyClick`、`QTest::mouseDClick`——这要求 `Test` 组件已被 link。master 当前已经包含。

### 4.4 QML 测试应用 CMakeLists 适配

PR #27 的 `tests-functionnal/funq-test-qml-app/CMakeLists.txt` 已经有 Qt5/Qt6 双兼容（见 2.3 节）。可以直接用。

retsubhtym 的 `tests-functionnal/funq-qml-test-app/CMakeLists.txt` 还没核对，需要检查是否兼容 Qt6。

### 4.5 player.cpp 的 Qt6 兼容代码统一

master 当前和 retsubhtym 都对 `mouse_click`、`mouse_dclick`、`path.swap`、`grabWidget` 等做了 Qt5/Qt6 双兼容。**合并策略：保留 master 的 Qt6 兼容代码，丢弃 retsubhtym 的冗余分支**，只取 retsubhtym 独有的 QML 集成部分（quick_items_find、quick_item_key_click 等）。

---

## 五、推荐合并顺序（VS2022 + Qt6.0 目标，10 阶段）

### 阶段 A：基础设施（VS2022 工具链）

1. **修改 `server/setup.py`**：把 `'Visual Studio 16 2019'` 改为 `'Visual Studio 17 2022'`。
2. **应用 jer-chao 的 `server/CMakeLists.txt` MSVC 分支**（/W4、/permissive-、/WX）。
3. **应用 jer-chao 的 `server/libFunq/CMakeLists.txt` WIN32 OUTPUT_NAME**。
4. **应用 jer-chao 的 `server/setup.py` Debug 模式灵活切换**（66/17 改动）。
5. **丢弃 jer-chao 的所有 VS 工程文件和编译产物**——用 VS2022 打开 CMake 项目自动生成。

### 阶段 B：Python 现代化基线

6. **应用 grandag 的 f-string / 删 object 基类 / super() 无参**（13 个 `client/funq/*.py`）。**必须早于 retsubhtym**，避免三方冲突。

### 阶段 C：服务端 C++ 大改（最复杂，需要 Qt6 兼容性检查）

7. **PR #17 player.cpp 拆分**：手工做等价拆分（不能 `git am patchs/17.patch`，因为 base 是 release-1.1.5）。注意保留 master 已有的 Qt6 兼容分支。
8. **PR #27 QuickItem children**：在拆分后的 `player_commands_quickitems.cpp` 上加 `quick_item_children` 命令 + `childItems()` 递归。
9. **retsubhtym 的 player.cpp 改动**：手工应用 `quick_items_find`、`quick_item_find_by_property`、`quick_item_key_click`、`quick_item_key_press` 等独有命令。**保留 master 的 Qt6 兼容代码，丢弃 retsubhtym 冗余的 Qt5/Qt6 分支**。
10. **retsubhtym 的 pick.cpp.h 重写**：完整重写（包括 `WindowHighlightOverlay` 高亮覆盖层、`QGuiApplication::topLevelWindows()` 使用）。
11. **retsubhtym 的 objectpath.cpp/h**：新增 `findQuickItems` 函数。
12. **retsubhtym 的 funq.cpp `eventFilter` 改进**：返回 handled 值，正确接受事件。
13. **grandag 的 player.cpp/h 改动**（`widgets_list` recursive 参数、`list_actions` → `list_commands`）：独立于拆分，追加。

### 阶段 D：pytest 端到端

14. **应用 retsubhtym 的 `client/funq/pytestplugin.py`**（203 行，无下划线）。
15. **应用 retsubhtym 的 `client/setup.py` pytest11 entry point**。
16. **删除** `client/funq/noseplugin.py`（PR #79/#83 的意图，但 retsubhtym 已隐含）。
17. **删除** `client/funq/tests/test_noseplugin.py`。

### 阶段 E：CI / 文档

18. **重写 `.github/workflows/main.yml`**：基于 pytest 写法（PR #79 的版本是 nosetests 写法），加上 PR #27 的 QML 测试步骤。
19. **跳过 PR #7 全部**（base=release-1.1.5 跨度大，且 patch 不完整，缺 pointFromString 实现）。
20. README.rst 用 retsubhtym 的版本（19/2 小幅调整），加 Qt6 说明。

### 阶段 F：功能性测试应用

21. **应用 PR #27 的 `tests-functionnal/funq-test-qml-app/`**（CMakeLists.txt + main.cpp + qml/children.qml + resources.qrc）+ `test_qml_item_children.py`。CMakeLists 已经有 Qt5/Qt6 双兼容。
22. **应用 retsubhtym 的 `tests-functionnal/funq-qml-test-app/`**（CMakeLists.txt + main.cpp）+ `test_focus_cycling.py` + `test_focus_cycling_qml.py`。先检查 CMakeLists 是否 Qt6 兼容。

### 阶段 G：版本号

23. `client/funq/__init__.py` 和 `server/funq_server/__init__.py` 改为 `__version__ = '1.5.1'`。

### 阶段 H：client Python 端 Qt6 兼容

24. **检查 client/funq/models.py**：retsubhtym 的 QML API 与 Qt6 兼容（Python 端与 Qt 版本无关，但底层调用依赖 C++ 命令）。
25. **检查 client/funq/tools.py**：retsubhtym 的 QtKeyDict 是字符串映射（与 Qt 版本无关）。

### 阶段 I：验证

26. **CMake 配置测试**：
    ```cmd
    cmake -G "Visual Studio 17 2022" -A x64 -S server -B build/server
    ```
27. **检查 Qt6 find_package 是否成功**：确认所有组件（Core、Gui、Network、Widgets、Test、Quick）找到。
28. **编译**：
    ```cmd
    cmake --build build/server --config Release
    ```
29. **修复编译错误**（预期会有的）：
    - `qApp->postEvent(..., new QMouseEvent(...))` 在 Qt6 中需要 `QPointF` 而非 `QPoint`（PR #7 mouse_move 的问题）
    - `qApp` 在多线程/headless 场景下可能为 nullptr（master 已有部分检查）
    - 模板 `mouse_click<T*>` 在 Qt6 的 `QTest::mouseClick` 重载可能有签名变化
30. **运行单元测试**：`make -C server/tests check`
31. **运行功能测试**：
    ```cmd
    cd tests-functionnal/funq-test-qml-app && cmake -G "Visual Studio 17 2022" -A x64 . && cmake --build .
    cd ../.. && nosetests -w client/
    ```
    （或 pytest，看合并阶段 D 是否成功）

### 阶段 J：清理

32. **删除 PR #7 的过时引用**：如果合入过任何 PR #7 部分（如 mouse_move 命名），需要在文档、模型 API 里去掉。
33. **更新 README.rst**：标注 Qt6.0 + VS2022 支持。
34. **更新 CHANGELOG.md**：合并日志。

---

## 六、要丢弃的改动

| 来源 | 原因 |
|------|------|
| **PR #60 全部** | 方向相反（Py2 兼容），retsubhtym 已经 Py3-only |
| **PR #79 的 pytest 集成部分** | retsubhtym 的 `pytestplugin.py` 是超集 |
| **PR #79 的 noseplugin 删除** | 已被 retsubhtym 隐含覆盖 |
| **PR #79 的 .github/workflows** | 是 nosetests 写法，必须重写 |
| **PR #83 全部** | draft、是 PR #79 子集、被覆盖 |
| **PR #7 全部** | base 跨度大 + patch 不完整（缺 pointFromString）+ 作者自撤回递归功能 |
| **jer-chao 的所有 .vcxproj/.filters/.sln** | VS2019 生成，VS2022 需重新生成 |
| **jer-chao 的所有编译产物** | `.tlog`、`.obj`、`.recipe`、`.lib.recipe`、`Release/`、`x64/Release/`、`*.dir/` |
| **grandag 的 noseplugin.py 改动** | 文件被删除 |
| **retsubhtym 的 player.cpp Qt6 兼容冗余分支** | 与 master 的 Qt6 兼容代码重复 |
| **retsubhtym 的 pick.cpp 与 master 冲突的部分** | master 已有 Qt6 兼容，保留 master 版本 |
| **任何 fork 的 `.git/`** | 不在合并范围 |

---

## 七、需要手工介入的环节（按优先级）

### 7.1 优先级 1：服务端 C++ 三方合并（必须手工）

**`server/libFunq/player.cpp/h`**——4 源叠加：
1. master 当前 Qt5/Qt6 双兼容（已合并 LibrePCB fork）
2. grandag 的 `widgets_list` recursive + `list_actions` → `list_commands`
3. PR #17 拆分（player.cpp → 7 个文件）
4. PR #27 `quick_item_children`（+41 行）
5. retsubhtym 大改（QML 集成、QTest::mouseClick 等）

**操作步骤**：
1. 以 master 当前 player.cpp 为起点（已含 Qt6 兼容）
2. 应用 grandag 的 widgets_list recursive 改动（独立，2 个函数）
3. 把 player.cpp 按 PR #17 拆分——注意把 Qt6 兼容分支放到合适的位置（保留 `#if QT_VERSION_MAJOR >= 6`）
4. 在拆分后的 `player_commands_quickitems.cpp` 上加 PR #27 的 `quick_item_children`
5. 在 player.h 加 `quick_item_children` 声明
6. 应用 retsubhtym 独有的命令（`quick_items_find`、`quick_item_find_by_property`、`quick_item_key_click`、`quick_item_key_press`），**用 QTest::mouseClick/QTest::keyClick 替代 qApp->postEvent**（更标准）
7. 应用 retsubhtym 的 `mouse_click<T*>` 模板改造（已有 Qt6 分支）
8. 应用 retsubhtym 的 `findQuickItems` 集成
9. 在 player.h 加新命令声明

### 7.2 优先级 1：pick.cpp 重写（必须手工）

master 当前 pick.cpp 已经 Qt6 兼容但实现简陋。retsubhtym 的 pick.cpp 是完整重写（带 WindowHighlightOverlay 高亮、QQuickItem 集成）。

**决策**：**整体替换为 retsubhtym 版本**（保留 `WindowHighlightOverlay` 类、`QGuiApplication::topLevelWindows()` 调用、`findQuickItemAt` 递归查找）。

但要保留 master 的 Qt6 兼容代码（`#if QT_VERSION_MAJOR >= 6` 分支）——retsubhtym 的版本也要确认有这些分支。

### 7.3 优先级 2：QML 测试应用统一

PR #27 的 `funq-test-qml-app/`（带 `-test-`）和 retsubhtym 的 `funq-qml-test-app/`（少一个 test）路径不同。

**建议**：保留 PR #27 的命名（更清晰），重命名 retsubhtym 的目录，或**保留两个**（它们内容不同：PR #27 测 children，retsubhtym 测 focus cycling）。

### 7.4 优先级 2：grandag 的 widgets_list recursive 合并到 master

master 当前 `widgets_list` 已经支持 `oid` 参数。grandag 加 `recursive` bool 参数。

**操作**：直接在 master 的 `Player::widgets_list` 函数上加 `recursive` 字段解析，调用 `recursive_list_widget(..., recursive)`。

### 7.5 优先级 3：VS 工程文件重新生成

合并完后用 VS2022 打开 `server/CMakeLists.txt`，让 IDE 生成新的 `.vcxproj` 和 `.sln`。这些文件加进 `.gitignore` 不进版本库。

---

## 八、VS2022 + Qt6.0 编译时预期的问题

| 问题 | 原因 | 解决 |
|------|------|------|
| `error C2440: cannot convert from QPoint to QPointF` | Qt6 中 `QMouseEvent` 构造函数要求 `QPointF` | 加 `#if QT_VERSION_MAJOR >= 6` 分支 |
| `unresolved external symbol pointFromString` | PR #7 头文件声明了但没实现 | 不合并 PR #7 |
| `QDesktopWidget` 已弃用 | Qt6 中移除 | 用 `QScreen`（master 已处理） |
| `QApplication::topLevelWidgets()` 行为变化 | Qt6 中返回 `QWidget*` 但 `QWindow` 需要 `QGuiApplication` | 用 `QGuiApplication::topLevelWindows()`（retsubhtym 已用） |
| `path.swap()` 移除 | Qt6 中 `QList::swap` 改名为 `swapItemsAt` | master 已用 `#if QT_VERSION_MAJOR >= 6` |
| `QPixmap::grabWidget` 移除 | Qt6 中改成 `QWidget::grab()` | master 已用 `#if QT_VERSION_MAJOR >= 6` |
| `QPixmap::grabWindow(QApplication::desktop()->winId())` | Qt6 中 `QApplication::desktop()` 移除 | 用 `QScreen::grabWindow()`（master 已用 `#if QT_VERSION_MAJOR >= 6`） |
| `qApp->quit()` 在某些场景不工作 | Qt6 headless | `qApp->exit()`（PR #8 已修复） |
| 基线中的 `quick_item_click` 在 Qt6 返回错误 | 原始实现把 Qt6 直接判为不支持 | 提交 `8ee4d55` 使用 `QTest::mouseClick`/`mouseDClick`，已在 Qt 6.11.2 下编译通过；Qt 6.0 尚未实测 |
| `QT_QUICK_LIB` 未定义 | Qt::Quick 模块未启用 | 确认 CMake `find_package` 找到 Quick |

---

## 九、已执行合并记录（截至 2026-09-15）

当前目标仓库 `HEAD` 是 `1859c5a`。从 `c6c0129` 开始，本轮已经创建并提交以下 18 个提交：

| 提交 | 来源 | 实际内容 |
|------|------|----------|
| `48c2d8e` | `https://github.com/jer-chao/funq` | 在 `server/CMakeLists.txt` 中为 MSVC 使用 `/W4`、`/permissive-` 和可选 `/WX`，避免把 GCC 选项传给 `cl.exe`。 |
| `6e1bb2c` | `https://github.com/jer-chao/funq` | Windows 下把共享库输出名设为 `libFunq`。 |
| `9611363` | `https://github.com/jer-chao/funq` | `server/setup.py` 支持 Debug/Release，并使用 VS2022 生成器。 |
| `1394c66` | `https://github.com/grandag/funq` | 合入 Python 语法现代化改动；没有改变 Qt 行为。 |
| `a8614e6` | `https://github.com/retsubhtym/funq` | 在 `ObjectPath` 中增加 `findQuickItems`，支持同一路径返回多个 QML 项。 |
| `eb61c7f` | `https://github.com/retsubhtym/funq` | 让 `eventFilter` 根据 picker 是否处理事件返回结果。 |
| `40a0ab8` | `https://github.com/grandag/funq` | 增加 `widgets_list` 的 `recursive` 参数，并将服务端命令名从 `list_actions` 改为 `list_commands`。 |
| `405164e` | `https://github.com/retsubhtym/funq` | 合入 QWidget/QQuickItem picker 高亮实现；保留 `pick.h` 中的 `Q_OBJECT`。 |
| `4d45b78` | `https://github.com/parkouss/funq/pull/17` | 按 PR #17 拆分 `player.cpp`，增加 item model、graphics item、QtQuick 和公共工具源文件，并更新 CMake；同时修复拆分后的头文件依赖。 |
| `037d52f` | `https://github.com/parkouss/funq/pull/27` | 增加 `quick_item_children` 服务端命令，支持递归列出 QML 子项。 |
| `8ee4d55` | `https://github.com/retsubhtym/funq` | 增加 QtQuick 单击/双击、键盘点击/按键保持、多路径匹配和按属性查找；同步 Python 客户端接口和 Qt 键值表。只合入了这些独立功能，没有整体替换 retsubhtym 的所有提交。 |
| `22c7709` | `https://github.com/parkouss/funq/pull/27` | 补齐 Python 客户端的 `QuickItem.children()`、递归 `QuickItems` 创建和子项对象构造。 |
| `75ef0d1` | `https://github.com/parkouss/funq/pull/27` | 加入 PR #27 的 QtQuick children 测试程序、资源文件、测试基类配置和递归/非递归测试。 |
| `bebb34e` | `https://github.com/retsubhtym/funq` commit `c3b5e37`、`bc3ce1b` | 增加 `pytest11` 入口和 pytest 插件，保留原 nose 入口；加入 pytest 可选依赖，并保留截图配置的后续安全检查。 |
| `b3ac750` | `https://github.com/retsubhtym/funq` commit `bc3ce1b` | `active_widget` 同时激活 `QWindow` 和 `QWidget`；加入 QWidget 与 QML 两组 focus cycling 测试和配置。 |
| `86806ff` | `https://github.com/parkouss/funq/pull/27`、`https://github.com/retsubhtym/funq` commit `bc3ce1b` | 在 Linux、macOS、Windows CI 中构建 QML 测试程序；使 focus QML 程序兼容 Qt5/Qt6、MSVC，并支持 `--exit-after-startup`。 |
| `438f638` | `https://github.com/retsubhtym/funq` commits `adb6311`、`bc3ce1b`；`https://github.com/parkouss/funq/pull/27` | 版本号更新到 `1.5.1`，补充 pytest/Qt6/VS2022 文档和 CHANGELOG；CHANGELOG 使用 UTF-8 BOM。 |
| `1859c5a` | `https://github.com/parkouss/funq/pull/27` | 让 PR #27 的 QML children 测试程序处理 `--exit-after-startup`，避免 CI 注入检查挂起。 |

### 9.1 已完成的编译验证

- 每个 C++ 阶段都通过 `cmake --fresh` 全新配置，没有把旧缓存当成验证结果。
- 使用 VS2022 MSVC 19.44、CMake 3.30.5、Ninja 1.12.1、Qt 6.11.2；配置日志中的 `configure_exit=0`，构建日志中的 `build_exit=0`。
- CMake 输出确认找到 QtQuick，并生成 `gh/_final_server_build/libFunq/FunqStatic.lib`、`gh/_final_server_build/libFunq/libFunq.dll`、`testLibFunq.exe` 和 `testProtocole.exe`。
- `pick.h` 保留 `Q_OBJECT`；全新配置后 `gh/_final_server_build/libFunq/FunqStatic_autogen/mocs_compilation.cpp` 包含 `moc_pick.cpp`。
- `client/funq`、`server/funq_server` 和新增功能测试 Python 文件均通过 `python -m compileall`。
- QWidget 测试程序、focus QML 测试程序、PR #27 children QML 测试程序均使用 MSVC + Qt 6.11.2 全新配置并编译成功。
- `testProtocole.exe` 运行通过。`testLibFunq.exe` 能生成但在 `QT_QPA_PLATFORM=offscreen` 下初始化阶段长时间无响应，已停止；不能声称该测试运行通过。
- 当前验证使用 Qt 6.11.2，不是 Qt 6.0；Qt 6.0 未安装，因此没有声称已经验证 Qt 6.0 的实际编译。
- 当前 `git status` 干净，未提交的合并代码为零。

### 9.2 合并范围结论

- 本轮计划中决定保留的代码改动已经完成：PR #17、PR #27，以及 grandag、jer-chao、retsubhtym 中与目标功能和 VS2022/Qt6 兼容性直接相关的改动均已提交。
- `noseplugin.py` 没有删除。pytest 已作为新增入口提供，旧 nose 测试仍可继续使用；因此 CI 只增加了 QML 程序构建和注入检查，没有未经验证地把全部 nose 测试改写成 pytest。
- PR #7、PR #60、PR #79、PR #83 的不兼容、重复或不完整部分没有合入；原因和具体冲突仍保留在本文前面的分析中。
- jer-chao 的 VS2019 工程文件和编译产物没有合入；VS2022 使用 CMake 重新生成工程。

---

## 十、不在合并范围

- jer-chao 提交的所有编译产物（`.tlog`、`.recipe`、`Release/`、`x64/`、`*.dir/`）—— 不进版本库。
- 任何 fork 的 `.git/` 内部文件。
- 仓库根目录的 `forks.md`、`pr.md`、`merge.md`、`gh/`、`.vscode/` 这些是分析工作产物。
- VS2019 生成的 `.vcxproj`、`.filters`、`.sln`（VS2022 需要重新生成）。

## 十一、执行中遇到的问题

### 11.1 CMake 缓存

这次失败不是 CMake 缓存没有刷新。`gh/_build.bat` 每次都执行：

```text
cmake --fresh -G Ninja -DCMAKE_BUILD_TYPE=Release -S funq\funq\server -B funq\gh\_test_build2
cmake --build funq\gh\_test_build2
```

日志显示每次都重新生成了 `Automatic MOC` 和目标文件。失败位置是源代码编译错误，修正后重新配置和编译均通过。

### 11.2 `pick.h` 与 Qt MOC

一次处理中用 PowerShell 写入 `pick.h`/`pick.cpp` 时产生了 UTF-16 文件。Qt 的 `AUTOMOC` 没有正常识别 `pick.h` 中的 `Q_OBJECT`，因此没有生成 `moc_pick.cpp`。处理结果如下：

- 将文件恢复为 UTF-8 编码。
- 保留 `pick.h` 中的 `Q_OBJECT`，没有删除它。
- 用 `cmake --fresh` 重新生成后确认 `moc_pick.cpp` 存在。
- Qt 6.11.2 下重新编译成功。

这说明 `pick.h` 不能因为拆分或修复 MOC 而去掉 `Q_OBJECT`。

### 11.3 PR #17 拆分后的依赖错误

首次编译 `4d45b78` 的工作区改动时出现了真实的 C++ 错误：

- `dragndropresponse.cpp` 和 `shortcutresponse.cpp` 仍使用已经移出 `player.h` 的 `WidgetLocatorContext`。
- `player_utils.cpp` 和 item model 文件使用 `objectPath`，但没有引入或使用 `ObjectPath` 命名空间。
- graphics item 文件使用 `graphicsItemId`、`graphicsItemFromId` 和 `QBuffer`，但缺少对应命名空间和头文件。

已分别加入 `player_utils.h`、`ObjectPath` 作用域和 `QBuffer`，随后全新构建通过。这些修复已经包含在 `4d45b78` 中。

### 11.4 当前仍有的警告

构建成功但仍有两个值得记录的警告来源：

- `player_commands_itemmodel.cpp` 中 `logicalIndex` 可能未初始化，MSVC 报 `C4701`。
- 原有 `funq.cpp`/`pick.cpp` 中调用 `getenv`，MSVC 报 `C4996`。

本轮没有把与合并目标无关的警告改成错误，也没有擅自修改这些旧逻辑。CMake 配置中 `WrapVulkanHeaders` 未找到，但 QtQuick、核心库和目标库均正常生成。

### 11.5 Git 工作区权限提示

目标仓库和 `retsubhtym` 目录的文件所有者与当前运行账户不同，Git 曾提示 `detected dubious ownership`。使用仓库对应的 `safe.directory` 参数后，读取、暂存和提交都成功；这不是代码或编译问题。
