# funq Python 客户端

## 简介

`funq` Python 客户端用于编写 Qt 应用的功能测试。它通过 `libFunq` 提供的本地 TCP 服务与正在运行的 Qt 应用通信，可以查找和操作 QWidget、QQuickWindow、QQuickItem、模型和动作。

本目录是 `funq` 项目的 Python 部分。服务端负责将 `libFunq` 注入 Qt 应用，客户端负责启动测试应用、建立连接并提供 Python API。

项目使用 CeCILL v2.1 许可证，详细内容见 [LICENCE.txt](LICENCE.txt)。项目主页：<https://github.com/parkouss/funq>。

## 依赖

- Python 3.5 或更高版本。
- 与被测试应用使用相同 Qt 主版本编译的 `funq-server`。
- `nose` 用于保留旧测试入口，`pytest` 用于推荐的 pytest 插件入口。
- Windows 环境需要可用的 CMake、Visual Studio 和 Qt 开发环境；Linux 或 macOS 需要对应的 Qt 编译工具链。

依赖列表位于 [requirements.txt](requirements.txt)。

## 安装

在本目录执行：

```shell
python -m pip install -r requirements.txt
python -m pip install .
```

开发期间可以使用可编辑安装：

```shell
python -m pip install -e .
```

同时需要安装并构建 `funq-server`。安装后，命令行中应能找到 `funq` 注入程序；如果它不在 `PATH` 中，可以在测试命令中通过 `--funq-attach-exe` 指定完整路径。

## 快速集成

下面的流程把一个已有 Qt 应用接入功能测试。

### 1. 准备服务端

使用和目标 Qt 应用相同的 Qt 版本构建 `funq-server`，并确认可以运行：

```shell
funq --version
```

Qt 版本不一致时，注入库可能无法加载。Windows 下建议使用 VS2022 和 CMake 构建服务端。

### 2. 创建配置文件

在测试目录创建 `funq.conf`：

```ini
[demo]
executable = /absolute/path/to/demo-application
funq_port = 0
cwd = /absolute/path/to/application-directory
screenshot_on_error = 1
```

`executable` 是 Qt 应用的路径，`funq_port = 0` 表示让系统选择可用端口。Windows 示例可以使用反斜杠路径，也可以使用正斜杠路径。

如果希望通过别名查找控件，可以增加别名文件，例如 `demo.alias`：

```text
main_button = MainWindow::QPushButton
```

然后在配置节中加入：

```ini
aliases = demo.alias
```

### 3. 编写测试

创建 `test_demo.py`：

```python
from funq.testcase import FunqTestCase


class TestDemo(FunqTestCase):
    __app_config_name__ = 'demo'

    def test_main_window(self):
        window = self.funq.active_widget()
        self.assertIsNotNone(window)

        button = self.funq.widget(alias='main_button')
        button.click()
```

`FunqTestCase` 会根据 `__app_config_name__` 启动配置中的应用，测试结束后负责关闭连接和进程。需要同时操作多个应用时，可以使用 `MultiFunqTestCase` 和 `__app_config_names__`。

### 4. 运行测试

推荐使用 pytest：

```shell
python -m pytest --with-funq --funq-conf funq.conf test_demo.py
```

如果 `funq` 不在 `PATH` 中：

```shell
python -m pytest --with-funq \
    --funq-conf funq.conf \
    --funq-attach-exe /absolute/path/to/funq \
    test_demo.py
```

项目仍保留 nose 插件，旧测试可以继续使用：

```shell
nosetests --with-funq --funq-conf funq.conf test_demo.py
```

### 5. 操作 QML 项目

对于 QML 应用，`active_widget()` 通常返回 `QuickWindow`，可以通过对象 ID 查找 QML 项目：

```python
window = self.funq.active_widget()
button = window.item(id='submitButton')
button.click()

for child in button.children(recursive=True).iter():
    print(child.properties())
```

当多个项目具有相同路径时，可以使用 `window.items(path='...')` 获取所有匹配项目；也可以使用 `window.find_item_by_property()` 按 QML 属性查找项目。

## 常用客户端 API

直接连接一个已经运行并监听 `9999` 端口的应用：

```python
from funq.client import FunqClient


client = FunqClient(host='127.0.0.1', port=9999)
window = client.active_widget()
window.item(id='submitButton').click()
```

常用操作包括：

- `client.widget(...)`：按路径或别名查找 QWidget。
- `client.active_widget()`：获取当前活动窗口。
- `widget.click()`、`widget.dclick()`：模拟鼠标操作。
- `widget.properties()`、`widget.set_property(...)`：读取或修改 Qt 属性。
- `quick_window.item(...)`、`quick_window.items(...)`：查找 QML 项目。
- `quick_item.children(recursive=True)`：读取 QML 子项目。
- `client.take_screenshot(...)`：保存当前应用截图。

## 配置项

pytest 插件支持以下常用参数：

- `--with-funq`：启用 funq 集成。
- `--funq-conf`：指定配置文件，默认是当前目录下的 `funq.conf`。
- `--funq-attach-exe`：指定 funq 注入程序。
- `--funq-gkit` 和 `--funq-gkit-file`：选择图形工具包别名配置。
- `--funq-screenshot-folder`：指定失败截图目录。
- `--funq-snooze-factor`：调整内部等待时间。

这些参数也可以通过对应的 `NOSE_FUNQ_*` 环境变量设置。

