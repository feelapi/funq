# funq 各 Fork 改动分析

## 总览

当前目录下有 1 个原始仓库和 4 个 fork。原始仓库路径是 `funq`，其余 4 个 fork 都是在 `funq v1.2.0` 基础上做的二次开发。

| 目录 | 角色 | 源文件数 | 修改 | 新增 | 缺失 | 版本号 | 核心主题 |
|---|---|---:|---:|---:|---:|---|---|
| `funq` | 上游原始 | 123 | — | — | — | 1.2.0 | 基准 |
| `grandag` | fork | 122 | 20 | 0 | 0 | 1.2.0 | Python 3 现代化重构 + 服务端两个新命令 |
| `jer-chao` | fork | 131 | 3 | 9 | 0 | 1.2.0 | Visual Studio / MSVC 构建适配 |
| `librepcb` | fork | 122 | 0 | 0 | 0 | 1.2.0 | 字节级完全一致，未做任何改动 |
| `retsubhtym` | fork | 127 | 18 | 5 | 0 | 1.5.1 | QML 测试增强 + pytest 插件 + 交互式高亮 picker |

文件统计口径：递归遍历每个目录，跳过所有 `.obj / .tlog / .pdb / .ilk / .exe / .dll / .recipe / .lastbuildstate / .cache / .idb / .lib` 等编译产物后，按 MD5 + 字节数比较得到的差异。

---

## 1. funq（原始仓库）

- 这是 OpenStack 风格的 Qt 功能测试框架（funq = "FUNctional Qt"），2019-08-12 发布的 1.2.0。
- 客户端用 Python + nose 插件，服务端 `libFunq` 用 C++/Qt 注入到被测进程，通过 TCP 协议通信。
- 支持 QWidget（含 QGraphicsView）和 QML（QQuickItem / QQuickWindow）两种对象树查找。
- 提供 `--pick` 模式：按住 Ctrl + Shift 单击窗口，把对象路径、属性打印到 stdout，方便录制测试脚本。
- 自带的 `tests-functionnal/funq-test-app` 是一个 Qt Widgets 的小测试程序。

仓库结构完全相同：

```
funq/
├── client/           # 客户端 Python 包（含 nose 插件）
├── server/           # 服务端 libFunq（C++）+ 启动脚本
├── doc-dev/          # 开发者文档（Sphinx）
├── tests-functionnal/# 功能测试
└── CHANGELOG.md / README.rst / .gitignore / .clang-format ...
```

---

## 2. grandag — Python 3 现代化重构 + 服务端小增强

### 改动文件清单（20 个）

```
.gitignore
client/doc/conf.py
client/funq/aliases.py
client/funq/client.py
client/funq/errors.py
client/funq/models.py
client/funq/noseplugin.py
client/funq/screenshoter.py
client/funq/testcase.py
client/funq/tests/test_client.py
client/funq/tests/test_noseplugin.py
client/funq/tests/test_screenshoter.py
client/funq/tools.py
doc-dev/conf.py
server/funq_server/runner.py
server/funq_server/runner_win.py
server/libFunq/player.cpp
server/libFunq/player.h
server/setup.py
tests-functionnal/test_retrieve_widget.py
```

### 主要改动

#### 2.1 客户端 Python 现代化（最核心改动）

把 1.x 风格的 Python 代码改成更地道的 Python 3 写法：

- **字符串格式**：所有 `%` 和 `.format()` 全部替换为 f-string。
  - 例：`"%s" % name` 变成 `f"{name}"`，`'{0}.png'.format(c)` 变成 `f'{c}.png'`，`"%r" % path` 变成 `f"{path}"`。
  - 涉及文件：aliases.py、client.py、errors.py（间接）、models.py、noseplugin.py、screenshoter.py、testcase.py、tools.py、runner.py、runner_win.py、setup.py。
- **去掉冗余 `object` 基类**：`class FunqClient(object):` 变成 `class FunqClient():`，`class TreeItem(object):` 变成 `class TreeItem():`，等等。
- **`super()` 无参调用**：`super(FunqPlugin, self).options(...)` 变成 `super().options(...)`。
- **删掉 docstring 后面多余的 `pass`**：如 `HooqAliasesKeyError`、`HooqAliasesInvalidLineError`。
- **删掉类与 docstring 之间多余的空行**：让代码更紧凑。
- **去掉字符串前的 `u''` 前缀**（Python 3 默认 unicode）：在 `client.py`、`aliases.py`、`test_retrieve_widget.py`、`tests/test_*.py`、`doc/conf.py`、`doc-dev/conf.py` 里出现。
- **去掉过时的 `# pylint: disable=W0142 / E1103`** 注释，因为修复后已经不需要了。
- **docstring 一句话统一加句号**：在 `tools.py` 里把单行 docstring 分成多行。

#### 2.2 `client/funq/tools.py`：`wait_for` 改成可以返回值

原版 `wait_for(func, ...)` 只能拿到布尔结果。如果回调里要返回数据，就得用 `wdata = [None]` 这种闭包变量中转。新版让回调返回 `(True, value)`，`wait_for` 会把 `value` 直接返回：

```python
def wait_for(func, timeout, timeout_interval):
    time_start = time.monotonic()
    while True:
        res = func()
        if res is True:
            return True
        if isinstance(res, tuple) and res[0] is True:
            return res[1]
        if time.monotonic() - time_start >= timeout:
            if isinstance(res, Exception):
                raise res
            raise TimeOutError()
        time.sleep(timeout_interval)
```

附带的副作用：把原来模块顶层的 `_which(program)` 函数挪进 `which(program)` 内部作为内层函数，更内聚。

#### 2.3 `client/funq/client.py`：用 `wait_for` 的新返回值，省掉闭包中转

```python
# 旧
wdata = [None]
def get_action():
    try:
        wdata[0] = self.send_command('widget_by_path', path=path)
        return True
    except FunqError as err:
        if err.classname != 'InvalidWidgetPath':
            raise
        return err
wait_for(get_action, timeout, timeout_interval)
action = Action.create(self, wdata[0])

# 新
def get_action():
    try:
        return True, self.send_command('widget_by_path', path=path)
    except FunqError as err:
        if err.classname != 'InvalidWidgetPath':
            raise
        return err
wdata = wait_for(get_action, timeout, timeout_interval)
action = Action(self, wdata)
```

`Action.create` / `Widget.create` 也都换成了构造函数。

顺带把一条法语错误信息翻译成英语：

```python
# 旧
"Pas de réponse de l'application testée - probablement un crash."
# 新
"No response from the tested application - probably a crash."
```

#### 2.4 `client/funq/models.py`：`TreeItem` / `TreeItems` 重写

| 项 | 旧 | 新 |
|---|---|---|
| 创建方式 | `@classmethod create(cls, client, data)` 工厂方法 | `__init__(self, client, data)` 构造函数 |
| 字典查找 | `cls.ITEM_CLASS` 类属性 | `BaseItems` 用 `__init_subclass__(item_class=...)` 自动绑定 |
| `iter()` | 自实现递归迭代 | `__iter__` 用生成器递归 yield，`iter()` 保留作向后兼容 |

`WidgetMetaClass` 也改成了实例化协议：新增 `__call__`，根据被测对象的 `classes` 列表自动派发到具体子类（如 `QPushButton`、`QComboBox`），不再依赖全局 `CPP_CLASSES` 字典。

整体方向是把 funq 的 Python 模型层从“类方法构造器”改成“真正的类实例”，更符合 Python 习惯，也方便子类化。

#### 2.5 服务端 C++ 新增 `actions_list` 命令

在 `player.cpp` 末尾新增 `Player::actions_list` 方法：

```cpp
QtJson::JsonObject Player::actions_list(const QtJson::JsonObject & command) {
    bool with_properties = command["with_properties"].toBool();
    QtJson::JsonObject result, resultAction;
    QList<QAction *> actions;
    if (command.contains("oid")) {
        ObjectLocatorContext ctx(this, command, "oid");
        if (ctx.hasError()) return ctx.lastError;
        for (QObject * obj : ctx.obj->children())
            actions += obj->findChildren<QAction *>();
    } else {
        for (QWidget * widget : QApplication::topLevelWidgets())
            actions += widget->findChildren<QAction *>();
    }
    for (QAction * action : actions) {
        dump_object(action, resultAction, with_properties);
        result[ObjectPath::objectName(action)] = resultAction;
    }
    return result;
}
```

- 客户端可传入 `oid` 限定到某个父对象，或者不传，列举整个应用所有 `QAction`。
- 在 `player.h` 里声明这个新方法。

#### 2.6 服务端 `widgets_list` 新增 `recursive` 参数

原来 `widgets_list` 默认递归遍历所有子控件，调用方没法关掉。新版把 `recursive_list_widget` 加了一个 `bool recursive` 参数，由命令里的 `"recursive"` 字段控制。当 `recursive=false` 时，只列出直接子级，避免在大型界面上 dump 几万个对象。

#### 2.7 `player.h` 重命名：`list_actions` → `list_commands`

```cpp
// 旧
QtJson::JsonObject list_actions(const QtJson::JsonObject & command);
// 新
QtJson::JsonObject list_commands(const QtJson::JsonObject & command);
```

实现没动，只是把名字改成符合实际语义（这个方法返回的是 Player 暴露的命令列表，不是 QAction）。

#### 2.8 `tools.py` / `runner_win.py` 的小修复

- `_which()` 在 Windows 下自动补 `.exe` 后缀（如果文件名还没有 `.exe`）。
- `runner_win.py` 中 DLL 注入失败的日志改成统一用十六进制：
  ```python
  # 旧
  message += "{} returned {}. ".format(function_name, return_value)
  # 新
  message += "{} returned 0x{:X}. ".format(function_name, return_value)
  ```
  顺手删掉了一段 `try/except TypeError` 的 fallback 代码（hex 格式本身已经支持 int）。

#### 2.9 `.gitignore` 增加两项

```
.qt
*.a
```

#### 2.10 测试用例小修

`tests-functionnal/test_retrieve_widget.py`：

```python
self.assertEqual(lbl.classes, [u'QLabel', u'QFrame', u'QWidget', u'QObject'])
# →
self.assertEqual(lbl.classes, ['QLabel', 'QFrame', 'QWidget', 'QObject'])
```

### 小结

grandag 没有引入任何新文件，也没有跑测试应用层面的功能，纯属“代码清洁”。最实质的服务端改动只有两个：新增 `actions_list` 命令，以及给 `widgets_list` 加 `recursive` 开关。

---

## 3. jer-chao — Visual Studio / MSVC 构建适配

### 改动文件清单（3 个源码 + 9 个 VS 工程文件）

```
M  server/CMakeLists.txt
M  server/libFunq/CMakeLists.txt
M  server/setup.py
+  server/funq.sln
+  server/ALL_BUILD.vcxproj
+  server/ALL_BUILD.vcxproj.filters
+  server/ZERO_CHECK.vcxproj
+  server/ZERO_CHECK.vcxproj.filters
+  server/libFunq/Funq.vcxproj
+  server/libFunq/Funq.vcxproj.filters
+  server/libFunq/FunqStatic.vcxproj
+  server/libFunq/FunqStatic.vcxproj.filters
+  server/Release/FunqStatic.lib
```

注意那 9 个新增文件都是 Visual Studio 自动生成的工程文件 / 编译产物（`.sln` / `.vcxproj` / `.filters` / `Release/FunqStatic.lib`），是 jer-chao 在 Windows 上用 VS 构建一次后留下来的。

### 主要改动

#### 3.1 `server/CMakeLists.txt`：MSVC 编译选项分支

```cmake
if(MSVC)
  # MSVC 使用 /W4 作为合理的严格级别
  add_compile_options(/W4)
  # 启用标准符合性检查
  add_compile_options(/permissive-)
else()
  # GCC/Clang 使用传统的 -Wall -Wextra -Wpedantic
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

if(BUILD_DISALLOW_WARNINGS)
  if(MSVC)
    # MSVC 使用 /WX 将警告视为错误
    add_compile_options(/WX)
  else()
    add_compile_options(-Werror)
  endif()
endif()
```

- GCC / Clang 走 `-Wall -Wextra -Wpedantic` 和 `-Werror`。
- MSVC 走 `/W4`（相当于较高严格等级）和 `/WX`（把警告当错误），并打开 `/permissive-` 让编译器按标准来。
- 注释是中文，说明作者是中文用户。

#### 3.2 `server/libFunq/CMakeLists.txt`：Windows 上输出文件名

```cmake
if(WIN32)
    set_target_properties(Funq PROPERTIES OUTPUT_NAME "libFunq")
endif()
```

让 `SHARED` 库在 Windows 下生成 `libFunq.dll` 而不是 `Funq.dll`，匹配 Python 端 `funq_server/runner.py` 找库的逻辑（`funqlib_name = 'libFunq.dll'`）。

#### 3.3 `server/setup.py`：用 cmake + VS 生成器替代 make

关键变化：

- 强制指定 VS 2019 生成器：
  ```python
  if IS_WINDOWS:
      cmake_cmd.extend(['-G', 'Visual Studio 16 2019', '-A', 'x64'])
  ```
- 构建动作：
  ```python
  if IS_WINDOWS:
      make_cmd = [self.cmake_path, '--build', '.', '--config', buildtype]
  else:
      make_cmd = [self.make_path]
  ```
- 库路径分支：
  ```python
  if IS_WINDOWS:
      src_path = os.path.join('libFunq', buildtype, self.funqlib_name)
  else:
      src_path = os.path.join('libFunq', self.funqlib_name)
  ```
  Windows 下 VS 生成器会按配置类型把 dll 放到 `libFunq/Release/libFunq.dll`，所以路径要多带一段 `Release`。
- 默认构建类型从 Debug 改成 Release（更友好于生产）：
  ```python
  if self.debug is None:
      env_debug = os.environ.get('FUNQ_DEBUG')
      if env_debug:
          self.debug = env_debug.lower() in ('1', 'true', 'yes')
      else:
          self.debug = False
  build_type = 'Debug' if self.debug else 'Release'
  print(f'Building {build_type} version...')
  ```
- `force` 清理分支也分平台：Windows 用 `cmake --build ... --clean-first`，其他用 `make clean`。
- 文件中带中文注释，例如 `✅ 修改：从 True 改为 None，表示未设置`、`# MSVC 使用 /W4 作为合理的严格级别`。

### 小结

jer-chao 完全没动 funq 的运行时逻辑，唯一目标就是把构建链路从“Unix + make + GCC”迁到“Windows + VS 2019 + MSVC”。提交时把生成的 `.sln` / `.vcxproj` / `Release/FunqStatic.lib` 一并提交，这本身不算干净（一般应该 gitignore 掉），但确实是它改动的一部分。

---

## 4. librepcb — 完全一致，没有任何改动

逐字节对比结果：

```
diffs: 0
extra in funq: []
extra in librepcb: []
```

`librepcb/funq` 这个目录的源文件和 `funq/` 完全相同，没有任何增删修改。最有可能的解释：LibrePCB 项目（一个开源 EDA 工具）某段时间调研过 funq，把它 clone 到自己的子目录里做参考，但还没开始定制。可以视为“未启用的 fork 副本”。

---

## 5. retsubhtym — QML 测试 + pytest + 交互式 picker，1.5.1

### 改动文件清单

```
M  .clang-format
M  .gitignore
M  README.rst
M  client/doc/localise_widget.rst
M  client/funq/__init__.py          # 版本号 1.2.0 → 1.5.1
M  client/funq/models.py
M  client/funq/tools.py
M  client/setup.py                  # 增加 pytest11 entry point
M  server/funq_server/__init__.py   # 版本号 1.2.0 → 1.5.1
M  server/funq_server/runner.py
M  server/libFunq/funq.cpp
M  server/libFunq/objectpath.cpp
M  server/libFunq/objectpath.h
M  server/libFunq/pick.cpp          # 大改
M  server/libFunq/pick.h            # 大改
M  server/libFunq/player.cpp        # 大改
M  server/libFunq/player.h          # 大改
M  tests-functionnal/funq.conf
+  client/funq/pytestplugin.py      # 新增 pytest 插件
+  tests-functionnal/test_focus_cycling.py
+  tests-functionnal/test_focus_cycling_qml.py
+  tests-functionnal/funq-qml-test-app/CMakeLists.txt
+  tests-functionnal/funq-qml-test-app/main.cpp
```

### 主要改动

#### 5.1 版本号升级 1.2.0 → 1.5.1

`client/funq/__init__.py` 和 `server/funq_server/__init__.py` 都改成 `__version__ = '1.5.1'`。

#### 5.2 新增 pytest 插件（最大亮点）

**新增文件**：`client/funq/pytestplugin.py`（约 230 行）。

把 funq 的 nose 插件完整重写成了 pytest 插件，提供了完全等价的命令行选项：

```
--with-funq / --funq         启用 funq 集成
--funq-conf                  配置文件（默认 funq.conf）
--funq-gkit                  图形工具包 profile
--funq-gkit-file             gkit 定义文件
--funq-attach-exe            注入用的 funq 可执行路径
--funq-trace-tests           测试开始/结束日志文件
--funq-trace-tests-encoding  日志编码
--funq-screenshot-folder     失败截图保存目录
--funq-snooze-factor         超时倍率
```

实现要点：

- 用 `pytest_addoption` 注册选项（每个选项都支持同名环境变量，比如 `NOSE_FUNQ_CONF`）。
- 用 `pytest_configure` / `pytest_unconfigure` 管理生命周期。
- 用 `pytest_runtest_setup` / `pytest_runtest_teardown` 输出“Starting test … / Ending test …”。
- 用 `pytest_runtest_makereport`（`hookwrapper=True`）钩到测试结果，失败时调用 `ScreenShoter` 截图，和原来 nose 插件的行为一致。
- 通过 `ApplicationRegistry.register_from_conf` 复用 funq 现有的应用注册机制。

**`client/setup.py`** 注册 entry point：

```python
entry_points={
    'nose.plugins.0.10': ['funq = funq.noseplugin:FunqPlugin'],
    'pytest11': ['funq = funq.pytestplugin'],
},
```

**`README.rst`** 增加一段说明，告诉用户用 pytest 跑测试的方法：

```
Running tests with pytest
=========================

pytest --with-funq --funq-conf path/to/funq.conf
```

#### 5.3 服务端 `player.cpp / player.h`：QML 操作大幅增强

新增和扩展的 player 命令：

- **`quick_items_find`**：按路径返回所有匹配的 QQuickItem 列表（旧版 `quick_item_find` 只返回第一个）。
  ```cpp
  QList<QQuickItem *> items = ObjectPath::findQuickItems(ctx.widget, path);
  // 把每个 item 的 oid / dump_object 输出到 result["items"] 数组里
  ```
- **`quick_item_find_by_property`**：递归搜索 QML 子树，找 `property_name == property_value` 的第一个 item。
  ```cpp
  static QQuickItem * _findQuickItemByProperty(QQuickItem * item,
                                                const QString & propName,
                                                const QString & propValue);
  ```
- **`quick_item_click`** 扩展：可选 `xpos` / `ypos`（不传就点中心）、可选 `mouseAction: "doubleclick"`。
- 新增 **`quick_item_key_click`** 和 **`quick_item_key_press`**：
  ```cpp
  void key_click(T * w, Qt::Key button, Qt::KeyboardModifiers modifier = Qt::NoModifier) {
      QTest::keyClick(w, button, modifier, 10);
  }
  void key_press(T * w, Qt::Key button, Qt::KeyboardModifiers modifier = Qt::NoModifier,
                 int durationMs = 800) {
      QTest::keyPress(w, button, modifier, 0);
      QThread::msleep(durationMs);
      QTest::keyRelease(w, button, modifier, 0);
  }
  ```
  `key_press` 会在 `QTest::keyPress` 和 `keyRelease` 之间 `QThread::msleep(durationMs)`，用来测试长按场景。

- 老的 `mouse_click` / `mouse_dclick` 模板加了 Qt6 分支：
  ```cpp
  #if QT_VERSION_MAJOR >= 6
      QTest::mouseClick(w, button, Qt::NoModifier, pos, 10);
  #else
      // 老的手工 postEvent 逻辑
  #endif
  ```
  之前 Qt6 上 quick_item_click 会直接返回 `Qt5Only` 错误，现在 Qt6 也能跑了。

- `active_widget` 增加了把所有窗口 raise / activate 的逻辑：
  ```cpp
  for (auto w : QApplication::allWindows()) w->raise();
  // ...
  if (QWindow *win = qobject_cast<QWindow *>(active)) win->requestActivate();
  else if (QWidget *w = qobject_cast<QWidget *>(active)) w->activateWindow();
  ```
  这是为了支持多应用同时跑的功能测试（见下文 focus cycling 测试）。

`objectpath.cpp / objectpath.h` 加了 `findQuickItems` 接口，返回 `QList<QQuickItem *>`（路径按 `::` 切分，逐层 `childItems()` 过滤）。

#### 5.4 服务端 `pick.cpp / pick.h`：交互式高亮 picker（最复杂改动）

这是整个 fork 里代码量变化最大的一处：`pick.cpp` 从 3.7 KB 涨到 21 KB，`pick.h` 从 2.4 KB 涨到 4.3 KB。

**核心新功能**：

1. **Hover 高亮**：鼠标在窗口上移动时，根据 Ctrl+Shift 是否按下，决定是否在被悬停的控件上画红色半透明矩形（`QColor(255, 0, 0, 127)`）。
2. **拦截点击**：在 Ctrl+Shift 按下时，鼠标 click 不传给应用，由 picker 自己消化（避免误触发业务逻辑）。
3. **几何信息输出**：打印控件时，除了原来的 path + properties，还输出：
   - `Object type: <meta class name>`
   - `Geometry`（兼容 QWidget 的 `geometry` property 以及 QML 的 `x/y/width/height`，对 QML 还会回退到 `implicitWidth/implicitHeight`）
   - 对 `QGraphicsView` 内部 `QGraphicsItem`，输出对应的 item id 和几何。
4. **Squish-like 行为**：每次输出之间插一行 `--------------` 分隔。
5. **Alt 修饰键过滤**：Ctrl+Shift+Alt 时，只把按钮类控件作为候选（`QQuickAbstractButton / QQuickButton / QQuickCheckBox / QQuickRadioButton / QQuickSwitch / QQuickMenuItem`，或类名含 "Button" 的 QQuickItem）。

**实现细节**：

- 新增抽象类 `HighlightOverlay`：
  ```cpp
  class HighlightOverlay {
  public:
      virtual ~HighlightOverlay() {}
      virtual void showRect(const QRect & globalRect) = 0;
      virtual void hide() = 0;
  };
  ```
- 两条具体实现：
  - `WidgetHighlightOverlay`：继承 `QWidget`，用 `paintEvent + QPainter` 画红色矩形。
  - `WindowHighlightOverlay`：继承 `QWindow`，用 `QBackingStore + QPainter` 在 QML 应用（只有 `QGuiApplication` 没有 `QApplication`）里画红色矩形。
- `Pick` 构造函数根据应用类型选择合适的 overlay：
  ```cpp
  if (qobject_cast<QApplication *>(QCoreApplication::instance())) {
      m_highlightOverlay = new WidgetHighlightOverlay();
  } else if (qobject_cast<QGuiApplication *>(QCoreApplication::instance())) {
      m_highlightOverlay = new WindowHighlightOverlay();
  }
  ```
- `handleEvent` 处理 `MouseMove / MouseButtonPress / MouseButtonRelease` 三类事件。
- `computeHighlightTarget` 内部用 `QApplication::widgetAt`（Widgets 应用）/ `QGuiApplication::topLevelAt`（QML 应用）找候选控件。

**funq.cpp 的小改动配合它**：

```cpp
bool Funq::eventFilter(QObject * receiver, QEvent * event) {
    bool handled = m_pick->handleEvent(receiver, event);
    if (handled) {
        event->accept();
    }
    return handled;
}
```

之前 picker 不管按不按都返回 `false` 表示事件未消费；现在 pick 真正可以吞掉事件，让应用收不到点击。

**文档同步更新** `client/doc/localise_widget.rst`，描述新的输出格式和高亮行为。

#### 5.5 客户端 `models.py`：配合服务端的 Python API 扩展

- `QuickItem.click(xpos=-1, ypos=-1)`：坐标参数化。
- `QuickItem.dclick()`：双击。
- `QuickItem.key_click(key, modifiers)` / `QuickItem.key_press(key, modifiers, duration)`：把 `key` 在 `QtKeyDict` 里查 hex，`modifiers` 在 `QtKeyboardModifierDict` 里查 hex，再发给服务端 `quick_item_key_click` / `quick_item_key_press`。
- `QuickWindow.find_item_by_property(name, value)`：调服务端 `quick_item_find_by_property`。
- `QuickWindow.items(alias=None, path=None)`：调服务端 `quick_items_find`。
- 顺带把原来用 `wait_for` 的 `TreeItem` 类也改成直接构造函数调用（与 grandag 方向一致）。

#### 5.6 客户端 `tools.py`：新增键位映射字典

文件从 3.7 KB 涨到 10 KB，新增两个字典：

- **`QtKeyDict`**：把 Qt 的 `Qt::Key` 枚举名字映射到 hex 码，覆盖：
  - ASCII 可打印字符（`Key_A → 0x41`，`Key_0 → 0x30`，等等）
  - Latin-1 补充字符（`Key_nobreakspace → 0x0a0`，`Key_ydiaeresis → 0x0ff`）
  - Qt 自定义键（`Key_Escape → 0x01000000`，`Key_F1 → 0x01000030`，`Key_Super_L → 0x01000053`，一直到 `Key_unknown → 0x01ffffff`）
- **`QtKeyboardModifierDict`**：`Qt::KeyboardModifier` 映射到 hex 码：
  ```python
  QtKeyboardModifierDict = {
      "NoModifier": "0x00000000",
      "ShiftModifier": "0x02000000",
      "ControlModifier": "0x04000000",
      "AltModifier": "0x08000000",
      "MetaModifier": "0x10000000",
      "KeypadModifier": "0x20000000",
      "GroupSwitchModifier": "0x40000000",
      "KeyboardModifierMask": "0xfe000000",
  }
  ```

**⚠️ 这个文件有一个 bug**：文件头几行的引号没对齐：

```python
# 旧（funq）
# -*- coding: utf-8 -*-
# Copyright: SCLE SFE
# Contributor: Julien Pagès <j.parkouss@gmail.com>

# 新（retsubhtym）
# -*- coding: "utf-8 -*-            ← 前面多了一个 "，后面少了 "
# Copyright: "SCLE SFE              ← 前面多了一个 "
# Contributor: "Julien Pagès <...   ← 前面多了一个 "
```

Python 在找不到合法 coding 声明时会回落到 UTF-8 默认，所以不会直接 SyntaxError，但这个 header 实际上是非法的。如果未来某个工具要按 coding 声明去校验/转换文件，就会出错。建议上游合并前修一下。

#### 5.7 `tests-functionnal/funq.conf`：新增 QML 应用配置

```ini
[app_test_2]
executable = ./funq-test-app/funq-test-app
funq_port = 0
cwd = .
aliases = app_test.alias
screenshot_on_error = 1

[app_qml_test]
executable = ./funq-qml-test-app/build/funq-qml-test-app
funq_port = 0
cwd = .
screenshot_on_error = 1

[app_qml_test_2]
executable = ./funq-qml-test-app/build/funq-qml-test-app
funq_port = 0
cwd = .
screenshot_on_error = 1
```

`funq_port = 0` 让服务端自动挑一个空闲端口，`funq_server/runner.py` 会读 `FUNQ_PORT` 环境变量，正好和下文 `MultiFunqTestCase` 多应用并行测试的设计配合。

顺手删掉了原来的 `executable_stdout = NULL / executable_stderr = NULL`，让标准输出 / 标准错误继续输出到控制台（方便调试）。

#### 5.8 新增 `tests-functionnal/test_focus_cycling.py` / `test_focus_cycling_qml.py`

两个测试都继承 `MultiFunqTestCase`，用 `__app_config_names__` 一次启动多个被测应用，反复切换焦点来验证多应用并存时的对象树行为。

```python
class TestFocusCycling(MultiFunqTestCase):
    __app_config_names__ = ['app_test', 'app_test_2']

    def test_cycle_focus_between_two_apps(self):
        app1 = self.funq['app_test']
        app2 = self.funq['app_test_2']
        for _ in range(10):
            w1 = app1.active_widget()
            self.assertIsNotNone(w1, "app1 should have an active window")
            w2 = app2.active_widget()
            self.assertIsNotNone(w2, "app2 should have an active window")
            # ...
```

这正好呼应了 `player.cpp` 中 `active_widget` 里新加的 `raise / requestActivate / activateWindow` 逻辑。

#### 5.9 新增 `tests-functionnal/funq-qml-test-app/`（CMake + main.cpp）

最小可用的 Qt QML 测试程序：

- `CMakeLists.txt`：用 `find_package(Qt ... Quick QuickControls2)`，要求 C++17，开启 `CMAKE_AUTOMOC`。
- `main.cpp`：内嵌一段 QML 源（`R"QML(...)QML"`），创建一个 `ApplicationWindow`，里面有两个 `Button`：`btn_one` / `btn_two`，`objectName` 都被显式命名（方便 funq 路径定位）。

这是用来验证 `quick_items_find` / `quick_item_find_by_property` 等新命令的最小 demo 应用。

#### 5.10 `.gitignore`：大改

- 删掉了 funq 原来自带的精简 gitignore。
- 用 gitignore.io 风格的“全家桶”：把 Qt / CMake / Ninja / Python / C++ / PyInstaller / Django / VirtualEnv 等模板全部拼进去，结果从 462 字节涨到 4675 字节。

#### 5.11 `.clang-format`：精简

删掉了 `RawStringFormats` 那段（funq 里有但其实没用上）：

```diff
- RawStringFormats:
-   - Delimiter:       pb
-     Language:        TextProto
-     BasedOnStyle:    google
```

### 小结

retsubhtym 是四个 fork 里动作最大的一个，单独把版本号推到 `1.5.1`。它围绕三个方向：

1. **QML 测试能力**：键盘事件、坐标点击、按属性查找、批量查找、Qt6 兼容。
2. **多应用并行**：focus cycling 测试 + `MultiFunqTestCase` + 服务端 raise / activate 窗口。
3. **pytest 集成**：把 nose 插件整套功能移植到 pytest，让现代 Python 用户用得顺手。

交互式 picker 的 Squish-like 高亮是这个 fork 的招牌功能（按住 Ctrl+Shift 鼠标 hover 时实时框红框），实现上同时支持 Widgets 应用和 QML 应用两套路径。

---

## 横向对比

| 维度 | grandag | jer-chao | librepcb | retsubhtym |
|---|---|---|---|---|
| 版本号变化 | 无 | 无 | 无 | 1.2.0 → 1.5.1 |
| 运行时功能新增 | `actions_list`、`widgets_list` 的 `recursive` 参数 | 无 | 无 | `quick_items_find` / `quick_item_find_by_property` / `quick_item_key_click` / `quick_item_key_press` / QTest 键盘鼠标、Qt6 兼容、Picker 高亮 |
| 测试框架 | 仍是 nose | 仍是 nose | 仍是 nose | **新增 pytest 插件** |
| 构建平台 | Linux / GCC 保持 | **新增 Windows MSVC / VS 2019 支持** | 与上游同 | 与上游同 |
| 客户端 Python 风格 | 全面 Python 3 现代化 | 不变 | 不变 | 在 `models.py / tools.py` 部分现代化 + 加键位字典 |
| 服务端 C++ 改动 | 小（player.cpp） | 无 | 无 | 大（player.cpp + pick.cpp + objectpath.cpp + funq.cpp） |
| 是否提交构建产物 | 否 | **是**（`.sln` / `.vcxproj` / `Release/FunqStatic.lib`） | 否 | 否 |
| 是否新增 demo 应用 | 否 | 否 | 否 | **新增 QML 测试程序** |
| 已知缺陷 | 无 | 提交了编译产物 | 无 | `tools.py` 顶部编码声明引号不匹配 |

简单一句话概括四个 fork：

- **grandag**：代码清洁 + 两个 server 命令。
- **jer-chao**：只为了在 Windows / VS 上能编译。
- **librepcb**：还没开始改。
- **retsubhtym**：把 funq 升级成了一个支持 QML 键盘交互 + pytest + 多应用并行 + 交互式高亮 picker 的现代化测试框架。
