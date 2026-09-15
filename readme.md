# funq

`funq` 是一个使用 Python 为 Qt 应用编写功能测试的工具，同时支持
QWidget 应用和 QML 应用。测试代码通过 TCP 连接访问被测试应用中的
`libFunq` 服务，可以查找控件、读取和修改属性、模拟鼠标键盘操作，
也可以操作 QML 项目、模型和 QAction。

项目地址：<https://github.com/parkouss/funq>

## 许可证

`funq` 使用 CeCILL v2.1 许可证。完整条款见 [LICENCE.txt](LICENCE.txt)。

许可证的限制主要影响 `funq` 源码或编译产物的再分发。如果被测试项目的
许可证与 CeCILL v2.1 不兼容，不应把 `funq` 随项目源码或发布的二进制文件
一起交付；在测试环境中使用源码或链接 `funq` 库通常不受这个问题影响。
实际分发前请以许可证原文和项目的法律要求为准。

## 项目组成

`funq` 由服务端和 Python 客户端两部分组成：

- **funq-server**：C++ 服务端，包含 `libFunq` 动态库和 `funq` 注入命令。
  注入命令启动目标 Qt 程序，并让程序加载 `libFunq`；动态库在目标程序
  内启动 TCP 服务，处理来自 Python 客户端的操作请求。
- **funq Python 客户端**：负责读取 `funq.conf`、启动或连接目标程序，
  并提供 `FunqClient`、`FunqTestCase` 和各类 QWidget/QML 操作对象。
  客户端同时提供 pytest 和 nose 插件。

测试程序、`libFunq` 和 Python 客户端应使用兼容的 Qt 主版本。例如，使用
Qt 6 编译的目标程序应配套使用 Qt 6 构建的 `funq-server`。Qt 主版本、
编译器和目标架构不一致时，注入库可能无法加载或无法正常通信。

## 最小示例

先用 `funq` 启动目标程序，并指定服务监听地址和端口：

```shell
funq --host 127.0.0.1 --port 9000 path/to/YourApp
```

然后在另一个 Python 进程中连接并操作控件：

```python
from funq.client import FunqClient


client = FunqClient(host="127.0.0.1", port=9000, aliases="demo.alias")
try:
    button = client.widget(alias="main_button")
    button.click()
finally:
    client.close()
```

上面的 `main_button` 需要在别名文件中定义。没有别名时，可以直接传入
控件路径：

```python
button = client.widget(path="mainWindow::QPushButton")
```

`--host 0.0.0.0` 可以让其他机器连接，但会扩大服务的可访问范围；只在
本机测试时建议使用 `127.0.0.1`。

## 安装

### 从 PyPI 安装

服务端安装时会编译 C++ 部分，客户端安装时会安装 Python 测试接口：

```shell
python -m pip install funq-server
python -m pip install funq
```

`funq-server` 构建需要 CMake、Qt 开发包和对应的 C++ 编译器。Windows
环境建议使用 Visual Studio 2022、CMake 和 Qt 6；Linux、macOS 使用对应
平台的 Qt 和 C++ 工具链。

### 从源码安装

在仓库根目录执行：

```shell
cd server
python -m pip install .

cd ../client
python -m pip install -r requirements.txt
python -m pip install .
```

开发期间可以使用可编辑安装：

```shell
cd server
python -m pip install -e .

cd ../client
python -m pip install -e .
```

### Windows、VS2022 和 Qt 6

服务端的 `setup.py` 使用 CMake 构建 `libFunq`。在 PowerShell 中，可以
先指定 Qt 版本和 Qt 安装目录，再安装服务端：

```powershell
$env:FUNQ_QT_MAJOR_VERSION = "6"
$env:CMAKE_PREFIX_PATH = "C:/Qt/6.0.0/msvc2022_64"
cd server
python -m pip install .
```

如果 CMake 不在 `PATH` 中，可以设置 `FUNQ_CMAKE_PATH`；非 Windows 环境
还可以用 `FUNQ_MAKE_PATH` 指定构建工具。也可以直接使用 CMake 检查或
构建服务端：

```shell
cmake -S server -B build -DQT_MAJOR_VERSION=6 \
    -DCMAKE_PREFIX_PATH=C:/Qt/6.0.0/msvc2022_64 -DBUILD_TESTS=ON
cmake --build build --config Release
```

Qt 安装目录和生成器必须与本机实际环境一致。更换 Qt 版本、编译器或
生成器后，如果 CMake 仍使用旧配置，应删除构建目录中的 CMake 缓存后
重新配置。

## 用配置文件集成测试

推荐让测试框架通过 `funq.conf` 启动被测试程序。配置文件是 INI 格式，
每个节对应一个应用；每个节至少需要 `executable`：

```ini
[demo]
executable = C:/work/demo/demo.exe
args = --test-mode
cwd = C:/work/demo
funq_port = 0
aliases = demo.alias
screenshot_on_error = yes
executable_stdout = NULL
executable_stderr = NULL
timeout_connection = 10
```

重要配置项如下：

- `executable`：目标程序路径。相对路径相对于 `funq.conf` 所在目录。
- `args`：启动目标程序时追加的命令行参数。
- `cwd`：目标程序的工作目录。相对路径同样相对于配置文件目录。
- `funq_port`：服务端端口。设置为 `0` 时，启动本地目标程序前由系统选取
  可用端口；多个应用可以都使用 `0`。
- `aliases`：控件或 QML 项目的别名文件路径。
- `timeout_connection`：连接服务端的超时时间，单位为秒，默认是 10 秒。
- `screenshot_on_error`：设为 `yes` 或 `1` 后，测试失败时保存截图。
- `executable_stdout`、`executable_stderr`：目标程序输出文件；设为
  `NULL` 时丢弃对应输出。
- `attach`：默认启用外部注入。设为 `no` 或 `0` 时，表示目标程序已经
  编译了 `libFunq`，客户端只通过环境变量激活它。
- `with_valgrind`、`valgrind_args`：Linux 等平台调试内存问题时使用。

别名文件把稳定的测试名称映射到 Qt 对象路径。例如：

```text
main_button = mainWindow::QWidget::btnTest
```

测试中可以使用 `self.funq.widget(alias="main_button")`，这样控件层级
调整时只需修改别名文件。找不到控件时，可以先调用
`self.funq.widgets_list(with_properties=True)` 查看对象树和属性。

### 连接已经运行的程序

如果程序已经由 `funq` 启动，或程序自身已经编译并启动了 `libFunq`，
可以用 `socket://` 配置为连接模式，不再由测试框架启动它：

```ini
[running_demo]
executable = socket://127.0.0.1
funq_port = 9000
```

也可以把 `socket://127.0.0.1` 换成目标机器的地址。远程连接前应确认
网络和防火墙设置，并避免把没有访问控制的测试服务暴露到不可信网络。

## 继承 FunqTestCase 编写测试

### 单个应用

测试类应继承 `funq.testcase.FunqTestCase`，并通过
`__app_config_name__` 指定 `funq.conf` 中的节名：

```python
from funq.testcase import FunqTestCase


class TestLogin(FunqTestCase):
    __app_config_name__ = "demo"

    def test_login_button(self):
        window = self.funq.active_widget()
        self.assertIsNotNone(window)

        button = self.funq.widget(alias="main_button")
        self.assertTrue(button.properties()["enabled"])
        button.click()
```

运行测试时，插件会根据配置启动目标程序、连接 `libFunq`，并把客户端
放到 `self.funq`。每个测试结束后，`FunqTestCase` 会关闭连接并结束由
它启动的目标程序；连接到 `socket://` 的程序不会由它负责启动或关闭。

如果需要在项目中复用公共操作，可以继续继承 `FunqTestCase`，建立自己
的测试基类：

```python
from funq.testcase import FunqTestCase


class DemoTestCase(FunqTestCase):
    __app_config_name__ = "demo"

    def click_main_button(self):
        self.funq.widget(alias="main_button").click()

    def status_text(self):
        status = self.funq.widget(path="mainWindow::statusBar::QLabel")
        return status.properties()["text"]


class TestWorkflow(DemoTestCase):
    def test_workflow(self):
        self.click_main_button()
        self.assertEqual(self.status_text(), "Ready")
```

子类会继承基类的 `__app_config_name__` 和公共方法，也可以在需要时覆盖
`__app_config_name__` 使用另一个配置节。若自定义 `setUp` 或 `tearDown`，
应先调用父类实现，避免跳过 funq 的连接和清理流程：

```python
class TestWithSetup(DemoTestCase):
    def setUp(self):
        super().setUp()
        self.click_main_button()
```

`self.funq` 只应在测试实例已经完成 `setUp` 后使用。测试断言建议使用
`unittest.TestCase` 提供的 `assertEqual`、`assertTrue`、`assertIsNotNone`
等方法，以便测试框架正确报告失败。

### 同时测试多个应用

需要同时启动多个应用时，继承 `MultiFunqTestCase`，并指定配置节名称
列表。`self.funq` 此时是以配置节名为键的字典：

```python
from funq.testcase import MultiFunqTestCase


class TestTwoApplications(MultiFunqTestCase):
    __app_config_names__ = ("editor", "viewer")

    def test_exchange_data(self):
        editor = self.funq["editor"]
        viewer = self.funq["viewer"]

        editor.widget(alias="copy_button").click()
        self.assertEqual(
            viewer.widget(alias="status_label").properties()["text"],
            "Copied",
        )
```

`editor` 和 `viewer` 应分别出现在 `funq.conf` 中。为了避免固定端口冲突，
本地启动的多个应用可以分别将 `funq_port` 设为 `0`。

### 参数化测试和跳过测试

`FunqTestCase` 也支持 `parameterized`、`with_parameters` 和 `todo`：

```python
from funq.testcase import FunqTestCase, parameterized


class TestValues(FunqTestCase):
    __app_config_name__ = "demo"

    @parameterized("one", "One")
    @parameterized("two", "Two")
    def test_status_value(self, expected):
        self.assertEqual(self.funq.widget(alias="status_label")
                         .properties()["text"], expected)
```

## QWidget 操作

`FunqClient.widget()` 返回 QWidget 或其派生类对应的 Python 对象。常用
操作包括：

```python
window = self.funq.active_widget()
button = self.funq.widget(alias="main_button")

button.click()
button.dclick()
button.keyclick("hello")
button.shortcut("Ctrl+S")
button.set_property("text", "保存")
button.resize(width=800, height=600)

properties = button.properties()
self.assertTrue(properties["visible"])
```

还可以通过 `self.funq.action(...)` 查找并触发 `QAction`，通过
`self.funq.widgets_list()` 获取完整 QWidget 树，通过
`self.funq.take_screenshot("screen.png")` 保存当前应用截图。`Widget.grab()`
可以只保存某个控件的图像。

## QML 操作

QML 应用通常通过 `active_widget()` 得到 `QuickWindow`，再按 QML 的
`id` 查找项目：

```python
from funq.testcase import FunqTestCase


class TestQmlButton(FunqTestCase):
    __app_config_name__ = "qml_demo"

    def test_button(self):
        quick_window = self.funq.active_widget()
        button = quick_window.item(id="submitButton")

        self.assertTrue(button.properties()["visible"])
        button.click()
```

`QuickWindow.item()` 还支持 `path` 和 `alias`。同一路径下有多个项目时，
可以调用 `quick_window.items(path="...")`；需要按属性查找时，可以调用
`quick_window.find_item_by_property("objectName", "submitButton")`。
QML 项目还支持递归读取子项目：

```python
root = quick_window.item(id="root")
for child in root.children(recursive=True).iter():
    print(child.properties())
```

QML 测试要求服务端构建时找到 Qt Quick 模块，并且 Qt 主版本与目标
QML 程序一致。

## 运行测试

### pytest

客户端安装后，pytest 可以自动发现 `funq` 插件；执行时用
`--with-funq` 开启集成：

```shell
python -m pytest --with-funq --funq-conf funq.conf
```

也可以只运行某个文件或某个测试：

```shell
python -m pytest --with-funq --funq-conf funq.conf test_login.py
python -m pytest --with-funq --funq-conf funq.conf \
    test_login.py::TestLogin::test_login_button
```

如果 `funq` 命令不在 `PATH` 中，显式指定注入程序：

```shell
python -m pytest --with-funq \
    --funq-conf funq.conf \
    --funq-attach-exe /path/to/funq
```

常用选项还包括：

- `--funq-gkit`、`--funq-gkit-file`：选择图形工具包及其别名配置。
- `--funq-screenshot-folder`：指定失败截图目录。
- `--funq-snooze-factor`：按比例调整内部等待时间。
- `--funq-trace-tests`：记录测试开始和结束信息。

这些选项也可以使用对应的 `NOSE_FUNQ_*` 环境变量。

### nose

旧测试可以继续使用 nose 插件：

```shell
nosetests --with-funq --funq-conf funq.conf
```

测试文件通常命名为 `test*.py`，测试类继承 `FunqTestCase` 或
`MultiFunqTestCase`，测试方法命名为 `test*`。

## 常见问题

### 连接超时

确认目标程序确实启动、`funq_port` 未被其他进程占用、测试使用的端口
与目标程序监听的端口一致。启动本地程序时可将 `funq_port` 设为 `0`，
连接已运行程序时必须填写实际端口。

### 找不到控件

先检查控件的 `objectName`、对象路径和别名文件。可以调用
`widgets_list(with_properties=True)` 查看服务端实际返回的对象树；QML
项目则检查 `id` 是否写在目标 QML 项目中，以及当前窗口是否是预期的
`QuickWindow`。

### 注入库无法加载

检查目标程序与 `libFunq` 的 Qt 主版本、编译器、架构和运行库是否兼容。
Windows 下还要确认 Qt 的 DLL 可以被目标程序找到，且服务端是用与目标
程序匹配的 VS2022/Qt 构建的。

## 兼容性

项目支持 Python 3.5 及以上版本，支持 Qt 5 和 Qt 6，并可运行在
GNU/Linux、macOS 和 Windows。当前 Windows 构建使用 CMake 和 Visual
Studio 2022；Qt 6 项目应使用相同 Qt 6 主版本构建服务端和被测试程序。

## 文档和贡献

完整文档见 <https://funq.readthedocs.io/>。欢迎通过 GitHub 提交 issue、
pull request 或项目关注来参与改进。

感谢 Yann De Poulpiquet 和 Riad Lezzar 编写最早的一批 funq 功能测试，
也感谢 Jean-Luc Rouzoul、Dominique Constant 和 Mickaël Guérin 对项目的
支持。

